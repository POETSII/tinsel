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


.PHONY: one
one:
	make -C $(TINSEL_ROOT)/rtl verilog
	qsys-generate -syn S5_DDR3_QSYS.qsys
	quartus_sh --flow compile Golden_top.qpf

.PHONY: report
report:
	quartus_dse Golden_top.qpf --report utilization
	quartus_dse Golden_top.qpf --report fmax_summary

.PHONY: update-mif
update-mif:
	make -C $(TINSEL_ROOT)/apps/boot
	quartus_cdb --update_mif Golden_top.qpf
	quartus_asm Golden_top.qpf

.PHONY: download-sof
download-sof:
	quartus_pgm Golden_top.cdf

.PHONY: clean
clean:
	rm -f *.mif *.rpt *.msg *.summary *.sld *.sopcinfo *.jdi
	rm -f *.pin *.done *.qws *.sof *.csv *.qws *.smsg
	rm -rf db incremental_db S5_DDR3_QSYS .qsys_edit reconfig_mif
