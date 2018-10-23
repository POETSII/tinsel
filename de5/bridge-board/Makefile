TINSEL_ROOT = ../..
include $(TINSEL_ROOT)/globals.mk

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

.PHONY: all
all: 
	make -C $(TINSEL_ROOT)/rtl verilog
	make -C megafuns
	qsys-generate -syn SoC.qsys
	quartus_dse DE5.qpf    \
    --num-seeds 11              \
    --launcher local            \
    --num-concurrent 4
	quartus_dse DE5.qpf --report utilization
	quartus_dse DE5.qpf --report fmax_summary

.PHONY: one
one: 
	make -C $(TINSEL_ROOT)/rtl verilog
	make -C megafuns
	qsys-generate -syn SoC.qsys
	quartus_sh --flow compile DE5.qpf

.PHONY: report
report:
	quartus_dse DE5.qpf --report utilization
	quartus_dse DE5.qpf --report fmax_summary

.PHONY: download-sof
download-sof:
	quartus_pgm DE5.cdf

.PHONY: clean
clean:
	make -C megafuns clean
	rm -f *.mif *.rpt *.msg *.summary *.sld *.sopcinfo *.jdi
	rm -f *.pin *.done *.qws *.sof *.csv *.qws *.smsg
	rm -rf dse* db incremental_db SoC .qsys_edit reconfig_mif SoC .qsys-edit
