/* THIS FILE HAS BEEN GENERATED AUTOMATICALLY. */
/* DO NOT MODIFY. INSTEAD, MODIFY THE genld.sh SCRIPT. */

OUTPUT_ARCH( "riscv" )

MEMORY
{
  instrs  : ORIGIN = 512, LENGTH = 7680
  globals : ORIGIN = 0x100000, LENGTH = 0x1ff00000
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
