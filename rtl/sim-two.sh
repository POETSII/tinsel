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
done <<< `python3 ../config.py envs`
echo "rtl/sim-singleboard.sh"

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

PIDS=""

# Start boardctrld
echo "Starting boardctrld"
$BOARDCTRLD &
PIDS="$PIDS $!"

echo "Lauching simulator at position (0, 0) with board id 0"
BOARD_ID=0 make BOARD_ID=0 runsim | grep -v Warning &
PIDS="$PIDS $!"

echo "Lauching simulator at position (1, 0) with board id 1"
BOARD_ID=1 make BOARD_ID=1 runsim | grep -v Warning &
PIDS="$PIDS $!"

NORTH_ID_BASE=4
SOUTH_ID_BASE=8
EAST_ID_BASE=12
WEST_ID_BASE=16


# create mesh links.
$UDSOCK join "@tinsel.b0.4" "@tinsel.b1.4" &
PIDS="$PIDS $!"
$UDSOCK join "@tinsel.b0.8" "@tinsel.b1.8" &
PIDS="$PIDS $!"
$UDSOCK join "@tinsel.b0.12" "@tinsel.b1.12" &
PIDS="$PIDS $!"
$UDSOCK join "@tinsel.b0.16" "@tinsel.b1.16" &
PIDS="$PIDS $!"


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
