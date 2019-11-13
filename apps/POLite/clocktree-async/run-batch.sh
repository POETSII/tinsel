#!/bin/bash

MHZ=220000000
CUTOFF=25000000

function size {
  D=$1
  B=$2

  N=0
  for I in $(seq 0 $D); do
    N=$(($N + $B ** $I))
  done

  echo $N
}

R="1 2 3 4 5 6 7 8 9 10 12 14 16"
for B in $R; do
  for D in $R; do
    N=$(size $D $B)
    if (( $N < $CUTOFF )); then
      C=$(./run $D $B | grep Cycles | cut -d' ' -f 3)
      T=$(bc -l <<< "scale=8; $C/$MHZ")
      echo $D, $B, $N, $T
      sleep 6
    fi
  done
  echo
done
