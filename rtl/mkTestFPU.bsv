// SPDX-License-Identifier: BSD-2-Clause
import FPU::*; // for types
import FPUOps::*; // for types

// import BlueCheck :: *;
import StmtFSM::*;
import Assert::*;
import Interface::*;
import Clocks::*;

module mkSoftFPU (FPU);

  // Request & response ports
  InPort#(FPUReq) reqPort <- mkInPort;
  OutPort#(FPUResp) respPort <- mkOutPort;

  // result
  // req.in.val = 1;
  // req.in.invalid = 0;
  // req.in.overflow = 0;
  // req.in.underflow = 0;
  // req.in.divByZero = 0;
  // req.in.inexact = 0;
  //
  interface In reqIn = reqPort.in;
  interface BOut respOut;
    method Action get;
    endmethod
    method Bool valid = False;
    method FPUResp value = ?;
  endinterface


endmodule


//////////////////////////////////////////
// Tests!
//////////////////////////////////////////
// module [BlueCheck] checkFPU();
//   /* Specification instance */
//   BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) spec <- mkBlockRamTrueMixedOpts_SIMULATE(testBlockRamOpts);
//
//   /* Implmentation instance */
//   BlockRamTrueMixed#(AddrA, DataA, AddrB, DataB) imp <- mkFPU();
//
//   equiv("input"    , spec.reqPort    , imp.reqPort);
//   equiv("output"   , spec.respPort   , imp.respPort);
// endmodule
//
// module [Module] mkBRAMTest ();
//     blueCheck(checkBRAM);
//
// endmodule

(* always_ready, always_enabled *)
interface Altera14nmFPMacPrimIfc;
  method Action put(Bit#(3) ena, Bit#(32) ax, Bit#(32) ay, Bit#(32) az, Bit#(32) chainin);
  // method Action clr(Bit#(2) clr);
  // method Action clk(Bit#(3) clk);
  method Bit#(32) resulta;
  method Bit#(32) chainout;
  method Bit#(1) mult_invalid;
  method Bit#(1) mult_inexact;
  method Bit#(1) mult_overflow;
  method Bit#(1) mult_underflow;
  method Bit#(1) adder_invalid;
  method Bit#(1) adder_inexact;
  method Bit#(1) adder_overflow;
  method Bit#(1) adder_underflow;
endinterface

// generic wrapper for the 14nm DSP
import "BVI" fourteennm_fp_mac_wrapper =
  module mkAltera14nmFPMac_prim (Altera14nmFPMacPrimIfc);
    Reset rstP <- invertCurrentReset();
    // Clock clk <- exposeCurrentClock;

    parameter operation_mode = "sp_mult";
    parameter ax_clock = "0";
    parameter ay_clock = "0";
    parameter adder_input_clock = "0";
    parameter output_clock = "0";
    parameter clear_type = "sclr";



    default_clock clk_0(clk_0) <- exposeCurrentClock; // vector port of clocks; connected in the wrapper.
    input_clock clk_1(clk_1) <- exposeCurrentClock; // vector port of clocks; connected in the wrapper.
    input_clock clk_2(clk_2) <- exposeCurrentClock;

    // input_clock clk1(clk1, (*unused*) clk_gate) = clk;
    // input_clock clk2(clk2, (*unused*) clk_gate) = clk;

    default_reset no_reset;
    input_reset clr_0 (clr_0) = rstP;
    input_reset clr_1 (clr_1) = rstP;
    // input_reset clr_1 (clr[0]) = rstP;

    method put(ena, ax, ay, az, chainin) enable ((*inhigh*) EN_0);
    // method clr(clr) enable ((*inhigh*) EN_1) clocked_by(clk);
    // method clk(clk) enable ((*inhigh*) EN_2) clocked_by(clk);

    method resulta resulta;
    method chainout chainout;
    method mult_invalid mult_invalid;
    method mult_inexact mult_inexact;
    method mult_overflow mult_overflow;
    method mult_underflow mult_underflow;
    method adder_invalid adder_invalid;
    method adder_inexact adder_inexact;
    method adder_overflow adder_overflow;
    method adder_underflow adder_underflow;

    schedule (put) C (put);
    schedule (put) CF (resulta, chainout, mult_invalid, mult_inexact, mult_overflow, mult_underflow, adder_invalid, adder_inexact, adder_overflow, adder_underflow);
    schedule (resulta, chainout, mult_invalid, mult_inexact, mult_overflow, mult_underflow, adder_invalid, adder_inexact, adder_overflow, adder_underflow) CF
             (resulta, chainout, mult_invalid, mult_inexact, mult_overflow, mult_underflow, adder_invalid, adder_inexact, adder_overflow, adder_underflow);
  endmodule


(* always_ready, always_enabled *)
interface Altera14nmFPMacIfc;
  method Action put(Bit#(3) ena, Bit#(32) ax, Bit#(32) ay, Bit#(32) az, Bit#(32) chainin);
  method Bit#(32) resulta;
  method Bit#(32) chainout;
  method Bit#(1) mult_invalid;
  method Bit#(1) mult_inexact;
  method Bit#(1) mult_overflow;
  method Bit#(1) mult_underflow;
  method Bit#(1) adder_invalid;
  method Bit#(1) adder_inexact;
  method Bit#(1) adder_overflow;
  method Bit#(1) adder_underflow;
endinterface



module mkAltera14nmFPMac(Altera14nmFPMacIfc);
  Altera14nmFPMacPrimIfc dsp <- mkAltera14nmFPMac_prim();

  Clock clk <- exposeCurrentClock;
  Reset rstP <- invertCurrentReset();

  // (* no_implicit_conditions *)
  // rule drive_clk_rst;
  //   // dsp.clk( {clk.getClockValue, clk.getClockValue} );
  //   // dsp.clr( {rstP, rstP, rstP} );
  // endrule

  Wire#(Bit#(3)) ena <- mkDWire(3'b000);
  Wire#(Bit#(32)) ax <- mkDWire(32'h00000000); // 3
  Wire#(Bit#(32)) ay <- mkDWire(32'h00000000); // 2
  Wire#(Bit#(32)) az <- mkDWire(32'h00000000); // 0
  Wire#(Bit#(32)) chainin <- mkDWire(32'h00000000); // 0

  (* no_implicit_conditions *)
  rule putvals;
    dsp.put(ena, ax, ay, az, chainin);
  endrule

  method Action put(Bit#(3) ena_in, Bit#(32) ax_in, Bit#(32) ay_in, Bit#(32) az_in, Bit#(32) chainin_in);
    ena <= ena_in;
    ax <= ax_in;
    ay <= ay_in;
    az <= az_in;
    chainin <= chainin_in;
  endmethod

  method Bit#(32) resulta = dsp.resulta;
  method Bit#(32) chainout = dsp.chainout;
  method Bit#(1) mult_invalid = dsp.mult_invalid;
  method Bit#(1) mult_inexact = dsp.mult_inexact;
  method Bit#(1) mult_overflow = dsp.mult_overflow;
  method Bit#(1) mult_underflow = dsp.mult_underflow;
  method Bit#(1) adder_invalid = dsp.adder_invalid;
  method Bit#(1) adder_inexact = dsp.adder_inexact;
  method Bit#(1) adder_overflow = dsp.adder_overflow;
  method Bit#(1) adder_underflow = dsp.adder_underflow;

endmodule
// module mkTestFPUOp();
//
//   FPUOp fpAddSub  <- mkFPMult;
//
//   Stmt test =
//   seq
//
//     action
//       $dumpvars;
//     endaction
//
//     action
//       FPUOpInput req;
//
//       req.arg1 = 33'h00004040; // 3
//       // req.arg1 = 33'h0000803f; // 1
//       req.arg2 = 33'h00000040; // 2
//       req.addOrSub = 1;
//       req.lowerOrUpper = 0;
//       req.cmpEQ = 0;
//       req.cmpLT = 0;
//
//       fpAddSub.put(req);
//     endaction
//
//     action
//       FPUOpInput req;
//
//       req.arg1 = 33'h00004040; // 3
//       // req.arg1 = 33'h0000803f; // 1
//       req.arg2 = 33'h00000040; // 2
//       req.addOrSub = 1;
//       req.lowerOrUpper = 0;
//       req.cmpEQ = 0;
//       req.cmpLT = 0;
//
//       fpAddSub.put(req);
//     endaction
//
//     action
//       FPUOpInput req;
//
//       req.arg1 = 33'h00004040; // 3
//       // req.arg1 = 33'h0000803f; // 1
//       req.arg2 = 33'h00000040; // 2
//       req.addOrSub = 1;
//       req.lowerOrUpper = 0;
//       req.cmpEQ = 0;
//       req.cmpLT = 0;
//
//       fpAddSub.put(req);
//     endaction
//
//     action
//       FPUOpInput req;
//
//       req.arg1 = 33'h00004040; // 3
//       // req.arg1 = 33'h0000803f; // 1
//       req.arg2 = 33'h00000040; // 2
//       req.addOrSub = 1;
//       req.lowerOrUpper = 0;
//       req.cmpEQ = 0;
//       req.cmpLT = 0;
//
//       fpAddSub.put(req);
//     endaction
//
//
//     delay(50);
//
//   endseq;
//
//   mkAutoFSM(test);
//
// endmodule

module mkTestFPU();

  Altera14nmFPMacIfc op  <- mkAltera14nmFPMac;


  Stmt test =
  seq

    action
      $dumpvars;
    endaction

    action
      op.put(3'b001, 32'h00004040, 32'h00000040, 32'h00000000, 32'h00000000);
    endaction

    delay(10);

  endseq;

  mkAutoFSM(test);

endmodule
