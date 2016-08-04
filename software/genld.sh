#!/bin/bash

# Load config parameters
. ../config.sh

DataMemStart=$((2**$LogInstrsPerCore))

cat - << EOF
OUTPUT_ARCH( "riscv" )

DATA_MEM_TOP = 0x100000;

SECTIONS
{
  /* Instruction memory */
  . = 0x0;
  .text   : { *.o(.text*) }
  /* Data memory */
  . = $DataMemStart;
  .bss    : { *.o(.bss*) }
  .data   : { *.o(.data*) }
  .rodata : { *.o(.rodata*) }
}
EOF
