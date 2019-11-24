#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

# Check for unsupported RV32IMF instructions in a given ELF file.

if [ -z "$1" ]; then
  echo "Usage: checkelf.sh <FILE>"
fi

DUMP=$(riscv64-unknown-elf-objdump -d $1)

# Errors
T="[\.\s]"
ES="\secall$T|ebreak$T"
ES="$ES|\scsrrs$T|\scsrrc$T|\scsrrwi$T|\scsrrsi$T|\scsrrci$T"
ES="$ES|\s[^f]div$T|divu$T|\srem$T|\sremu$T"
ES="$ES|\sfsqrt$T|fmin$T|fmax$T|\sfclassify$T"
ES="$ES|\sfmadd$T|fmsub$T|fnmadd$T|\sfnmsub$T"

if echo "$DUMP" | grep -q -E "$ES"; then
  echo "ERROR: $1 uses unsupported instructions:"
  echo "$DUMP" | grep -o -E "$ES"
  exit -1
fi

# Warnings
WS="fcvt.s.wu|fcvt.wu.s"

if echo "$DUMP" | grep -q -E "$WS"; then
  echo "WARNING: $1 uses partially-supported instructions:"
  echo "$DUMP" | grep -o -E "$WS"
  echo "See archtecture guide for details"
fi

exit 0
