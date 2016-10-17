package TestArrayOfQueue;

// ============================================================================
// Imports
// ============================================================================

import FIFOF        :: *;
import Vector       :: *;
import Interface    :: *;
import ConfigReg    :: *;
import Queue        :: *;
import ArrayOfQueue :: *;
import Util         :: *;
import DReg         :: *;

// Interface to C functions
import "BDPI" function ActionValue#(Bit#(32)) getUInt32();
import "BDPI" function ActionValue#(Bit#(8)) getChar();

// ============================================================================
// Constants
// ============================================================================

// Operator encoding
Bit#(8) opENQ      = 73;  // 'I': insert (enqueue) item
Bit#(8) opEND      = 69;  // 'E': end of operation stream
Bit#(8) opDEQ      = 82;  // 'R': remove (dequeue) item
Bit#(8) opDEQDELAY = 68;  // 'D': delay in dequeue stream
Bit#(8) opENQDELAY = 67;  // 'C': delay in enqueue stream

// ============================================================================
// Types
// ============================================================================

// Queue request
// (Fields read straight from stdin are all 32 bits)
typedef struct {
  Bit#(8)  op;
  Bit#(32) index;
  Bit#(32) item;
  Bit#(32) delay;
} QueueReq deriving (Bits);

// ============================================================================
// Implementation
// ============================================================================

`define NumQueues 2

module testArrayOfQueue ();

  // Create array
  ArrayOfQueue#(`NumQueues, 2, Bit#(32)) array <- mkArrayOfQueue;

  // Constants
  // ---------

  Integer maxQueueId = 2 ** `NumQueues - 1;

  // Read raw requests from stdin and enqueue to queueReqs
  // -----------------------------------------------------

  // Raw requests from stdin
  FIFOF#(QueueReq) enqReqs <- mkUGSizedFIFOF(64);
  FIFOF#(QueueReq) deqReqs <- mkUGSizedFIFOF(64);

  // Goes high when the opEND operation is encountered
  Reg#(Bool) allReqsGathered <- mkReg(False);

  rule gatherRequests (! allReqsGathered && enqReqs.notFull
                                         && deqReqs.notFull);
    QueueReq req = ?;
    let op <- getChar();
    req.op = op;
    if (op == opEND)
      allReqsGathered <= True;
    else if (op == opENQDELAY || op == opDEQDELAY) begin
      let n <- getUInt32();
      req.delay = n;
      if (op == opENQDELAY)
        enqReqs.enq(req);
      else
        deqReqs.enq(req);
    end else if (op == opENQ || op == opDEQ) begin
      let index <- getUInt32();
      myAssert(index <= fromInteger(maxQueueId),
                "TestArrayOfQueue.bsv: queue id too large");
      req.index = index;
      if (op == opENQ) begin
        let item <- getUInt32();
        req.item = item;
        enqReqs.enq(req);
      end else
        deqReqs.enq(req);
    end
  endrule

  // Issue enq requests
  // ------------------

  Reg#(Bit#(4)) retries <- mkConfigReg(0);

  rule issueEnqRequests(enqReqs.notEmpty && enqReqs.first.op == opENQ);
    let req = enqReqs.first;
    if (array.canEnq) begin
      $display("I %d %d", req.index, req.item);
      array.enq(truncate(req.index), req.item);
      enqReqs.deq;
      retries <= 0;
    end else begin
      if (retries < 5)
        retries <= retries + 1;
      else begin
        enqReqs.deq;
        retries <= 0;
      end
    end
  endrule

  // Issue deq requests
  // ------------------

  // Pipeline registers
  Reg#(Bool)     deqFire2 <- mkDReg(False);
  Reg#(Bit#(32)) deqReg2  <- mkRegU;
  Reg#(Bool)     deqFire3 <- mkDReg(False);
  Reg#(Bit#(32)) deqReg3  <- mkRegU;
  Reg#(Bool)     deqFire4 <- mkDReg(False);
  Reg#(Bit#(32)) deqReg4  <- mkRegU;
  Reg#(Bool)     deqFire5 <- mkDReg(False);
  Reg#(Bit#(32)) deqReg5  <- mkRegU;

  rule issueDeqRequests1(deqReqs.notEmpty &&
                           deqReqs.first.op == opDEQ);
    let req = deqReqs.first;
    if (array.canTryDeq(truncate(req.index))) begin
      array.tryDeq(truncate(req.index));
      deqReg2 <= req.index;
      deqFire2 <= True;
      deqReqs.deq;
    end
  endrule

  rule issueDeqRequests2 (deqFire2);
    deqReg3 <= deqReg2;
    deqFire3 <= True;
  endrule

  rule issueDeqRequests3 (deqFire3);
    if (array.canDeq) begin
      array.deq;
      deqReg4 <= deqReg3;
      deqFire4 <= True;
   end
  endrule

  rule issueDeqRequests4 (deqFire4);
    deqReg5 <= deqReg4;
    deqFire5 <= True;
  endrule

  rule issueDeqRequests5 (deqFire5);
    $display("R %d %d", deqReg5, array.itemOut);
  endrule

  // Handle delay requests
  // ---------------------

  Reg#(Bit#(32)) delayCounter1 <- mkReg(0);
  Reg#(Bit#(32)) delayCounter2 <- mkReg(0);

  rule delay1 (deqReqs.notEmpty && deqReqs.first.op == opDEQDELAY);
    if (delayCounter1 == deqReqs.first.delay) begin
      delayCounter1 <= 0;
      deqReqs.deq;
    end else begin
      delayCounter1 <= delayCounter1+1;
    end
  endrule

  rule delay2 (enqReqs.notEmpty && enqReqs.first.op == opENQDELAY);
    if (delayCounter2 == enqReqs.first.delay) begin
      delayCounter2 <= 0;
      enqReqs.deq;
    end else begin
      delayCounter2 <= delayCounter2+1;
    end
  endrule

  // Termination condition
  // ---------------------

  rule terminate (allReqsGathered && 
                    !deqReqs.notEmpty &&
                    !enqReqs.notEmpty &&
                      !(deqFire2 || deqFire3 ||
                        deqFire4 || deqFire5));
    $finish(0);
  endrule
endmodule

endpackage
