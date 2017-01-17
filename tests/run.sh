#!/bin/bash

export PATH=$PATH:$(realpath ../bin)

case "$1" in
  jtag)
    HOSTLINK=hostlink
    ;;
  sim)
    HOSTLINK=hostlink-sim
    ;;
  *)
    echo "Usage: "
    echo "  run.sh sim       Connect to tinsel machine in simulation"
    echo "  run.sh jtag      Connect to tinsel machine over JTAG"
    exit -1
    ;;
esac

if [ -z "`which $HOSTLINK`" ]; then
  echo "Can't find $HOSTLINK executable"
  echo "(Did you forget to build hostlink?)"
  exit -1
fi

for FILE in *.S; do
  TEST=$(basename $FILE .S)
  echo -ne "$TEST\t"
  LD_LIBRARY_PATH=$QUARTUS_ROOTDIR/linux64 $HOSTLINK \
    $TEST.code.v $TEST.data.v -o -n 1 > $TEST.out
  RESULT=$(cut -d ' ' -f 3 $TEST.out)
  if [ "$RESULT" == "1" ]; then
    echo "PASSED"
  else
    NUM=$((16#$RESULT/2))
    echo "FAILED #$NUM"
    exit -1
  fi
done
