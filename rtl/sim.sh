#! env bash
make sim
make -C ../apps/boot sim


UDSOCK="../hostlink/udsock"
if [ ! -e "$UDSOCK" ]; then
  make -C ../hostlink udsock
  echo 'Cannot find udsock tool'
  exit
fi


BOARDCTRLD="../hostlink/sim/boardctrld"
if [ ! -e "$BOARDCTRLD" ]; then
  make -C ../hostlink sim/boardctrld
  exit
fi


# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

$BOARDCTRLD &
PIDS="$PIDS $!"

MESH_MAX_X=$((2 ** $MeshXBitsWithinBox))
MESH_MAX_Y=$((2 ** $MeshYBitsWithinBox))
echo "Max mesh dimensions: $MESH_MAX_X x $MESH_MAX_Y"

# Currently limited to one box in simulation
MESH_X=$MeshXLenWithinBox
MESH_Y=$MeshYLenWithinBox
echo "Using mesh dimensions: $MESH_X x $MESH_Y"

# Convert coords to board id
function fromCoords {
  echo $(($2 * $MESH_MAX_X + $1))
}


ID=$(fromCoords 0 0)
echo "Lauching simulator at position (0, 0) with board id $ID"
BOARD_ID=$ID ./de5Top | grep -v Warning &
PIDS="$PIDS $!"

sleep 5

../apps/boot/sim

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
