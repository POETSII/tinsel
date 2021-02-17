import subprocess
import os
import time
from random import seed
from random import random
from random import uniform

# global vars
seed(1)
noofobs = 768
noofstates = 8
# 1 in every . .
targ_mark_ratio = 10

# model vars
minor_allele_prob = 0.2
targ_minor_allele_prob = 0.001
eff_pop_size = 1000000
epsilon = 10000

'''
init_prob = np.random.random(no_states)
init_prob /= init_prob.sum()

for _ in range(no_states):
    row = np.random.random(no_states)
    row /= row.sum()
    trans_prob.append(row)

for _ in range(no_states):
    row = np.random.random(no_sym)
    row /= row.sum()
    emis_prob.append(row)
'''
'''
# Remove previous files
if os.path.exists('model.cpp'):
        os.remove('model.cpp')

if os.path.exists('model.h'):
        os.remove('model.h')
'''
if os.path.exists('results.csv'):
        os.remove('results.csv')

# Create oberseration list
with open('results.csv', "a") as f:
    f.write("const uint32_t observation[NOOFOBS][2] = {\n")

for ob_no in range(noofobs):

    with open('results.csv', "a") as f:
        f.write('    { ')
        f.write(str(on_no) + ', ')
        if random() > minor_allele_prob:
            f.write('0 },\n')
        else:
            f.write('1 },\n')

with open('results.csv', "a") as f:
    f.write("};")

# Create model
with open('results.csv', "a") as f:
    f.write("const uint8_t hmm_labels[NOOFSTATES][NOOFOBS] = {\n")

for state in range(noofstates):

    with open('results.csv', "a") as f:
        f.write('    { ')

        for ob_no in range(noofobs):
            if random() > minor_allele_prob:
                f.write('0')
            else:
                f.write('1')
            if ob_no != noofobs - 1
                f.write(',')

        f.write(' },')

with open('results.csv', "a") as f:
    f.write("};")


