#!/bin/bash

trap quit INT
function quit() {
  echo
  echo "Caught CTRL-C. Exiting."
  pkill boardctrld &\
  pkill hostlink &\
  pkill POST &\
  pkill bluetcl
}

# make clean
make -C apps/boot all

bash -c "cd rtl && make sim"
# make -C apps/boot all
pkill boardctrld
pkill bluetcl
make -C hostlink sim/boardctrld sim/POST
# make -C apps/hello sim all
bash -c "cd rtl && ./sim-singleboard.sh" & bash -c "cd hostlink && sleep 5 && echo starting POST && sim/POST"


wait
