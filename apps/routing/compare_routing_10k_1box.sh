#!/bin/bash

mkdir -p working/10k_1box

>&2 echo "Gen 2d"
bin/create_app_graph_2d_n4_mesh 100 100 > working/10k_1box/2d_n4_mesh_100k.json
bin/create_app_graph_2d_n4_torus 100 100 > working/10k_1box/2d_n4_torus_100k.json
bin/create_app_graph_2d_n8_mesh 100 100 > working/10k_1box/2d_n8_mesh_100k.json
bin/create_app_graph_2d_n8_torus 100 100 > working/10k_1box/2d_n8_torus_100k.json

>&2 echo "Gen 3d"
bin/create_app_graph_3d_n6_mesh 22 22 22 > working/10k_1box/3d_n6_mesh_100k.json
bin/create_app_graph_3d_n6_torus 22 22 22 > working/10k_1box/3d_n6_torus_100k.json
bin/create_app_graph_3d_n26_mesh 22 22 22 >working/10k_1box/3d_n26_mesh_100k.json
bin/create_app_graph_3d_n26_torus 22 22 22 > working/10k_1box/3d_n26_torus_100k.json

for i in working/10k_1box/*.json ; do
    ./compare_routing.sh $i 3 2
done
