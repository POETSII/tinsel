#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

TINSEL_ROOT=/local/tinsel

if [ "$UID" != "0" ]; then
  echo "This script must be run as root"
  exit 1
fi

function stop {
  echo "Stopping tinsel service"

  # Turn off power to the worker boards
  echo "Turning off worker FPGAs"
  $TINSEL_ROOT/bin/fpga-power.sh off
  sleep 1
  
  # If pciestreamd is alive, kill it
  echo "Terminating pciestreamd"
  killall -q -9 pciestreamd
  sleep 1

  # If boardctrld is alive, kill it
  echo "Terminating boardctrld"
  killall -q -9 boardctrld
  sleep 1
}

function start {
  stop
  echo "Starting tinsel service"

  # Try to load PCIeStream Daemon kernel module
  if [[ $(lsmod | grep dmabuffer) ]]; then
    echo "Kernel module 'dmabuffer' already loaded"
  else
    if modprobe dmabuffer; then
      echo "Loaded kernel module 'dmabuffer'"
    else
      # Rebuild kernel module and try again
      dkms build dmabuffer -v 1.0
      dkms install dmabuffer -v 1.0
      modprobe dmabuffer
    fi  
  fi

  # Determine bridge board's PCIe BAR
  BAR=$(lspci -d 1172:0de5 -v   | \
        grep "Memory at"        | \
        sed 's/.*Memory at \([a-h0-9]*\).*/\1/')
  if [ ! ${#BAR} -eq 8 ]; then
    echo "Failed to determine bridge board's PCIe BAR"
    exit 1
  fi

  # Start the PCIeStream Daemon
  echo "Starting pciestreamd (BAR=$BAR)"
  $TINSEL_ROOT/hostlink/pciestreamd $BAR 2>&1 > /tmp/pciestreamd.log &

  # Start the Board Control Daemon
  echo "Starting boardctrld"
  $TINSEL_ROOT/hostlink/boardctrld 2>&1 > /tmp/boardctrld.log &
}

case $1 in
  start)
    start
  ;;

  stop)
    stop
  ;;

  *)
    echo "Usage: tinsel.sh (start|stop)"
  ;;
esac
