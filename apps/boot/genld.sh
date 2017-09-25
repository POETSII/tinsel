#!/bin/bash

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../../config.py envs`

cat - << EOF
/* THIS FILE HAS BEEN GENERATED AUTOMATICALLY. */
/* DO NOT MODIFY. INSTEAD, MODIFY THE genld.sh SCRIPT. */

OUTPUT_ARCH( "riscv" )

DRAM_TOP = $BytesPerDRAM;

MEMORY
{
  /* Define max length of boot loader */
  boot : ORIGIN = 0, LENGTH = $MaxBootImageBytes
}

SECTIONS
{
  /* Instruction memory */
  /* (No data sections allowed in boot loader) */
  .text : { *.o(.text*) } > boot
}
EOF
