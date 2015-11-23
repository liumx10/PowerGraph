import os
import sys

filename = sys.argv[1]
edges = {}
for line in open(filename, 'r').readlines():
    numbers = line.strip().split(' ')
    if edges.has_key(numbers[0]):
        edges[numbers[0]].append(numbers[1])
    else:
        edges[numbers[0]] = [ numbers[1] ]

    if edges.has_key(numbers[1]):
        edges[numbers[1]].append(numbers[0])
    else:
        edges[numbers[1]] = [ numbers[0] ]
f = open("facebook.adj", 'w')
for k in edges:
    edges[k] = list(set(edges[k]))
    f.write(k+" "+ str(len(edges[k])) + " ")
    for e in edges[k]:
        f.write(e+" ")
    f.write('\n')
f.close()


