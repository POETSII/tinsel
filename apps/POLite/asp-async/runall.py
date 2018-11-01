import os
from subprocess import call

import argparse
import random

parser = argparse.ArgumentParser(description='Run tests for ASP')
parser.add_argument('with_generate', default=False, help='regen cubes')

# parser.add_argument('--sum', dest='accumulate', action='store_const',
#                     const=sum, default=max,
#                     help='sum the integers (default: find the max)')


def run_command(args):
    print(args)
    call(args, shell=True)

#cube_sizes = [(x,y) for x in range(3,10) for y in range(3,10) if x <= 7 and y <= 7 ]

cube_sizes = [(4,4), (5,5), (6,6), (6,9), (7,7)]
# for x,y in cube_sizes:
#     run_command("./GenHypercube {x} {y} > edges_{x}_{y}.txt".format(x=x,y=y))       

cube_sizes = [(4,2)]
RANDOM_ID = random.randint(0,1000)
NUM_SOURCES = [1,2,4,8,16,32,64]
X_LEN = [1]
Y_LEN = [1]
# run_command("scp -r build byron:asp")
for ns in NUM_SOURCES:
    for x_len in X_LEN:
        for y_len in Y_LEN:
            run_command("NUM_SOURCES={} make all".format(ns))
            run_command("scp -r build byron:asp")

            for x,y in cube_sizes:
                for i in range(2):
                    run_command("""ssh -t byron "cd asp; source /local/ecad/setup-quartus17v0.bash; ./build/run edges_{x}_{y}.txt | tee -a results{i}.json" """.format(x=x,y=y,i=RANDOM_ID))
                    #print("x={} y={} ns={} x_len={} y_len={}".format(x,y,ns,x_len,y_len))
                    exit(0)