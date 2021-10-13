#!/bin/bash
#  SIM_BINARY="./de5Top"

SIM_BINARY=./mkDE10Top

if [ ! -e $SIM_BINARY ]; then
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
done <<< `python ../config.py envs`

MESH_MAX_X=$((2 ** $MeshXBitsWithinBox))
MESH_MAX_Y=$((2 ** $MeshYBitsWithinBox))
echo "Max mesh dimensions: $MESH_MAX_X x $MESH_MAX_Y"

# Currently limited to one box in simulation
MESH_X=$MeshXLenWithinBox
MESH_Y=$MeshYLenWithinBox
echo "Using mesh dimensions: $MESH_X x $MESH_Y"

# HOST0_X=0
# HOST0_Y=0
# HOST1_X=0
# HOST1_Y=1
# echo "Connecting bridge board at location ($HOST0_X, $HOST0_Y)"
# echo "Connecting bridge board at location ($HOST1_X, $HOST1_Y)"

# Check dimensions
if [ $MESH_X -gt $MESH_MAX_X ] || [ $MESH_Y -gt $MESH_MAX_Y ] ; then
  echo "ERROR: max mesh dimensions exceeded"
  exit
fi

# Convert coords to board id
function fromCoords {
  echo $(($2 * $MESH_MAX_X + $1))
}

LAST_X=$(($MESH_X - 1))
LAST_Y=$(($MESH_Y - 1))

# Socket ids for each link
NORTH_ID_BASE=4
SOUTH_ID_BASE=8
EAST_ID_BASE=12
WEST_ID_BASE=16

PIDS=""

# Start boardctrld
echo "Starting boardctrld"
$BOARDCTRLD &
PIDS="$PIDS $!"

# Run one simulator per board
for X in $(seq 0 $LAST_X); do
  for Y in $(seq 0 $LAST_Y); do
    ID=$(fromCoords $X $Y)
    echo "Lauching simulator at position ($X, $Y) with board id $ID"
    BOARD_ID=$ID make runsim | grep -v Warning &
    PIDS="$PIDS $!"
  done
done

# # Run bridge board
# HOST_ID=-1
# echo "Lauching bridge board simulator with board id $HOST_ID"
# BOARD_ID=$HOST_ID LD_PRELOAD="/home/tparks/upstream/bsc/inst/lib/VPI/libbdpi.so /home/tparks/Projects/POETS/tinsel/rtl/de5BridgeTop.so" ./de5BridgeTop &
# PIDS="$PIDS $!"

# Create horizontal links
for Y in $(seq 0 $LAST_Y); do
  for X in $(seq 0 $LAST_X); do
    A=$(fromCoords $X $Y)
    B=$(fromCoords $(($X+1)) $Y)
    if [ $(($X+1)) -lt $MESH_X ]; then
      for I in $(seq 1 $NumEastWestLinks); do
        E=$(($EAST_ID_BASE + $I - 1))
        W=$(($WEST_ID_BASE + $I - 1))
        $UDSOCK join "@tinsel.b$A.$E" "@tinsel.b$B.$W" &
        PIDS="$PIDS $!"
      done
    fi
  done
done

# Create vertical links
for X in $(seq 0 $LAST_X); do
  for Y in $(seq 0 $LAST_Y); do
    A=$(fromCoords $X $Y)
    B=$(fromCoords $X $(($Y+1)))
    if [ $(($Y+1)) -lt $MESH_Y ]; then
      for I in $(seq 1 $NumNorthSouthLinks); do
        N=$(($NORTH_ID_BASE + $I - 1))
        S=$(($SOUTH_ID_BASE + $I - 1))
        $UDSOCK join "@tinsel.b$A.$N" "@tinsel.b$B.$S" &
        PIDS="$PIDS $!"
      done
    fi
  done
done

# # Connect bridge board to mesh
# ENTRY1_ID=$(fromCoords $HOST1_X $HOST1_Y)
# $UDSOCK join "@tinsel.b$ENTRY1_ID.$WEST_ID_BASE" \
#              "@tinsel.b$HOST_ID.$NORTH_ID_BASE" &
# PIDS="$PIDS $!"
# ENTRY0_ID=$(fromCoords $HOST0_X $HOST0_Y)
# $UDSOCK join "@tinsel.b$ENTRY0_ID.$WEST_ID_BASE" \
#              "@tinsel.b$HOST_ID.$SOUTH_ID_BASE" &
# PIDS="$PIDS $!"

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
