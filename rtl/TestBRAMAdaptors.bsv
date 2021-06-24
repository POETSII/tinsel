// SPDX-License-Identifier: BSD-2-Clause
import BlockRam::*; // for types

import BlueCheck :: *;

typedef UInt#(4) AddrA;
typedef UInt#(4) AddrB;
typedef Bit#(8) DataA;
typedef Bit#(8) DataB;

//////////////////////////////////////////
// Tests!
//////////////////////////////////////////
module [BlueCheck] checkBRAM ();
  /* Specification instance */
  BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedOpts_SIMULATE(defaultBlockRamOpts);

  /* Implmentation instance */
  BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) imp <- mkBlockRamTrueMixedOpts_S10(defaultBlockRamOpts);

  equiv("putA"    , spec.putA    , imp.putA);
  equiv("outA"   , spec.dataOutA   , imp.dataOutA);
  equiv("putB", spec.putB, imp.putB);
  equiv("outB"    , spec.dataOutB    , imp.dataOutB);
endmodule

module [Module] mkBRAMTest ();
  blueCheck(checkBRAM);
endmodule
