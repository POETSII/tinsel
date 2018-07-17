package Util;

import DReg      :: *;
import ConfigReg :: *;
import Vector    :: *;

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

// Assertion
function Action myAssert(Bool b, String s) =
  action
    if (!b && genC()) begin
      $display("Assertion failed: ", s);
      $finish();
    end
  endaction;

// Alternative encoding of the Maybe type
typedef struct {
  Bool valid;
  t value;
} Option#(type t) deriving (Bits);

// Friendly constructor for Option type
function Option#(t) option(Bool valid, t value) =
  Option { valid: valid, value: value };

// Set/reset flip-flop
interface SetReset;
  method Action set;
  method Action clear;
  method Bool value;
endinterface

module mkSetReset#(Bool init) (SetReset);
  Reg#(Bool) state <- mkConfigReg(init);
  PulseWire setWire <- mkPulseWire;
  PulseWire clearWire <- mkPulseWire;

  rule update;
    if (setWire)
      state <= True;
    else if (clearWire)
      state <= False;
  endrule

  method Action set;
    setWire.send;
  endmethod

  method Action clear;
    clearWire.send;
  endmethod

  method Bool value = state;
endmodule

// Simple counter
interface Count#(numeric type n);
  method Action inc;
  method Action dec;
  method Action incBy(Bit#(n) amount);
  method Action decBy(Bit#(n) amount);
  method Bool notFull;
  method Bit#(n) value;
  method Bit#(n) available;
endinterface

module mkCount#(Integer maxVal) (Count#(n));
  // State
  Reg#(Bit#(n)) count <- mkReg(0);
  Reg#(Bool) full <- mkReg(False);

  // Wires
  Wire#(Bit#(n)) incWire <- mkDWire(0);
  Wire#(Bit#(n)) decWire <- mkDWire(0);

  // Rules
  rule update;
    Bit#(n) newCount = (count + incWire) - decWire;
    count <= newCount;
    full <= newCount == fromInteger(maxVal);
  endrule

  // Methods
  method Action inc;
    incWire <= 1;
  endmethod

  method Action incBy(Bit#(n) amount);
    incWire <= amount;
  endmethod

  method Action dec;
    decWire <= 1;
  endmethod

  method Action decBy(Bit#(n) amount);
    decWire <= amount;
  endmethod

  method Bool notFull = !full;

  method Bit#(n) value = count;

  method Bit#(n) available = fromInteger(maxVal) - count;
endmodule

// A VReg is a register that can only be read
// on the clock cycle after it is written
module mkVReg (Reg#(t)) provisos (Bits#(t, twidth));
  Reg#(t) register <- mkRegU;
  Wire#(t) valWire <- mkDWire(?);
  Reg#(Bool) valid <- mkDReg(False);

  rule update;
    register <= valWire;
  endrule

  method Action _write (t val);
    valWire <= val;
    valid   <= True;
  endmethod

  method t _read if (valid) = register;
endmodule

// Convert 4-bit nibble to hex digit
function Bit#(8) hexDigit(Bit#(4) nibble) =
  nibble >= 10 ? 55 + zeroExtend(nibble) : 48 + zeroExtend(nibble);

// Generate vector containing integers
function Vector#(n, Integer) genVectorFrom(Integer from);
  function add(a, b) = a+b;
  return Vector::map(add(from), genVector());
endfunction

// The following type-class allows convenient construction of lists, e.g.
//
//   List#(String) xs = list("push", "pop", "top");
//
typeclass MkList#(type a, type b) dependencies (a determines b);
  function a mkList(List#(b) acc);
endtypeclass

instance MkList#(List#(a), a);
  function List#(a) mkList(List#(a) acc) = List::reverse(acc);
endinstance

instance MkList#(function b f(a elem), a) provisos (MkList#(b, a));
  function mkList(acc, elem) = mkList(Cons(elem, acc));
endinstance

function b list() provisos (MkList#(b, a));
  return mkList(Nil);
endfunction

// The following type-class allows convenient construction of vectors, e.g.
//
//   Vector#(3, String) xs = vector("push", "pop", "top");
//
typeclass MkVector#(type a, type b, type n)
    dependencies (a determines (n, b));
  function a mkVector(Vector#(n, b) acc);
endtypeclass

instance MkVector#(Vector#(n, a), a, n);
  function Vector#(n, a) mkVector(Vector#(n, a) acc) =
Vector::reverse(acc);
endinstance

instance MkVector#(function b f(a elem), a, n)
  provisos (MkVector#(b, a, TAdd#(n, 1)));
  function mkVector(acc, elem) = mkVector(Vector::cons(elem, acc));
endinstance

function b vector() provisos (MkVector#(b, a, 0));
  return mkVector(Vector::nil);
endfunction

endpackage
