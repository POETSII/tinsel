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

// A module for maintaining a set of unique ids
// (Implemented as a bidirectional shift register)
interface SetOfIds#(numeric type logSize);
  method Action init;
  method Action insert(Bit#(logSize) x);
  method Action remove;
  method Bit#(logSize) item;
  method Bool notEmpty;
endinterface

module mkSetOfIds (SetOfIds#(logSize));
  // State
  Vector#(TExp#(logSize), Reg#(Bit#(logSize))) elems <- replicateM(mkRegU);
  Vector#(TExp#(logSize), Reg#(Bool)) valids <- replicateM(mkReg(False));

  // Wires
  PulseWire doInit <- mkPulseWire;
  PulseWire doRemove <- mkPulseWire;
  RWire#(Bit#(logSize)) doInsert <- mkRWire;

  // Values
  Integer size = valueOf(TExp#(logSize));

  // Rules
  rule update;
    if (doInit) begin
      for (Integer i = 0; i < size; i=i+1) begin
        elems[i] <= fromInteger(i);
        valids[i] <= True;
      end
    end else begin
      case (doInsert.wget) matches
        tagged Invalid: begin
          for (Integer i = 0; i < size-1; i=i+1)
            if (doRemove) begin
              elems[i] <= elems[i+1];
              valids[i] <= valids[i+1];
            end
          if (doRemove) valids[size-1] <= False;
        end
        tagged Valid .x: begin
          for (Integer i = 0; i < size-1; i=i+1)
            if (!doRemove) begin
              elems[i+1] <= elems[i];
              valids[i+1] <= valids[i];
            end
          elems[0] <= x;
          valids[0] <= True;
        end
      endcase
    end
  endrule

  // Methods
  method Action init;
    doInit.send;
  endmethod

  method Action insert(Bit#(logSize) x);
    doInsert.wset(x);
  endmethod

  method Action remove;
    doRemove.send;
  endmethod

  method Bit#(logSize) item = elems[0];

  method Bool notEmpty = valids[0];
endmodule

endpackage
