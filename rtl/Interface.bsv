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

import Queue :: *;
import Util :: *;

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

endpackage
