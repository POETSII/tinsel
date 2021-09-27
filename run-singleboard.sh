#!/bin/bash
# make clean
make -C apps/boot
make -C hostlink boardctrld POST
make -C DE10Pro/DE10-reference-project update-mif-flash
sleep 1
./hostlink/POST
