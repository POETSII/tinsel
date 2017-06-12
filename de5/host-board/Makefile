TINSEL_ROOT = ../..
include $(TINSEL_ROOT)/globals.mk

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

.PHONY: all
all:
	make -C $(TINSEL_ROOT)/rtl verilog
	qsys-generate -syn SoC.qsys
	quartus_sh --flow compile DE5.qpf

.PHONY: download-sof
download-sof:
	quartus_pgm DE5.cdf

.PHONY: clean
clean:
	rm -f *.mif *.rpt *.msg *.summary *.sld *.sopcinfo *.jdi
	rm -f *.pin *.done *.qws *.sof *.csv *.qws *.smsg
	rm -rf db incremental_db SoC .qsys_edit reconfig_mif
