#!/bin/bash

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

rm -f S5_DDR3_QSYS.qsys
if [ "$EnableQDRIISRAM" == "False" ]; then
  echo "Disabling QDRII SRAMs"
  ln -s SoC_No_QDR.qsys S5_DDR3_QSYS.qsys
  echo '' > setup-qdr.tcl
else
  echo "Enabling QDRII SRAMs"
  ln -s SoC_With_QDR.qsys S5_DDR3_QSYS.qsys
  echo 'set_global_assignment -name VERILOG_MACRO "ENABLE_QDR_SRAMs=1"' > \
    setup-qdr.tcl
fi
