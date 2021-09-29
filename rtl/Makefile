# SPDX-License-Identifier: BSD-2-Clause
# Tinsel root
TINSEL_ROOT = ..

include $(TINSEL_ROOT)/globals.mk

# Obtain config parameters
DEFS = $(shell python ../config.py defs)
STRATIX10 = $(shell python ../config.py print Stratix10)
STRATIXV = $(shell python ../config.py print StratixV)

# A literal space.
space :=
space +=

# Joins elements of the list in arg 2 with the given separator.
#   1. Element separator.
#   2. The list.
join-with = $(subst $(space),$1,$(strip $2))

BSINCLUDE=+
BSINCLUDE+=../DE10Pro/DE10-reference-project/src/DE10-libs/src/bsv
BSINCLUDE+=../DE10Pro/DE10-reference-project/src/BlueStuff/BlueUtils
BSINCLUDE+=../DE10Pro/DE10-reference-project/src/BlueStuff/BlueBasics
BSINCLUDE+=../DE10Pro/DE10-reference-project/src/BlueStuff/AXI
BSINCLUDE+=../DE10Pro/DE10-reference-project/src/BlueStuff/SocketPacketUtils
BSINCLUDE+=../DE10Pro/DE10-reference-project/src/BlueStuff

# BSV compiler flags
BSC = bsc
BSCFLAGS = -suppress-warnings S0015 \
           -suppress-warnings G0023 \
           -steps-warn-interval 750000 -check-assert \
           +RTS -K128M -RTS \
					 -p $(call join-with,:,$(BSINCLUDE))


VPIINCL=$(dir $(shell which bsc))../lib/VPI/

# Top level module
ifeq ($(STRATIX10),True)
TOPFILE = DE10Top.bsv
TOPMOD = mkDE10Top

# Top level module for bridge board
HOSTTOPFILE = DE5BridgeTop.bsv
HOSTTOPMOD = de5BridgeTop
endif

ifeq ($(STRATIXV),True)
TOPFILE = DE5Top.bsv
TOPMOD = de5Top

# Top level module for bridge board
HOSTTOPFILE = DE5BridgeTop.bsv
HOSTTOPMOD = de5BridgeTop
endif

$(info $$STRATIXV is [${STRATIXV}])
$(info $$STRATIX10 is [${STRATIX10}])
$(info $$TOPMOD is [${TOPMOD}])
$(info $$QP is [${QP}])

.PHONY: sim
sim: $(TOPMOD) $(HOSTTOPMOD)

.PHONY: verilog
verilog: $(TOPMOD).v $(HOSTTOPMOD).v

$(TOPMOD): *.bsv *.c InstrMem.hex
	make -C $(TINSEL_ROOT)/apps/boot
	make -C $(TINSEL_ROOT)/hostlink udsock
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE -sim -g $(TOPMOD) -u $(TOPFILE)
	$(BSC) $(BSCFLAGS) -Xc -I$(VPIINCL) -Xc -L$(VPIINCL) -sim -o $(TOPMOD) -e $(TOPMOD) *.c

$(TOPMOD).v: *.bsv $(QP)/InstrMem.mif
	make -C $(TINSEL_ROOT)/apps/boot
	$(BSC) $(BSCFLAGS) -opt-undetermined-vals -unspecified-to X \
         $(DEFS) -u -verilog -g $(TOPMOD) $(TOPFILE)

$(HOSTTOPMOD): *.bsv *.c
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE -sim -g $(HOSTTOPMOD) -u $(HOSTTOPFILE)
	$(BSC) $(BSCFLAGS) -Xc -I$(VPIINCL) -sim -o $(HOSTTOPMOD) -e $(HOSTTOPMOD) *.c

$(HOSTTOPMOD).v: *.bsv
	$(BSC) $(BSCFLAGS) -opt-undetermined-vals -unspecified-to X \
         $(DEFS) -u -verilog -g $(HOSTTOPMOD) $(HOSTTOPFILE)

.PHONY: runsim
runsim:
	LD_PRELOAD=$(VPIINCL)/libbdpi.so:mkDE10Top.so ./mkDE10Top

testMem: *.bsv *.c
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE \
         -sim -g testMem -u TestMem.bsv
	$(BSC) $(BSCFLAGS) -sim -o testMem -e testMem *.c

testMailbox: *.bsv *.c
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE \
         -sim -g testMailbox -u TestMailbox.bsv
	$(BSC) $(BSCFLAGS) -sim -o testMailbox -e testMailbox *.c

InstrMem.hex:
	make -C $(TINSEL_ROOT)/apps/boot

$(QP)/InstrMem.mif:
	make -C $(TINSEL_ROOT)/apps/boot

.PHONY: test-mem
test-mem: testMem

.PHONY: test-mailbox
test-mailbox: testMailbox

.PHONY: test-bram
test-bram:  *.bsv
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE \
         -sim -g mkBRAMTest -u TestBRAMAdaptors.bsv
	$(BSC) $(BSCFLAGS) -Xc -I$(VPIINCL) -sim -o mkBRAMTest -e mkBRAMTest *.c
	LD_PRELOAD=$(VPIINCL)/libbdpi.so:mkBRAMTest.so ./mkBRAMTest

.PHONY: test-jtaguart
test-jtaguart:  *.bsv
	$(BSC) $(BSCFLAGS) $(DEFS) -show-range-conflict \
         -sim -g mkSimJtagUartTop -u testJtagUart.bsv
	$(BSC) $(BSCFLAGS) -Xc -I$(VPIINCL) -sim -o mkSimJtagUartTop -e mkSimJtagUartTop *.c
	LD_PRELOAD=$(VPIINCL)/libbdpi.so:mkSimJtagUartTop.so ./mkSimJtagUartTop

# VHDLSIM=$(shell find ../DE10Pro/DE10-reference-project/quartus/ip/fp -wholename "*fpS10AddSub/*sim/*vhd")
# FPSIMINCLUDE = -vsearch +:$(call join-with,:,$(shell find ../DE10Pro/DE10-reference-project/quartus/ip/fp -type d -name sim)):$(call join-with,:,$(shell find ../scratch/sim_models/lpm_mult -type d -name sim))
FPSIMINCLUDE = -vsearch +:../scratch/sim_models/fp
VHDLSIM = ../scratch/sim_models/fp/dspba_library_package.vhd $(shell find ../scratch/sim_models/fp -name "fp*vhd")
.PHONY: test-fpu
test-fpu:  *.bsv
	$(BSC) $(BSCFLAGS) $(DEFS) -show-range-conflict -verilog -g mkTestFPU -u mkTestFPU.bsv
	$(BSC) $(BSCFLAGS) $(DEFS) -o mkTestFPU -verilog mkTestFPU.bsv
	bsc -D BSV_TIMESCALE=1ns/1ps -verilog -vsim modelsim $(FPSIMINCLUDE) -e mkTestFPU -o mkTestFPU.out $(VHDLSIM)
	# $(BSC) $(BSCFLAGS)  -o mkTestFPU -e mkTestFPU *.c


.PHONY: clean
clean:
	rm -f *.cxx *.o *.h *.ba *.bo *.so *.ipinfo
	rm -f InstrMem.hex DataMem.hex RunQueue.hex
	rm -f testMem de5Top testMailbox de5BridgeTop
	rm -f de5Top.v mkCore.v mkDCache.v mkMailbox.v mkDebugLinkRouter.v
	rm -f mkDE10Top.v mkDE10Top
	rm -f mkFPU.v mkMeshRouter.v
	rm -f de5BridgeTop.v
	rm -rf test-mem-log
	rm -rf test-mailbox-log
	rm -rf test-array-of-queue-log
	rm -f vpi_wrapper_getBoardId.c
	rm -rf libraries work_mkTestFPU
