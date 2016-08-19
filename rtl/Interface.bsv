// Copyright (c) Matthew Naylor

package Interface;

// There are two main aims of this library:
//   1. abstract away from the kind of queues used to
//      connect modules together; and
//   2. allow synthesis boundaries between modules to
//      be easily created.

// Typical usage is as follows:
//   * If a module needs an output with control-flow, then it
//     creates an "output port" using mkOutPort.  The module writes
//     to the port using "port.canPut" and "port.put()" and returns
//     the "port.out" interface.
//   * If a module needs an input with control-flow, then it
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
import Util      :: *;
import Assert    :: *;
import List      :: *;
import Vector    :: *;

// =============================================================================
// In & Out interfaces
// =============================================================================

// These are handshake interfaces with control-flow

interface Out#(type t);
  (* always_ready *)
  method Action tryGet;
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
  Wire#(Bool) doGetWire <- mkDWire(False);

  method Bool canGet = putWireValid;
  method Action get;
    doGetWire <= True;
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
module mkMergeTree#(MergeMethod m, module#(SizedQueue#(n, t)) mkQ,
                      Vector#(m, Out#(t)) vec) (Out#(t))
         provisos (Bits#(t, twidth));
  let out <- mkMergeTreeList(m, mkQ, Vector::toList(vec));
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

    // Fill the queue for each sink
    rule distribute (inPort.canGet && getKey(inPort.value) == fromInteger(i));
      if (outPorts[i].canPut) begin
        outPorts[i].put(inPort.value);
        inPort.get;
      end
    endrule
  end

  // Interface
  return inPort.in;
endmodule

endpackage
