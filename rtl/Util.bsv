package Util;

import DReg :: *;
import Vector :: *;

// Useful function for constructing a mux with a one-hot select
function t when(Bool b, t x)
    provisos (Bits#(t, tWidth), Add#(_, 1, tWidth));
  return unpack(signExtend(pack(b)) & pack(x));
endfunction

// Mux with a one-hot selector
function t oneHotSelect(Vector#(n, Bool) oneHot, Vector#(n, t) vec)
    provisos (Bits#(t, tWidth),
              Add#(_a, 1, tWidth),
              Add#(_b, 1, n));
  return unpack(fold( \| , zipWith(when, oneHot, map(pack, vec))));
endfunction

// Binary encoder: convert from one-hot to binary
function Bit#(n) encode(Vector#(TExp#(n), Bool) oneHot)
  provisos (Add#(_a, 1, n),
            Add#(_b, 1, TExp#(n)));
  return oneHotSelect(oneHot, map(fromInteger, genVector));
endfunction

// Are all bits high?
function Bool allHigh(Bit#(n) x) = unpack(reduceAnd(x));

// Are all bits low?
function Bool allLow(Bit#(n) x) = !unpack(reduceOr(x));

// Are all bools high?
function Bool andVec(Vector#(n, Bool) bools) = allHigh(pack(bools));

// Alternative encoding of the Maybe type
typedef struct {
  Bool valid;
  t value;
} Option#(type t) deriving (Bits);

// Friendly constructor for Option type
function Option#(t) option(Bool valid, t value) =
  Option { valid: valid, value: value };

// Simple counter
interface Count#(numeric type n);
  method Action inc;
  method Action dec;
  method Bool notFull;
  method Bit#(n) value;
endinterface

module mkCount#(Integer maxVal) (Count#(n));
  // State
  Reg#(Bit#(n)) count <- mkReg(0);
  Reg#(Bool) full <- mkReg(False);

  // Wires
  PulseWire incWire <- mkPulseWire;
  PulseWire decWire <- mkPulseWire;

  // Rules
  rule update;
    Bit#(n) incAmount = 0;
    if (incWire && !decWire) begin
      incAmount = 1;
      full <= count == fromInteger(maxVal-1);
    end else if (!incWire && decWire) begin
      incAmount = -1;
      full <= False;
    end
    count <= count + incAmount;
  endrule

  // Methods
  method Action inc;
    incWire.send;
  endmethod

  method Action dec;
    decWire.send;
  endmethod

  method Bool notFull = !full;

  method Bit#(n) value = count;
endmodule

// A VReg is a register that can only be read
// on the clock cycle after it is written
module mkVReg (Reg#(t)) provisos (Bits#(t, twidth));
  Reg#(t) register <- mkRegU;
  Reg#(Bool) valid <- mkDReg(False);

  method Action _write (t val);
    register <= val;
    valid    <= True;
  endmethod

  method t _read if (valid) = register;
endmodule

endpackage
