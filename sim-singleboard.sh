#!/bin/bash
# make clean
bash -c "cd rtl && make sim"
pkill boardctrld
pkill bluetcl
make -C hostlink sim/boardctrld sim/POST
make -C apps/hello sim all
bash -c "cd rtl && ./sim-singleboard.sh" & bash -c "cd hostlink && sleep 5 && echo starting POST && sim/POST"

trap quit INT
function quit() {
  echo
  echo "Caught CTRL-C. Exiting."
  pkill boardctrld
  pkill hostlink
  pkill POST
  pkill bluetcl
}

wait
