// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package Interface;

// There are two main aims of this library:
//   1. abstract away from the kind of queues used to
//      connect modules together; and
//   2. allow synthesis boundaries between modules to
//      be easily created.

// Typical usage is as follows:
//   * If a module needs an output with flow-control, then it
//     creates an "output port" using mkOutPort.  The module writes
//     to the port using "port.canPut" and "port.put()" and returns
//     the "port.out" interface.
//   * If a module needs an input with flow-control, then it
//     creates an "input port" using mkInPort.  The module reads from
//     the port using "port.canGet" and "port.get" and returns the
//     "port.in" interface.
//   * Output interfaces (e.g. "port.out") can be connected to input
//     interfaces (e.g. "port.in") using the "connectUsing" module.
//     There are a variety of ways to implement this connection,
//     each with different performance characteristics.

// =============================================================================
// Imports
// =============================================================================

import Queue     :: *;
import Assert    :: *;
import List      :: *;
import Vector    :: *;
import ConfigReg :: *;

// =============================================================================
// In & Out interfaces
// =============================================================================

// I/O interfaces with flow-control

interface Out#(type t);
  (* always_ready *)
  method Action tryGet; // No implicit condition on valid
  (* always_enabled *)
  method Bool valid;
  (* always_enabled *)
  method t value;
endinterface

interface In#(type t);
  (* always_ready *)
  method Action tryPut(t val);
  (* always_enabled *)
  method Bool didPut;
endinterface

// =============================================================================
// Buffered Out interface
// =============================================================================

// This type is equivalent to the Out interface (but is used with a
// different semantics).  It is intended to capture buffered output
// interfaces at the type level.

interface BOut#(type t);
  method Action get;
  (* always_enabled *)
  method Bool valid; // No implicit condition on valid
  (* always_enabled *)
  method t value;
endinterface

// =============================================================================
// Ouput ports
// =============================================================================

// An output port provides canPut and put methods for the writer, and
// an output interface for the reader.

interface OutPort#(type t);
  // For the writer of the port
  method Bool canPut;
  method Action put(t val);
  // For the reader of the port
  interface Out#(t) out;
endinterface

module mkOutPort (OutPort#(t))
  provisos (Bits#(t, twidth));

  Wire#(Bool) canPutWire <- mkDWire(False);
  Wire#(Bool) putWireValid <- mkDWire(False);
  Wire#(t) putWireContents <- mkDWire(?);

  method Bool canPut = canPutWire;
  method Action put(t val);
    putWireValid <= True;
    putWireContents <= val;
  endmethod

  interface Out out;
    method Action tryGet;
      canPutWire <= True;
    endmethod
    method Bool valid = putWireValid;
    method t value = putWireContents;
  endinterface
endmodule

// =============================================================================
// Input ports
// =============================================================================

// An input port provides canGet and get methods for the reader, and
// an input interface for the writer.

interface InPort#(type t);
  // For the reader of the port
  method Bool canGet;
  method Action get;
  (* always_enabled *)
  method t value;
  // For the writer of the port
  interface In#(t) in;
endinterface

module mkInPort (InPort#(t))
  provisos(Bits#(t, twidth));

  Wire#(Bool) putWireValid <- mkDWire(False);
  Wire#(t) putWireContents <- mkDWire(?);
  PulseWire doGetWire <- mkPulseWireOR;

  method Bool canGet = putWireValid;
  method Action get;
    doGetWire.send;
  endmethod
  method t value = putWireContents;

  interface In in;
    method Action tryPut(t val);
      putWireValid <= True;
      putWireContents <= val;
    endmethod
    method Bool didPut = doGetWire;
  endinterface
endmodule

// =============================================================================
// Connecting In and Out interfaces
// =============================================================================

// Connect an Out interface to an In interface using the given queue.
module connectUsing#(
  module#(SizedQueue#(n, t)) mkQ, Out#(t) out, In#(t) in) (Empty)
  provisos (Bits#(t, twidth));

  SizedQueue#(n, t) q <- mkQ;

  rule connection1a;
    if (q.notFull) out.tryGet;
  endrule

  rule connection1b;
    if (out.valid) q.enq(out.value);
  endrule

  rule connection2a;
    if (q.canDeq) in.tryPut(q.dataOut);
  endrule

  rule connection2b;
    if (in.didPut) q.deq;
  endrule
endmodule

// Connect an Out interface to queue.
module connectToQueue#(Out#(t) out, SizedQueue#(n, t) q) (Empty)
  provisos (Bits#(t, twidth));

  rule connection1a;
    if (q.notFull) out.tryGet;
  endrule

  rule connection1b;
    if (out.valid) q.enq(out.value);
  endrule
endmodule

// =============================================================================
// Helper functions on In and Out interfaces
// =============================================================================

// Apply pure function to value flowing through In interface
module onIn#(function t f(u x), In#(t) in) (In#(u));
  method Action tryPut(u val) = in.tryPut(f(val));
  method Bool didPut = in.didPut;
endmodule

// Apply pure function to value flowing through Out interface
module onOut#(function u f(t x), Out#(t) out) (Out#(u));
  method Action tryGet = out.tryGet;
  method Bool valid = out.valid;
  method u value = f(out.value);
endmodule

// Apply pure function to value flowing through BOut interface
module onBOut#(function u f(t x), BOut#(t) out) (BOut#(u));
  method Action get = out.get;
  method Bool valid = out.valid;
  method u value = f(out.value);
endmodule

// Convert BOut to Out
function Out#(t) fromBOut(BOut#(t) out) =
  interface Out
    method Action tryGet = out.get;
    method Bool valid = out.valid;
    method t value = out.value;
  endinterface;

// A null In port accepts and discards all inputs
module mkNullIn (In#(t));
  method Action tryPut(u val); endmethod
  method Bool didPut = True;
endmodule

// A null Out port never produces any output
module mkNullOut (Out#(t));
  method Action tryGet; endmethod
  method Bool valid = False;
  method t value = ?;
endmodule

// A null BOut port never produces any output
module mkNullBOut (BOut#(t));
  method Action get; endmethod
  method Bool valid = False;
  method t value = ?;
endmodule

// Optionally disable an Out interface
function Out#(t) enableOut(Bool en, Out#(t) out) =
  interface Out
    method Action tryGet = out.tryGet;
    method Bool valid = en && out.valid;
    method t value = out.value;
  endinterface;

// Optionally disable a BOut interface
function BOut#(t) enableBOut(Bool en, BOut#(t) out) =
  interface BOut
    method Action get = out.get;
    method Bool valid = en && out.valid;
    method t value = out.value;
  endinterface;

// Convert queue to BOut interface
function BOut#(t) queueToBOut(SizedQueue#(n, t) q) =
  interface BOut
    method Action get = q.deq;
    method Bool valid = q.canDeq && q.canPeek;
    method t value = q.dataOut;
  endinterface;

// =============================================================================
// Merge unit
// =============================================================================

interface MergeUnit#(type t);
  interface In#(t)  inA;
  interface In#(t)  inB;
  interface Out#(t) out;
endinterface

// Left-biased merge unit
module mkMergeUnit (MergeUnit#(t))
  provisos (Bits#(t, twidth));

  // Ports
  InPort#(t) inPortA <- mkInPort;
  InPort#(t) inPortB <- mkInPort;
  OutPort#(t) outPort <- mkOutPort;

  // Rules
  rule merge (outPort.canPut);
    // Consume input
    if (inPortA.canGet) inPortA.get;
    else if (inPortB.canGet) inPortB.get;
    // Produce output
    if (inPortA.canGet || inPortB.canGet)
      outPort.put(inPortA.canGet ? inPortA.value : inPortB.value);
  endrule

  // Interface
  interface In  inA = inPortA.in;
  interface In  inB = inPortB.in;
  interface Out out = outPort.out;
endmodule

// Fair merge unit
module mkMergeUnitFair (MergeUnit#(t))
  provisos (Bits#(t, twidth));

  // Ports
  InPort#(t) inPortA <- mkInPort;
  InPort#(t) inPortB <- mkInPort;
  OutPort#(t) outPort <- mkOutPort;

  // State
  Reg#(Bool) prevChoiceWasA <- mkReg(False);

  // Rules
  rule merge (outPort.canPut);
    Bool chooseB = inPortB.canGet && (!inPortA.canGet || prevChoiceWasA);
    // Consume input
    if (chooseB) inPortB.get;
    else if (inPortA.canGet) inPortA.get;
    // Produce output
    if (inPortA.canGet || inPortB.canGet) begin
      outPort.put(chooseB ? inPortB.value : inPortA.value);
      prevChoiceWasA <= !chooseB;
    end
  endrule

  // Interface
  interface In  inA = inPortA.in;
  interface In  inB = inPortB.in;
  interface Out out = outPort.out;
endmodule

// Allow the merge method to be specified as a module parameter
typedef enum { LeftBiased, Fair } MergeMethod deriving (Eq);

// Merge unit helper: merge two output interfaces to a single output interface
module mkMergeTwo#(MergeMethod m, module#(SizedQueue#(n, t)) mkQ,
                     Out#(t) a, Out#(t) b) (Out#(t))
         provisos (Bits#(t, twidth));

  // Create a merge unit
  MergeUnit#(t) merger;
  if (m == LeftBiased) merger <- mkMergeUnit;
  else merger <- mkMergeUnitFair;

  // Connect output interfaces to merge unit
  connectUsing(mkQ, a, merger.inA);
  connectUsing(mkQ, b, merger.inB);

  // Return output of merge unit
  return merger.out;
endmodule

// Merge unit helper: merge two buffered output interfaces to a
// single output interface
module mkMergeTwoB#(MergeMethod m, BOut#(t) a, BOut#(t) b) (Out#(t))
         provisos (Bits#(t, twidth));

  // Create a merge unit
  MergeUnit#(t) merger;
  if (m == LeftBiased) merger <- mkMergeUnit;
  else merger <- mkMergeUnitFair;

  // Connect output interfaces to merge unit
  connectDirect(a, merger.inA);
  connectDirect(b, merger.inB);

  // Return output of merge unit
  return merger.out;
endmodule

// =============================================================================
// Tree-based request merger
// =============================================================================

// Merge a list of output interfaces to a single output interface
module mkMergeTreeList#(MergeMethod m, module#(SizedQueue#(n, t)) mkQ,
                         List#(Out#(t)) list) (Out#(t))
         provisos (Bits#(t, twidth));

  Integer n = length(list);
  staticAssert(n > 0, "mergeTree applied to empty list");

  List#(Out#(t)) xs = list;
  while (n > 1) begin
    let y <- mkMergeTwo(m, mkQ, xs[0], xs[1]);
    xs = List::append(List::drop(2, xs), List::cons(y, Nil));
    n=n-1;
  end

  return xs[0];
endmodule

// As above, but for vectors instead of lists
module mkMergeTree#(MergeMethod m, module#(SizedQueue#(d, t)) mkQ,
                      Vector#(n, Out#(t)) vec) (Out#(t))
         provisos (Bits#(t, twidth));
  let out <- mkMergeTreeList(m, mkQ, Vector::toList(vec));
  return out;
endmodule

// Similar to above, but takes a vector of buffered output interfaces
module mkMergeTreeB#(MergeMethod m, module#(SizedQueue#(d, t)) mkQ,
                       Vector#(n, BOut#(t)) vec) (Out#(t))
         provisos (Bits#(t, twidth));

  // Reduce first level of tree using mkMergeTwoB
  List#(Out#(t)) xs = Nil;
  for (Integer i = 0; i < valueOf(n); i=i+2) begin
    let x <- i+1 == valueOf(n) ? convertBOutToOut(vec[i]) :
                                 mkMergeTwoB(m, vec[i], vec[i+1]);
    xs = List::cons(x, xs);
  end

  let out <- mkMergeTreeList(m, mkQ, List::reverse(xs));
  return out;
endmodule

// =============================================================================
// Response distributor
// =============================================================================

module mkResponseDistributor#
         (function Bit#(TLog#(n)) getKey(t val),
          module#(SizedQueue#(m, t)) mkQ,
          Vector#(n, In#(t)) sinks) (In#(t))
         provisos (Bits#(t, twidth));

  InPort#(t) inPort <- mkInPort;
  Vector#(n, OutPort#(t)) outPorts <- replicateM(mkOutPort);

  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    // Put a queue in front of each sink
    connectUsing(mkQ, outPorts[i].out, sinks[i]);

    // Is response for this sink?
    Bool selected = valueOf(n) == 1 ? True :
                      getKey(inPort.value) == fromInteger(i);

    // Fill the queue for each sink
    rule distribute (inPort.canGet && selected);
      if (outPorts[i].canPut) begin
        outPorts[i].put(inPort.value);
        inPort.get;
      end
    endrule
  end

  // Interface
  return inPort.in;
endmodule

// =============================================================================
// Routines for buffered interfaces
// =============================================================================

// Connect a buffered output interface to an input interface, without
// needing an intermediate buffer.  (Attempting this with an
// unbuffered output interface yields a combinatorial cycle.)
module connectDirect#(BOut#(t) out, In#(t) in) ()
  provisos (Bits#(t, twidth));

  rule connection1;
    if (out.valid) in.tryPut(out.value);
  endrule

  rule connection2;
    if (in.didPut) out.get;
  endrule
endmodule

// Convert a buffered output interface to an unbuffered one
module convertBOutToOut#(BOut#(t) a) (Out#(t))
         provisos (Bits#(t, twidth));
  PulseWire doTryGet <- mkPulseWire;
  method Action tryGet; doTryGet.send; if (a.valid) a.get; endmethod
  method Bool valid = doTryGet && a.valid;
  method t value = a.value;
endmodule

// =============================================================================
// Serialiser
// =============================================================================

interface Serialiser#(type typeIn, type typeOut);
  interface In#(typeIn) parallelIn;
  interface Out#(typeOut) serialOut;
endinterface

// Parallel to serial conversion
module mkSerialiser (Serialiser#(typeIn, typeOut))
  provisos (Bits#(typeIn, widthIn),
            Bits#(typeOut, widthOut),
            Mul#(widthOut, n, widthIn));

  // Ports
  InPort#(typeIn) inPort <- mkInPort;
  OutPort#(typeOut) outPort <- mkOutPort;

  // Shift register
  Vector#(n, Reg#(typeOut)) shiftReg <- replicateM(mkConfigRegU);

  // Unary encoding of number of elements in shift register
  Reg#(Bit#(n)) count <- mkConfigReg(0);

  rule step;
    // Load new items?
    Bool load = inPort.canGet &&
                  (count == 0 || (count == 1 && outPort.canPut));
    // Shift item out?
    Bool shift = outPort.canPut && count != 0;
    // Emit item
    if (shift) outPort.put(shiftReg[0]);
    // Only shift the register when not loading it
    if (shift && !load) begin
      for (Integer i = 0; i < valueOf(n)-1; i=i+1)
        shiftReg[i] <= shiftReg[i+1];
      count <= count >> 1;
    end else if (load) begin
      inPort.get;
      Vector#(n, typeOut) vec = unpack(pack(inPort.value));
      for (Integer i = 0; i < valueOf(n); i=i+1)
        shiftReg[i] <= vec[i];
      count <= ~0;
    end
  endrule

  // Interfaces
  interface In parallelIn = inPort.in;
  interface Out serialOut = outPort.out;
endmodule

// =============================================================================
// Deserialiser
// =============================================================================

interface Deserialiser#(type typeIn, type typeOut);
  interface In#(typeIn) serialIn;
  interface Out#(typeOut) parallelOut;
endinterface

// Serial to parallel conversion
module mkDeserialiser (Deserialiser#(typeIn, typeOut))
  provisos (Bits#(typeIn, widthIn),
            Bits#(typeOut, widthOut),
            Mul#(widthIn, n, widthOut));

  // Ports
  InPort#(typeIn) inPort <- mkInPort;
  OutPort#(typeOut) outPort <- mkOutPort;

  // Shift register
  Vector#(n, Reg#(typeIn)) shiftReg <- replicateM(mkConfigRegU);

  // Unary encoding of number of unused elements in shift register
  Reg#(Bit#(n)) space <- mkConfigReg(~0);

  // Wires controlling update of space count
  PulseWire resetSpace <- mkPulseWire;
  PulseWire decSpace <- mkPulseWire;

  // Update space
  rule updateSpace;
    if (decSpace && resetSpace)
      space <= ~0 >> 1;
    else if (resetSpace)
      space <= ~0;
    else if (decSpace)
      space <= space >> 1;
  endrule

  // Consume input
  rule consume (inPort.canGet);
    if (space != 0 || (space == 0 && outPort.canPut)) begin
      inPort.get;
      shiftReg[valueOf(n)-1] <= inPort.value;
      for (Integer i = 0; i < valueOf(n)-1; i=i+1)
        shiftReg[i] <= shiftReg[i+1];
      decSpace.send;
    end
  endrule

  // Produce output
  rule produce (outPort.canPut);
    if (space == 0) begin
      outPort.put(unpack(pack(readVReg(shiftReg))));
      resetSpace.send;
    end
  endrule

  // Interfaces
  interface In serialIn = inPort.in;
  interface Out parallelOut = outPort.out;
endmodule

// =============================================================================
// Reduction connectors
// =============================================================================

// Reduce a list of interfaces down to a given number of interfaces,
// by merging.

// First, a module to reduce by up to factor of two.
module mkReduceOneLevel#(
         function module#(Out#(t)) mergeTwo(Out#(t) a, Out#(t) b),
           List#(Out#(t)) inps)
         (List#(Out#(t)))
       provisos (Bits#(t, twidth));

  // Number of inputs
  Integer numIn = List::length(inps);

  if (numIn <= 1)
    return inps;
  else begin
    let out <- mergeTwo(inps[0], inps[1]);
    let rest <- mkReduceOneLevel(mergeTwo, List::drop(2, inps));
    return (Cons(out, rest));
  end
endmodule

// Now reduce as many levels as required
module mkReduce#(
         function module#(Out#(t)) mergeTwo(Out#(t) a, Out#(t) b),
           Integer n, List#(Out#(t)) inps)
         (List#(Out#(t)))
       provisos (Bits#(t, twidth));

   // Number of inputs
  Integer numIn = length(inps);

  if (numIn <= n)
    return inps;
  else begin
    let out <- mkReduceOneLevel(mergeTwo, inps);
    let list <- mkReduce(mergeTwo, n, out);
    return list;
  end
endmodule

// Connect 'from' ports to 'to' ports,
// where 'length(from)' may be more than 'length(to)'.
// Works by fair-merging of 'from' ports.
module reduceConnect#(
         function module#(Out#(t)) mergeTwo(Out#(t) a, Out#(t) b),
           List#(Out#(t)) from, List#(In#(t)) to) ()
         provisos (Bits#(t, twidth));

  // Count inputs and outputs
  Integer numFrom = List::length(from);
  Integer numTo = List::length(to);

  // Merge down to 'numTo' ports
  let inter <- mkReduce(mergeTwo, numTo, from);
  Integer numInter = List::length(inter);

  // Now connect
  for (Integer i = 0; i < numTo; i=i+1) begin
    if (i < numInter)
      connectUsing(mkUGShiftQueue1(QueueOptFmax), inter[i], to[i]);
    else begin
       // Connect terminator
      BOut#(t) nullOut <- mkNullBOut;
      connectDirect(nullOut, to[i]);
    end
  end

endmodule

endpackage
