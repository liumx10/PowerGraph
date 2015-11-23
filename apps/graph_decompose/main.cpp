#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <graphlab.hpp>
#include <graphlab/engine/omni_engine.hpp>
#include <math.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>

using namespace graphlab;

struct single_point{
	int area;
	int id;
	int depth;
	bool done;
	bool origin;
	single_point(){
		id = 0;
		area = -1;
		depth = 0;
		done = false;
		origin = false;
	}
	explicit single_point(int a){
		id = a;
		area = -1;
		depth = 0;
		origin = false;
		done = false;
	}
	void save(graphlab::oarchive &oarc) const{
		oarc << area << id << depth << done;
	}
	void load(graphlab::iarchive &iarc){
		iarc >> area >> id >> depth >> done;
	}
};

typedef graphlab::distributed_graph<single_point, graphlab::empty> graph_type;

bool line_parser(graph_type &graph, const std::string& filename, const std::string& textline){
	std::stringstream strm(textline);
	graphlab::vertex_id_type vid;
	strm >> vid;
	graph.add_vertex(vid, single_point(vid));
	while(1){
		graphlab::vertex_id_type other_vid;
		strm >> other_vid; 
		if (strm.fail())
			return true;
		graph.add_edge(vid, other_vid);
		graph.add_edge(other_vid, vid);
	}
	return true;
}
struct Gather{
	int area;
	int depth;
	Gather(){
		area = -1;
	}
	Gather& operator+=(const Gather& other){
		if (other.area > -1){
			area = other.area;
			depth = other.depth;
		}
		return *this;
	}

	void save(graphlab::oarchive& oarc) const{
		oarc<< area << depth;
	}
	void load(graphlab::iarchive& iarc){
		iarc >> area >> depth;
	}
};

class Decompose:
	public graphlab::ivertex_program<graph_type, Gather>,
	public graphlab::IS_POD_TYPE	
{
	public:
		edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex) const{
			return graphlab::IN_EDGES;
		}

		Gather gather(icontext_type& context, const vertex_type& vertex, edge_type& edge) const{
			Gather g;
			if (edge.source().data().done == true){
				g.area = edge.source().data().area;
				g.depth = edge.source().data().depth;
			}
			return g;
		}

		void apply(icontext_type& context, vertex_type& vertex, const Gather& total){
			if (vertex.data().done==true){
				return;
			}
			vertex.data().done = true;
			if (total.area < 0 || vertex.data().origin == true){
				vertex.data().area = vertex.id();
				vertex.data().depth = 0;
			}else{
				vertex.data().area = total.area;
				vertex.data().depth = total.depth + 1;
			}
		}

		edge_dir_type scatter_edges(icontext_type &context, const vertex_type& vertex) const{
			return graphlab::OUT_EDGES;
		}

		void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const{
			if (edge.target().data().done == false){
				context.signal(edge.target());
			}
		}
};

struct BFSGather{
	int depth;
	BFSGather(){
		depth = -1;
	}
	BFSGather& operator+= (const BFSGather& other){
		if (depth <other.depth){
			depth = other.depth;
		}
		return *this;
	}
	void save(graphlab::oarchive& oarc) const{
		oarc << depth;
	}
	void load(graphlab::iarchive& iarc){
		iarc >> depth;
	}
};

class BFSDiameter: public graphlab::ivertex_program<graph_type, BFSGather>,
	public graphlab::IS_POD_TYPE{
public:
	edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex)const{
		return graphlab::IN_EDGES;
	}
	BFSGather gather(icontext_type& context, const vertex_type& vertex, edge_type& edge) const{
		BFSGather g;
		if (edge.source().data().area == vertex.data().area && edge.source().data().done == true){
			g.depth = edge.source().data().depth;
		}
		return g;
	}
	void apply(icontext_type& context, vertex_type& vertex, const BFSGather& total){
		if (vertex.data().done == true){
			return;
		}
		vertex.data().done = true;
		vertex.data().depth = total.depth+1;
		//std::cout << vertex.data().diameter << " ";
	}
	edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const {
		return graphlab::OUT_EDGES;
	}
	void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
		if (edge.target().data().done == false && edge.target().data().area == vertex.data().area){
			context.signal(edge.target());
		}
	}
};

class SetOrigin:
	public graphlab::ivertex_program<graph_type, int>,
	public graphlab::IS_POD_TYPE{
public:
	edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex)const{
		return graphlab::NO_EDGES;
	}
	int gather(icontext_type& context, const vertex_type& vertex, edge_type& edge) const{
		return 1;
	}
	void apply(icontext_type& context, vertex_type& vertex, const int& total){
		vertex.data().origin = true;
	}
	edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const {
		return graphlab::NO_EDGES;
	}
	void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
	}
};

class ClearAll:
	public graphlab::ivertex_program<graph_type, int>,
	public graphlab::IS_POD_TYPE{
public:
	edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex)const{
		return graphlab::NO_EDGES;
	}
	int gather(icontext_type& context, const vertex_type& vertex, edge_type& edge) const{
		return 1;
	}
	void apply(icontext_type& context, vertex_type& vertex, const int& total){
		vertex.data().done = false;
		vertex.data().depth = -1;
		vertex.data().area = -1;
		vertex.data().origin = false;
	}
	edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const {
		return graphlab::NO_EDGES;
	}
	void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
	}
};

class ClearLaunch:
	public graphlab::ivertex_program<graph_type, int>,
	public graphlab::IS_POD_TYPE{
public:
	edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex)const{
		return graphlab::NO_EDGES;
	}
	int gather(icontext_type& context, const vertex_type& vertex, edge_type& edge) const{
		return 1;
	}
	void apply(icontext_type& context, vertex_type& vertex, const int& total){
		vertex.data().done = false;
		vertex.data().depth = -1;
		vertex.data().origin = false;
	}
	edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const {
		return graphlab::NO_EDGES;
	}
	void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
	}
};

int k; 
int n; 
int m;

bool select_node(const graph_type::vertex_type & vertex, std::vector<int> v){
	if (std::find(v.begin(), v.end(), vertex.id()) != v.end()){
		return true;
	}else{
		return false;
	}
}


struct GatherResult{
	std::map<int, std::vector<int> > areas;
	std::map<int, int> diameter;
	std::map<int, int> end_id;
	int max_diameter;

	GatherResult(): areas(){
	}
	GatherResult& operator += (const GatherResult& other){
		std::map<int, std::vector<int> > map = other.areas;
		for (std::map<int, std::vector<int> >::const_iterator iter = other.areas.begin(); iter!= other.areas.end(); ++iter){
			int key = iter->first;
			std::vector<int> vec = map[key];
			if (areas.find(key) == areas.end()){
				areas.insert(std::pair<int, std::vector<int> >(key, vec) );
			}else{
				for (unsigned int i=0; i<vec.size(); i++){
					int id = vec[i];
					areas[key].push_back(id);
				}
			}
		}
		
		for (std::map<int, int>::const_iterator iter = other.diameter.begin(); iter!= other.diameter.end(); ++iter){
			int key = iter->first;
			if (diameter.find(key) != diameter.end()){
				if (iter->second > diameter[key]){
					diameter[key] = iter->second;
					end_id[key] = other.end_id.find(key)->second;
				}
			}else{
				diameter[key] = iter->second;
				end_id[key] = other.end_id.find(key)->second;
			}
		}
		if (other.max_diameter > max_diameter){
			max_diameter = other.max_diameter;
		}
		return *this;
	}
	void save(graphlab::oarchive &oarc) const{
		oarc << areas << diameter << end_id << max_diameter;
	}
	void load(graphlab::iarchive &iarc){
		iarc >> areas >> diameter >> end_id >> max_diameter;
	}
};

GatherResult get_compose_result(const graph_type::vertex_type& vertex){
	GatherResult gr;
	gr.areas.insert(std::pair<int, std::vector<int> >(vertex.data().area, std::vector<int>(1, vertex.id()) ) );
	gr.diameter.insert(std::pair<int, int>(vertex.data().area, vertex.data().depth));
	gr.end_id.insert(std::pair<int, int>(vertex.data().area, vertex.id()));
	gr.max_diameter = vertex.data().depth;

	return gr;
}


void show_result(GatherResult &gr){
	std::cout << " =======================" << std::endl << "max diameter: "<< gr.max_diameter << std::endl;
	for (std::map<int, std::vector<int> >::const_iterator iter=gr.areas.begin(); iter != gr.areas.end(); iter++){
		int key = iter->first;
		std::cout << key << " diameter: " << gr.diameter[key]  << " end_id: " << gr.end_id[key] << " set size: " << gr.areas[key].size() << std::endl;
	}
}
int main(int argc, char** argv){
	graphlab::mpi_tools::init(argc, argv);
	graphlab::distributed_control dc;
	
	k = atoi(argv[1]);
	graph_type graph(dc);
	graph.load("facebook.txt", line_parser);
	graph.finalize();
	
	dc.cout() << "hello world" << std::endl;
	n = graph.num_vertices(); 
	m = graph.num_edges();
	printf("n: %d, m: %d, k: %d\n", n, m, k);
	int iterator = atoi(argv[2]);
	double T = 100.0, r = 0.9;

	std::vector<int> select_id;
	std::vector<int> cur_id;
	int s = k;
	while(s > 0){
		int new_id = rand()%n;
		if (std::find(select_id.begin(), select_id.end(), new_id) == select_id.end() ){
			s--;
			select_id.insert(select_id.begin(), new_id);
		}
	}
	cur_id = select_id;

	int cur_max_dia = 100000;

	graphlab::omni_engine<ClearLaunch> clear_engine(dc, graph, "sync");
	graphlab::omni_engine<Decompose> compose_engine(dc, graph, "sync");
	graphlab::omni_engine<BFSDiameter> bfs_engine(dc, graph, "sync");
	graphlab::omni_engine<SetOrigin> origin_engine(dc, graph, "sync");
	graphlab::omni_engine<ClearAll> clearall_engine(dc, graph, "sync");
	for(int i=0; i<iterator; i++){
		std::cout << "=========== clear ============ " << std::endl;	
		clearall_engine.signal_all();
		clearall_engine.start();
		
		std::cout << "=========== decompose =========" << std::endl;
		boost::function<bool(const graph_type::vertex_type&)> _fn(boost::bind(select_node, _1, select_id));
		graphlab::vertex_set start_set = graph.select(_fn);
		
		origin_engine.signal_vset(start_set);
		origin_engine.start();
		compose_engine.signal_vset(start_set);
		compose_engine.start();

		GatherResult gr = graph.map_reduce_vertices<GatherResult>(get_compose_result);
		std::vector<int> end_id;
		for (std::map<int, int>::const_iterator iter=gr.end_id.begin(); iter != gr.end_id.end(); iter++){
			end_id.push_back(iter->second);
		}
		show_result(gr);
	
		std::cout << "=========== clear ==============" << std::endl;	
		clear_engine.signal_all();
		clear_engine.start();
		std::cout << "=========== bfs =================" << std::endl;	
		boost::function<bool(const graph_type::vertex_type&)> _fn2(boost::bind(select_node, _1, end_id));
		graphlab::vertex_set start_set2 = graph.select(_fn2);
		
		origin_engine.signal_vset(start_set2);
		origin_engine.start();
		bfs_engine.signal_vset(start_set2);
		bfs_engine.start();

		gr = graph.map_reduce_vertices<GatherResult>(get_compose_result);
		show_result(gr);
		if (gr.max_diameter < cur_max_dia){
			cur_max_dia = gr.max_diameter;
			cur_id = select_id;
		}else{
			continue;
		}
	
		int s = 2;
		if (argc > 3){
			s = atoi(argv[3]);
		}
		while(s > 0){
			//select_id.pop_back();
			int new_id = rand()%n;
			std::cout << new_id << std::endl;
			if (std::find(select_id.begin(), select_id.end(), new_id) == select_id.end() ){
				select_id.pop_back();
				s--;
				select_id.insert(select_id.begin(), new_id);
			}
		}
		T = r*T;
	}

	graphlab::mpi_tools::finalize();
}
