#!/bin/bash

export PATH=$PATH:$(realpath ../bin)

HOSTLINK=hostlink
if [ "$1" == "sim" ]; then
  HOSTLINK=hostlink-sim
fi

if [ -z "`which $HOSTLINK`" ]; then
  echo "Can't find $HOSTLINK executable"
  exit -1
fi

make --quiet
for FILE in *.S; do
  TEST=$(basename $FILE .S)
  echo -ne "$TEST\t"
  $HOSTLINK boot $TEST.code.v $TEST.data.v -o
  $HOSTLINK dump -n 1 > $TEST.out
  RESULT=$(cut -d ' ' -f 3 $TEST.out)
  if [ "$RESULT" == "1" ]; then
    echo "PASSED"
  else
    NUM=$((16#$RESULT/2))
    echo "FAILED #$NUM"
    exit -1
  fi
done
