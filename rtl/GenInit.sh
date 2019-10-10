#!/bin/bash

# Generate memory initialisation files

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

MaxSlot=$(((2**LogMsgsPerMailbox) - 1))
ThreadsPerMailbox=$((2**$LogThreadsPerMailbox))

# Emit hex file
for I in $(seq $ThreadsPerMailbox $MaxSlot); do
  printf "%x\n" $I
done >> FreeSlots.hex

# Emit MIF file
../bin/hex-to-mif.py FreeSlots.hex $LogMsgsPerMailbox > ../de5/FreeSlots.mif
