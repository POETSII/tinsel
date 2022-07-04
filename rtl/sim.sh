#!/bin/bash
#  SIM_BINARY="./de5Top"

make clean
make sim

BOARD_ID=0 make runsim &
sleep 5
../hostlink/udsock inout @tinsel.b0.0

# On CTRL-C, call quit()
trap quit INT
function quit() {
  echo
  echo "Caught CTRL-C. Exiting."
  for PID in "$PIDS"; do
    kill $PID 2> /dev/null
  done
}

wait
