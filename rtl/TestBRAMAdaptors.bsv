// SPDX-License-Identifier: BSD-2-Clause
import BlockRam::*; // for types

import BlueCheck :: *;

typedef UInt#(2) AddrA;
typedef UInt#(4) AddrB;
typedef Bit#(28) DataA;
typedef Bit#(7) DataB;

// Default options
BlockRamOpts testBlockRamOpts =
  BlockRamOpts {
    //readDuringWrite: DontCare,
    readDuringWrite: OldData,
    style: "AUTO",
    registerDataOut: True,
    initFile: Invalid
  };


//////////////////////////////////////////
// Tests!
//////////////////////////////////////////
module [BlueCheck] checkBRAM ();
  /* Specification instance */
  BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedOpts_SIMULATE(testBlockRamOpts);

  /* Implmentation instance */
  BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) imp <- mkBlockRamTrueMixedOpts(testBlockRamOpts);
  // BlockRamTrueMixedByteEn#(AddrA, DataA, AddrB, DataB, TDiv#(SizeOf#(DataA), 8)) imp <- mkBlockRamTrueMixedBEOpts_SIMULATE(defaultBlockRamOpts);

  equiv("putA"    , spec.putA    , imp.putA);
  equiv("outA"   , spec.dataOutA   , imp.dataOutA);
  equiv("putB", spec.putB, imp.putB);
  equiv("outB"    , spec.dataOutB    , imp.dataOutB);
endmodule

module [Module] mkBRAMTest ();
  blueCheck(checkBRAM);
endmodule
