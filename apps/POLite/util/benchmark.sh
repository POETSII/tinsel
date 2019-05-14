#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

# Run POLite benchmarks, varying the number of boards used

# Location of benchmarks
BENCHMARKS_ROOT=$(pwd)/../

# Locations of results (output) directory
RESULTS_ROOT=$(pwd)/results/

# Benchmarks to use
BENCHMARKS="pagerank-sync asp-sync"

# Number of boxes to use
XBOXES=1
YBOXES=2

# Vary number of boards used
XBOARDS="3"
YBOARDS="1 2 3 4"

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
  echo -n >> $RESULTS_ROOT/$G/$B.txt
  for Y in $YBOARDS; do
    for X in $XBOARDS; do
      pushd .
      # Run application
      cd $BENCHMARKS_ROOT/$B/build/
      rm -f stats.txt
      R=$(POLITE_BOARDS_X=$X \
          POLITE_BOARDS_Y=$Y \
          HOSTLINK_BOXES_X=$XBOXES \
          HOSTLINK_BOXES_Y=$YBOXES \
          ./run $1)
      popd
      # Compute stats
      cat $BENCHMARKS_ROOT/$B/build/stats.txt | \
        awk -v boardsX=$X -v boardsY=$Y -f sumstats.awk > \
        $RESULTS_ROOT/$G/$B-stats${X}x${Y}.txt
      # Append time to results
      TIME=$(cat $RESULTS_ROOT/$G/$B-stats${X}x${Y}.txt \
               | grep "Time (s)" \
               | cut -d' ' -f 4)
      let N="$X*$Y"
      echo "$N $TIME" >> $RESULTS_ROOT/$G/$B.txt
      sleep 3
    done
  done
done
