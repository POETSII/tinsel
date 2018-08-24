#!/bin/bash

if [ "$1" != "off" -a "$1" != "on" ]; then
  echo "Usage: $0 (on|off)"
  exit 0
fi

# Product and vendor id of power-board USB hubs
VENDOR_ID=1a40
PRODUCT_ID=0101

INDEX=0
for DEV in /sys/bus/usb/devices/*; do 
  VendorId=$(cat $DEV/idVendor 2> /dev/null)
  ProductId=$(cat $DEV/idProduct 2> /dev/null)
  if [ "$VendorId" == $VENDOR_ID -a "$ProductId" == "$PRODUCT_ID" ]; then
    D=`basename $DEV`
    if [ "$1" == "on" ]; then
      echo $D > /sys/bus/usb/drivers/usb/bind 2> /dev/null
    else
      echo $D > /sys/bus/usb/drivers/usb/unbind 2> /dev/null
    fi
  fi
done

sleep 1
