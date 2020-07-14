import subprocess
import os
import time
from random import seed
from random import random
from random import uniform

# global vars
seed(1)
run_max = 11
run_step = 1
obs_ratio = 10
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

# Remove previous files
if os.path.exists('model.cpp'):
        os.remove('model.cpp')

if os.path.exists('model.h'):
        os.remove('model.h')

if os.path.exists('results.csv'):
        os.remove('results.csv')

with open('results.csv', "a") as f:
    f.write("States,Observations,Panel_Size,Proc_Time\n")

for no_states in range(10, run_max, run_step):

    no_obs = no_states * obs_ratio

    no_targmark = 2

    for observation in range(1, no_obs - 1):
        if observation % targ_mark_ratio == 0:
            no_targmark = no_targmark + 1

    with open('model.cpp', "w") as f:
        f.write("// SPDX-License-Identifier: BSD-2-Clause\n")
        f.write("#include <stdint.h>\n")
        f.write('#include "model.h"\n')
        f.write('/***************************************************\n')
        f.write(' * Edit values between here ->\n')
        f.write(' * ************************************************/\n')
        f.write('\n')
        f.write('// Model\n')
        f.write('\n')
        f.write('// HMM States Labels\n')
        f.write('const uint8_t hmm_labels[NOOFSTATES][NOOFOBS] = { \n')
        for dim1 in range(0, no_states):
            f.write('    { ')
            for dim2 in range(0, no_obs):
                if random() > minor_allele_prob:
                    f.write('0')
                else:
                    f.write('1')
                if dim2 < no_obs - 1:
                    f.write(', ')
                else:
                    f.write(' ')
            if dim1 == no_states - 1:
                f.write('}\n')
            else:
                f.write('},\n')
        f.write('};\n')
        f.write('\n')
        f.write('// Target Markers \n')
        f.write('const uint32_t observation[NOOFTARGMARK][2] = { \n')
        for dim in range(0, no_targmark):
            f.write('    { ')
            if dim == 0:
                f.write('0')
            elif dim == no_targmark - 1:
                f.write(str(no_obs - 1))
            else:
                f.write(str(dim * targ_mark_ratio))

            f.write(', ')
            if random() > targ_minor_allele_prob:
                f.write('0 ')
            else:
                f.write('1 ')

            if dim == no_targmark - 1:
                f.write('}\n')
            else:
                f.write('},\n')

        f.write('};\n')
        f.write('\n')
        f.write('// dm -> Genetic Distances \n')
        f.write('const float dm[NOOFOBS-1] = {\n')
        for dim1 in range(0, no_obs - 1):
            f.write(str(format(uniform(0.0000000135741, 0.0000045218641), '.15f')))
            if dim == no_obs - 2:
                f.write('\n')
            else:
                f.write(',\n')
        f.write('};\n')
        f.write('\n')
        f.write('/***************************************************\n')
        f.write(' * <- And here\n')
        f.write(' * ************************************************/\n')
        f.write('\n')


    with open('model.h', "w") as f:
        f.write('// SPDX-License-Identifier: BSD-2-Clause\n')
        f.write('#ifndef _MODEL_H_\n')
        f.write('#define _MODEL_H_\n')
        f.write('\n')
        f.write('#include <stdint.h>\n')
        f.write('\n')
        f.write('// Parameters\n')
        f.write('\n')
        f.write('/***************************************************\n')
        f.write(' * Edit values between here ->\n')
        f.write(' * ************************************************/\n')
        f.write('\n')
        f.write('#define NOOFSTATES (' + str(no_states) + ')\n')
        f.write('#define NOOFOBS (' + str(no_obs) + ')\n')
        f.write('#define NOOFTARGMARK (' + str(no_targmark) + ')\n')
        f.write('#define NE (' + str(eff_pop_size) + ')\n')
        f.write('#define ERRORRATE (' + str(epsilon) + ')\n')
        f.write('\n')
        f.write('// Pre-processor Switches\n')
        f.write('//#define PRINTDIAG (1)\n')
        f.write('\n')
        f.write('/***************************************************\n')
        f.write(' * <- And here\n')
        f.write(' * ************************************************/\n')
        f.write('\n')
        f.write('extern const uint32_t observation[NOOFTARGMARK][2];\n')
        f.write('extern const float dm[NOOFOBS-1];\n')
        f.write('extern const uint8_t hmm_labels[NOOFSTATES][NOOFOBS];\n')
        f.write('\n')
        f.write('#endif\n')
        f.write('\n')

    subprocess.call(['g++', '-w', 'model.cpp', 'main.cpp', '-o', 'run'])


    result = subprocess.check_output(['./run'], shell=True)

    with open('results.csv', "a") as f:
        f.write(str(no_states) + ',' + str(no_obs) + ',' + str(no_states * no_obs) + ',' + str(result) + '\n')
        print('States = ' + str(no_states) + ' completed' + '\n')
        print('Observations = ' + str(no_obs) + '\n')
        print('Reference Panel = ' + str(no_states * no_obs) + '\n\n')


    time.sleep(10)

