// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package QueueArray;

// =============================================================================
// Imports
// =============================================================================

import BlockRam     :: *;
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

module mkQueueArray (QueueArray#(logNumQueues, logQueueSize, elemType))
  provisos (Bits#(elemType, elemTypeSize));

  // Block RAM storing front pointers
  BlockRamOpts ptrOpts = defaultBlockRamOpts;
  ptrOpts.readDuringWrite = OldData;
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

  // State
  PulseWire enqStage1Go <- mkPulseWire;
  Wire#(Bit#(logNumQueues)) enqIndexWire <- mkDWire(?);
  Wire#(elemType) enqItemWire <- mkDWire(?);
  Reg#(Bit#(logNumQueues)) enqIndex2 <- mkConfigRegU;
  Reg#(elemType) enqItem2 <- mkConfigRegU;
  Reg#(Bool) enqStage2Go <- mkDReg(False);

  PulseWire deqStage1Go <- mkPulseWire;
  Wire#(Bit#(logNumQueues)) deqIndexWire <- mkDWire(?);
  Reg#(Bit#(logNumQueues)) deqIndex2 <- mkConfigRegU;
  Reg#(Bool) deqStage2Go <- mkDReg(False);
  Wire#(Bool) canDeqWire <- mkDWire(False);
  PulseWire doDeqWire <- mkPulseWire;

  // Rules
  // =====

  (* mutually_exclusive = "enqStage1, enqStage2" *)
  rule enqStage1 (enqStage1Go);
    ramFront.putA(False, enqIndexWire, ?);
    ramBack.putA(False, enqIndexWire, ?);
    enqIndex2 <= enqIndexWire;
    enqItem2 <= enqItemWire;
    enqStage2Go <= True;
  endrule

  rule enqStage2 (enqStage2Go);
    if (ramFront.dataOutA+1 == ramBack.dataOutA) begin
      ramFront.putA(False, enqIndex2, ?);
      ramBack.putA(False, enqIndex2, ?);
      enqStage2Go <= True;
    end else begin
      ramData.putA(True, {enqIndex2, ramBack.dataOutB}, enqItem2);
      ramBack.putA(True, enqIndex2, ramBack.dataOutB+1);
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
    if (ramFront.dataOutB != ramBack.dataOutB)
      canDeqWire <= True;
  endrule

  rule deqStage2b (deqStage2Go);
    if (doDeqWire) begin
      ramFront.putB(True, deqIndex2, ramFront.dataOutB+1);
    end
  endrule

  // Methods
  // =======

  // Guard on the enq method
  method Bool canEnq = !enqStage2Go;

  // Put an item into a specified queue
  method Action enq(Bit#(logNumQueues) index, elemType item);
    enqStage1Go.send;
    enqIndexWire <= index;
    enqItemWire <= item;
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
