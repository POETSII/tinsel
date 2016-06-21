Tinsel (working name) is a basic multi-threaded processor implementing
the 32-bit RISC-V ISA (RV32I).  It's designed for low resource usage
and high clock rate on modern FPGAs.

On Terasic's [DE5-NET](de5-net.terasic.com), Tinsel acheives an Fmax
of 450MHz and uses 500 ALMs (0.2%) and 113K block RAM bits (0.2%).

This repo contains an initial snapshot, not really intended for
general use yet.

Directory layout:

  * [rtl](rtl/) -- Bluespec and Verilog RTL designs.

  * [de5](de5/) -- basic Quartus project for Terasic's
    [DE5-NET](de5-net.terasic.com) that instantiates a single
    Tinsel core connected to a 400MHz PLL.

  * [software](software/) -- sample C program that runs on
    Tinsel.
