#!/bin/bash

TINSEL_ROOT=/root/tinsel

function stop {
  echo "Stopping tinsel service"

  # Try to exit PCIeStream Daemon gracefully
  echo -n e | $TINSEL_ROOT/hostlink/udsock in @pciestream-ctrl

  # Turn off power to the worker boards
  $TINSEL_ROOT/hostlink/tinsel-power off
  sleep 1
  
  # If pciestreamd is still alive, kill it
  killall -9 pciestreamd
  sleep 1
}

function start {
  stop
  echo "Starting tinsel service"

  # Reset the power management boards
  $TINSEL_ROOT/bin/reset-psocs.sh
 
  # Load PCIeStream Daemon kernel module
  insmod $TINSEL_ROOT/hostlink/driver/dmabuffer.ko

  # Determine bridge board's PCIe BAR
  BAR=$(lspci -d 1172:0de5 -v   | \
        grep "Memory at"        | \
        sed 's/.*Memory at \([a-h0-9]*\).*/\1/')
  if [ ! ${#BAR} -eq 8 ]; then
    echo "Failed to determine bridge board's PCIe BAR"
    exit 1
  fi

  # Start the PCIeStream Daemon
  $TINSEL_ROOT/bin/pciestreamd $BAR &
}

case $1 in
  start)
    start
  ;;

  stop)
    stop
  ;;

  restart)
    start
  ;;

  reboot)
    stop
    reboot
  ;;

  *)
    echo "Usage: tinsel.sh (start|stop|restart|reboot)"
  ;;
esac
