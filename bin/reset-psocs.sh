#!/bin/bash

VENDOR_ID="04b4"
PRODUCT_ID="f139"

function openocd_reset {
  while [ 1==1 ]; do
    OUT=$(openocd -f interface/kitprog.cfg -c "kitprog_serial $1" \
            -c kitprog_init_acquire_psoc -f target/psoc5lp.cfg \
            -c 'init; reset; sleep 1000; shutdown' 2>&1 | grep Error)
    if [ -z "$OUT" ]; then
      break
    fi
    sleep 1
  done
}

for DEV in /sys/bus/usb/devices/*; do 
  VendorId=$(cat $DEV/idVendor 2> /dev/null)
  ProductId=$(cat $DEV/idProduct 2> /dev/null)
  if [ "$VendorId" == $VENDOR_ID -a "$ProductId" == "$PRODUCT_ID" ]; then
    SerialNum=$(cat $DEV/serial 2> /dev/null)
    openocd_reset $SerialNum
  fi
done
