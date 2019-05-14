#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

# Check for unsupported RV32IMF instructions in a given ELF file.

if [ -z "$1" ]; then
  echo "Usage: checkelf.sh <FILE>"
fi

DUMP=$(riscv64-unknown-elf-objdump -d $1)

# Errors
ES="\secall\s|ebreak\s"
ES="$ES|\scsrrs\s|csrrc\s|\scsrrwi\s|\scsrrsi\s|\scsrrci\s"
ES="$ES|\s[^f]div\s|divu\s|\srem\s|\sremu\s"
ES="$ES|\sfsqrt\s|fmin\s|fmax\s|\sfclassify\s"
ES="$ES|\sfmadd\s|fmsub\s|fnmadd\s|\sfnmsub\s"

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
