#!/bin/bash

TINSEL_ROOT=/local/tinsel
QUARTUS_PGM=/local/ecad/altera/17.0/quartus/bin/quartus_pgm

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
}

function start {

  stop
  echo "Starting tinsel service"

  # Load PCIeStream Daemon kernel module
  modprobe dmabuffer

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
}

case $1 in
  start)
    start
    echo "Start done"
  ;;

  stop)
    stop
  ;;

  *)
    echo "Usage: tinsel.sh (start|stop)"
  ;;
esac
