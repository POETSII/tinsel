// Copyright (c) Matthew Naylor

package ArrayOfQueue;

// =============================================================================
// Imports
// =============================================================================

import BlockRam  :: *;
import Util      :: *;
import DReg      :: *;
import ConfigReg :: *;

// =============================================================================
// Interface
// =============================================================================

// The ArrayOfQueue data structure is parameterised by the number of
// queues in the array, the max size of each queue, and the type of
// each queue item.

interface ArrayOfQueue#(numeric type logNumQueues,
                        numeric type logQueueSize,
                        type itemType);
  // Enqueue an item into the queue at the given array index
  method Action enq(Bit#(logNumQueues) index, itemType item);
  // Explicit guard on above method
  method Bool canEnq;
  // Was an item sucessfully enqueued?
  // (Valid on 2nd cycle after call to "enq")
  method Bool didEnq;
  // Prepare to dequeue an item from the queue at the given array index
  method Action tryDeq(Bit#(logNumQueues) index);
  // Can an item be sucessfully dequeued?
  // (Valid on 2nd cycle after call to "tryDeq")
  method Bool canDeq;
  // Dequeue the item
  // (Must only be called on 2nd cycle after call to "tryDeq")
  method Action deq;
  // The item dequeued
  // (Valid on 4th cycle after call to "tryDeq")
  method itemType itemOut;
endinterface

// =============================================================================
// Types
// =============================================================================

// Record containing a queue's meta data
typedef struct {
  Bit#(n) front;
  Bit#(n) back;
  Bit#(TAdd#(n, 1)) length;
} QueueStatus#(numeric type n) deriving (Bits);

// =============================================================================
// Implementation
// =============================================================================

module mkArrayOfQueue (ArrayOfQueue#(logNumQueues, logQueueSize, itemType))
  provisos (Log#(TExp#(logQueueSize), logQueueSize),
            Bits#(itemType, itemTypeWidth));

  // Queue meta data
  BlockRamOpts metaDataOpts = defaultBlockRamOpts;
  metaDataOpts.readDuringWrite = OldData;
  BlockRamTrueMixed#(
    // Read/write port
    Bit#(logNumQueues), QueueStatus#(logQueueSize),
    // Read-only port
    Bit#(logNumQueues), QueueStatus#(logQueueSize)) metaData <-
      mkBlockRamTrueMixedOpts(metaDataOpts);

  // Queue contents
  BlockRam#(Bit#(TAdd#(logNumQueues, logQueueSize)), itemType) contents <-
    mkBlockRam;

  // Max queue size
  Integer queueCapacity = 2 ** valueof(logQueueSize);

  // Dequeue rules
  // =============

  // Wire used to trigger a new dequeue request
  PulseWire doTryDeq <- mkPulseWire;
  PulseWire doDeq <- mkPulseWire;

  // Index of queue to dequeue
  Wire#(Bit#(logNumQueues)) deqIndexWire <- mkDWire(?);

  // Pipeline registers
  Reg#(Bit#(logNumQueues)) deqIndexReg  <- mkConfigRegU;
  Reg#(Bit#(logNumQueues)) deqIndexReg1 <- mkConfigRegU;
  Reg#(Bit#(logNumQueues)) deqIndexReg2 <- mkConfigRegU;

  // Is queue empty?
  Bool empty = metaData.dataOutB.length == 0;

  // Signal enqueue success
  Wire#(Bool) didEnqWire <- mkDWire(False);

  rule deqStage (doDeq);
    // Obtain queue status and update front-pointer & length
    QueueStatus#(logQueueSize) status = metaData.dataOutB;
    let newStatus = status;
    newStatus.front = status.front + 1;
    newStatus.length = status.length - 1;
    // Commit new data if queue not empty
    if (!empty) begin
      // Update meta data
      metaData.putA(True, deqIndexReg2, newStatus);
      // Fetch data
      contents.read({deqIndexReg, status.front});
      // Announce success
      didEnqWire <= True;
    end
  endrule

  rule deqSave;
    deqIndexReg  <= deqIndexWire;
    deqIndexReg1 <= deqIndexReg;
    deqIndexReg2 <= deqIndexReg1;
  endrule

  // Enqueue rules
  // =============

  // Wire used to trigger a new enqueue request
  PulseWire doEnq <- mkPulseWire;

  // Wire/register holding queue index to enqueue into
  Wire#(Bit#(logNumQueues)) enqIndexWire <- mkDWire(?);
  Reg#(Bit#(logNumQueues))  enqIndexReg  <- mkConfigRegU;

  // Item to enqueue
  Reg#(itemType) enqItemReg <- mkConfigRegU;

  // State of enqueue unit
  Reg#(Bit#(2)) enqStage <- mkConfigReg(0);

  // Dequeue rules may lead to abortion of enqueue unit
  Bool abort0 = doDeq || doTryDeq && deqIndexWire == enqIndexWire;
  Bool abort1 = doDeq && enqIndexReg == deqIndexReg2;
  Bool abort2 = doTryDeq && deqIndexWire == enqIndexReg;

  rule enqStage0 (enqStage == 0 && doEnq && !abort0);
    metaData.putA(False, enqIndexWire, ?);
    enqStage <= 1;
  endrule

  rule enqStage1 (enqStage == 1);
    enqStage <= (abort1 || abort2) ? 0 : 2;
  endrule

  rule enqStage2a (enqStage == 2 && !doDeq && !abort2);
    // Obtain queue status and update back-pointer & length
    QueueStatus#(logQueueSize) status = metaData.dataOutA;
    let newStatus = status;
    newStatus.back = status.back+1;
    newStatus.length = status.length+1;
    // Commit new data unless queue full
    if (status.length != fromInteger(queueCapacity)) begin
      // Update meta data
      metaData.putA(True, enqIndexReg, newStatus);
      // Write new item to memory
      contents.write({enqIndexReg, status.back}, enqItemReg);
      // Signal success
      didEnqWire <= True;
    end
    // Move back to initial stage
    enqStage <= 0;
  endrule

  rule enqStage2b (enqStage == 2 && (doDeq || abort2));
    enqStage <= 0;
  endrule

  rule enqSave;
    enqIndexReg <= enqIndexWire;
  endrule

  // Methods
  // =======

  method Action tryDeq(Bit#(logNumQueues) index);
    metaData.putB(False, index, ?);
    deqIndexWire <= index;
    doTryDeq.send;
  endmethod

  method Action enq(Bit#(logNumQueues) index, itemType item);
    enqIndexWire <= index;
    enqItemReg <= item;
    doEnq.send;
  endmethod

  method Action deq;
    doDeq.send;
  endmethod

  method Bool canEnq = enqStage == 0;
  method Bool didEnq = didEnqWire;
  method Bool canDeq = !empty;
  method itemType itemOut = contents.dataOut;

endmodule

endpackage
