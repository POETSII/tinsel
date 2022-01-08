#!/bin/bash

mkdir -p working/100k

>&2 echo "Gen 2d"
bin/create_app_graph_2d_n4_mesh 316 316 > working/100k/2d_n4_mesh_100k.json
bin/create_app_graph_2d_n4_torus 316 316 > working/100k/2d_n4_torus_100k.json
bin/create_app_graph_2d_n8_mesh 316 316 > working/100k/2d_n8_mesh_100k.json
bin/create_app_graph_2d_n8_torus 316 316 > working/100k/2d_n8_torus_100k.json

>&2 echo "Gen 3d"
bin/create_app_graph_3d_n6_mesh 46 46 46 > working/100k/3d_n6_mesh_100k.json
bin/create_app_graph_3d_n6_torus 46 46 46 > working/100k/3d_n6_torus_100k.json
bin/create_app_graph_3d_n26_mesh 46 46 46 >working/100k/3d_n26_mesh_100k.json
bin/create_app_graph_3d_n26_torus 46 46 46 > working/100k/3d_n26_torus_100k.json

for i in working/100k/*.json ; do
    ./compare_routing.sh $i
done
