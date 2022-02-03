#!/usr/bin/env python3

import sys
import random

n=int(sys.argv[1])
m=int(sys.argv[2])

edges=set()

for i in range(n):
    for j in random.sample(range(n), m):
        print(f"{i} {j}")


