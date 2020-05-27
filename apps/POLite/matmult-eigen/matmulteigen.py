import subprocess
import os
import numpy as np
import time


# Remove previous files
if os.path.exists('main.cpp'):
        os.remove('main.cpp')

if os.path.exists('results.csv'):
        os.remove('results.csv')

with open('results.csv', "a") as f:
    #f.write("Dimension,Nodes,Map_Time,Init_Time,Send_Time,Proc_Time\n")
    f.write("Dimension,Nodes,Proc_Time\n")

# Calculate matrices from iteration number

step_size = 5
run_max = 801

for dimension in range(10, run_max, step_size):

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

    with open('main.cpp', "w") as f:
        f.write("#include <iostream>\n")
        f.write('#include "Eigen/Dense"\n')
        f.write("#include <chrono>\n")
        f.write('\n')
        f.write('using Eigen::MatrixXf;\n')
        f.write('\n')
        f.write('int main()\n')
        f.write('{\n')
        f.write('\n')
        f.write('MatrixXf m1({x},{y});\n'.format(x=dimension, y=dimension))
        f.write('m1 << ')

        for dim1 in range(0, dimension):
            for dim2 in range(0, dimension):
                if (dim1 == dimension-1) and (dim2 == dimension-1):
                    f.write('{val}'.format(val=str(matrix_a[dim1][dim2])))
                else:
                    f.write('{val},'.format(val=str(matrix_a[dim1][dim2])))

        f.write(';\n')
        f.write('MatrixXf m2({x},{y});\n'.format(x=dimension, y=dimension))
        f.write('m2 << ')

        for dim1 in range(0, dimension):
            for dim2 in range(0, dimension):
                if (dim1 == dimension-1) and (dim2 == dimension-1):
                    f.write('{val}'.format(val=str(matrix_b[dim1][dim2])))
                else:
                    f.write('{val},'.format(val=str(matrix_b[dim1][dim2])))

        f.write(';\n')



        f.write('\n')
        f.write('MatrixXf m;\n')
        f.write('\n')
        f.write('// Record start time\n')
        f.write('auto start = std::chrono::high_resolution_clock::now();\n')
        f.write('\n')
        f.write('m = m1 * m2;\n')
        f.write('\n')
        f.write('// Record end time\n')
        f.write('auto finish = std::chrono::high_resolution_clock::now();\n')
        f.write('\n')
        f.write('std::chrono::duration<double> elapsed = finish - start;\n')
        f.write('\n')
        f.write('std::cout << m << std::endl;\n')
        f.write('\n')
        f.write('std::cout << elapsed.count();\n')
        f.write('}')

    matrix_a_arr = np.array(matrix_a)
    matrix_b_arr = np.array(matrix_b)
    matrix_ans_arr = matrix_a_arr.dot(matrix_b_arr)

    subprocess.call(['g++', '-w', 'main.cpp', '-o', 'run'])

    #response = subprocess.check_output(['./run'], cwd='build', shell=True)
    response = subprocess.check_output(['./run'], shell=True)
    result = response.split('\n')

    result_list = []

    for i, r in enumerate(result):
        if i < dimension:
            elements = r.split(' ')
            for element in elements:
                if element != '':
                    result_list.append(int(element))
        else:
            proc_time = str(r)

    result_arr = np.array(result_list).reshape((dimension, dimension))

    #print(result_arr)
    #print(np.array_equal(result_arr, matrix_ans_arr))
    #print(proc_time)

    if np.array_equal(result_arr, matrix_ans_arr):
        with open('results.csv', "a") as f:
            f.write(str(dimension) + ',' + str(dimension ** 3) + ',' + proc_time + '\n')
            print('Dimension = ' + str(dimension) + ' completed')
            print('Number of nodes = ' + str(dimension ** 3) + ', proc_time = ' + proc_time + '\n')
