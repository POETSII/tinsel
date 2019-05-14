#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

PCIE_NUM=$(lspci -d 1172:0de5 |cut -d ' ' -f 1)
echo 1 > /sys/bus/pci/devices/0000:$PCIE_NUM/reset
