#!/bin/bash

mkdir -p working/1M

>&2 echo "Gen 2d"
bin/create_app_graph_2d_n4_mesh 1000 1000 > working/1M/2d_n4_mesh_1M.json
bin/create_app_graph_2d_n4_torus 1000 1000 > working/1M/2d_n4_torus_1M.json
bin/create_app_graph_2d_n8_mesh 1000 1000 > working/1M/2d_n8_mesh_1M.json
bin/create_app_graph_2d_n8_torus 1000 1000 > working/1M/2d_n8_torus_1M.json

>&2 echo "Gen 3d"
bin/create_app_graph_3d_n6_mesh 100 100 100 > working/1M/3d_n6_mesh_1M.json
bin/create_app_graph_3d_n6_torus 100 100 100 > working/1M/3d_n6_torus_1M.json
bin/create_app_graph_3d_n26_mesh 100 100 100 >working/1M/3d_n26_mesh_1M.json
bin/create_app_graph_3d_n26_torus 100 100 100 > working/1M/3d_n26_torus_1M.json

for i in working/1M/*.json ; do
    ./compare_routing.sh $i
done