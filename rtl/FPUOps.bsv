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
  Bit#(1)  nan;
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
      nan: 0,
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
        nan: pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0),
        inexact: pack(exc.inexact)
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
    op.put(in.addOrSub, in.arg1[31:0], in.arg2[31:0]);
  endmethod

  method FPUOpOutput out =
    FPUOpOutput {
      val: op.res,
      nan: op.nan,
      overflow: op.overflow,
      underflow: op.underflow,
      divByZero: 0,
      inexact: 0
    };
endmodule
     
`endif

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
        nan: pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0),
        inexact: pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

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
      nan: op.nan,
      overflow: op.overflow,
      underflow: op.underflow,
      divByZero: 0,
      inexact: 0
    };
endmodule

`endif

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
        nan: pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0 || isZero(in2)),
        inexact: pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

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
      nan: op.nan,
      overflow: op.overflow,
      underflow: op.underflow,
      divByZero: op.divByZero,
      inexact: 0
    };
endmodule

`endif

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
        nan: 0,
        overflow: 0,
        underflow: 0,
        divByZero: 0,
        inexact: 0
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

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
      val: zeroExtend(cmpEQ[0] == 1 ? op.eq : (cmpLT == 1 ? op.lt : op.lte)),
      nan: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0,
      inexact: 0
    };
endmodule

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
    UInt#(32) intIn = unpack(in.arg1[31:0]);
    Tuple2#(Float, Exception) pair =
      vFixedToFloat(intIn, 5'd0, Rnd_Nearest_Even);
    match {.out, .exc} = pair;
    pipeline[`FPConvertLatency-1] <=
      FPUOpOutput {
        val: pack(out),
        nan: pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0),
        inexact: pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

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
      nan: 0,
      overflow: 0,
      underflow: 0,
      divByZero: 0
    };
endmodule

`endif

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
    Float val = unpack(in.arg1[31:0]);
    Tuple2#(UInt#(32), Exception) pair =
      vFloatToFixed(5'd0, val, Rnd_Nearest_Even);
    match {.outUInt, .exc} = pair;
    Bit#(32) out = pack(outUInt);
    pipeline[`FPConvertLatency-1] <=
      FPUOpOutput {
        val: out,
        nan: pack(exc.invalid_op),
        overflow: pack(exc.overflow),
        underflow: pack(exc.underflow),
        divByZero: pack(exc.divide_0),
        inexact: pack(exc.inexact)
      };
  endmethod

  method FPUOpOutput out = pipeline[0];
endmodule

`else

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
      nan: op.nan,
      overflow: op.overflow,
      underflow: op.underflow,
      divByZero: 0
    };
endmodule

`endif

endpackage
