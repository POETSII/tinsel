// SPDX-License-Identifier: BSD-2-Clause
package FPUOps;

// This package provides the following FPU operations:
//
//   * floating-point add, subtract, multiplication, and division
//   * floating-point comparison
//   * floating-point to integer conversion
//   * integer to floating-point conversion
//   * integer multiplication
//
// Although not a floating point operation, integer multiplication is
// implemented by the FPU (it has a non-zero latency and consumes
// valuable resources that may be worth sharing between cores).

import Mult          :: *;
import Vector        :: *;
import ConfigReg     :: *;
import FloatingPoint :: *;
import Clocks        :: *; // invert reset


// =============================================================================
// Interface
// =============================================================================

// The inputs to an FPU operation
typedef struct {
  // Up to two arguments
  Bit#(33) arg1;
  Bit#(33) arg2;
  // Control signal for the FP adder/subtractor
  // (Is it an add or subtract operation?)
  Bit#(1) addOrSub;
  // Control signal for the integer multiplier
  // (Produce lower or upper 32 bits of 64-bit result?)
  Bit#(1) lowerOrUpper;
  // Control signals for the comparitor
  Bit#(1) cmpEQ; // Equality comparison
  Bit#(1) cmpLT; // Less-than comparison
} FPUOpInput deriving (Bits, FShow);

// The result of an FPU operation
typedef struct {
  Bit#(32) val;
  Bit#(1)  invalid;
  Bit#(1)  overflow;
  Bit#(1)  underflow;
  Bit#(1)  divByZero;
  Bit#(1)  inexact;
} FPUOpOutput deriving (Bits);

// An FPU operation
(* always_ready, always_enabled *)
interface FPUOp;
  method Action put(FPUOpInput in);
  method FPUOpOutput out;
endinterface

`ifdef Stratix10
(* always_ready, always_enabled *)
interface AlteraS10FPFuncIfc;
  method Action put(Bit#(32) a, Bit#(32) b);
  method Bit#(32) res;
endinterface
`endif

// =============================================================================
// Integer multiplier
// =============================================================================

// Pipelined 33-bit signed mutlitplier
// With ability to select upper or lower 32 bits of result
// Output valid after three cycles
module mkIntMult (FPUOp);
  // Combinatorial multiplier
  Mult#(33) mult <- mkSignedMult;

  // Result after 1, 2, and 3 cycles respectively
  Reg#(Bit#(66)) reg1 <- mkConfigRegU;
  Reg#(Bit#(66)) reg2 <- mkConfigRegU;
  Reg#(Bit#(32)) reg3 <- mkConfigRegU;

  // Selector after 1 and 2 cycles respectively
  Reg#(Bit#(1)) sel1 <- mkConfigRegU;
  Reg#(Bit#(1)) sel2 <- mkConfigRegU;

  rule update;
    reg2 <= reg1;
    sel2 <= sel1;
    reg3 <= sel2 == 0 ? reg2[31:0] : reg2[63:32];
  endrule

  method Action put(FPUOpInput in);
    reg1 <= mult.mult(in.arg1, in.arg2);
    sel1 <= in.lowerOrUpper;
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: reg3,
      invalid: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule

// =============================================================================
// Floating point add/subtract
// =============================================================================

`ifdef SIMULATE

module mkFPAddSub (FPUOp);
  // Simulate pipeline
  Vector#(`FPAddSubLatency, Reg#(FPUOpOutput)) pipeline <-
    replicateM(mkConfigRegU);

  rule shift;
    for (Integer i = 0; i < `FPAddSubLatency-1; i=i+1)
      pipeline[i] <= pipeline[i+1];
  endrule

  method Action put(FPUOpInput in);
    Float in1 = unpack(in.arg1[31:0]);
    Float in2 = unpack(in.arg2[31:0]);
    match {.out, .exc} =
      addFP(in1, in.addOrSub == 0 ? in2 : negate(in2), Rnd_Nearest_Even);
    pipeline[`FPAddSubLatency-1] <=
      FPUOpOutput {
        val: pack(out),
        invalid: 0, //pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0),
        inexact: 0 //pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

(* always_ready, always_enabled *)
interface AlteraFPAddSubIfc;
  method Action put(Bit#(1) addOrSub, Bit#(32) x, Bit#(32) y);
  method Bit#(32) res;
  method Bit#(1) nan;
  method Bit#(1) overflow;
  method Bit#(1) underflow;
endinterface

`ifdef StratixV

import "BVI" AlteraFPAddSub =
  module mkAlteraFPAddSub (AlteraFPAddSubIfc);
    default_clock clk(clock, (*unused*) clk_gate);
    default_reset no_reset;

    method put(add_sub, dataa, datab) enable ((*inhigh*) EN) clocked_by(clk);
    method result res;
    method nan nan;
    method overflow overflow;
    method underflow underflow;

    schedule (put) C (put);
    schedule (put) CF (res, nan, overflow, underflow);
    schedule (res, nan, overflow, underflow) CF
             (res, nan, overflow, underflow);
  endmodule

module mkFPAddSub (FPUOp);
  AlteraFPAddSubIfc op <- mkAlteraFPAddSub;

  method Action put(FPUOpInput in);
    op.put(~in.addOrSub, in.arg1[31:0], in.arg2[31:0]);
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: op.overflow,
      underflow: op.underflow,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif // StratixV

`ifdef Stratix10
(* always_ready, always_enabled *)
interface AlteraS10FPAddSubIfc;
  method Action put(Bit#(1) s, Bit#(32) a, Bit#(32) b);
  method Bit#(32) res;
  // method Action areset(Bit#(1) rst);
endinterface


// de10 version. 14 cycles latency target
import "BVI" fpS10AddSub =
  module mkAlteraS10FPAddSub (AlteraS10FPAddSubIfc);
    Reset rstP <- invertCurrentReset();

    default_clock clk(clk, (*unused*) clk_gate);
    default_reset no_reset;

    input_reset a_rst (areset) = rstP;

    method put(opSel, a, b) enable ((*inhigh*) EN) clocked_by(clk);
    // method areset(rst) enable ((*inhigh*) EN_1);
    method q res reset_by(default_reset);

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF
             (res);
    // schedule (areset) CF (put, res, areset);

  endmodule

module mkFPAddSub (FPUOp);
  AlteraS10FPAddSubIfc op <- mkAlteraS10FPAddSub;

  Reg#(FPUOpInput) in_w_or_nothing <- mkReg( FPUOpInput{arg1:0, arg2:0, addOrSub:0, lowerOrUpper:0, cmpEQ:0, cmpLT:0 } );

  (* no_implicit_conditions *)
  rule send;
    op.put(in_w_or_nothing.addOrSub, in_w_or_nothing.arg1[31:0], in_w_or_nothing.arg2[31:0]);
  endrule

  method Action put(FPUOpInput in);
    in_w_or_nothing <= in;
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule

`endif // Stratix10

`endif // SIMULATE

// =============================================================================
// Floating point multiply
// =============================================================================

`ifdef SIMULATE

module mkFPMult (FPUOp);
  // Simulate pipeline
  Vector#(`FPMultLatency, Reg#(FPUOpOutput)) pipeline <-
    replicateM(mkConfigRegU);

  rule shift;
    for (Integer i = 0; i < `FPMultLatency-1; i=i+1)
      pipeline[i] <= pipeline[i+1];
  endrule

  method Action put(FPUOpInput in);
    Float in1 = unpack(in.arg1[31:0]);
    Float in2 = unpack(in.arg2[31:0]);
    match {.out, .exc} = multFP(in1, in2, Rnd_Nearest_Even);
    pipeline[`FPMultLatency-1] <=
      FPUOpOutput {
        val: pack(out),
        invalid: 0, // pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0),
        inexact: 0 //  pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

`ifdef StratixV

(* always_ready, always_enabled *)
interface AlteraFPMultIfc;
  method Action put(Bit#(32) x, Bit#(32) y);
  method Bit#(32) res;
  method Bit#(1) nan;
  method Bit#(1) overflow;
  method Bit#(1) underflow;
endinterface

import "BVI" AlteraFPMult =
  module mkAlteraFPMult (AlteraFPMultIfc);
    default_clock clk(clock, (*unused*) clk_gate);
    default_reset no_reset;

    method put(dataa, datab) enable ((*inhigh*) EN) clocked_by(clk);
    method result res;
    method nan nan;
    method overflow overflow;
    method underflow underflow;

    schedule (put) C (put);
    schedule (put) CF (res, nan, overflow, underflow);
    schedule (res, nan, overflow, underflow) CF
             (res, nan, overflow, underflow);
  endmodule

module mkFPMult (FPUOp);
  AlteraFPMultIfc op <- mkAlteraFPMult;

  method Action put(FPUOpInput in);
    op.put(in.arg1[31:0], in.arg2[31:0]);
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: op.overflow,
      underflow: op.underflow,
      divByZero: 0,
      inexact: 0
    };
endmodule

`endif // StratixV

`ifdef Stratix10
(* always_ready, always_enabled *)
interface AlteraS10FPMultIfc;
  method Action put(Bit#(32) a, Bit#(32) b);
  method Bit#(32) res;
endinterface

import "BVI" fpS10Mult =
  module mkAlteraS10FPMult (AlteraS10FPFuncIfc);
    Reset rstP <- invertCurrentReset();

    default_clock clk(clk, (*unused*) clk_gate);
    default_reset no_reset;
    input_reset a_rst (areset) = rstP;

    method put(a, b) enable ((*inhigh*) EN) clocked_by(clk);
    method q res;

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF
             (res);
  endmodule

module mkFPMult (FPUOp);
  AlteraS10FPFuncIfc op <- mkAlteraS10FPMult;
  Reg#(FPUOpInput) in_w_or_nothing <- mkReg( FPUOpInput{arg1:0, arg2:0, addOrSub:0, lowerOrUpper:0, cmpEQ:0, cmpLT:0 } );

  (* no_implicit_conditions *)
  rule send;
    op.put(in_w_or_nothing.arg1[31:0], in_w_or_nothing.arg2[31:0]);
  endrule

  method Action put(FPUOpInput in);
    in_w_or_nothing <= in;
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif // Stratix10

`endif // SIMULATE

// =============================================================================
// Floating point divide
// =============================================================================

`ifdef SIMULATE

module mkFPDiv (FPUOp);
  // Simulate pipeline
  Vector#(`FPDivLatency, Reg#(FPUOpOutput)) pipeline <-
    replicateM(mkConfigRegU);

  rule shift;
    for (Integer i = 0; i < `FPDivLatency-1; i=i+1)
      pipeline[i] <= pipeline[i+1];
  endrule

  method Action put(FPUOpInput in);
    Float in1 = unpack(in.arg1[31:0]);
    Float in2 = unpack(in.arg2[31:0]);
    // Need to avoid passing zero to second arg of divFP,
    // otherwise simulator dies with floating point exception
    Float in2safe = isZero(in2) ? qnan() : in2;
    match {.out, .exc} = divFP(in1, in2safe, Rnd_Nearest_Even);
    pipeline[`FPDivLatency-1] <=
      FPUOpOutput {
        val: pack(out),
        invalid: 0, //pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0 || isZero(in2)),
        inexact: 0 // pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else // not SIMULATE

`ifdef StratixV
(* always_ready, always_enabled *)
interface AlteraFPDivIfc;
  method Action put(Bit#(32) x, Bit#(32) y);
  method Bit#(32) res;
  method Bit#(1) nan;
  method Bit#(1) overflow;
  method Bit#(1) underflow;
  method Bit#(1) divByZero;
endinterface

import "BVI" AlteraFPDiv =
  module mkAlteraFPDiv (AlteraFPDivIfc);
    default_clock clk(clock, (*unused*) clk_gate);
    default_reset no_reset;

    method put(dataa, datab) enable ((*inhigh*) EN) clocked_by(clk);
    method result res;
    method nan nan;
    method overflow overflow;
    method underflow underflow;
    method division_by_zero divByZero;

    schedule (put) C (put);
    schedule (put) CF (res, nan, overflow, underflow, divByZero);
    schedule (res, nan, overflow, underflow, divByZero) CF
             (res, nan, overflow, underflow, divByZero);
  endmodule

module mkFPDiv (FPUOp);
  AlteraFPDivIfc op <- mkAlteraFPDiv;

  method Action put(FPUOpInput in);
    op.put(in.arg1[31:0], in.arg2[31:0]);
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: op.overflow,
      underflow: op.underflow,
      divByZero: 0,
      inexact: 0
    };
endmodule

`endif // StratixV

`ifdef Stratix10
(* always_ready, always_enabled *)
interface AlteraS10FPDivIfc;
  method Action put(Bit#(32) a, Bit#(32) b);
  method Bit#(32) res;
endinterface

import "BVI" fpS10Div =
  module mkAlteraS10FPDiv (AlteraS10FPFuncIfc);
    Reset rstP <- invertCurrentReset();

    default_clock clk(clk, (*unused*) clk_gate);
    default_reset no_reset;

    input_reset a_rst (areset) = rstP;

    method put(a, b) enable ((*inhigh*) EN) clocked_by(clk);
    method q res;

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF
             (res);
  endmodule


module mkFPDiv (FPUOp);
  AlteraS10FPFuncIfc op <- mkAlteraS10FPDiv;

  Reg#(FPUOpInput) in_w_or_nothing <- mkReg( FPUOpInput{arg1:0, arg2:0, addOrSub:0, lowerOrUpper:0, cmpEQ:0, cmpLT:0 } );

  (* no_implicit_conditions *)
  rule send;
    op.put(in_w_or_nothing.arg1[31:0], in_w_or_nothing.arg2[31:0]);
  endrule

  method Action put(FPUOpInput in);
    in_w_or_nothing <= in;
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif // Stratix10

`endif // not SIMULATE

// =============================================================================
// Floating point comparison
// =============================================================================

`ifdef SIMULATE

module mkFPCompare (FPUOp);
  // Simulate pipeline
  Vector#(`FPCompareLatency, Reg#(FPUOpOutput)) pipeline <-
    replicateM(mkConfigRegU);

  rule shift;
    for (Integer i = 0; i < `FPCompareLatency-1; i=i+1)
      pipeline[i] <= pipeline[i+1];
  endrule

  method Action put(FPUOpInput in);
    Float in1 = unpack(in.arg1[31:0]);
    Float in2 = unpack(in.arg2[31:0]);
    let order = compareFP(in1, in2);
    pipeline[`FPCompareLatency-1] <=
      FPUOpOutput {
        val: zeroExtend(
               pack(in.cmpEQ == 1 ? order == EQ :
                      (in.cmpLT == 1 ? order == LT :
                         (order == LT || order == EQ)))),
        invalid: 0,
        overflow: 0,
        underflow: 0,
        divByZero: 0,
        inexact: 0
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

`ifdef StratixV
(* always_ready, always_enabled *)
interface AlteraFPCompareIfc;
  method Action put(Bit#(32) x, Bit#(32) y);
  method Bit#(1) eq;
  method Bit#(1) lt;
  method Bit#(1) lte;
endinterface

import "BVI" AlteraFPCompare =
  module mkAlteraFPCompare (AlteraFPCompareIfc);
    default_clock clk(clock, (*unused*) clk_gate);
    default_reset no_reset;

    method put(dataa, datab) enable ((*inhigh*) EN) clocked_by(clk);
    method aeb eq;
    method alb lt;
    method aleb lte;

    schedule (put) C (put);
    schedule (put) CF (eq, lt, lte);
    schedule (eq, lt, lte) CF (eq, lt, lte);
  endmodule


module mkFPCompare (FPUOp);
  AlteraFPCompareIfc op <- mkAlteraFPCompare;

  Vector#(`FPCompareLatency, Reg#(Bit#(1))) cmpEQ <- replicateM(mkConfigRegU);
  Vector#(`FPCompareLatency, Reg#(Bit#(1))) cmpLT <- replicateM(mkConfigRegU);

  rule shift;
    for (Integer i = 0; i < `FPCompareLatency-1; i=i+1) begin
      cmpEQ[i] <= cmpEQ[i+1];
      cmpLT[i] <= cmpLT[i+1];
    end
  endrule


  method Action put(FPUOpInput in);
    op.put(in.arg1[31:0], in.arg2[31:0]);
    cmpEQ[`FPCompareLatency-1] <= in.cmpEQ;
    cmpLT[`FPCompareLatency-1] <= in.cmpLT;
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: zeroExtend(cmpEQ[0] == 1 ? op.eq :
             (cmpLT[0] == 1 ? op.lt : op.lte)),
      invalid: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif // StratixV

`ifdef Stratix10
import "BVI" fpS10LT =
  module mkAlteraS10FPLT (AlteraS10FPFuncIfc);
    Reset rstP <- invertCurrentReset();
    default_clock clk(clk, (*unused*) clk_gate);
    default_reset no_reset;
    input_reset a_rst (areset) = rstP;

    method put(a, b) enable ((*inhigh*) EN) clocked_by(clk);
    method q res;

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF
             (res);
  endmodule

import "BVI" fpS10EQ =
  module mkAlteraS10FPEQ (AlteraS10FPFuncIfc);
    Reset rstP <- invertCurrentReset();

    default_clock clk(clk, (*unused*) clk_gate);
    default_reset no_reset;

    input_reset a_rst (areset) = rstP;

    method put(a, b) enable ((*inhigh*) EN) clocked_by(clk);
    method q res;

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF
             (res);
  endmodule

import "BVI" fpS10LTE =
  module mkAlteraS10FPLTE (AlteraS10FPFuncIfc);
    Reset rstP <- invertCurrentReset();

    default_clock clk(clk, (*unused*) clk_gate);
    default_reset no_reset;

    input_reset a_rst (areset) = rstP;

    method put(a, b) enable ((*inhigh*) EN) clocked_by(clk);
    method q res;

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF
             (res);
  endmodule

module mkFPCompare (FPUOp);
  AlteraS10FPFuncIfc op_lt <- mkAlteraS10FPLT;
  AlteraS10FPFuncIfc op_eq <- mkAlteraS10FPEQ;
  AlteraS10FPFuncIfc op_lte <- mkAlteraS10FPLTE;

  Vector#(`FPCompareLatency, Reg#(Bit#(1))) cmpEQ <- replicateM(mkConfigRegU);
  Vector#(`FPCompareLatency, Reg#(Bit#(1))) cmpLT <- replicateM(mkConfigRegU);

  Reg#(FPUOpInput) in_w_or_nothing <- mkReg( FPUOpInput{arg1:0, arg2:0, addOrSub:0, lowerOrUpper:0, cmpEQ:0, cmpLT:0 } );


  rule shift;
    for (Integer i = 0; i < `FPCompareLatency-1; i=i+1) begin
      cmpEQ[i] <= cmpEQ[i+1];
      cmpLT[i] <= cmpLT[i+1];
    end
  endrule

  (* no_implicit_conditions *)
  rule send;
    op_lt.put(in_w_or_nothing.arg1[31:0], in_w_or_nothing.arg2[31:0]);
    op_eq.put(in_w_or_nothing.arg1[31:0], in_w_or_nothing.arg2[31:0]);
    op_lte.put(in_w_or_nothing.arg1[31:0], in_w_or_nothing.arg2[31:0]);
    cmpEQ[`FPCompareLatency-1] <= in_w_or_nothing.cmpEQ;
    cmpLT[`FPCompareLatency-1] <= in_w_or_nothing.cmpLT;
  endrule



  method Action put(FPUOpInput in);
    in_w_or_nothing <= in;
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: zeroExtend(cmpEQ[0] == 1 ? op_eq.res :
             (cmpLT[0] == 1 ? op_lt.res : op_lte.res)),
      invalid: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif // Stratix10

`endif

// =============================================================================
// Integer to floating point conversion
// =============================================================================

`ifdef SIMULATE

module mkFPFromInt (FPUOp);
  // Simulate pipeline
  Vector#(`FPConvertLatency, Reg#(FPUOpOutput)) pipeline <-
    replicateM(mkConfigRegU);

  rule shift;
    for (Integer i = 0; i < `FPConvertLatency-1; i=i+1)
      pipeline[i] <= pipeline[i+1];
  endrule

  method Action put(FPUOpInput in);
    Int#(32) intIn = unpack(in.arg1[31:0]);
    UInt#(32) uintIn = unpack(in.arg1[31:0]);
    Tuple2#(Float, Exception) pair =
      vFixedToFloat(intIn < 0 ? -uintIn : uintIn, 5'd0, Rnd_Nearest_Even);
    match {.out, .exc} = pair;
    pipeline[`FPConvertLatency-1] <=
      FPUOpOutput {
        val: pack(intIn < 0 ? negate(out) : out),
        invalid: 0, // pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0),
        inexact: 0 // pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

`ifdef StratixV
(* always_ready, always_enabled *)
interface AlteraFPFromIntIfc;
  method Action put(Bit#(32) x);
  method Bit#(32) res;
endinterface

import "BVI" AlteraFPFromInt =
  module mkAlteraFPFromInt (AlteraFPFromIntIfc);
    default_clock clk(clock, (*unused*) clk_gate);
    default_reset no_reset;

    method put(dataa) enable ((*inhigh*) EN) clocked_by(clk);
    method result res;

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF (res);
  endmodule

module mkFPFromInt (FPUOp);
  AlteraFPFromIntIfc op <- mkAlteraFPFromInt;

  method Action put(FPUOpInput in);
    op.put(in.arg1[31:0]);
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif //  StratixV

`ifdef Stratix10
(* always_ready, always_enabled *)
interface AlteraS10FPIntConvertIfc;
  method Action put(Bit#(32) x);
  method Bit#(32) res;
endinterface

import "BVI" fpS10FromInt =
  module mkAlteraS10FPFromInt (AlteraS10FPIntConvertIfc);
    Reset rstP <- invertCurrentReset();

    default_clock clk(clk, (*unused*) clk_gate);
    default_reset no_reset;

    input_reset a_rst (areset) = rstP;

    method put(a) enable ((*inhigh*) EN) clocked_by(clk);
    method q res;

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF (res);
  endmodule

module mkFPFromInt (FPUOp);
  AlteraS10FPIntConvertIfc op <- mkAlteraS10FPFromInt;
  Reg#(FPUOpInput) in_w_or_nothing <- mkReg( FPUOpInput{arg1:0, arg2:0, addOrSub:0, lowerOrUpper:0, cmpEQ:0, cmpLT:0 } );


  (* no_implicit_conditions *)
  rule send;
    op.put(in_w_or_nothing.arg1[31:0]);
  endrule

  method Action put(FPUOpInput in);
    in_w_or_nothing <= in;
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif // Stratix10

`endif // SIMULATE

// =============================================================================
// Floating point to integer conversion
// =============================================================================

`ifdef SIMULATE

module mkFPToInt (FPUOp);
  // Simulate pipeline
  Vector#(`FPConvertLatency, Reg#(FPUOpOutput)) pipeline <-
    replicateM(mkConfigRegU);

  rule shift;
    for (Integer i = 0; i < `FPConvertLatency-1; i=i+1)
      pipeline[i] <= pipeline[i+1];
  endrule

  method Action put(FPUOpInput in);
    Float f = unpack(in.arg1[31:0]);
    Float val = unpack({1'b0, in.arg1[30:0]});
    Tuple2#(UInt#(32), Exception) pair =
      vFloatToFixed(5'd0, val, Rnd_Nearest_Even);
    match {.outUInt, .exc} = pair;
    Bit#(32) out = pack(outUInt);
    pipeline[`FPConvertLatency-1] <=
      FPUOpOutput {
        val: out >= 32'h80000000 || isNaN(f) || isInfinity(f) ?
          (in.arg1[31] == 0 || isNaN(f) ? 32'h7fffffff : 32'h80000000) :
          (in.arg1[31] == 0 ? out : -out),
        invalid: 0, // pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0),
        inexact: 0 // pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

`ifdef StratixV
(* always_ready, always_enabled *)
interface AlteraFPToIntIfc;
  method Action put(Bit#(32) x);
  method Bit#(32) res;
  method Bit#(1) nan;
  method Bit#(1) overflow;
  method Bit#(1) underflow;
endinterface

import "BVI" AlteraFPToInt =
  module mkAlteraFPToInt (AlteraFPToIntIfc);
    default_clock clk(clock, (*unused*) clk_gate);
    default_reset no_reset;

    method put(dataa) enable ((*inhigh*) EN) clocked_by(clk);
    method result res;
    method nan nan;
    method overflow overflow;
    method underflow underflow;

    schedule (put) C (put);
    schedule (put) CF (res, nan, overflow, underflow);
    schedule (res, nan, overflow, underflow) CF
             (res, nan, overflow, underflow);
  endmodule

module mkFPToInt (FPUOp);
  AlteraFPToIntIfc op <- mkAlteraFPToInt;

  method Action put(FPUOpInput in);
    op.put(in.arg1[31:0]);
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: op.overflow,
      underflow: op.underflow,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif // StratixV

`ifdef Stratix10
import "BVI" fpS10ToInt =
  module mkAlteraS10FPToInt (AlteraS10FPIntConvertIfc);
    Reset rstP <- invertCurrentReset();

    default_clock clk(clk, (*unused*) clk_gate);
    default_reset no_reset;

    input_reset a_rst (areset) = rstP;

    method put(a) enable ((*inhigh*) EN) clocked_by(clk);
    method q res;

    schedule (put) C (put);
    schedule (put) CF (res);
    schedule (res) CF (res);
  endmodule

module mkFPToInt (FPUOp);
  AlteraS10FPIntConvertIfc op <- mkAlteraS10FPToInt;
  Reg#(FPUOpInput) in_w_or_nothing <- mkReg( FPUOpInput{arg1:0, arg2:0, addOrSub:0, lowerOrUpper:0, cmpEQ:0, cmpLT:0 } );


  (* no_implicit_conditions *)
  rule send;
    op.put(in_w_or_nothing.arg1[31:0]);
  endrule

  method Action put(FPUOpInput in);
    in_w_or_nothing <= in;
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      invalid: 0,
      overflow: 0, //op.overflow,
      underflow: 0, //op.underflow,
      divByZero: 0,
      inexact: 0
    };
endmodule
`endif // Stratix10

`endif

endpackage
