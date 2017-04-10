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

$(TOPMOD): *.bsv *.c
	make -C $(TINSEL_ROOT)/apps/boot
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE -sim -g $(TOPMOD) -u $(TOPFILE)
	$(BSC) $(BSCFLAGS) -sim -o $(TOPMOD) -e $(TOPMOD) *.c

$(TOPMOD).v: *.bsv
	make -C $(TINSEL_ROOT)/apps/boot
	$(BSC) $(BSCFLAGS) -opt-undetermined-vals -unspecified-to X \
         $(DEFS) -u -verilog -g $(TOPMOD) $(TOPFILE)

testMem: *.bsv *.c
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE \
         -sim -g testMem -u TestMem.bsv
	$(BSC) $(BSCFLAGS) -sim -o testMem -e testMem *.c

testMailbox: *.bsv *.c
	$(BSC) $(BSCFLAGS) $(DEFS) -D SIMULATE \
         -sim -g testMailbox -u TestMailbox.bsv
	$(BSC) $(BSCFLAGS) -sim -o testMailbox -e testMailbox *.c

.PHONY: sim
sim: $(TOPMOD)

.PHONY: verilog
verilog: $(TOPMOD).v

.PHONY: test-mem
test-mem: testMem

.PHONY: test-mailbox
test-mailbox: testMailbox

.PHONY: clean
clean:
	rm -f *.cxx *.o *.h *.ba *.bo *.so *.ipinfo
	rm -f InstrMem.hex DataMem.hex RunQueue.hex
	rm -f testMem de5Top testMailbox
	rm -f de5Top.v mkCore.v mkDCache.v mkMailbox.v mkHostLinkRouter.v mkSFU.v
	rm -rf test-mem-log
	rm -rf test-mailbox-log
	rm -rf test-array-of-queue-log
