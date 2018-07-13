#!/bin/bash

TINSEL_ROOT=/local/tinsel
QUARTUS_PGM=/local/ecad/altera/17.0/quartus/bin/quartus_pgm

if [ "$UID" != "0" ]; then
  echo "This script must be run as root"
  exit 1
fi

function stop {
  echo "Stopping tinsel service"

  # Try to exit PCIeStream Daemon gracefully
  RUNNING=$(netstat -anx | grep pciestream)
  if [ ! -z "$RUNNING" ]; then
    echo "Sending exit command to pciestreamd"
    echo -n e | $TINSEL_ROOT/hostlink/udsock in @pciestream-ctrl
  fi

  # Turn off power to the worker boards
  echo "Turning off worker FPGAs"
  $TINSEL_ROOT/hostlink/tinsel-power off
  sleep 1
  
  # If pciestreamd is still alive, kill it
  echo "Terminating pciestreamd"
  killall -q -9 pciestreamd
  sleep 1
}

function start {
  # Reset the power management boards
  echo "Resetting PSoC power management boards"
  $TINSEL_ROOT/bin/reset-psocs.sh

  stop
  echo "Starting tinsel service"

  # Load PCIeStream Daemon kernel module
  MOD_LOADED=$(lsmod | grep dmabuffer)
  if [ -z "$MOD_LOADED" ]; then
    insmod $TINSEL_ROOT/hostlink/driver/dmabuffer.ko
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
}

case $1 in
  start)
    start
    echo "Start done"
  ;;

  stop)
    stop
  ;;

  restart)
    start
  ;;

  reboot)
    stop
    echo "Reprogramming bridge board"
    $QUARTUS_PGM -m jtag -o "p;$TINSEL_ROOT/sof/tinsel-0.3-bridge.sof"
    reboot
  ;;

  *)
    echo "Usage: tinsel.sh (start|stop|restart|reboot)"
  ;;
esac
