#!/bin/bash

# Obtain config parameters as compile flags
DEFS=`python ../config.py defs`

# Load config parameters into shell variables
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

# Bluespec compiler flags
BSC="bsc"
BSCFLAGS="-wait-for-license -suppress-warnings S0015 \
          -steps-warn-interval 200000 -check-assert $DEFS "

# Determine top-level module
TOPFILE=DE5Top.bsv
TOPMOD=de5Top

# Determine compiler options
case "$1" in
  sim)
    BSCFLAGS="$BSCFLAGS -D SIMULATE"
  ;;
  verilog)
    BSCFLAGS="$BSCFLAGS +RTS -K16M -RTS"
    SYNTH=1
  ;;
  test-mem)
    TOPFILE=TestMem.bsv
    TOPMOD=testMem
    BSCFLAGS="$BSCFLAGS -D SIMULATE -suppress-warnings G0023 "
  ;;
  test-mailbox)
    TOPFILE=TestMailbox.bsv
    TOPMOD=testMailbox
    BSCFLAGS="$BSCFLAGS -D SIMULATE"
  ;;
  test-array-of-queue)
    TOPFILE=TestArrayOfQueue.bsv
    TOPMOD=testArrayOfQueue
    BSCFLAGS="$BSCFLAGS -D SIMULATE"
  ;;
  *)
    echo 'Build targets: '
    echo '  sim                  generate simulator'
    echo '  verilog              generate verilog'
    echo '  test-mem             cache/memory test bench'
    echo '  test-mailbox         mailbox test bench'
    echo '  test-array-of-queue  ArrayOfQueue test bench'
    exit
  ;;
esac

# Build
echo Compiling $TOPMOD in file $TOPFILE
if [ "$SYNTH" = "1" ]
then
  BSCFLAGS="-opt-undetermined-vals -unspecified-to X $BSCFLAGS"
  eval "$BSC $BSCFLAGS -u -verilog -g $TOPMOD $TOPFILE"
else
  if eval "$BSC $BSCFLAGS -sim -g $TOPMOD -u $TOPFILE"
  then
    if eval "$BSC $BSCFLAGS -sim -o $TOPMOD -e $TOPMOD *.c"
    then
        echo Compilation complete
    else
        echo Failed to generate executable simulation model
    fi
  else
    echo Failed to compile
  fi
fi
