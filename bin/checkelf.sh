#!/bin/bash

# Check for unsupported RV32IMF instructions in a given ELF file.

if [ -z "$1" ]; then
  echo "Usage: checkelf.sh <FILE>"
fi

DUMP=$(riscv64-unknown-elf-objdump -S $1)

# Errors
ES="ecall|ebreak"
ES="$ES|csrrs|csrrc|csrrwi|csrrsi|csrrci"
ES="$ES|div|divu|rem|remu"
ES="$ES|fsqrt|fmin|fmax|fclassify"

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
