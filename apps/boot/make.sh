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

# Generate the linker script
./genld.sh > link.ld

# Build object files
$CC $CFLAGS -Wall -c -o boot.o boot.c
$CC $CFLAGS -Wall -c -o entry.o entry.S

# Link
$LD -melf32lriscv -G 0 -T link.ld -o boot.elf entry.o boot.o

# Emit Intel Hex
InstrBytes=$((2**$LogInstrsPerCore * 4))
$OBJCOPY --only-section=.text -O ihex boot.elf InstrMem.ihex

# Convert Intel Hex file to Altera MIF
# (used to initialise BRAM contents in Quartus)
ihex-to-img.py InstrMem.ihex mif 0 4 $InstrBytes > $QP_DIR/InstrMem.mif

# Convert Intel Hex files to Bluesim hex files
# (used to initialise BRAM contents in Bluesim)
ihex-to-img.py InstrMem.ihex hex 0 4 $InstrBytes > $RTL_DIR/InstrMem.hex
