// Copyright (c) Matthew Naylor

package Queue;

// This module defines a "Queue", similar to Bluespec's FIFOF, but
// where the "notEmpty" condition need not imply that output data can
// be read.  The validity of output data is instead captured by a
// "canDeq" method.  This interface allows a richer range of FIFO
// implementations that are more efficient in terms of area and Fmax.

// =======
// Imports
// =======

import BlockRam  :: *;
import ConfigReg :: *;
import Util      :: *;
import DReg      :: *;
import Vector    :: *;
import FIFOF     :: *;

// =========
// Interface
// =========

interface SizedQueue#(numeric type logSize, type elemType);
  method Action enq(elemType x);
  method Action deq;
  method elemType dataOut;
  method Bool notFull;
  method Bool notEmpty;
  method Bool canDeq;
endinterface

typedef SizedQueue#(1, elemType) Queue#(type elemType);
typedef SizedQueue#(0, elemType) Queue1#(type elemType);

// ====================
// Register-based Queue
// ====================

// Very similar to Bluespec's mkFIFOF, i.e. a 2-element FIFO
// implemented using registers.

// Strengths:
//   * full throughput (parallel enq and deq on every cycle)
//   * no combinatorial path between notFull and deq (a chain of
//     queues will not result in a huge path)
// Weaknesses: 
//   * there's a mux on the element being enqueued

// Unguarded version
module mkUGQueue (Queue#(elemType))
  provisos (Bits#(elemType, elemWidth));

  // State
  Reg#(Bool) frontValid <- mkReg(False);
  Reg#(elemType) frontReg <- mkRegU;
  Reg#(Bool) bufferValid <- mkReg(False);
  Reg#(elemType) buffer <- mkRegU;

  // Wires
  PulseWire doDeq <- mkPulseWire;
  PulseWire doEnq <- mkPulseWire;
  Wire#(elemType) enqVal <- mkDWire(?);

  // Rules
  rule update;
    Bool updateBuffer = False;
    if (!frontValid || doDeq) begin
      frontValid <= bufferValid || doEnq;
      frontReg <= bufferValid ? buffer : enqVal;
      if (bufferValid) begin
        bufferValid <= doEnq;
        updateBuffer = True;
      end
    end else if (doEnq) begin
      bufferValid <= True;
      updateBuffer = True;
    end
    if (updateBuffer) buffer <= enqVal;
  endrule

  // Methods
  method Action deq;
    doDeq.send;
  endmethod

  method Action enq(elemType x);
    doEnq.send;
    enqVal <= x;
  endmethod

  method elemType dataOut = frontReg;
  method Bool notFull = !bufferValid;
  method Bool notEmpty = frontValid;
  method Bool canDeq = frontValid;

endmodule

// =================
// Shift-based Queue
// =================

// N-element queue implemented using a shift register.

// Strength: no muxes. Input element goes straight to a register and
// output element comes straight from a register
//
// Weakness: there's an N-cycle latency between enqueuing an element
// and being able to dequeue it, where N is the queue capacity.

// There are two modes of operation
//   1. Optimise throughput:
//        * max throughput = 100%
//        * but there's a combinatorial path between notFull and deq
//   2. Optimise Fmax:
//        * no combinatorial path between notFull and deq
//        * but max throughput = N/(N+1), where N is the queue capacity

typedef enum { QueueOptFmax, QueueOptThroughput } QueueOpt deriving (Eq);

// The core version allows any size of queue to be created
module mkUGShiftQueueCore#(QueueOpt opt) (SizedQueue#(size, elemType))
  provisos (Bits#(elemType, elemWidth),
            Add#(1, _any, size));

  // State
  Vector#(size, Reg#(Bool)) valids <- replicateM(mkReg(False));
  Vector#(size, Reg#(elemType)) elems <- replicateM(mkRegU);

  // Wires
  PulseWire doDeq <- mkPulseWire;
  RWire#(elemType) doEnq <- mkRWire;

  // Values
  Integer endIndex = valueOf(size)-1;

  // Rules
  rule update;
    // Shift elements
    Bool shift = doDeq;
    for (Integer i = 0; i < endIndex; i=i+1) begin
      shift = shift || !valids[i];
      if (shift) begin
        elems[i] <= elems[i+1];
        valids[i] <= valids[i+1];
      end
    end
    // Insert new element?
    case (doEnq.wget) matches
      tagged Invalid: if (shift) valids[endIndex] <= False;
      tagged Valid .x: begin
        elems[endIndex] <= x;
        valids[endIndex] <= True;
      end
    endcase
  endrule

  // Methods
  method Action deq;
    doDeq.send;
  endmethod

  method Action enq(elemType x);
    doEnq.wset(x);
  endmethod

  method elemType dataOut = elems[0];
  method Bool notFull = !fold( \&& , readVReg(valids))
                     || (opt == QueueOptFmax ? False : doDeq);
  method Bool notEmpty = fold( \|| , readVReg(valids));
  method Bool canDeq = valids[0];

endmodule

// This version requires the size to be a power of 2
module mkUGShiftQueue#(QueueOpt opt) (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth),
            Add#(1, _any, TExp#(logSize)));

  // State
  SizedQueue#(TExp#(logSize), elemType) q <- mkUGShiftQueueCore(opt);

  // Methods
  method Action deq = q.deq;
  method Action enq(elemType x) = q.enq(x);
  method elemType dataOut = q.dataOut;
  method Bool notFull = q.notFull;
  method Bool notEmpty = q.notEmpty;
  method Bool canDeq = q.canDeq;

endmodule

// Shorthands
module mkUGShiftQueue1#(QueueOpt opt) (Queue1#(elemType))
  provisos (Bits#(elemType, elemTypeWidth));
  let q <- mkUGShiftQueue(opt); return q;
endmodule

module mkUGShiftQueue2#(QueueOpt opt) (Queue#(elemType))
  provisos (Bits#(elemType, elemTypeWidth));
  let q <- mkUGShiftQueue(opt); return q;
endmodule

// ================
// BRAM-based Queue
// ================

// This version has similar semantics as a Bluespec SizedFIFO, but
// with an extra cycle of latency to improve Fmax.

module mkUGSizedQueue (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));

  // Max length of queue
  Integer maxLength = 2 ** valueOf(logSize);

  FIFOF#(elemType) fifo <- mkUGSizedFIFOF(maxLength);
  Queue#(elemType) buffer <- mkUGQueue;

  rule buffering (fifo.notEmpty && buffer.notFull);
    buffer.enq(fifo.first);
    fifo.deq;
  endrule

  // Methods
  method Action deq;
    buffer.deq;
  endmethod

  method Action enq(elemType x);
    fifo.enq(x);
  endmethod

  method elemType dataOut = buffer.dataOut;
  method Bool notFull = fifo.notFull;
  method Bool notEmpty = buffer.notEmpty || fifo.notEmpty;
  method Bool canDeq = buffer.canDeq;

endmodule

module mkUGSizedQueuePrefetch (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));

  SizedQueue#(logSize, elemType) q <- mkUGSizedQueue;
  return q;
endmodule

endpackage
