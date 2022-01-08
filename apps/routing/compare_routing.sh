#!/bin/bash

APP_GRAPH=$1

WORKING=${APP_GRAPH}.working 

mkdir -p ${WORKING}

bin/create_system_heirarchy_0p8 6 8 > ${WORKING}/0p8.json
bin/create_system_heirarchy_0p6 6 8 > ${WORKING}/0p6.json

for METHOD in bfs random metis scotch ; do
    for ARCH in 0p8 0p6 ; do
        >&2 echo "Doing ${METHOD} on ${ARCH}"
        POLITE_PLACER="${METHOD}" bin/place_graph_POLite ${WORKING}/${ARCH}.json ${APP_GRAPH} > ${WORKING}/placement.${METHOD}.${ARCH}.json

        bin/print_routing_statistics ./${WORKING}/placement.${METHOD}.${ARCH}.json ${APP_GRAPH} > ${WORKING}/placement.${METHOD}.${ARCH}.csv
    done
done
