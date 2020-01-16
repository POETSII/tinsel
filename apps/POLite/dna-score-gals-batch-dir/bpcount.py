######################################
# Base Pair Counter
#
# Usage: python bpcount.py fasta-filename
######################################
import sys

filename = sys.argv[1]

bpcount = 0

with open(filename, "r") as f1:
    for line in f1:
        for ch in line:
            if ch in 'ACGT':
                bpcount = bpcount + 1

print(bpcount)

