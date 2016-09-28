#!/bin/bash

rm -f *.cxx *.o *.h *.ba *.bo *.so *.ipinfo
rm -f InstrMem.hex DataMem.hex RunQueue.hex
rm -f testMem de5Top sockitTop testMailbox
rm -f de5Top.v mkCore.v mkDCache.v sockitTop.v
rm -rf test-mem-log
rm -rf test-mailbox-log
