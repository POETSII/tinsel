// SPDX-License-Identifier: BSD-2-Clause
import BlockRam::*; // for types

import BlueCheck :: *;
import StmtFSM::*;
import Assert::*;
import Vector::*;
import Clocks    :: *;

// typedef UInt#(2) AddrA;
// typedef UInt#(4) AddrB;
// typedef Bit#(28) DataA;
// typedef Bit#(7) DataB;

// works
// typedef UInt#(3) AddrA;
// typedef UInt#(5) AddrB;
// typedef Bit#(64) DataA;
// typedef Bit#(16) DataB;


typedef UInt#(4) AddrA; // 2 A's
typedef UInt#(6) AddrB; // 16 B's
typedef Bit#(64) DataA;
typedef Bit#(16) DataB;


//////////////////////////////////////////
// Tests!
//////////////////////////////////////////
module [BlueCheck] checkBRAMTrueMixedBE#(BlockRamOpts opts) (Empty);

  /* Specification instance */
  BlockRamTrueMixedBE#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedBEOpts_SIMULATE(opts);

  /* Implmentation instance */
  BlockRamTrueMixedBE#(AddrA, DataA, AddrB, DataB) imp <- mkBlockRamTrueMixedBEOpts_S10(opts);

  equiv("putA"    , spec.putA    , imp.putA);
  equiv("outA"   , spec.dataOutA   , imp.dataOutA);
  equiv("putB", spec.putB, imp.putB);
  equiv("outB"    , spec.dataOutB    , imp.dataOutB);
endmodule

module [BlueCheck] checkBRAMTrueMixed#(BlockRamOpts opts) (Empty);

  /* Specification instance */
  BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedOpts_SIMULATE(opts);

  /* Implmentation instance */
  BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) imp <- mkBlockRamTrueMixedOpts_S10(opts);

  equiv("putA"    , spec.putA    , imp.putA);
  equiv("outA"   , spec.dataOutA   , imp.dataOutA);
  equiv("putB", spec.putB, imp.putB);
  equiv("outB"    , spec.dataOutB    , imp.dataOutB);
endmodule

module [BlueCheck] checkBRAMTrueMixedBEPortable#(BlockRamOpts opts) (Empty);

  /* Specification instance */
  BlockRamTrueMixedBE#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedBEOpts_SIMULATE(opts);

  /* Implmentation instance */
  BlockRamTrueMixedBE#(AddrA, DataA, AddrB, DataB) imp <- mkBlockRamPortableTrueMixedBEOpts(opts);

  equiv("putA"    , spec.putA    , imp.putA);
  equiv("outA"   , spec.dataOutA   , imp.dataOutA);
  equiv("putB", spec.putB, imp.putB);
  equiv("outB"    , spec.dataOutB    , imp.dataOutB);
endmodule

module [BlueCheck] checkLUTRamTrueMixedBE#(BlockRamOpts opts) (Empty);

  /* Specification instance */
  BlockRamTrueMixedBE#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedBEOpts_SIMULATE(opts);

  /* Implmentation instance */
  BlockRamTrueMixedBE#(AddrA, DataA, AddrB, DataB) imp <- mkLUTRamTrueMixedBEOpts(opts);

  Reg#(Bit#(TAdd#(SizeOf#(AddrA), 1))) addr_a <- mkReg(0);
  Stmt zero = seq
  $display("zeroing");

    for (addr_a<=0; addr_a<16; addr_a<=addr_a+1) seq
      $display("zeroed %d", addr_a);
      spec.putA(True, unpack(truncate(addr_a)), 64'h0);
      imp.putA(True, unpack(truncate(addr_a)), 64'h0);
    endseq

    $display("zeroed");
    spec.putB(False, 0, 0, 0);
    imp.putB(False, 0, 0, 0);

    delay(5);

  endseq;

  pre("Zero", zero);

  equiv("putA"    , spec.putA    , imp.putA);
  equiv("outA"   , spec.dataOutA   , imp.dataOutA);
  equiv("putB", spec.putB, imp.putB);
  equiv("outB"    , spec.dataOutB    , imp.dataOutB);
endmodule





module [Module] mkBRAMTest ();

  // Default options
  BlockRamOpts oldNoReg =
    BlockRamOpts {
      readDuringWrite: OldData,
      style: "AUTO",
      registerDataOut: False,
      initFile: Invalid
    };

  BlockRamOpts oldReg =
    BlockRamOpts {
      readDuringWrite: OldData,
      style: "AUTO",
      registerDataOut: True,
      initFile: Invalid
    };

  BlockRamOpts dontCareNoReg =
    BlockRamOpts {
      readDuringWrite: DontCare,
      style: "AUTO",
      registerDataOut: False,
      initFile: Invalid
    };

  BlockRamOpts dontCareReg =
    BlockRamOpts {
      readDuringWrite: DontCare,
      style: "AUTO",
      registerDataOut: True,
      initFile: Invalid
    };

  Clock clk <- exposeCurrentClock;
  Reset rst <- exposeCurrentReset;
  MakeResetIfc r <- mkReset(0, True, clk);
  Reset brams_reset = rst; //mkResetEither(r.new_rst, rst);

  Vector#(4, BlockRamOpts) opts = newVector();
  opts[0] = oldNoReg;
  opts[1] = oldReg;
  opts[2] = dontCareNoReg;
  opts[3] = dontCareReg;

  Vector#(12, FSM) testers = newVector();

  let params = bcParams;
  // params.interactive   = False;
  // params.outputFIFO    = tagged Valid out;


  for (Integer optctr=0; optctr<4; optctr=optctr+1) begin
      let tm <- mkModelChecker(checkBRAMTrueMixed(opts[optctr], reset_by brams_reset), bcParams);
      testers[optctr*3] <- mkFSM(tm);
      let tmbe <- mkModelChecker(checkBRAMTrueMixedBE(opts[optctr], reset_by brams_reset), bcParams);
      testers[optctr*3+1] <- mkFSM(tmbe);
      let tmbeport <- mkModelChecker(checkBRAMTrueMixedBEPortable(opts[optctr], reset_by brams_reset), bcParams);
      testers[optctr*3+2] <- mkFSM(tmbeport);
  end

  Stmt lutRamStmts <- mkModelChecker(checkLUTRamTrueMixedBE(oldNoReg, reset_by brams_reset), bcParams);
  FSM lutRamTester <- mkFSM(lutRamStmts);


  Reg#(Int#(32)) test_iter <- mkReg(0);
  Stmt test = seq

    // for (test_iter<=0; test_iter<12; test_iter<=test_iter+1) seq
    //   $display("starting test ", test_iter, " ######################################## ");
    //   testers[test_iter].start();
    //   await(testers[test_iter].done());
    //   delay(5);
    // endseq

    $display("starting test lutRamTester ######################################## ");
    lutRamTester.start();
    await(lutRamTester.done());
    $display("done test lutRamTester ######################################## ");
  endseq;

  mkAutoFSM( test );
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
