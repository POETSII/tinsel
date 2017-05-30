#!/bin/bash

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: sim.sh [MESH_X] [MESH_Y]"
  exit
fi

if [ ! -e "./de5Top" ]; then
  echo 'Simulator not find, try "make sim" first'
  exit
fi

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

MESH_MAX_X=$((2 ** $LogMeshXLen))
MESH_MAX_Y=$((2 ** $LogMeshYLen))
echo "Max mesh dimensions: $MESH_MAX_X x $MESH_MAX_Y"

MESH_X=$1
MESH_Y=$2
echo "Using mesh dimensions: $MESH_X x $MESH_Y"

# Check dimensions
if [ $MESH_X -gt $MESH_MAX_X ] || [ $MESH_Y -gt $MESH_MAX_Y ] ; then
  echo "ERROR: specified dimensions exceed the maximum"
  exit
fi

# Convert coords to board id
function fromCoords {
  echo $(($1 * $MESH_MAX_Y + $2))
}

LAST_X=$(($MESH_X - 1))
LAST_Y=$(($MESH_Y - 1))

# Create named pipes
for X in $(seq 0 $LAST_X); do
  for Y in $(seq 0 $LAST_Y); do
    ID=$(fromCoords $X $Y)
    for P in $(seq 1 4); do
      PIPE_IN="/tmp/tinsel.in.b$ID.$P"
      PIPE_OUT="/tmp/tinsel.out.b$ID.$P"
      if [ ! -e $PIPE_IN ]; then
        echo "Creating $PIPE_IN"
        mkfifo $PIPE_IN
      fi
      if [ ! -e $PIPE_OUT ]; then
        echo "Creating $PIPE_OUT"
        mkfifo $PIPE_OUT
      fi
    done
  done
done

# Connect the named pipes
PIDS=""
for X in $(seq 0 $LAST_X); do
  for Y in $(seq 0 $LAST_Y); do
    ID=$(fromCoords $X $Y)
    # North
    if [ $(($Y+1)) -lt $MESH_Y ]; then
      N=$(fromCoords $X $(($Y+1)))
      cat /tmp/tinsel.out.b$ID.1 > /tmp/tinsel.in.b$N.2 &
      PIDS="$PIDS $!"
    else
      cat /tmp/tinsel.out.b$ID.1 > /dev/null &
      PIDS="$PIDS $!"
    fi
    # South
    if [ $(($Y-1)) -ge 0 ]; then
      S=$(fromCoords $X $(($Y-1)))
      cat /tmp/tinsel.out.b$ID.2 > /tmp/tinsel.in.b$S.1 &
      PIDS="$PIDS $!"
    else
      cat /tmp/tinsel.out.b$ID.2 > /dev/null &
      PIDS="$PIDS $!"
    fi
    # East
    if [ $(($X+1)) -lt $MESH_X ]; then
      E=$(fromCoords $(($X+1)) $Y)
      cat /tmp/tinsel.out.b$ID.3 > /tmp/tinsel.in.b$E.4 &
      PIDS="$PIDS $!"
    else
      cat /tmp/tinsel.out.b$ID.3 > /dev/null &
      PIDS="$PIDS $!"
    fi
    # West
    if [ $(($X-1)) -ge 0 ]; then
      W=$(fromCoords $(($X-1)) $Y)
      cat /tmp/tinsel.out.b$ID.4 > /tmp/tinsel.in.b$W.3 &
      PIDS="$PIDS $!"
    else
      cat /tmp/tinsel.out.b$ID.4 > /dev/null &
      PIDS="$PIDS $!"
    fi
  done
done

# Run one simulator per board
for X in $(seq 0 $LAST_X); do
  for Y in $(seq 0 $LAST_Y); do
    ID=$(fromCoords $X $Y)
    echo "Lauching simulator at position ($X, $Y) with board id $ID"
    BOARD_ID=$ID ./de5Top | grep -v Warning &
    PIDS="$PIDS $!"
  done
done

# On CTRL-C, call quit()
trap quit INT
function quit() {
  echo
  echo "Caught CTRL-C. Exiting."
  echo "Kill list: $PIDS"
  for PID in "$PIDS"; do
    kill $PID
  done
}

wait
