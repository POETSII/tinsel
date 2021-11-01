#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

PCIE_NUM=$(lspci -d 1172:de10 |cut -d ' ' -f 1)
echo 1 > /sys/bus/pci/devices/0000:$PCIE_NUM/reset
echo 1 > /sys/bus/pci/devices/0000:$PCIE_NUM/remove # remove/rescan required for DE10 on AMD - otherwise device BAR listed as "virtual"
echo 1 > /sys/bus/pci/rescan
setpci -d 1172:de10 COMMAND=0x06:0x06
lspci -d 1172:de10 -v -nn
