#!/bin/bash

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

# Compute space available for instructions
MaxInstrBytes=$((4 * 2**$LogInstrsPerCore - $MaxBootImageBytes))

# Data section address and length
if [ "$UseCaches" = "True" ]; then
  DataBase="0x100000"
  DataLength="0x1ff00000"
else
  # Boot loader uses first two message slots
  DataReserved=$((2 * (2 ** $LogBytesPerMsg)))
  DataBase=$((4 * (2 ** $LogInstrsPerCore) + $DataReserved))
  DataLength=$((2 ** ($LogBytesPerMsg+$LogMsgsPerThread) - $DataReserved))
fi

cat - << EOF
/* THIS FILE HAS BEEN GENERATED AUTOMATICALLY. */
/* DO NOT MODIFY. INSTEAD, MODIFY THE genld.sh SCRIPT. */

OUTPUT_ARCH( "riscv" )

MEMORY
{
  instrs  : ORIGIN = $MaxBootImageBytes, LENGTH = $MaxInstrBytes
  globals : ORIGIN = $DataBase, LENGTH = $DataLength
}

SECTIONS
{
  .text   : { *.o(.text*) }             > instrs
  .bss    : { *.o(.bss*) }              > globals = 0
  .rodata : { *.o(.rodata*) }           > globals
  .sdata  : { *.o(.sdata*) }            > globals
  .data   : { *.o(.data*) }             > globals
  __heapBase = ALIGN(.);
}
EOF
