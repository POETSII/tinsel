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
  provisos (Bits#(elemType, elemTypeSize));

  // Block RAM storing front pointers
  BlockRamOpts ptrOpts = defaultBlockRamOpts;
  ptrOpts.readDuringWrite = DontCare;
  ptrOpts.registerDataOut = False;
  BlockRamTrue#(Bit#(logNumQueues), Bit#(logQueueSize))
    ramFront <- mkBlockRamTrueMixedOpts(ptrOpts);

  // Block RAM storing back pointers
  BlockRamTrue#(Bit#(logNumQueues), Bit#(logQueueSize))
    ramBack <- mkBlockRamTrueMixedOpts(ptrOpts);

  // Block RAM storing queue data
  BlockRamOpts dataOpts = defaultBlockRamOpts;
  dataOpts.readDuringWrite = DontCare;
  dataOpts.registerDataOut = False;
  BlockRamTrue#(Bit#(TAdd#(logNumQueues, logQueueSize)), elemType)
    ramData <- mkBlockRamTrueMixedOpts(dataOpts);

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
