TINSEL_ROOT = ..
include $(TINSEL_ROOT)/globals.mk

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

.PHONY: all
all:
	make -C $(TINSEL_ROOT)/rtl verilog
	qsys-generate -syn S5_DDR3_QSYS.qsys
	quartus_dse Golden_top.qpf    \
    --num-seeds 11              \
    --explore timing_aggressive \
    --launcher local            \
    --num-concurrent 4
	quartus_dse Golden_top.qpf --report utilization
	quartus_dse Golden_top.qpf --report fmax_summary

.PHONY: report
report:
	quartus_dse Golden_top.qpf --report utilization
	quartus_dse Golden_top.qpf --report fmax_summary

.PHONY: clean
clean:
	rm -f *.mif
