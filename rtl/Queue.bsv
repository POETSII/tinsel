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
import Assert    :: *;
import DReg      :: *;
import Vector    :: *;

// For BlueCheck test benches:
/*
import StmtFSM   :: *;
import BlueCheck :: *;
import FIFOF     :: *;
import Clocks    :: *;
*/

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
  method Bool canPeek;
  method Bool spaceFor(Integer n);
endinterface

typedef SizedQueue#(1, elemType) Queue#(type elemType);
typedef SizedQueue#(0, elemType) Queue1#(type elemType);

// ====================
// Register-based Queue
// ====================

// Very similar to Bluespec's mkFIFOF, i.e. a 2-element FIFO
// implemented using registers.  But mkFIFOF is implemented in verilog
// and looks like it uses more area than the implementation below.

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
  method Bool canPeek = frontValid;
  method Bool spaceFor(Integer n) =
    error ("Queue.spaceFor() not implemented");

endmodule

// Guarded version
module mkQueue (Queue#(elemType))
  provisos (Bits#(elemType, elemWidth));

  // State
  Queue#(elemType) q <- mkUGQueue;

  // Methods
  method Action deq if (q.canDeq);
    q.deq;
  endmethod

  method Action enq(elemType x) if (q.notFull);
    q.enq(x);
  endmethod

  method elemType dataOut if (q.canPeek) = q.dataOut;
  method Bool notFull = q.notFull;
  method Bool notEmpty = q.notEmpty;
  method Bool canDeq = q.canDeq;
  method Bool canPeek = q.canPeek;
  method Bool spaceFor(Integer n) = q.spaceFor(n);
endmodule

/*
// Test bench
// ----------

module [Specification] regQueueSpec#(Reset r) ();
  // Specification instance
  FIFOF#(Bit#(8)) fifo <- mkFIFOF(reset_by r);

  // Implementation instance
  Queue#(Bit#(8)) q <- mkQueue(reset_by r);

  // Properties
  equiv("enq", fifo.enq, q.enq);
  equiv("deq", fifo.deq, q.deq);
  equiv("front", fifo.first, q.dataOut);
  equiv("notFull", fifo.notFull, q.notFull);
  equiv("notEmpty", fifo.notEmpty, q.notEmpty);
  parallel(list("enq", "deq"));
endmodule

// The test bench
module [Module] regQueueTest ();
  Clock clk <- exposeCurrentClock;
  MakeResetIfc r <- mkReset(0, True, clk);
  blueCheckID(regQueueSpec(r.new_rst), r);
endmodule
*/

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

// Unguarded version
module mkUGShiftQueue#(QueueOpt opt) (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth),
            Add#(1, _any, TExp#(logSize)));

  // State
  Vector#(TExp#(logSize), Reg#(Bool)) valids <- replicateM(mkReg(False));
  Vector#(TExp#(logSize), Reg#(elemType)) elems <- replicateM(mkRegU);

  // Wires
  PulseWire doDeq <- mkPulseWire;
  RWire#(elemType) doEnq <- mkRWire;

  // Values
  Integer endIndex = 2**valueOf(logSize)-1;

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
  method Bool canPeek = valids[0];
  method Bool spaceFor(Integer n) =
    error ("Queue.spaceFor() not implemented");
endmodule

// Guarded version
module mkShiftQueue#(QueueOpt opt) (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth),
            Add#(1, _any, TExp#(logSize)));

  // State
  SizedQueue#(logSize, elemType) q <- mkUGShiftQueue(opt);

  // Methods
  method Action deq if (q.canDeq);
    q.deq;
  endmethod

  method Action enq(elemType x) if (q.notFull);
    q.enq(x);
  endmethod

  method elemType dataOut = q.dataOut;
  method Bool notFull = q.notFull;
  method Bool notEmpty = q.notEmpty;
  method Bool canDeq = q.canDeq;
  method Bool canPeek = q.canPeek;
  method Bool spaceFor(Integer n) = q.spaceFor(n);
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


/*
// Test bench
// ----------

module [Specification] shiftQueueSpec#(Reset r) ();
  // Specification instance
  FIFOF#(Bit#(8)) fifo <- mkSizedFIFOF(4, reset_by r);

  // Implementation instance
  SizedQueue#(2, Bit#(8)) q <- mkShiftQueue(QueueOptFmax, reset_by r);

  // Obtain function for making assertions
  Ensure ensure <- getEnsure;

  function Bool check =
    q.canDeq ? fifo.first == q.dataOut : True;

  // Properties
  equiv("enq", fifo.enq, q.enq);
  equiv("deq", fifo.deq, q.deq);
  prop("check", check);
  equiv("notFull", fifo.notFull, q.notFull);
  equiv("notEmpty", fifo.notEmpty, q.notEmpty);
  parallel(list("enq", "deq"));
endmodule

// The test bench
module [Module] shiftQueueTest ();
  Clock clk <- exposeCurrentClock;
  MakeResetIfc r <- mkReset(0, True, clk);
  blueCheckID(shiftQueueSpec(r.new_rst), r);
endmodule
*/

// ================
// BRAM-based Queue
// ================

// Similar to Bluespec's mkSizedFIFOF but introduces a one-cycle delay
// between dequeuing an element and obtaining the dequeued element.
// This permits an implementation using a buffered (2-cycle latent)
// block RAM and "don't care" read-during-write semantics, enabling
// high clock frequencies.  In addition, the initial contents of the
// queue can be specified.

// When "deq" is invoked, the dequeued item becomes available on the
// "dataOut" bus on the next clock cycle.  "deq" will not fire if the
// queue was empty on the previous cycle -- this condition is captured
// by a guard on the "deq" method and also by the "canDeq" method:
// "canDeq" and "notEmpty" are not equivalent.

// Unguarded version
module mkUGSizedQueue (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));
  QueueInit init;
  init.size = 0;
  init.file = Invalid;
  let q <- mkUGSizedQueueInit(init);
  return q;
endmodule

// Guarded version
module mkSizedQueue (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));
  QueueInit init;
  init.size = 0;
  init.file = Invalid;
  let q <- mkSizedQueueInit(init);
  return q;
endmodule

// Options
typedef struct {
  Integer size;
  Maybe#(String) file;
} QueueInit;

// Unguarded version
module mkUGSizedQueueInit#(QueueInit init) (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));

  // Max length of queue
  Integer maxLength = 2 ** valueOf(logSize);

  // Block RAM to hold contents of queue
  BlockRamOpts ramOpts = defaultBlockRamOpts;
  ramOpts.initFile = init.file;
  BlockRam#(Bit#(logSize), elemType) ram <- mkBlockRamOpts(ramOpts);

  // State
  Reg#(Bit#(logSize)) front <- mkReg(0);
  Reg#(Bit#(logSize)) back <- mkReg(fromInteger(init.size % maxLength));
  Reg#(Bit#(TAdd#(logSize, 1))) length <- mkReg(fromInteger(init.size));
  Reg#(Bool) empty <- mkReg(init.size == 0);
  Reg#(Bool) full <- mkReg(init.size == maxLength);
  Reg#(Bool) deqEnable <- mkReg(False);
  Reg#(Bool) canPeekReg <- mkReg(False);

  // Wires
  PulseWire doDeq <- mkPulseWire;
  RWire#(elemType) doEnq <- mkRWire;

  // Rules
  rule update;
    let incFront = front+1;
    let newFront = doDeq ? incFront : front;
    ram.read(newFront);
    front <= newFront;
    let lengthInc = 0;
    case (doEnq.wget) matches
      tagged Invalid:
        if (doDeq) begin
          full <= False;
          lengthInc = -1;
        end
      tagged Valid .x: begin
        ram.write(back, x);
        back <= back+1;
        if (!doDeq) begin
          full <= length == fromInteger(maxLength-1);
          empty <= False;
          lengthInc = 1;
        end
      end
    endcase
    length <= length + lengthInc;
    if (doDeq && length == 1) begin
      deqEnable <= False;
      if (! isValid(doEnq.wget)) empty <= True;
    end else
      deqEnable <= !empty;
    canPeekReg <= deqEnable && !doDeq;
  endrule

  // Methods
  method Action deq;
    dynamicAssert(deqEnable, "Queue: deq() called when canDeq() false");
    doDeq.send;
  endmethod

  method Action enq(elemType x);
    dynamicAssert(!full, "Queue: enq() called when queue full");
    doEnq.wset(x);
  endmethod

  method elemType dataOut = ram.dataOut;
  method Bool notFull = !full;
  method Bool notEmpty = !empty;
  method Bool canDeq = deqEnable;
  method Bool canPeek = canPeekReg;
  method Bool spaceFor(Integer n) = length < fromInteger(maxLength-n);
endmodule

// Guarded version
module mkSizedQueueInit#(QueueInit init) (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));

  // State
  SizedQueue#(logSize, elemType) q <- mkUGSizedQueueInit(init);

  // Methods
  method Action deq if (q.canDeq);
    q.deq;
  endmethod

  method Action enq(elemType x) if (q.notFull);
    q.enq(x);
  endmethod

  method elemType dataOut = q.dataOut;
  method Bool notFull = q.notFull;
  method Bool notEmpty = q.notEmpty;
  method Bool canDeq = q.canDeq;
  method Bool canPeek = q.canPeek;
  method Bool spaceFor(Integer n) = q.spaceFor(n);
endmodule

/*
// Test bench
// ----------

// A BlueCheck test bench that asserts an equivalance between a
// Bluespec sized-FIFO and a queue.

module [Specification] sizedQueueSpec#(Reset r) ();
  // Specification instance (a 4-element sized-FIFO)
  FIFOF#(Bit#(8)) fifo <- mkSizedFIFOF(4, reset_by r);

  // Was there a dequeue on the previous cycle?
  Reg#(Bool) didDeq <- mkDReg(False);
  Reg#(Bit#(8)) prevFirst <- mkRegU;
  rule updatePrevFirst;
    prevFirst <= fifo.first;
  endrule

  // Implementation instance (a 4-element queue)
  SizedQueue#(2, Bit#(8)) q <- mkSizedQueue(reset_by r);

  Action dequeue = action
    await(q.canDeq); q.deq; didDeq <= True;
  endaction;

  // Properties
  equivf(2, "enq", fifo.enq, q.enq);
  equiv("deq", fifo.deq, dequeue);
  prop("first", didDeq ? prevFirst == q.dataOut : True);
  prop("peek", q.canPeek ? fifo.first == q.dataOut : True);
  equiv("notFull", fifo.notFull, q.notFull);
  equiv("notEmpty", fifo.notEmpty, q.notEmpty);
  parallel(list("enq", "deq"));
endmodule

// The test bench
module [Module] sizedQueueTest ();
  Clock clk <- exposeCurrentClock;
  MakeResetIfc r <- mkReset(0, True, clk);
  blueCheckID(sizedQueueSpec(r.new_rst), r);
endmodule
*/

// ==============================
// BRAM-based Queue with Prefetch
// ==============================

// This version has similar semantics as a Bluespec SizedFIFOF, at the
// cost of two registers and a mux.

module mkUGSizedQueuePrefetch (SizedQueue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));
  // State
  SizedQueue#(logSize, elemType) q <- mkUGSizedQueue;
  Reg#(elemType) frontReg <- mkRegU;
  Reg#(Bool) frontValid <- mkReg(False);
  Reg#(elemType) buffer <- mkRegU;
  Reg#(Bool) bufferValid <- mkReg(False);
  Reg#(Bool) qValid <- mkDReg(False);

  // Wires
  PulseWire doDeq <- mkPulseWire;

  // Rules
  rule update;
    Bool updateBuffer = False;
    if (!frontValid || doDeq) begin
      frontValid <= bufferValid || qValid;
      frontReg <= bufferValid ? buffer : q.dataOut;
      if (bufferValid) begin
        bufferValid <= qValid;
        updateBuffer = True;
      end
      if (q.canDeq) begin
        q.deq;
        qValid <= True;
      end
    end else if (qValid) begin
      bufferValid <= True;
      updateBuffer = True;
    end
    if (updateBuffer) buffer <= q.dataOut;
  endrule

  // Methods
  method Action deq;
    doDeq.send;
  endmethod

  method Action enq(elemType x);
    q.enq(x);
  endmethod

  method elemType dataOut = frontReg;
  method Bool notFull = q.notFull;
  method Bool notEmpty = frontValid || qValid || q.notEmpty;
  method Bool canDeq = frontValid;
  method Bool canPeek = frontValid;
  method Bool spaceFor(Integer n) =
        error ("Queue.spaceFor() not implemented");
endmodule

/*
// Test bench
// ----------

module [Specification] sizedQueuePrefetchSpec#(Reset r) ();
  // Specification instance (a 4-element sized-FIFO)
  FIFOF#(Bit#(8)) fifo <- mkSizedFIFOF(4, reset_by r);

  // Implementation instance (a 4-element queue)
  SizedQueue#(2, Bit#(8)) q <- mkUGSizedQueuePrefetch(reset_by r);

  // Properties
  equivf(2, "enq", fifo.enq, q.enq);
  prop("deq", action await(q.canDeq); q.deq; fifo.deq; endaction);
  prop("first", q.canDeq ? q.dataOut == fifo.first : True);
  //equiv("notFull", fifo.notFull, q.notFull);
  equiv("notEmpty", fifo.notEmpty, q.notEmpty);
  parallel(list("enq", "deq"));
endmodule

// The test bench
module [Module] sizedQueuePrefetchTest ();
  Clock clk <- exposeCurrentClock;
  MakeResetIfc r <- mkReset(0, True, clk);
  blueCheckID(sizedQueuePrefetchSpec(r.new_rst), r);
endmodule
*/

endpackage
