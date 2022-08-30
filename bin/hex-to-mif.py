#!/usr/bin/python
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) Matthew Naylor
#
# Script to convert a hex file (a file where each line is a hex value)
# to a .mif file suitable for Quartus.

import sys

def usage():
  print("Usage: hex-to-mif.py <input.hex> <width>")
  print("")
  print("  Units of <width> is bits")

if len(sys.argv) != 3:
  usage()
  sys.exit()

try:
  width = int(sys.argv[2])
except:
  print("Invalid parameters")
  usage()
  sys.exit()

lineCount = 0
vals = []
try:
  file = open(sys.argv[1], "rt")
  upperAddr = 0
  for line in file:
    val = int(line[0:-1], 16)
    vals.append(val)
    lineCount = lineCount+1
except:
  print("Syntax error on line", lineCount)
  sys.exit()

print("DEPTH =", len(vals), ";")
print("WIDTH =", width, ";")
print("ADDRESS_RADIX = DEC ;")
print("DATA_RADIX = DEC ;")
print("CONTENT")
print("BEGIN")
addr = 0
for val in vals:
  print(addr, ": ", str(val), ";")
  addr = addr+1
print("END")
