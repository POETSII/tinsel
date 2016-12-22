#!/bin/bash

# Tinsel root
TINSEL_ROOT=../../

# Include directory
INC=$TINSEL_ROOT/include

# Location of RTL
RTL_DIR=$TINSEL_ROOT/rtl

# Location of quartus project
QP_DIR=$TINSEL_ROOT/de5

# Put scripts into path
export PATH=$TINSEL_ROOT/bin:$PATH

# Parameters
ARCH="RV32I"
CC="riscv64-unknown-elf-gcc"
LD="riscv64-unknown-elf-ld"
OBJCOPY="riscv64-unknown-elf-objcopy"
CFLAGS="-march=$ARCH -msoft-float -O2 -I $INC"

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python $TINSEL_ROOT/config.py envs`

# Ensure that include files have been generated
pushd . > /dev/null
cd $INC
./make.sh
popd > /dev/null

# Generate linker script
./genld.sh > link.ld

# Build object files
$CC $CFLAGS -Wall -c -o hello.o hello.c

# Link
$LD -melf32lriscv -G 0 -T link.ld -o hello.elf hello.o

# Emit verilog hex files
$OBJCOPY -O verilog --only-section=.text hello.elf code.v
$OBJCOPY -O verilog --remove-section=.text \
  --set-section-flags .bss=alloc,load,contents hello.elf data.v
