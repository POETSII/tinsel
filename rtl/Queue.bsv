// Copyright (c) Matthew Naylor

package Queue;

// This module defines a "Queue", similar to Bluespec's sized-FIFO,
// but introduces a one-cycle delay between dequeuing an element and
// obtaining the dequeued element.  This permits an implementation
// using a buffered (2-cycle latent) block RAM and "don't care"
// read-during-write semantics, enabling high clock frequencies.  In
// addition, the initial contents of the queue can be specified.

// =======
// Imports
// =======

import BlockRam  :: *;

// For BlueCheck test bench:
/*
import StmtFSM   :: *;
import BlueCheck :: *;
import FIFOF     :: *;
import Clocks    :: *;
*/

// =========
// Interface
// =========

// When "deq" is invoked, the dequeued item becomes available on the
// "dataOut" bus on the next clock cycle.  Note that "deq" will not
// fire if the queue was empty on the previous cycle -- this condition
// is captured by a guard on the "deq" method and also by the "canDeq"
// method: "canDeq" and "notEmpty" are not equivalent.

interface Queue#(numeric type logSize, type elemType);
  method Action enq(elemType x);
  method Action deq;
  method elemType dataOut;
  method Bool notFull;
  method Bool notEmpty;
  method Bool canDeq;
  method Bool spaceFor(Integer n);
endinterface

// =======
// Options
// =======

typedef struct {
  Integer size;
  Maybe#(String) file;
} QueueInit;

// ===============
// Implementations
// ===============

module mkQueue (Queue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));
  QueueInit init;
  init.size = 0;
  init.file = Invalid;
  let q <- mkQueueInit(init);
  return q;
endmodule

module mkQueueInit#(QueueInit init) (Queue#(logSize, elemType))
  provisos (Bits#(elemType, elemWidth));

  // Max length of queue
  Integer maxLength = 2 ** valueOf(logSize);

  // Block RAM to hold contents of queue
  BlockRamOpts ramOpts = defaultBlockRamOpts;
  ramOpts.registerDataOut = True;
  ramOpts.readDuringWrite = DontCare;
  ramOpts.initFile = init.file;
  BlockRam#(Bit#(logSize), elemType) ram <- mkBlockRamOpts(ramOpts);

  // State
  Reg#(Bit#(logSize)) front <- mkReg(0);
  Reg#(Bit#(logSize)) back <- mkReg(fromInteger(init.size % maxLength));
  Reg#(Bit#(TAdd#(logSize, 1))) length <- mkReg(fromInteger(init.size));
  Reg#(Bool) empty <- mkReg(init.size == 0);
  Reg#(Bool) full <- mkReg(init.size == maxLength);
  Reg#(Bool) deqEnable <- mkReg(False);

  // Wires
  PulseWire doDeq <- mkPulseWire;
  RWire#(elemType) doEnq <- mkRWire;

  // Rules
  rule update;
    Bit#(logSize) incFront = front+1;
    Bit#(logSize) newFront = doDeq ? incFront : front;
    ram.read(newFront);
    front <= newFront;
    Bool becomingEmpty = False;
    case (doEnq.wget) matches
      tagged Invalid:
        if (doDeq) begin
          full <= False;
          if (length == 1) begin
            empty <= True;
            becomingEmpty = True;
          end
          length <= length - 1;
        end
      tagged Valid .x: begin
        ram.write(back, x);
        back <= back+1;
        if (!doDeq) begin
          full <= length == fromInteger(maxLength-1);
          empty <= False;
          length <= length + 1;
        end
      end
    endcase
    if (becomingEmpty) deqEnable <= False;
    else deqEnable <= !empty;
  endrule

  // Methods
  method Action deq if (deqEnable);
    doDeq.send;
  endmethod

  method Action enq(elemType x) if (!full);
    doEnq.wset(x);
  endmethod

  method elemType dataOut = ram.dataOut;

  method Bool notFull = !full;

  method Bool notEmpty = !empty;

  method Bool canDeq = deqEnable;

  method Bool spaceFor(Integer n) = length < fromInteger(maxLength-n);
endmodule

/*
// ==========
// Test bench
// ==========

// A BlueCheck test bench that asserts an equivalance between a
// Bluespec sized-FIFO and a queue.  It's a resettable specification,
// allowing BlueCheck's iterative deepening and shrinking strategies.

module [Specification] queueSpec#(Reset r) ();
  // Specification instance (a 4-element sized-FIFO)
  FIFOF#(Bit#(8)) fifo <- mkSizedFIFOF(4, reset_by r);

  // Was fifo empty on previous cycle?
  Reg#(Bool) wasNotEmpty <- mkReg(True);
  rule updateWasNotEmpty;
    wasNotEmpty <= fifo.notEmpty;
  endrule

  // Implementation instance (a 4-element queue)
  Queue#(2, Bit#(8)) q <- mkQueue(reset_by r);

  // Obtain function for making assertions
  Ensure ensure <- getEnsure;

  // Check that when an item is dequeued, it is the correct item
  Stmt checkFirst = seq
    q.deq;
    action fifo.deq; ensure(q.dataOut == fifo.first); endaction
  endseq;

  // Properties
  equivf(2, "enq", fifo.enq, q.enq);
  equiv("deq", fifo.deq, q.deq);
  prop("checkFirst1", stmtWhen(q.canDeq, checkFirst));
  prop("checkFirst2", stmtWhen(fifo.notEmpty && wasNotEmpty, checkFirst));
  equiv("notFull", fifo.notFull, q.notFull);
  equiv("notEmpty", fifo.notEmpty, q.notEmpty);
  parallel(list("enq", "deq"));
endmodule

// The test bench
module queueTest ();
  Clock clk <- exposeCurrentClock;
  MakeResetIfc r <- mkReset(0, True, clk);
  blueCheckID(queueSpec(r.new_rst), r);
endmodule
*/

endpackage
