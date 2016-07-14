#!/bin/bash

# Obtain config parameters
. ../config.sh
DEFS="-D 'DeviceFamily=\"$DeviceFamily\"' \
      -D LogThreadsPerCore=$LogThreadsPerCore \
      -D LogInstrsPerCore=$LogInstrsPerCore \
      -D LogDataWordsPerCore=$LogDataWordsPerCore \
      -D LogCoresPerDCache=$LogCoresPerDCache \
      -D LogWordsPerLine=$LogWordsPerLine \
      -D LogBytesPerLine=$LogBytesPerLine \
      -D WordsPerLine=$WordsPerLine \
      -D LineSize=$LineSize \
      -D DCacheLogSetsPerThread=$DCacheLogSetsPerThread \
      -D DCacheLogNumWays=$DCacheLogNumWays \
      -D LogNumDCaches=$LogNumDCaches \
      -D DRAMLatency=$DRAMLatency"

# Bluespec compiler flags
BSC="bsc"
BSCFLAGS="-wait-for-license -suppress-warnings S0015 $DEFS"

case "$1" in
  sim)
    TOPFILE=Tinsel.bsv
    TOPMOD=tinselCoreSim
    BSCFLAGS="$BSCFLAGS -D SIMULATE"
  ;;
  verilog)
    TOPFILE=Tinsel.bsv
    TOPMOD=tinselCore
    SYNTH=1
  ;;
  tracegen)
    TOPFILE=TraceGen.bsv
    TOPMOD="traceGen"
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
