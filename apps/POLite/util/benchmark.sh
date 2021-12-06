#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

# Run POLite benchmarks, varying the number of boards used

# Location of benchmarks
BENCHMARKS_ROOT=$(pwd)/../

# Locations of results (output) directory
RESULTS_ROOT=$(pwd)/results/

# Benchmarks to use
BENCHMARKS="pagerank-sync asp-sync izhikevich-sync"

# Number of boxes to use
XBOXES=2
YBOXES=4

# Vary number of boards used
XBOARDS="6"
YBOARDS="8"

if [ "$1" = "" ]; then
  echo "Usage: benchmark.sh graph.txt"
  exit -1
fi

for B in $BENCHMARKS; do
  # Build application
  make -C $BENCHMARKS_ROOT/$B/ clean
  make -C $BENCHMARKS_ROOT/$B/
  # Graph name
  G=$(basename $1 .txt)
  # Create results dir, if it doesn't already exist
  mkdir -p $RESULTS_ROOT/$G
  # Clear results file
  for Y in $YBOARDS; do
    for X in $XBOARDS; do
      pushd .
      echo ======== Network $G ========
      # Run application
      cd $BENCHMARKS_ROOT/$B/build/
      rm -f stats.txt
      POLITE_BOARDS_X=$X \
        POLITE_BOARDS_Y=$Y \
        HOSTLINK_BOXES_X=$XBOXES \
        HOSTLINK_BOXES_Y=$YBOXES \
          ./run $1 > $RESULTS_ROOT/$G/$B-out${X}x${Y}.txt
      popd
      # Compute stats
      cat $BENCHMARKS_ROOT/$B/build/stats.txt | \
        awk -v boardsX=$X -v boardsY=$Y -f sumstats.awk > \
        $RESULTS_ROOT/$G/$B-stats${X}x${Y}.txt
      sleep 6
    done
  done
done
