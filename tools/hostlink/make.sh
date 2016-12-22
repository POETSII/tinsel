#!/bin/bash

# Include directory
INC=../../include

# Flags to C++ compiler
CPPFLAGS="-I $INC -O"

# Dertmine files to compile based on make target
case "$1" in
  sim)
    CPPFLAGS="$CPPFLAGS -DSIMULATE"
  ;;
  jtag)
    CPPFLAGS="$CPPFLAGS -ljtag_atlantic -ljtag_client"
    if [ -z "$QUARTUS_ROOTDIR" ]; then
      echo "Please set QUARTUS_ROOTDIR"
      exit -1
    fi
    CPPFLAGS="$CPPFLAGS -L $QUARTUS_ROOTDIR/linux64/"
  ;;
  *)
    echo 'Build targets: '
    echo '  sim'
    echo '  jtag'
    exit 0
  ;;
esac

# Ensure that includes files have been generated
pushd . > /dev/null
cd $INC
./make.sh
popd > /dev/null

g++ Boot.cpp -o boot $CPPFLAGS
