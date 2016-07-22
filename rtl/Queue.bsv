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

// =================================
// Implementation 1: 2-element Queue
// =================================

// Similar to Bluespec's mkFIFOF, except that it's possible for
// "nonEmpty" to be true and "canDeq" to be False at the same time.
// There is a 2-cycle latency between enqueing a value, say X, and
// being able to dequeue X.  Nevertheless, full throughput is
// maintained with parallel "enq" and "deq" permitted.  The
// implementation requires hardly any logic at all (notably no mux,
// unlike mkFIFOF).

// Unguarded version
module mkUGQueue (Queue#(elemType))
  provisos (Bits#(elemType, elemWidth));

  // State
  Reg#(Bool) frontValid <- mkReg(False);
  Reg#(elemType) front  <- mkRegU;
  Reg#(Bool) backValid  <- mkReg(False);
  Reg#(elemType) back   <- mkRegU;

  // Wires
  PulseWire doDeq <- mkPulseWire;
  RWire#(elemType) doEnq <- mkRWire;

  // Rules
  rule update;
    Bool shift = doDeq || !frontValid;
    if (shift) begin
      frontValid <= backValid;
      front      <= back;
    end
    case (doEnq.wget) matches
      tagged Invalid:
        if (shift) backValid <= False;
      tagged Valid .x: begin
        backValid <= True;
        back      <= x;
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

  method elemType dataOut = front;
  method Bool notFull = !(frontValid && backValid);
  method Bool notEmpty = frontValid || backValid;
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

module [Specification] queueSpec#(Reset r) ();
  // Specification instance
  FIFOF#(Bit#(8)) fifo <- mkFIFOF(reset_by r);

  // Implementation instance
  Queue#(Bit#(8)) q <- mkQueue(reset_by r);

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
module [Module] queueTest ();
  Clock clk <- exposeCurrentClock;
  MakeResetIfc r <- mkReset(0, True, clk);
  blueCheckID(queueSpec(r.new_rst), r);
endmodule
*/

// =================================
// Implementation 2: 1-element Queue
// =================================

// Same as Bluespec's mkUGFIFOF1, but implemented in Bluespec rather
// than Verilog.

// Unguarded version
module mkUGQueue1 (SizedQueue#(0, elemType))
  provisos (Bits#(elemType, elemWidth));

  // State
  Reg#(elemType) elem   <- mkConfigRegU;
  Reg#(Bool) elemValid  <- mkConfigReg(False);

  // Wires
  PulseWire deqFire <- mkPulseWire;
  PulseWire enqFire <- mkPulseWire;

  // Update rule
  rule update;
    if (deqFire) elemValid <= False;
    else if (enqFire) elemValid <= True;
  endrule

  // Methods
  method Action deq;
    deqFire.send;
  endmethod

  method Action enq(elemType x);
    enqFire.send;
    elem <= x;
  endmethod

  method elemType dataOut = elem;
  method Bool notFull = !elemValid;
  method Bool notEmpty = elemValid;
  method Bool canDeq = elemValid;
  method Bool canPeek = elemValid;
  method Bool spaceFor(Integer n) =
    error ("Queue.spaceFor() not implemented");
endmodule

// Guarded version
module mkQueue1 (SizedQueue#(0, elemType))
  provisos (Bits#(elemType, elemWidth));

  // State
  SizedQueue#(0, elemType) q <- mkUGQueue1;

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

// =============================
// Implementation 3: Sized Queue
// =============================

// Similar to Bluespec's mkSizedFIFOF but introduces a one-cycle delay
// between dequeuing an element and obtaining the dequeued element.
// This permits an implementation using a buffered (2-cycle latent)
// block RAM and "don't care" read-during-write semantics, enabling
// high clock frequencies.  In addition, the initial contents of the
// queue can be specified.

// When "deq" is invoked, the dequeued item becomes available on the
// "dataOut" bus on the next clock cycle.  Note that "deq" will not
// fire if the queue was empty on the previous cycle -- this condition
// is captured by a guard on the "deq" method and also by the "canDeq"
// method: "canDeq" and "notEmpty" are not equivalent.

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

// ===========================================
// Implementation 4: Sized Queue with Prefetch
// ===========================================

// Unlike a SizedQueue, this has no dequeue latency but costs two
// extra registers and a mux.

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
