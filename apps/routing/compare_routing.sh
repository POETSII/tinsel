#!/bin/bash

APP_GRAPH=$1

FPGA_LEN_X=6
FPGA_LEN_Y=8
if [[ "$2" != "" ]] ; then
    FPGA_LEN_X=$2
fi
if [[ "$3" != "" ]] ; then
    FPGA_LEN_Y=$3
fi

WORKING=${APP_GRAPH}.working 

mkdir -p ${WORKING}

bin/create_system_heirarchy_0p8 ${FPGA_LEN_X} ${FPGA_LEN_Y} > ${WORKING}/0p8.json
bin/create_system_heirarchy_0p6 ${FPGA_LEN_X} ${FPGA_LEN_Y} > ${WORKING}/0p6.json

for EFFORT in 0 1 2 4 8 ; do
    for METHOD in metis random direct dealer permutation bfs scotch ; do
        for ARCH in 0p8 0p6 ; do
            >&2 echo "Doing ${METHOD} on ${ARCH} with placer effort ${EFFORT}"
            >&2 echo "   Creating placement"
            POLITE_PLACER_EFFORT=${EFFORT} POLITE_PLACER="${METHOD}" bin/place_graph_POLite ${WORKING}/${ARCH}.json ${APP_GRAPH} > ${WORKING}/placement.${METHOD}.${ARCH}.e${EFFORT}.json

            >&2 echo "   Calculating statistics"
            bin/print_routing_statistics ${WORKING}/placement.${METHOD}.${ARCH}.e${EFFORT}.json ${APP_GRAPH} > ${WORKING}/placement.${METHOD}.${ARCH}.e${EFFORT}.csv

            >&2 echo "   Rendering"
            bin/print_system_heirarchy_as_svg_placement ${WORKING}/placement.${METHOD}.${ARCH}.e${EFFORT}.json ${APP_GRAPH} > ${WORKING}/placement.${METHOD}.${ARCH}.e${EFFORT}.svg
        done
    done
done

./plot_comparative_cdf.py "${WORKING}/placement.*.0p8.e0.csv" ${WORKING}/placement-all-e0.pdf
./plot_comparative_cdf.py "${WORKING}/placement.*.0p8.e8.csv" ${WORKING}/placement-all-e8.pdf
./plot_comparative_cdf.py "${WORKING}/placement.metis.0p8.*.csv" ${WORKING}/placement-metis-eall.pdf
./plot_comparative_cdf.py "${WORKING}/placement.random.0p8.*.csv" ${WORKING}/placement-random-eall.pdf