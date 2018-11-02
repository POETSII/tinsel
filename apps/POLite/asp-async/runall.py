import os
from subprocess import call

import argparse
import random
import itertools

# parser = argparse.ArgumentParser(description='Run tests for ASP')
# parser.add_argument('with_generate', default=False, help='regen cubes')

# parser.add_argument('--sum', dest='accumulate', action='store_const',
#                     const=sum, default=max,
#                     help='sum the integers (default: find the max)')

def run_command(args):
    print(args)
    res = call(args, shell=True)
    # if res != 0:
    #     exit(0)

#cube_sizes = [(x,y) for x in range(3,10) for y in range(3,10) if x <= 7 and y <= 7 ]
# run_command("scp -r build byron:asp")
# for x,y in cube_sizes:
#     run_command("./GenHypercube {x} {y} > edges_{x}_{y}.txt".format(x=x,y=y))       
#cube_sizes = [(4,4), (5,5), (6,6), (6,9), (7,7)]


# Overall params
RANDOM_ID = random.randint(0,1000)

# Runtime params
X_LEN = [1,2,3]
Y_LEN = [1,2]

# Compile time params
NUM_SOURCES = [1]#,4,16,64]
IDLES = [0, 1] #[0,1]
PLACE_EFFORT = [0, 8, 32]

#cube_sizes = [(4,4), (6,6), (6,9)]
# cube_sizes = [(6,9)]
# FILES = ["edges_{cx}_{cy}.txt" for cx, cy in cube_sizes]

FILES = ["edges_2d_{i}.txt".format(i=i) for i in [32, 256]]

for ns, idle in itertools.product(NUM_SOURCES, IDLES):
    run_command("NUM_SOURCES={ns} TINSEL_IDLE_SUPPORT={idle} make all".format(**locals()))
    run_command("scp -r build byron:asp")

    for i, fname, bx, by, pe in itertools.product(range(5), FILES, X_LEN, Y_LEN, PLACE_EFFORT):
        run_command("""ssh -t byron "cd asp; source /local/ecad/setup-quartus17v0.bash; ./build/run {fname} {bx} {by} {pe} | tee -a results{RANDOM_ID}.json" """.format(**locals()))