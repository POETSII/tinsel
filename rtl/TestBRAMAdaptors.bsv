// SPDX-License-Identifier: BSD-2-Clause
import BlockRam::*; // for types

import BlueCheck :: *;
import StmtFSM::*;
import Assert::*;


typedef UInt#(2) AddrA;
typedef UInt#(4) AddrB;
typedef Bit#(28) DataA;
typedef Bit#(7) DataB;

// works
// typedef UInt#(3) AddrA;
// typedef UInt#(5) AddrB;
// typedef Bit#(64) DataA;
// typedef Bit#(16) DataB;


// typedef UInt#(1) AddrA; // 2 A's
// typedef UInt#(5) AddrB; // 16 B's
// typedef Bit#(128) DataA;
// typedef Bit#(8) DataB;

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
  BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) imp <- mkBlockRamTrueMixedOpts_S10(testBlockRamOpts);
  // BlockRamTrueMixedByteEn#(AddrA, DataA, AddrB, DataB, TDiv#(SizeOf#(DataA), 8)) imp <- mkBlockRamTrueMixedBEOpts_SIMULATE(defaultBlockRamOpts);
  // rule driveB;
  //   spec.putB(False, 5, 0);
  //   imp.putB(False, 5, 0);
  // endrule

  // rule printB;
  //   $display("%x, %x", spec.dataOutB, imp.dataOutB);
  // endrule

  equiv("putA"    , spec.putA    , imp.putA);
  equiv("outA"   , spec.dataOutA   , imp.dataOutA);
  equiv("putB", spec.putB, imp.putB);
  equiv("outB"    , spec.dataOutB    , imp.dataOutB);
endmodule

// works fine
// module mkBRAMTest();
//   BlockRam#(AddrB, DataB) spec <- mkBlockRamOpts_SIMULATED(testBlockRamOpts);
//
//   Stmt test =
//   seq
//
//     action
//       spec.write(0,'h2A);
//       $display($time, " put True,0,'h2A");
//     endaction
//
//     action spec.read(0); endaction
//
//     // action
//     //   spec.putA(True,0,'hBBBBBBB);
//     //   imp.putA(True,0,'hBBBBBBB);
//     //   $display($time, " put True,0,'heb1a0ea");
//     // endaction
//
//
//     action
//       $display($time, " outB spec %x", spec.dataOut);
//     endaction
//
//   endseq;
//
//   mkAutoFSM(test);
// endmodule


module [Module] mkBRAMTest ();
  blueCheck(checkBRAM);
endmodule

// module mkBRAMTest();
//   BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedOpts_SIMULATE(testBlockRamOpts);
//   BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) imp <- mkBlockRamTrueMixedOpts_S10(testBlockRamOpts);
//
//   rule display;
//     $display($time, " outB %x, outB spec %x", imp.dataOutB, spec.dataOutB);
//   endrule
//
//
//   Stmt test =
//   seq
//
//     action
//       spec.putA(True,0,'heb1a0ea);
//       imp.putA(True,0,'heb1a0ea);
//       $display($time, " put putA(True,0,'heb1a0ea)");
//     endaction
//
//     action
//       spec.putB(False,12,'h7a);
//       imp.putB(False,12,'h7a);
//       $display($time, " put putB(False,12,'h7a)");
//     endaction
//     action
//       spec.putB(False, 3,'h6a);
//       imp.putB(False, 3,'h6a);
//       $display($time, " put putB(False, 3,'h6a)");
//     endaction
//     action
//       spec.putB(True, 7,'h40);
//       imp.putB(True, 7,'h40);
//       $display($time, " put putB(True, 7,'h40)");
//     endaction
//     action
//       spec.putB(True, 3,'h3b);
//       imp.putB(True, 3,'h3b);
//       $display($time, " put putB(True, 3,'h3b)");
//     endaction
//
//
//     action
//     endaction
//
//   endseq;
//
//   mkAutoFSM(test);
// endmodule


// module mkBRAMcfunsTest();
//   BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedOpts_SIMULATE(testBlockRamOpts);
//
//   Stmt test =
//   seq
//
//     action
//       spec.putA(True,0,'hAAAAAAAABBBBBBBBCCCCCCCCDDDDDDDD);
//       $display($time, " put True,0,'hAAAAAAAABBBBBBBBCCCCCCCCDDDDDDDD");
//     endaction
//
//     action
//       $display($time, " outA %x", spec.dataOutA);
//     endaction
//
//     action
//       spec.putA(False,0,'h0);
//       $display($time, " put False,0,'h0");
//     endaction
//
//     action
//       $display($time, " outA %x", spec.dataOutA);
//       dynamicAssert(spec.dataOutA == 128'hAAAAAAAABBBBBBBBCCCCCCCCDDDDDDDD, "blockram.c is reversing words");
//     endaction
//
//
//   endseq;
//
//   mkAutoFSM(test);
// endmodule
