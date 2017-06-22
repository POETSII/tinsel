#!/bin/bash

if [ ! -e "./de5Top" ]; then
  echo 'Simulator not find, try "make sim" first'
  exit
fi

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

MESH_MAX_X=$((2 ** $MeshXBits))
MESH_MAX_Y=$((2 ** $MeshYBits))
echo "Max mesh dimensions: $MESH_MAX_X x $MESH_MAX_Y"

MESH_X=$MeshXLen
MESH_Y=$MeshYLen
echo "Using mesh dimensions: $MESH_X x $MESH_Y"

HOST_X=0
HOST_Y=$(($MESH_Y))
echo "Connecting host board at location ($HOST_X, $HOST_Y)"

# Check dimensions
if [ $MESH_X -gt $MESH_MAX_X ] || [ $MESH_Y -gt $MESH_MAX_Y ] ; then
  echo "ERROR: max mesh dimensions exceeded"
  exit
fi

# Convert coords to board id
function fromCoords {
  echo $(($2 * $MESH_MAX_X + $1))
}

# Create fifo, if it doesn't exists
function createFifo {
  if [ ! -e $1 ]; then
    echo "Creating fifo $1"
    mkfifo $1
  fi
}

# Connect two fifos
function connect {
  trap '' INT
#  while [ 1 == 1 ]; do
    cat $1 > $2
#  done
}

LAST_X=$(($MESH_X - 1))
LAST_Y=$(($MESH_Y - 1))

# Create named pipes
for X in $(seq 0 $LAST_X); do
  for Y in $(seq 0 $LAST_Y); do
    ID=$(fromCoords $X $Y)
    for P in $(seq 0 4); do
      PIPE_IN="/tmp/tinsel.in.b$ID.$P"
      PIPE_OUT="/tmp/tinsel.out.b$ID.$P"
      createFifo "$PIPE_IN"
      createFifo "$PIPE_OUT"
    done
  done
done

# Create host pipes
HOST_ID=-1
HOST_IN="/tmp/tinsel.in.b$HOST_ID"
HOST_OUT="/tmp/tinsel.out.b$HOST_ID"
for P in 0 1 5; do
  createFifo "$HOST_IN.$P"
  createFifo "$HOST_OUT.$P"
done
createFifo "/tmp/pciestream-in"
createFifo "/tmp/pciestream-out"
createFifo "/tmp/pciestream-ctrl"

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
      if [ $X -ne $HOST_X -o $(($Y+1)) -ne $HOST_Y ]; then
        cat /tmp/tinsel.out.b$ID.1 > /dev/null &
        PIDS="$PIDS $!"
      fi
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

# Connect host board to mesh
ID=$(fromCoords 0 $(($MESH_Y-1)))
cat /tmp/tinsel.out.b$ID.1 > $HOST_IN.1 &
PIDS="$PIDS $!"
cat $HOST_OUT.1 > /tmp/tinsel.in.b$ID.1 &
PIDS="$PIDS $!"

# Connect host board to PCIe stream
connect "/tmp/tinsel.out.b$HOST_ID.5" "/tmp/pciestream-out" &
PIDS="$PIDS $!"
connect "/tmp/pciestream-in" "/tmp/tinsel.in.b$HOST_ID.5" &
PIDS="$PIDS $!"
cat /tmp/pciestream-ctrl > /dev/null &
PIDS="$PIDS $!"

# Run one simulator per board
for X in $(seq 0 $LAST_X); do
  for Y in $(seq 0 $LAST_Y); do
    ID=$(fromCoords $X $Y)
    echo "Lauching simulator at position ($X, $Y) with board id $ID"
    BOARD_ID=$ID ./de5Top &
    PIDS="$PIDS $!"
  done
done

# Run host board
echo "Lauching host board simulator at position ($HOST_X, $HOST_Y)" \
     "with board id $HOST_ID"
BOARD_ID=$HOST_ID ./de5HostTop &
PIDS="$PIDS $!"

# On CTRL-C, call quit()
trap quit INT
function quit() {
  echo
  echo "Caught CTRL-C. Exiting."
  #echo "Kill list: $PIDS"
  for PID in "$PIDS"; do
    kill $PID
  done
}

wait
