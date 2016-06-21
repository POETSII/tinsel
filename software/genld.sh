#!/bin/bash

# Load config parameters
. ../config.sh

DataWordsPerCore=$((2**$LogDataWordsPerCore))
DataMemTop=$((2*$DataWordsPerCore))

cat - << EOF
OUTPUT_ARCH( "riscv" )

DATA_MEM_TOP = $DataMemTop;

SECTIONS
{
  /* Instruction memory */
  . = 0x0;
  .text   : { *.o(.text*) }
  /* Data memory */
  . = $DataWordsPerCore;
  .bss    : { *.o(.bss*) }
  .data   : { *.o(.data*) }
  .rodata : { *.o(.rodata*) }
}
EOF
