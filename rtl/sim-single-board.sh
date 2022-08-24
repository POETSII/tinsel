#!/bin/bash

if [ ! -e "./de5Top" ]; then
  echo 'Simulator not find, try "make sim" first'
  exit
fi

UDSOCK="../hostlink/udsock"
if [ ! -e "$UDSOCK" ]; then
  echo 'Cannot find udsock tool'
  exit
fi

BOARDCTRLD="../hostlink/sim/boardctrld"
if [ ! -e "$BOARDCTRLD" ]; then
  echo 'Cannot find boardctrld'
  exit
fi

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python3 ../config.py envs`

PIDS=""

# Start boardctrld
echo "Starting boardctrld"
$BOARDCTRLD &
PIDS="$PIDS $!"

# Run simulator
echo "Starting single worker board"
BOARD_ID=0 ./de5Top | grep -v Warning &
PIDS="$PIDS $!"

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
