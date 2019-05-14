#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

# Wait until N FPGAs are visible via 'jtagconfig'

JTAGCONFIG=/local/ecad/altera/17.0/quartus/bin/jtagconfig

if [ -z "$1" ]; then
  echo "Usage: wait-for-fpgas.sh <N>"
  exit 1
fi

UP=0
while [ "$UP" -ne "$1" ]; do
  UP=$($JTAGCONFIG 2> /dev/null | grep -E '^[0-9]+)' | wc -l)
done
