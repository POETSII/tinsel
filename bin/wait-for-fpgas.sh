#!/bin/bash

# Wait until N FPGAs are visible via 'jtagconfig'

if [ -z "$1" ]; then
  echo "Usage: wait-for-fpgas.sh <N>"
  exit 1
fi

UP=0
while [ "$UP" -ne "$1" ]; do
  UP=$(jtagconfig | grep -E '^[0-9]+)' | wc -l)
done
