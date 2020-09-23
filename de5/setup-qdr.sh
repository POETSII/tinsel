#!/bin/bash

# Load config parameters
while read -r EXPORT; do
  eval $EXPORT
done <<< `python ../config.py envs`

echo 'project_open -current_revision Golden_top;' > setup-qdr.tcl

rm -f S5_DDR3_QSYS.qsys
if [ "$EnableQDRIISRAM" == "False" ]; then
  echo "Disabling QDRII SRAMs"
  ln -s SoC_No_QDR.qsys S5_DDR3_QSYS.qsys
  ln -s DE5Top_hw_No_QDR.tcl DE5Top_hw.tcl
else
  echo "Enabling QDRII SRAMs"
  ln -s SoC_With_QDR.qsys S5_DDR3_QSYS.qsys
  ln -s DE5Top_hw_With_QDR.tcl DE5Top_hw.tcl
  echo 'set_global_assignment -name VERILOG_MACRO "ENABLE_QDR_SRAMs=1"' >> \
    setup-qdr.tcl
fi

echo 'project_close' >> setup-qdr.tcl
