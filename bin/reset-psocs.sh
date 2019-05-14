#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

VENDOR_ID="04b4"
PRODUCT_ID="f139"
FROM_INDEX=0

if [ ! -z "$1" ]; then
  FROM_INDEX=$1
fi

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

INDEX=0
for DEV in /sys/bus/usb/devices/*; do 
  VendorId=$(cat $DEV/idVendor 2> /dev/null)
  ProductId=$(cat $DEV/idProduct 2> /dev/null)
  if [ "$VendorId" == $VENDOR_ID -a "$ProductId" == "$PRODUCT_ID" ]; then
    if [ "$INDEX" -ge "$FROM_INDEX" ]; then
      SerialNum=$(cat $DEV/serial 2> /dev/null)
      openocd_reset $SerialNum
    fi
    INDEX=$(expr $INDEX + 1)
  fi
done
