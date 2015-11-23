/*************************************************************************
    > File Name: main.cpp
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: Sun Nov 15 13:17:25 2015
 ************************************************************************/
#include <stdlib.h>
#include <stdio.h>

#include<iostream>
#include <graphlab.hpp>
#include <graphlab/engine/omni_engine.hpp>
#include <math.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>

using namespace graphlab;

struct single_point{
	int area;
	int id;
	bool launch;
	bool done;
	int count;
	int diameter;
	int depth;
	bool branch;
	single_point():area(-1), id(0), launch(false), done(false), diameter(0), branch(false){

	}
	explicit single_point(int a){
		id = a;
		area = -1;
		launch = false;
		done = false;
		diameter = 0;
		depth = 0;
		branch =false;
	}
	void save(graphlab::oarchive &oarc) const{
		oarc << area << id << launch << done << count << diameter << depth;
	}
	void load(graphlab::iarchive &iarc){
		iarc >> area >> id >> launch >> done >> count >> diameter >> depth;
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

int k;
int n;
int m;

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
				edge.source().data().branch = true;
			}
			return g;
		}

		void apply(icontext_type& context, vertex_type& vertex, const Gather& total){
			if (vertex.data().done==true){
				return;
			}
			vertex.data().done = true;
			if (total.area < 0){
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
	int diameter;
	BFSGather(){
		depth = -1;
		diameter = -1;
	}
	BFSGather& operator+= (const BFSGather& other){
		if (depth <other.depth){
			depth = other.depth;
		}
		return *this;
	}
	void save(graphlab::oarchive& oarc) const{
		oarc << depth << diameter;
	}
	void load(graphlab::iarchive& iarc){
		iarc >> depth >> diameter;
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
		if (edge.source().data().area == vertex.data().area && edge.source().data().launch == true){
			g.depth = edge.source().data().diameter;
			edge.source().data().branch = true;
		}
		return g;
	}
	void apply(icontext_type& context, vertex_type& vertex, const BFSGather& total){
		if (vertex.data().launch == true){
			return;
		}
		vertex.data().launch = true;
		vertex.data().diameter = total.depth+1;
		//std::cout << vertex.data().diameter << " ";
	}
	edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const {
		return graphlab::OUT_EDGES;
	}
	void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
		if (edge.target().data().launch == false && edge.target().data().area == vertex.data().area){
			context.signal(edge.target());
		}
	}
};

struct GreedyGather{
	int diameter;
	int area;
	GreedyGather(){
		area = -1;
		diameter = -1;
	}
	GreedyGather& operator+= (const GreedyGather& other){
		if (diameter > other.diameter){
			diameter = other.diameter;
			area = other.area;
		}
		return *this;
	}
	void save(graphlab::oarchive& oarc) const{
		oarc << area << diameter;
	}
	void load(graphlab::iarchive& iarc){
		iarc >> area >> diameter;
	}
};

class Greedy:
	public graphlab::ivertex_program<graph_type, GreedyGather>,
	public graphlab::IS_POD_TYPE{
public:
	edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex)const{
		return graphlab::IN_EDGES;
	}
	GreedyGather gather(icontext_type& context, const vertex_type& vertex, edge_type& edge) const{
		GreedyGather g;
		g.area = edge.source().data().area;
		g.diameter = edge.source().data().diameter;
		return g;
	}
	void apply(icontext_type& context, vertex_type& vertex, const GreedyGather& total){
		//std::cout << vertex.data().diameter << " ";
		if (vertex.data().branch){
			return;
		}
		if (total.diameter < vertex.data().diameter){
			vertex.data().diameter = total.diameter + 1;
			vertex.data().area = total.area;
			std::cout << "change" << std::endl;
		}
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
		vertex.data().launch = false;
		vertex.data().diameter = -1;
		vertex.data().branch = false;
	}
	edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const {
		return graphlab::NO_EDGES;
	}
	void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
	}
};

class bfs_compose_writer{
	public:
		std::string save_vertex(graph_type::vertex_type v){
			std::stringstream strm;
			strm << v.id() << ", " <<  v.data().area << ", " << v.data().depth << "\n";
			return strm.str();
		}
		std::string save_edge(graph_type::edge_type e){
			return "";
		}
};

int select_number = 0;
bool select_node(const graph_type::vertex_type& vertex){
	int id = vertex.id();
	int step = n/k;
	if  (n%k !=0 && id >= step && id%step == 0){
		select_number++;
		return true;
	}
	if (n%k==0 && id%step ==0 ){
		select_number++;
		return true;
	}
	else{
		return false;
	}
}

bool select_endid(const graph_type::vertex_type& vertex, std::vector<int> key){
	if (std::find(key.begin(), key.end(), vertex.id()) != key.end()){
		return true;
	}else{
		return false;
	}
}

struct GatherResult{
	std::map<int, std::vector<int> > areas;
	std::map<int, int> diameter;
	std::map<int, int> end_id;
	std::map<int, int> depth;

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
		
		for (std::map<int, int>::const_iterator iter = other.depth.begin(); iter!= other.depth.end(); ++iter){
			int key = iter->first;
			if (depth.find(key) != depth.end()){
				if (iter->second > depth[key]){
					depth[key] = iter->second;
					end_id[key] = other.end_id.find(key)->second;
				}
			}else{
				depth[key] = iter->second;
				end_id[key] = other.end_id.find(key)->second;
			}
		}
		return *this;
	}
	void save(graphlab::oarchive &oarc) const{
	/*	oarc << areas.size();
		std::map<int, std::vector<int> > map = areas;
		for(std::map<int, std::vector<int> >::const_iterator iter=areas.begin(); iter!=areas.end(); iter++){
			int key = iter->first;
			oarc << key << map[key].size();
			for (unsigned int i=0 ;i<map[key].size(); i++){
				oarc << map[key][i];
			}
			std::map<int, int>::const_iterator _iter = diameter.find(key);
			oarc << _iter->second;
			_iter = depth.find(key);
			oarc << _iter->second;
			_iter = end_id.find(key);
			oarc << _iter->second;
		}*/
		oarc << areas << diameter << depth << end_id;
//		std::cout << "save gather result" << std::endl;
//		oarc << diameter << depth << end_id;
	}
	void load(graphlab::iarchive &iarc){
//		std::cout << "trying to load" << std::endl;
/*		areas.clear();
		int size;
		iarc  >> size;
		for (int i=0; i<size; i++){
			int _size, key;
			iarc >> key >> _size;
			std::vector<int> v;
			areas.insert(std::pair<int, std::vector<int> >(key, v));
			for (int j=0; j<_size; j++){
				int k;
				areas[key].push_back(k);
			}
			int _diameter, _depth, _end_id;
			iarc >> _diameter >> _depth >> _end_id;
			diameter.insert(std::pair<int, int>(key, _diameter));
			depth.insert(std::pair<int, int>(key, _depth));
			end_id.insert(std::pair<int, int>(key, _end_id));
		}*/
		iarc >> areas >> diameter >> depth >> end_id;
		std::cout << diameter << std::endl;
//		std::cout << "load gather result " << std::endl;
//		iarc >> diameter >> depth >> end_id;
	}
};

class SetDiameter:
	public graphlab::ivertex_program<graph_type, int>,
	public graphlab::IS_POD_TYPE{
public:
	static GatherResult gr;

	edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex)const{
		return graphlab::NO_EDGES;
	}
	int gather(icontext_type& context, const vertex_type& vertex, edge_type& edge) const{
		return 1;
	}
	void apply(icontext_type& context, vertex_type& vertex, const int &total){
		//std::cout << vertex.data().diameter << " "
		vertex.data().diameter = gr.diameter[vertex.data().area];
	}
	edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const {
		return graphlab::NO_EDGES;
	}
	void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
	}

	static void setGr(GatherResult _gr){
		gr = _gr;
	}
};

GatherResult SetDiameter::gr;

GatherResult get_compose_result(const graph_type::vertex_type& vertex){
	GatherResult gr;
	gr.areas.insert(std::pair<int, std::vector<int> >(vertex.data().area, std::vector<int>(1, vertex.id()) ) );
	gr.depth.insert(std::pair<int, int>(vertex.data().area, vertex.data().depth));
	gr.diameter.insert(std::pair<int, int>(vertex.data().area, vertex.data().diameter));
	gr.end_id.insert(std::pair<int, int>(vertex.data().area, vertex.id()));
	return gr;
}

void show_result(GatherResult &gr){
	for (std::map<int, std::vector<int> >::const_iterator iter=gr.areas.begin(); iter != gr.areas.end(); iter++){
		int key = iter->first;
		std::cout << key << " depth: " << gr.depth[key] << ", diameter: " << gr.diameter[key] <<  " end_id: " << gr.end_id[key] << " set size: " << gr.areas[key].size() << std::endl;
	}
}
int main(int argc, char** argv){
	graphlab::mpi_tools::init(argc, argv);
	graphlab::distributed_control dc;

	k = atoi(argv[1]);
	graph_type graph(dc);
	graph.load("facebook.txt", line_parser);
	dc.cout() << "hello world!\n ";

		graphlab::omni_engine<Decompose> engine(dc, graph, "sync");

		n = graph.num_vertices();
		m = graph.num_edges();
	
	graphlab::vertex_set start_set = graph.select(select_node);
		printf("n: %d, m: %d, select_number; %d\n", n, m, select_number);	
		engine.signal_vset(start_set);
		engine.start();

	GatherResult gr = graph.map_reduce_vertices<GatherResult>(get_compose_result);
	show_result(gr);
	std::vector<int> end_id;
	for (std::map<int, int>::const_iterator iter=gr.end_id.begin(); iter != gr.end_id.end(); iter++){
		end_id.push_back(iter->second);
	}

	graphlab::omni_engine<ClearLaunch> clear_engine(dc, graph, "sync");
	graphlab::omni_engine<BFSDiameter> engine2(dc, graph, "sync");
	graphlab::omni_engine<SetDiameter> set_diameter(dc, graph, "sync");
	
	boost::function<bool(const graph_type::vertex_type&)> fn( boost::bind(select_endid, _1, end_id)); graphlab::vertex_set start_set2 = graph.select(fn); 
		std::cout << "start set2 size: "<< graph.vertex_set_size(start_set2) << std::endl;
		engine2.signal_vset(start_set2);
		engine2.start();
	

		gr = graph.map_reduce_vertices<GatherResult>(get_compose_result);
		show_result(gr);
	
		int iterator= 100;
		if (argc > 2){
			iterator = atoi(argv[2]);
		}
		graphlab::omni_engine<Greedy> engine3(dc, graph, "sync");
		for (int i=0; i<iterator; i++){
			SetDiameter::setGr(gr);
			set_diameter.signal_all();
			set_diameter.start();

			engine3.signal_all();
			engine3.start();
			gr = graph.map_reduce_vertices<GatherResult>(get_compose_result);				
			
			clear_engine.signal_all();
			clear_engine.start();

			end_id.clear();
			for (std::map<int, int>::const_iterator iter=gr.end_id.begin(); iter != gr.end_id.end(); iter++){
				end_id.push_back(iter->second);
			}
			boost::function<bool(const graph_type::vertex_type&)> _fn(boost::bind(select_endid, _1, end_id));
			start_set = graph.select(_fn);

			graphlab::omni_engine<BFSDiameter> engine2(dc, graph, "sync");
			engine2.signal_vset(start_set);
			engine2.start();
			
			gr = graph.map_reduce_vertices<GatherResult>(get_compose_result);	
			end_id.clear();
			for (std::map<int, int>::const_iterator iter=gr.end_id.begin(); iter != gr.end_id.end(); iter++){
				end_id.push_back(iter->second);
			}
			boost::function<bool(const graph_type::vertex_type&)> fn( boost::bind(select_endid, _1, end_id));
			graphlab::vertex_set start_set2 = graph.select(fn);
	
			clear_engine.signal_all();
			clear_engine.start();
	
			engine2.signal_vset(start_set2);
			engine2.start();
	
			gr = graph.map_reduce_vertices<GatherResult>(get_compose_result);
			show_result(gr);		
		}
	
	graph.save("output", bfs_compose_writer(), false, true, false, 1);
	graphlab::mpi_tools::finalize();
}
