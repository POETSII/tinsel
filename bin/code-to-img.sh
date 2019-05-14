#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

if [ -z "$CONFIG" ]; then
  echo "Please set CONFIG variable to point to config.py"
  exit -1
fi

if [ -z "$1" ]; then
  echo "ihex file expected as first argument"
  exit -1
fi

if [ -z "$2" ]; then
  echo "format expected as second argument: hex or mif"
  exit -1
fi

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python $CONFIG envs`

# Convert Intel Hex files to Bluesim hex files
# (used to initialise BRAM contents in Bluesim)
InstrBytes=$((2**$LogInstrsPerCore * 4))
ihex-to-img.py $2 $1 0 4 $InstrBytes
