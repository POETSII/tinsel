#!/bin/bash

# Obtain config parameters as compile flags
DEFS=`python ../config.py defs`

# Load config parameters into shell variables
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

# Bluespec compiler flags
BSC="bsc"
BSCFLAGS="-wait-for-license -suppress-warnings S0015 $DEFS "

# Determine top-level module
case `eval echo $TargetBoard` in
  DE5)
    TOPFILE=DE5Top.bsv
    TOPMOD=de5Top
  ;;
  SoCKit)
    TOPFILE=SoCKitTop.bsv
    TOPMOD=sockitTop
  ;;
  *)
    echo "Unknown target board '$TargetBoard'"
    exit
  ;;
esac

# Determine compiler options
case "$1" in
  sim)
    BSCFLAGS="$BSCFLAGS -D SIMULATE"
  ;;
  verilog)
    SYNTH=1
  ;;
  tracegen)
    TOPFILE=TraceGen.bsv
    TOPMOD=traceGen
    BSCFLAGS="$BSCFLAGS -D SIMULATE"
  ;;
  *)
    echo 'Usage: make.sh {sim|verilog|tracegen}'
    exit
  ;;
esac

# Build
echo Compiling $TOPMOD in file $TOPFILE
if [ "$SYNTH" = "1" ]
then
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
