OUTPUT_ARCH( "riscv" )

DRAM_TOP = 0x40000000;

SECTIONS
{
  /* Instruction memory */
  /* (No data sections allowed in boot loader) */
  .text : { *.o(.text*) }
}
