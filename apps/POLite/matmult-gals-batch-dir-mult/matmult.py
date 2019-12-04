import subprocess
import os
import numpy as np
import time
import threading


# Remove previous files
if os.path.exists('matrices.cpp'):
        os.remove('matrices.cpp')

if os.path.exists('matrices.h'):
        os.remove('matrices.h')

if os.path.exists('results.csv'):
        os.remove('results.csv')

with open('results.csv', "a") as f:
    f.write("Dimension,Nodes,Map_Time,Init_Time,Proc_Time\n")

# Calculate matrices from iteration number
step_size = 2
run_max = 11

for _ in range(10):

    for dimension in range(2, run_max, step_size):
    #for dimension in range(10, 11):

        row_odd = []
        row_even = []
        matrix_a = []
        matrix_b = []

        for num in range(0, dimension):
            row_odd.append(num)
            row_even.append((dimension - 1) - num)

        for col in range(0, dimension):
            if col % 2 == 0:
                matrix_a.append(row_odd)
                matrix_b.append(row_even)
            if col % 2 == 1:
                matrix_a.append(row_even)
                matrix_b.append(row_odd)

        with open('matrices.cpp', "w") as f:
            f.write("// SPDX-License-Identifier: BSD-2-Clause\n")
            f.write("#include <stdint.h>\n")
            f.write('#include "matrices.h"\n')
            f.write('/***************************************************\n')
            f.write(' * Edit values between here ->\n')
            f.write(' * ************************************************/\n')
            f.write('\n')
            f.write('// Matrix A as 2D array\n')
            f.write('const int32_t matrixA[MATAWID][MATALEN] = {\n')
            for dim1 in range(0, dimension):
                f.write('    { ')
                for dim2 in range(0, dimension):
                    f.write(str(matrix_a[dim1][dim2]))
                    if dim2 < dimension - 1:
                        f.write(', ')
                    else:
                        f.write(' ')
                if dim1 == dimension - 1:
                    f.write('}\n')
                else:
                    f.write('},\n')
            f.write('};\n')
            f.write('\n')
            f.write('// Matrix B as 2D array\n')
            f.write('const int32_t matrixB[MATAWID][MATALEN] = {\n')
            for dim1 in range(0, dimension):
                f.write('    { ')
                for dim2 in range(0, dimension):
                    f.write(str(matrix_b[dim1][dim2]))
                    if dim2 < dimension - 1:
                        f.write(', ')
                    else:
                        f.write(' ')
                if dim1 == dimension - 1:
                    f.write('}\n')
                else:
                    f.write('},\n')
            f.write('};\n')
            f.write('\n')
            f.write('/***************************************************\n')
            f.write(' * <- And here\n')
            f.write(' * ************************************************/\n')
            f.write('\n')
            f.write('uint32_t mult_possible = (MATALEN == MATBWID) ? 1 : 0;\n')
            f.write('\n')

        with open('matrices.h', "w") as f:
            f.write('// SPDX-License-Identifier: BSD-2-Clause\n')
            f.write('#ifndef _MATRICES_H_\n')
            f.write('#define _MATRICES_H_\n')
            f.write('\n')
            f.write('#include <stdint.h>\n')
            f.write('\n')
            f.write('// Parameters\n')
            f.write('\n')
            f.write('/***************************************************\n')
            f.write(' * Edit values between here ->\n')
            f.write(' * ************************************************/\n')
            f.write('\n')
            f.write('#define MATALEN (' + str(dimension) + ')\n')
            f.write('#define MATAWID (' + str(dimension) + ')\n')
            f.write('#define MATBLEN (' + str(dimension) + ')\n')
            f.write('#define MATBWID (' + str(dimension) + ')\n')
            f.write('\n')
            f.write('/***************************************************\n')
            f.write(' * <- And here\n')
            f.write(' * ************************************************/\n')
            f.write('\n')
            f.write('#define MESHLEN (MATBLEN / 2)\n')
            f.write('#define MESHWID (MATAWID / 2)\n')
            f.write('#define MESHHEI (MATALEN / 2)\n')
            f.write('\n')
            f.write('#define RETMATSIZE (MESHLEN * MESHWID)\n')
            f.write('\n')
            f.write('extern const int32_t matrixA[MATAWID][MATALEN];\n')
            f.write('extern const int32_t matrixB[MATBWID][MATBLEN];\n')
            f.write('\n')
            f.write('extern uint32_t mult_possible;\n')
            f.write('\n')
            f.write('#endif\n')
            f.write('\n')


        matrix_a_arr = np.array(matrix_a)
        matrix_b_arr = np.array(matrix_b)
        matrix_ans_arr = matrix_a_arr.dot(matrix_b_arr)

        subprocess.call(['make', 'clean'])

        response = subprocess.check_output(['make'])

        still_running = True
        timeout_time = int(((0.00004 * (dimension ** 3)) + 20) * 1.5)
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

                result_list = []

                for i, r in enumerate(result):
                    if i < dimension:
                        elements = r.split(' ')
                        for element in elements:
                            if element != '':
                                result_list.append(int(element))
                    else:
                        durations = r.split(',')
                        map_time = durations[0]
                        init_time = durations[1]
                        proc_time = durations[2]

        result_arr = np.array(result_list).reshape((dimension, dimension))

        #print(result_arr)
        #print(np.array_equal(result_arr, matrix_ans_arr))
        #print(proc_time)

        if np.array_equal(result_arr, matrix_ans_arr):
            with open('results.csv', "a") as f:
                f.write(str(dimension) + ',' + str(dimension ** 3) + ',' + str(map_time) +
                        ',' + str(init_time) + ',' + str(proc_time) + '\n')
                print('Dimension = ' + str(dimension) + ' completed')
                print('Number of nodes = ' + str(dimension ** 3) + ', map_time = ' + str(map_time) +
                      ', init_time = ' + str(init_time) +
                      ', proc_time = ' + str(proc_time) + '\n')

        time.sleep(10)