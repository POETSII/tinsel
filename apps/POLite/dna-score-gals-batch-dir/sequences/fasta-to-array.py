######################################
# FASTA Sequence to C Array Converter
#
# Usage: python fasta-to-array.py fasta-filename
######################################
import sys

filename = sys.argv[1]

with open(filename, "r") as f1:
    with open('array', "w") as f2:
        f2.write("{")
        for cnt1, line in enumerate(f1):
            for cnt2, ch in enumerate(line):
                if ch in 'ACGT':
                    if cnt1 == 0 and cnt2 == 0:
                        f2.write("'" + ch + "'")
                    else:
                        f2.write(",'" + ch + "'")
        f2.write("};")
