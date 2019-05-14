#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

# Script for putting SOF image into DE5-NET flash

if [ -z "$1" -o -z "$2" ]; then
  echo Please specify .sof file to put into flash
  echo "Usage: flash.sh [FILE].sof [CABLE]"
  exit
fi

SOF=$1
CABLE=$2

# First program the FPGA using S5_PFL.sof giving access to the flash.
# S5_PFL.sof is available on the Terasic CD and also  the tinsel bitfiles page.
quartus_pgm -m jtag -c $CABLE -o "p;S5_PFL.sof"

# Convert the sof file to a flash file
sof2flash --input=$SOF --output=flash_hw.flash --offset=0x20C0000 \
          --pfl --optionbit=0x00030000 --programmingmode=PS

# Finally put the image into flash
nios2-flash-programmer -c $CABLE --base=0x0 flash_hw.flash

# Remember to set factory load pin on DE5-NET to 0
