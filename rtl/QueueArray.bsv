// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package QueueArray;

// =============================================================================
// Imports
// =============================================================================

import BlockRam     :: *;
import Queue        :: *;
import Util         :: *;
import DReg         :: *;
import ConfigReg    :: *;

// =============================================================================
// Interface
// =============================================================================

// The QueueArray data structure is parameterised by the number of
// queues in the array and the max size of each queue.

interface QueueArray#(numeric type logNumQueues,
                      numeric type logQueueSize,
                      type elemType);
  // Guard on the enq method
  method Bool canEnq;

  // Put an item into a specified queue
  method Action enq(Bit#(logNumQueues) index, elemType item);

  // Try to dequeue an item from the specified queue
  method Action tryDeq(Bit#(logNumQueues) index);

  // The following two methods may be used on cycle after call to tryDeq
  // NOTE: must not call tryDeq and doDeq in same cycle
  method Bool canDeq;
  method Action doDeq;

  // Valid on the 2nd cycle after call to tryDeq
  method elemType itemOut;
endinterface

// =============================================================================
// Implementation
// =============================================================================

// Structure for holding enqueue requests internally
typedef struct {
  // Which Queue to write to?
  Bit#(logNumQueues) index;
  // Value to write
  elemType item;
} EnqReq#(numeric type logNumQueues, type elemType) deriving (Bits);

module mkQueueArray (QueueArray#(logNumQueues, logQueueSize, elemType))
  provisos (

    Bits#(elemType, elemTypeSize)
  );

  // Block RAM storing front pointers
  BlockRamOpts ptrOpts = defaultBlockRamOpts;
  ptrOpts.readDuringWrite = DontCare;
  ptrOpts.registerDataOut = False;
  BlockRamTrue#(Bit#(logNumQueues), Bit#(logQueueSize))
    ramFront <- mkBlockRamTrueOpts(ptrOpts);

  // Block RAM storing back pointers
  BlockRamTrue#(Bit#(logNumQueues), Bit#(logQueueSize))
    ramBack <- mkBlockRamTrueOpts(ptrOpts);

  // Block RAM storing queue data
  BlockRamOpts dataOpts = defaultBlockRamOpts;
  dataOpts.readDuringWrite = DontCare;
  dataOpts.registerDataOut = False;
  BlockRamTrue#(Bit#(TAdd#(logNumQueues, logQueueSize)), elemType)
    ramData <- mkBlockRamTrueOpts(dataOpts);

  // State of enqueue state machine
  Reg#(Bit#(1)) enqState <- mkConfigReg(0);

  // Enqueue requests
  Queue#(EnqReq#(logNumQueues, elemType)) enqReqs <- mkUGQueue;

  // Dequeue pipeline state
  PulseWire deqStage1Go <- mkPulseWire;
  Wire#(Bit#(logNumQueues)) deqIndexWire <- mkDWire(?);
  Reg#(Bit#(logNumQueues)) deqIndex2 <- mkConfigRegU;
  Reg#(Bool) deqStage2Go <- mkDReg(False);
  Wire#(Bool) canDeqWire <- mkDWire(False);
  PulseWire doDeqWire <- mkPulseWire;

  // Rules
  // =====

  rule enqueue (enqReqs.canDeq);
    let req = enqReqs.dataOut;
    if (enqState == 0) begin
      // Avoid ramFront R/W conflict with deqStage2b
      Bool stall = doDeqWire && req.index == deqIndex2;
      if (! stall) begin
        ramFront.putA(False, req.index, ?);
        ramBack.putA(False, req.index, ?);
        enqState <= 1;
      end
    end else if (enqState == 1) begin
      // Avoid ramBack R/W conflict with deqStage1
      Bool stall = deqStage1Go && req.index == deqIndexWire;
      if (stall)
        enqState <= 0;
      else if (ramBack.dataOutA+1 == ramFront.dataOutA)
        enqState <= 0;
      else begin
        ramData.putA(True, {req.index, ramBack.dataOutA}, req.item);
        ramBack.putA(True, req.index, ramBack.dataOutA+1);
        enqState <= 0;
        enqReqs.deq;
      end
    end
  endrule

  (* mutually_exclusive = "deqStage1, deqStage2b" *)
  rule deqStage1 (deqStage1Go);
    ramFront.putB(False, deqIndexWire, ?);
    ramBack.putB(False, deqIndexWire, ?);
    deqIndex2 <= deqIndexWire;
    deqStage2Go <= True;
  endrule

  rule deqStage2a (deqStage2Go);
    ramData.putB(False, {deqIndex2, ramFront.dataOutB}, ?);
    if (ramFront.dataOutB != ramBack.dataOutB) begin
      canDeqWire <= True;
    end
  endrule

  rule deqStage2b (deqStage2Go);
    if (doDeqWire) begin
      ramFront.putB(True, deqIndex2, ramFront.dataOutB+1);
    end
  endrule

  // Methods
  // =======

  // Guard on the enq method
  method Bool canEnq = enqReqs.notFull;

  // Put an item into a specified queue
  method Action enq(Bit#(logNumQueues) index, elemType item);
    enqReqs.enq(EnqReq { index: index, item: item });
  endmethod

  // Try to dequeue an item from the specified queue
  method Action tryDeq(Bit#(logNumQueues) index);
    deqStage1Go.send;
    deqIndexWire <= index;
  endmethod

  // The following two methods may be used on  cycle after call to tryDeq
  method Bool canDeq = canDeqWire;
  method Action doDeq;
    doDeqWire.send;
  endmethod

  // Valid on the 2nd cycle after call to tryDeq
  method elemType itemOut = ramData.dataOutB;

endmodule

endpackage
// typedef enum { WAIT, PROCESS, REPLY } QueueArrayState deriving (Bits, Eq);
//
// module mkQueueArray (QueueArray#(logNumQueues, logQueueSize, elemType))
//   provisos (
//     Bits#(elemType, elemBits),
//     Add#(0, TExp#(logNumQueues), numQueues),
//     Add#(1, __maybeZero, numQueues)
//   );
//
//   Vector#(numQueues, SizedQueue#(logQueueSize, elemType)) queues;
//   for (Integer i = 0; i < valueOf(numQueues); i = i+1)
//     queues[i] <- mkSizedQueue();
//
//   Reg#(Bool) valueBufValid <- mkReg(False);
//   Reg#(UInt#(logNumQueues)) valueBufQIdx <- mkReg(0);
//   Reg#(QueueArrayState) state <- mkReg(WAIT);
//
//   // In WAIT; we get a request. this consists of a queue index. Move to PROCESS
//   // in PROCESS, we make the request to the required queue
//   // in REPLY, we drive the result to the consumer _if_ they deq'd
//
//   rule sm_1 (state == PROCESS);
//     state <= REPLY;
//   endrule
//
//   rule sm_2 (state == REPLY);
//     state <= WAIT;
//   endrule
//
//   // Guard on the enq method
//   function Bool canEnqFn(SizedQueue#(logQueueSize, elemType) q) = q.notFull;
//   method Bool canEnq = fold(\&& , map(canEnqFn, queues));
//
//   // Put an item into a specified queue
//   method Action enq(Bit#(logNumQueues) index, elemType item);
//     queues[index].enq(item);
//   endmethod
//
//   // Try to dequeue an item from the specified queue
//   method Action tryDeq(Bit#(logNumQueues) index) if (state == WAIT);
//     // queues[index].deq;
//     UInt#(logNumQueues) idx_i = unpack(index);
//     valueBufQIdx <= idx_i;
//     state <= PROCESS;
//   endmethod
//
//   // The following two methods may be used on cycle after call to tryDeq
//   // NOTE: must not call tryDeq and doDeq in same cycle
//   method Bool canDeq if (state == PROCESS);
//     return queues[valueBufQIdx].canDeq;
//   endmethod
//
//   method Action doDeq() if (state == PROCESS);
//    queues[valueBufQIdx].deq;
//   endmethod
//
//   // Valid on the 2nd cycle after call to tryDeq
//   method elemType itemOut if (state == REPLY) = queues[valueBufQIdx].dataOut;
//
// endmodule
//
//
//
// `ifdef StratixV
// module mkQueueArray(QueueArray#(logNumQueues, logQueueSize, elemType));
//  QueueArray#(logNumQueues, logQueueSize, elemType) qa <- mkQueueArrays5();
//  return qa;
// endmodule
// `endif
//
// `ifdef Stratix10
// module mkQueueArray(QueueArray#(logNumQueues, logQueueSize, elemType))
//   provisos ( Bits#(elemType, elemBits), Add#(1, a__, TExp#(logNumQueues)) );
//   QueueArray#(logNumQueues, logQueueSize, elemType) qa <- mkQueueArrays5();
//   return qa;
// endmodule
// `endif
//
//
// endpackage
