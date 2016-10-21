#!/bin/bash

# Parameters
ARCH="RV32I"
CC="riscv64-unknown-elf-gcc"
AS="riscv64-unknown-elf-as"
LD="riscv64-unknown-elf-ld"
OBJCOPY="riscv64-unknown-elf-objcopy"
CFLAGS="-march=$ARCH -O2"
CFILES="main"

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

# Determine location of quartus project
QPDIR=../de5

# Build object files
OFILES=""
for F in $CFILES
do
  OFILES="$OFILES `basename $F.o`"
  $CC $CFLAGS -Wall -c -o `basename $F.o` $F.c
done
$AS -march=$ARCH entry.s -o entry.o

# Link
./genld.sh > link.ld
$LD -melf32lriscv -G 0 -T link.ld -o out.elf entry.o $OFILES

# Emit Intel Hex for code and data sections
InstrBytes=$((2**$LogInstrsPerCore * 4))
$OBJCOPY --only-section=.text -O ihex out.elf InstrMem.ihex
#$OBJCOPY --change-address=-$InstrBytes --remove-section=.text -O \
#         ihex out.elf DataMem.ihex

# Put scripts into path
export PATH=../scripts:$PATH

# Convert Intel Hex files to Altera mif files
# (used to initialise BRAM contents in Quartus)
ihex-to-img.py InstrMem.ihex mif 0 4 $InstrBytes > $QPDIR/InstrMem.mif
#ihex-to-img.py DataMem.ihex mif 0 4 $DataBytes > $QPDIR/DataMem.mif

# Convert Intel Hex files to Bluesim hex files
# (used to initialise BRAM contents in Bluesim)
ihex-to-img.py InstrMem.ihex hex 0 4 $InstrBytes > ../rtl/InstrMem.hex
#ihex-to-img.py DataMem.ihex hex 0 4 $DataBytes > ../rtl/DataMem.hex

# Generate RunQueue.hex
MaxThreadId=$((2 ** $LogThreadsPerCore - 1))
for T in $(seq 0 $MaxThreadId); do
  printf "%x\n" $T
done > ../rtl/RunQueue.hex

# Generate RunQueue.mif
Width=$(($LogInstrsPerCore + $LogThreadsPerCore + 2))
hex-to-mif.py ../rtl/RunQueue.hex $Width > $QPDIR/RunQueue.mif
