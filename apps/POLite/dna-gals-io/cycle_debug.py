import subprocess
import os
import numpy as np
import time
from random import seed
from random import randint
import threading

# global vars
seed(1)
run_max = 4001
run_step = 10
no_sym = 4
obs_ratio = 0.1
true_fbalgo = 0

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
    f.write("States,Observations,Panel_Size,Upper_count,Lower_Count,Proc_Time\n")

for no_states in range(30, run_max, run_step):

    trans_prob = []
    emis_prob = []

    no_obs = int(no_states * obs_ratio)

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

    with open('model.cpp', "w") as f:
        f.write("// SPDX-License-Identifier: BSD-2-Clause\n")
        f.write("#include <stdint.h>\n")
        f.write('#include "model.h"\n')
        f.write('/***************************************************\n')
        f.write(' * Edit values between here ->\n')
        f.write(' * ************************************************/\n')
        f.write('\n')
        f.write('// Model\n')
        f.write('// M -> Observable Symbols in HMM\n')
        f.write('uint32_t observation[NOOFOBS] = { ')
        for dim in range(0, no_obs):
            f.write(str(randint(0, no_sym - 1)))
            if dim < no_obs - 1:
                f.write(', ')
            else:
                f.write(' ')
        f.write('};\n')
        f.write('\n')
        f.write('// pi -> Initial Probabilities \n')
        f.write('float init_prob[NOOFSTATES] = { ')
        for dim in range(0, no_states):
            f.write(str(init_prob[dim]))
            if dim < no_states - 1:
                f.write(', ')
            else:
                f.write(' ')
        f.write('};\n')
        f.write('\n')
        f.write('// alpha -> Transition Probabilities \n')
        f.write('float trans_prob[NOOFSTATES][NOOFSTATES] = {\n')
        for dim1 in range(0, no_states):
            f.write('    { ')
            for dim2 in range(0, no_states):
                f.write(str(trans_prob[dim1][dim2]))
                if dim2 < no_states - 1:
                    f.write(', ')
                else:
                    f.write(' ')
            if dim1 == no_states - 1:
                f.write('}\n')
            else:
                f.write('},\n')
        f.write('};\n')
        f.write('\n')
        f.write('// b -> Emission Probabilities \n')
        f.write('float emis_prob[NOOFSTATES][NOOFSYM] = {\n')
        for dim1 in range(0, no_states):
            f.write('    { ')
            for dim2 in range(0, no_sym):
                if true_fbalgo:
                    f.write(str(emis_prob[dim1][dim2]))
                else:
                    f.write('0.9999')
                if dim2 < no_sym - 1:
                    f.write(', ')
                else:
                    f.write(' ')
            if dim1 == no_states - 1:
                f.write('}\n')
            else:
                f.write('},\n')
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
        f.write('#define NOOFSYM (' + str(no_sym) + ')\n')
        f.write('#define NOOFOBS (' + str(no_obs) + ')\n')
        f.write('\n')
        f.write('#define CYCDEBUG (1)\n')
        f.write('\n')
        f.write('/***************************************************\n')
        f.write(' * <- And here\n')
        f.write(' * ************************************************/\n')
        f.write('\n')
        f.write('extern uint32_t observation[NOOFOBS];\n')
        f.write('extern float init_prob[NOOFSTATES];\n')
        f.write('extern float trans_prob[NOOFSTATES][NOOFSTATES];\n')
        f.write('extern float emis_prob[NOOFSTATES][NOOFSYM];\n')
        f.write('\n')
        f.write('#endif\n')
        f.write('\n')

    subprocess.call(['make', 'clean'])

    response = subprocess.check_output(['make'])

    still_running = True
    timeout_time = int(((0.0005 * (no_states ** 2)) - (0.06 * no_states) + 17) * 6)
    print('Timeout -> {}s'.format(timeout_time))

    if 'error' in str(response.lower()):
        print('Compile Error')
    else:

        while still_running:

            wd = os.getcwd()
            kill = lambda process: process.kill()
            cmd = ['./run']
            run = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd='build')

            my_timer = threading.Timer(timeout_time, kill, [run])

            try:
                my_timer.start()
                stdout, stderr = run.communicate()

                print('stdout -> {}'.format(stdout))
                print('stderr -> {}'.format(stderr))

                if len(stderr) == 0 and len(stdout) != 0:
                    still_running = False
                else:
                    print('Timed Out or Returned Error')
                    time.sleep(10)


            finally:
                my_timer.cancel()

            os.chdir(wd)
            result = stdout.split('\n')

        for i, r in enumerate(result):
            durations = r.split(',')
            upper_count = durations[0]
            lower_count = durations[1]
            proc_time = durations[2]


    with open('results.csv', "a") as f:
        f.write(str(no_states) + ',' + str(no_obs) + ',' + str(no_states * no_obs) + ',' + str(upper_count) +
                ',' + str(lower_count) + ',' + str(proc_time) + '\n')
        print('State Size ' + str(no_states) + ' completed')
        print('Panel Size = ' + str(no_states * no_obs) + ', upper count = ' + str(upper_count) +
              ', lower_count = ' + str(lower_count) +
              ', proc_time = ' + str(proc_time) + '\n')

    time.sleep(10)
