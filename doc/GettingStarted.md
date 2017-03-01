# Tinsel Getting Started Guide

## Contents

1. [Install the RISC-V tools](#1-install-the-risc-v-tools)
2. [Build the HostLink tool](#2-build-the-hostlink-tool)
3. [Obtain an FPGA bitfile](#3-obtain-an-fpga-bitfile)
4. [Program the FPGA](#4-program-the-fpga)
5. [Run the RISC-V test suite](#5-run-the-risc-v-test-suite)
6. [Run the hello world app](#6-run-the-hello-world-app)
7. [Run the heat diffusion app](#7-run-the-heat-diffusion-app)

## 1. Install the RISC-V tools

If you have access to a Cambridge CL machine then simply add the
following to your `.bashrc`:

```
  export RISCV=/local/ecad/riscv-tools
  export PATH=$PATH:/local/ecad/riscv-tools/bin
  source /local/ecad/setup.bash
```

Otherwise download, build, and install the RISC-V compiler tools from:

  [https://github.com/riscv/riscv-tools](https://github.com/riscv/riscv-tools)

Ensure the `RISCV` environment variable is set and `$RISCV/bin` is in
your `PATH`.

## 2. Build the HostLink tool

Communicating with the tinsel machine is done using the HostLink tool.
To build this tool, you need Quartus installed and the environment
variable `QUARTUS_ROOTDIR` set.  From the `tinsel` root directory:

```
  make -C hostlink
```

## 3. Obtain an FPGA bitfile

Rather than building a tinsel FPGA bitfile from scratch, it is
recommanded that you download a pre-built bitfile from:

  [https://www.cl.cam.ac.uk/~mn416/bitfiles/](https://www.cl.cam.ac.uk/~mn416/bitfiles/)

Place your downloaded `.sof` file into the `de5` subdirectory and
rename it to `Golden_top.sof`.

If you want to build your own bitfile, then from the `tinsel` root
directory:

```
  make -C de5
```

## 4. Program the FPGA

Assuming the FPGA is connected and switched on:

```
  make -C de5 download-sof
```

The FPGA is now running the tinsel machine.

## 5. Run the RISC-V test suite

To ensure that the tinsel machine is working as expected, you might
run the RISC-V test suite.  From the `tinsel` root directory:

```
  make -C tests run-jtag
```

You should obtain the following output:

```
addi    PASSED
add     PASSED
andi    PASSED
and     PASSED
auipc   PASSED
beq     PASSED
bge     PASSED
bgeu    PASSED
blt     PASSED
bltu    PASSED
bne     PASSED
jalr    PASSED
jal     PASSED
lb      PASSED
lbu     PASSED
lh      PASSED
lhu     PASSED
lui     PASSED
lw      PASSED
mulh    PASSED
mulhsu  PASSED
mulhu   PASSED
mul     PASSED
ori     PASSED
or      PASSED
sb      PASSED
sh      PASSED
simple  PASSED
slli    PASSED
sll     PASSED
slti    PASSED
sltiu   PASSED
slt     PASSED
sltu    PASSED
srai    PASSED
sra     PASSED
srli    PASSED
srl     PASSED
sub     PASSED
sw      PASSED
xori    PASSED
xor     PASSED
```

## 6. Run the hello world app

If you've just run the test suite then you'll need reprogram the FPGA
to put the tinsel machine back to its original state.  (A quicker reset
mechanism is planned for future releases.)

```
  make -C de5 download-sof
```

Now, the "hello world" application is as follows:

```c
#include <tinsel.h>

int main()
{
  unsigned int me = tinselId();
  printf("Hello from thread 0x%x\n", me);
  return 0;
}
```

To run it from the `tinsel` root directory:

```
  make -C apps/hello run-jtag
```

You should see a line of text from each thread.  The text stream from
each thread is line-buffered, which is why we don't see text from
different threads intermingled.  The order in which the lines appear
depends on a race between the threads.

## 7. Run the heat diffusion app

First, reset the tinsel machine:

```
  make -C de5 download-sof
```

Now run the heat diffusion example:

```
  make -C apps/heat run-jtag
```

Assuming the number of timesteps (`nsteps` in `heat.c`) is 200,000
then viewing the image `fpga.ppm` should show:

![Output of heat diffusion application](heat.jpg)
