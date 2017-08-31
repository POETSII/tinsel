# Tinsel root
TINSEL_ROOT = ..

include $(TINSEL_ROOT)/globals.mk

# Obtain config parameters
DEFS = $(shell python ../config.py defs)

# BSV compiler flags
BSC = bsc
BSCFLAGS = -wait-for-license -suppress-warnings S0015 \
           -suppress-warnings G0023 \
           -steps-warn-interval 400000 -check-assert \
           +RTS -K32M -RTS

# Top level module
TOPFILE = DE5Top.bsv
TOPMOD = de5Top

# Top level module for bridge board
HOSTTOPFILE = DE5BridgeTop.bsv
HOSTTOPMOD = de5BridgeTop

.PHONY: sim
sim: $(TOPMOD) $(HOSTTOPMOD)

.PHONY: verilog
verilog: $(TOPMOD).v $(HOSTTOPMOD).v

$(TOPMOD): *.bsv *.c InstrMem.hex
	make -C $(TINSEL_ROOT)/apps/boot
	make -C $(TINSEL_ROOT)/hostlink udsock
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE -sim -g $(TOPMOD) -u $(TOPFILE)
	$(BSC) $(BSCFLAGS) -sim -o $(TOPMOD) -e $(TOPMOD) *.c

$(TOPMOD).v: *.bsv $(QP)/InstrMem.mif
	make -C $(TINSEL_ROOT)/apps/boot
	$(BSC) $(BSCFLAGS) -opt-undetermined-vals -unspecified-to X \
         $(DEFS) -u -verilog -g $(TOPMOD) $(TOPFILE)

$(HOSTTOPMOD): *.bsv *.c
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE -sim -g $(HOSTTOPMOD) -u $(HOSTTOPFILE)
	$(BSC) $(BSCFLAGS) -sim -o $(HOSTTOPMOD) -e $(HOSTTOPMOD) *.c

$(HOSTTOPMOD).v: *.bsv
	$(BSC) $(BSCFLAGS) -opt-undetermined-vals -unspecified-to X \
         $(DEFS) -u -verilog -g $(HOSTTOPMOD) $(HOSTTOPFILE)

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

.PHONY: clean
clean:
	rm -f *.cxx *.o *.h *.ba *.bo *.so *.ipinfo
	rm -f InstrMem.hex DataMem.hex RunQueue.hex
	rm -f testMem de5Top testMailbox de5BridgeTop
	rm -f de5Top.v mkCore.v mkDCache.v mkMailbox.v mkDebugLinkRouter.v mkFPU.v
	rm -f de5BridgeTop.v
	rm -rf test-mem-log
	rm -rf test-mailbox-log
	rm -rf test-array-of-queue-log
