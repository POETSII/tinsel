import subprocess
import os
import numpy as np
import time
import threading


# Remove previous files
if os.path.exists('results.csv'):
        os.remove('results.csv')

with open('results.csv', "a") as f:
    f.write("Run,Map_Time,Init_Time,Proc_Time\n")

# Calculate matrices from iteration number

step_size = 5
run_max = 801

for test in range(100):

    subprocess.call(['make', 'clean'])

    response = subprocess.check_output(['make'])

    still_running = True
    timeout_time = int(30)
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
            result = stdout.split(',')

            map_time = result[0]
            init_time = result[1]
            proc_time = result[2]


    #print(result_arr)
    #print(np.array_equal(result_arr, matrix_ans_arr))
    #print(proc_time)

    with open('results.csv', "a") as f:
        f.write(str(test) + ',' + str(map_time) +
                ',' + str(init_time) + ',' + str(proc_time) + '\n')
        print('Run = ' + str(test) + ' completed')
        print('map_time = ' + str(map_time) +
              ', init_time = ' + str(init_time) +
              ', proc_time = ' + str(proc_time) + '\n')

    time.sleep(10)