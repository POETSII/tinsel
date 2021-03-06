// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package Mailbox;

// =============================================================================
// Overview: scratchpad-based mailbox
// =============================================================================
//
// A mailbox serves some number of cores, defined by LogCoresPerMailbox,
// and allows messages to be sent between any two threads running on
// these cores.  Mailboxes can be connected together to form a network,
// allowing messages to be sent between any two threads on the same
// network.  Threads read and write messages via a memory-mapped
// scratchpad within a mailbox.  We refer to an aligned message-sized
// block in the scracthpad as a "message slot".
//
//                                                     
//                 +----------------------------------+
//                 |             Mailbox              |  
//                 |                                  | 
//                 |       +------------+             |  
//              <--------->| Scratchpad |             |  
//                 |       +------------+             |  
//                 |                                  |
//                 |       +---------------+          | 
//              <--------->| Transmit Unit |          |
//    Group        |       +---------------+          |<----- Flit in
//     of          |                                  | 
//    cores        |       +----------------+         |-----> Flit out
//              <--------->| Receive Unit   |         |
//                 |       +----------------+         |
//                 |                                  |
//                 |       +-----------+              | 
//              <--------->| Free Unit |              | 
//                 |       +-----------+              |
//                 |                                  | 
//                 +----------------------------------+
//                                              
//
// Scratchpad
// ----------
// 
// The scratchpad is a mixed-width dual-port block RAM with a 32-bit bus
// on the core side and a flit-sized bus on the network side (a
// message is comprised of several flits).  The size of the mailbox
// is in 32-bit words is
//
//   2^LogMsgSlotsPerMailbox * 2^LogMaxFlitsPerMsg * 2^LogWordsPerFlit.
//
// The first ThreadsPerMailbox message slots are reserved for message
// sending (one send slot per thread).  All other slots are used as
// a receive buffer.
// 
// One attraction of using a unified scratchpad for send and receive
// is that a message can be forwarded (recieved and sent) without
// serialising it through the 32-bit core.
// 
// Transmit Unit
// -------------
//
// The Transmit Unit accepts requests for a message slot in the
// scratchpad to be sent to a given destination.  When the Transmit
// Unit eventually sends the message, it will produce a response,
// notifying the thread that made the request.
//
// Receive Unit
// ------------
// 
// The Receive Unit contains a queue for each thread served by the
// mailbox.  Each queue holds message-pointers into the scratchpad.
// When a message arrives, it is written into the next free message
// slot in the scratchpad, and pointer to that slot will be inserted
// into one or more queues as indicated by the destination address(es)
// of the message (which can be a unicast or multicast address).
//
// In addition, a reference count will be maintained for each message
// slot indicating the total number of pointers to that slot in all
// the queues.  When a message arrives, the reference count will be
// incremented accordingly.  A linked list of unused message slots is
// maintained in the reference count memory.
//
// If any thread's queue is full, backpressure is applied to the network.
//
// Free Unit
// ---------
//
// When a thread indicates that it no longer needs the contents of a
// message slot, it will issue a "free" request.  This request will
// cause the slot's reference count to be decremented.  When the
// reference count reaches 0, the the hardware can reclaim that
// message slot for a future incoming message.  This is achieved by
// pushing a pointer to the slot into the linked list of unused
// message slots, maintained in the reference count memory.

// =============================================================================
// Imports
// =============================================================================

import Vector       :: *;
import Queue        :: *;
import Interface    :: *;
import BlockRam     :: *;
import ConfigReg    :: *;
import Util         :: *;
import Globals      :: *;
import DReg         :: *;
import FlitMerger   :: *;
import QueueArray   :: *;

// =============================================================================
// Types
// =============================================================================

// A single Mailbox may be shared my several multi-threaded cores
typedef Bit#(`LogThreadsPerMailbox) MailboxClientId;

// 32-bit word address in scratchpad memory
typedef Bit#(`LogWordsPerMailbox) MailboxWordAddr;

// Flit address in scratchpad memory
typedef Bit#(`LogFlitsPerMailbox) MailboxFlitAddr;

// Message address in scratchpad memory
typedef Bit#(`LogMsgsPerMailbox) MailboxMsgAddr;

// Reference count
typedef Bit#(TAdd#(`LogThreadsPerMailbox, 1)) RefCount;

// Scratchpad request
typedef struct {
  // Source of request
  MailboxClientId id;
  // Operation
  Bool isStore;
  // Thread-local word address
  MailboxWordAddr wordAddr;
  // For store operation: data to write
  Bit#(32) data;
  // For store operation: byte enables
  Bit#(4) byteEn;
} ScratchpadReq deriving (Bits, FShow);

// Scratchpad response
typedef struct {
  // Source of request
  MailboxClientId id;
  // Operation
  Bool isStore;
  // For load operation: data loaded
  Bit#(32) data;
} ScratchpadResp deriving (Bits);

// Transmit unit request
typedef struct {
  // Source of request
  MailboxClientId id;
  // Message address
  MailboxMsgAddr msgIndex;
  // Message length
  MsgLen len;
  // Destination thread
  NetAddr dest;
} TransmitReq deriving (Bits);

// Transmit unit response
typedef struct {
  // Source of request
  MailboxClientId id;
} TransmitResp deriving (Bits);

// Request to receive unit
typedef SleepStatus ReceiveReq;

// Response from receive unit
typedef struct {
  // Pointer to message received, for "receive" command
  MailboxMsgAddr data;
  // Status
  SleepStatus status;
} ReceiveResp deriving (Bits, FShow);

// Request to "free" unit
typedef struct {
  // Source of request
  MailboxClientId id;
  // Message slot to free
  MailboxMsgAddr slot;
} FreeReq deriving (Bits);

// Transmit pipeline token
typedef struct {
  NetAddr dest;
  Bool notFinalFlit;
} TransmitToken deriving (Bits);

// Type of elements residing in the multicast queues
typedef struct {
  // Pointer to message in scratchpad
  MailboxMsgAddr slot;
  // Destination threads
  Bit#(`ThreadsPerCore) dests;
} MulticastBufferEntry deriving (Bits);

// A bit vector of events that can cause a sleeping thread to wake up
// Bit 0: can-send
// Bit 1: can-receive
// Bit 2: idle, vote false
// Bit 3: idle, vote true
typedef Bit#(4) WakeEvent;

// Operation of thread in sleep queue
typedef enum { CanRecvOp, RecvOp, WaitUntilOp } SleepOp
  deriving (Bits, Eq, FShow);

// Status of thread in sleep queue
typedef struct {
  // Id of thread
  ThreadId id;
  // Operation
  SleepOp op;
  // If WaitUntilOp, the event(s) that the thread is waiting for
  WakeEvent wakeEvent;
  // Is there is an incoming message for the thread?
  Bool canRecv;
} SleepStatus deriving (Bits, FShow);

// =============================================================================
// Functions
// =============================================================================

// Convert byte address to message index
function MailboxMsgAddr byteAddrToMsgIndex(Bit#(32) addr);
  MailboxMsgAddr msgAddr = truncate(addr[31:`LogBytesPerMsg]);
  return msgAddr;
endfunction

// Convert message address to byte address
function Bit#(32) msgAddrToByteAddr(MailboxMsgAddr msgAddr);
  Bit#(`LogWordsPerFlit) wordOffset = 0;
  Bit#(`LogMaxFlitsPerMsg) flitOffset = 0;
  return {0, msgAddr, flitOffset, wordOffset, 2'b0};
endfunction

// =============================================================================
// Interface
// =============================================================================

interface Mailbox;
  // Core-side interfaces to scratchpad
  interface In#(ScratchpadReq)    spadReqIn;
  interface BOut#(ScratchpadResp) spadRespOut;
  // Core-side interfaces to transmit unit
  interface In#(TransmitReq)      txReqIn;
  interface BOut#(TransmitResp)   txRespOut;
  // Core-side interfaces to receive unit (one per core)
  interface Vector#(`CoresPerMailbox, ReceiveReqResp) rxReqResp;
  // Core-side interfaces to free unit
  interface In#(FreeReq)          freeReqIn;
  (* always_ready *) method Bit#(1) freeDone;
  // Network-side interface
  interface MailboxNet            net;
  // Initialise send slots (use extra send slot?)
  (* always_ready, always_enabled *)
  method Action initSendSlots(Option#(Bool) useExtraSendSlot);
endinterface

// Combined receive request/response interface
interface ReceiveReqResp;
  interface In#(ReceiveReq)    rxReqIn;
  interface BOut#(ReceiveResp) rxRespOut;
endinterface

interface MailboxNet;
  interface In#(Flit)   flitIn;
  interface BOut#(Flit) flitOut;
endinterface

// =============================================================================
// Implementation
// =============================================================================

(* synthesize *)
module mkMailbox (Mailbox);
  // True dual-port mixed-width scratchpad
  // (One flit-sized port and one word-sized port)
  BlockRamTrueMixedBE#(MailboxFlitAddr, FlitPayload, MailboxWordAddr, Bit#(32))
    scratchpad <- mkBlockRamTrueMixedBE;

  // Request & response ports
  InPort#(ScratchpadReq) spadReqPort  <- mkInPort;
  InPort#(TransmitReq)   txReqPort    <- mkInPort;
  InPort#(FreeReq)       freeReqPort  <- mkInPort;
  InPort#(Flit)          flitInPort   <- mkInPort;
  Vector#(`CoresPerMailbox, InPort#(ReceiveReq)) rxReqPorts <-
    replicateM(mkInPort);

  // Initialise free slots
  // =====================

  // Set of currently-unused message slots
  // By default, the first ThreadsPerMailbox slots are reserved for sending
  // Optionally, the first 2*ThreadsPerMailbox slots are reserved for sending
  SizedQueue#(`LogMsgsPerMailbox, Bit#(`LogMsgsPerMailbox))
    freeSlots <- mkUGSizedQueuePrefetch;

  // Reserve extra send slot?
  Wire#(Option#(Bool)) useExtraSendSlot <- mkBypassWire;

  // State of free slot initialiser
  Reg#(Bit#(1)) freeSlotsInitState <- mkConfigReg(0);

  // Have the free slots been initialised yet?
  Reg#(Bool) freeSlotsInitDone <- mkConfigReg(False);

  // Next slot to insert into free slot queue
  Reg#(Bit#(`LogMsgsPerMailbox)) freeSlotsInitNext <- mkConfigRegU;

  // Wait until config option available, which tells us how
  // many slots to reserve for sending
  rule initFreeSlots0 (freeSlotsInitState == 0);
    if (useExtraSendSlot.valid) begin
      freeSlotsInitNext <= useExtraSendSlot.value ?
        fromInteger(2*`ThreadsPerMailbox) : `ThreadsPerMailbox;
      freeSlotsInitState <= 1;
    end
  endrule

  // Initialise free slots
  rule initFreeSlots1 (!freeSlotsInitDone && freeSlotsInitState == 1);
    freeSlots.enq(freeSlotsInitNext);
    freeSlotsInitNext <= freeSlotsInitNext + 1;
    if (freeSlotsInitNext == fromInteger(2**`LogMsgsPerMailbox - 1))
      freeSlotsInitDone <= True;
  endrule

  // Message access unit
  // ===================

  // There is a conflict between the transmit and receive pipelines:
  // "receive" needs to write a flit to the scratchpad while
  // "transmit" needs to read a flit.  The message access unit
  // resolves this conflict: write takes priorty over read.
  // The read wires must not be written when the write wire is set.

  // Control wires for modifying messages in scratchpad
  Wire#(Bool) flitWriteWire <- mkDWire(False);
  Reg#(Bool) flitWriteReg <- mkRegU;
  Wire#(MailboxFlitAddr) flitReadIndexWire <- mkDWire(0);
  Wire#(MailboxFlitAddr) flitWriteIndexWire <- mkDWire(0);
  Reg#(FlitPayload) flitWriteDataReg <- mkConfigRegU;
  Reg#(MailboxFlitAddr) flitIndexReg <- mkRegU;

  // Use wires to issue flit access in scratchpad
  rule flitAccessUnit;
    flitWriteReg <= flitWriteWire;
    flitIndexReg <= flitReadIndexWire | flitWriteIndexWire;
    scratchpad.putA(
      flitWriteReg,
      flitIndexReg,
      flitWriteDataReg);
  endrule

  // Receive Unit
  // ============

  // Track which flit in a message is currently being received
  Reg#(MsgLen) recvFlitCount <- mkConfigReg(0);

  // RAM containing reference count for every message slot
  BlockRamOpts refCountOpts = defaultBlockRamOpts;
  refCountOpts.registerDataOut = False;
  BlockRamTrueMixed#(MailboxMsgAddr, RefCount, MailboxMsgAddr, RefCount)
    refCount <- mkBlockRamTrueMixedOpts(refCountOpts);

  // Signals for triggering a ref count update
  Reg#(Bool) setRefCount <- mkDReg(False);
  Reg#(RefCount) refCountReg <- mkConfigRegU;
  Reg#(Bit#(`LogMsgsPerMailbox)) refCountSlot <- mkConfigRegU;

  // Multicast buffer
  Vector#(`CoresPerMailbox,
    SizedQueue#(`LogMulticastBufferSize, MulticastBufferEntry))
      mcastBuffer <- replicateM(mkUGSizedQueue);

  rule receive0;
    let flit = flitInPort.value;
    // Determine destination threads
    Bit#(`ThreadsPerMailbox) destThreads = flit.dest.threads;
    // Determine if flit can be received
    Bool canRecv = True;
    for (Integer i = 0; i < `CoresPerMailbox; i=i+1)
      canRecv = canRecv && mcastBuffer[i].notFull;
    canRecv = canRecv && freeSlots.canPeek && freeSlots.canDeq;
    let slot = freeSlots.dataOut;
    // Try to consume an incoming flit
    if (flitInPort.canGet && canRecv) begin
      flitInPort.get;
      // Write flit to next free slot
      flitWriteWire <= True;
      flitWriteIndexWire <= { slot, recvFlitCount };
      flitWriteDataReg <= flit.payload;
      // Is this the final flit of a message?
      if (! flit.notFinalFlit) begin
        recvFlitCount <= 0;
        // Slot is no longer free
        freeSlots.deq;
        // Put pointer to new message into buffer
        Vector#(`CoresPerMailbox, Bit#(`ThreadsPerCore))
          vecDestThreads = unpack(destThreads);
        for (Integer i = 0; i < `CoresPerMailbox; i=i+1) begin
          MulticastBufferEntry mcastEntry =
            MulticastBufferEntry { dests: vecDestThreads[i], slot: slot };
          if (vecDestThreads[i] != 0) mcastBuffer[i].enq(mcastEntry);
        end
        // Set ref count for new slot
        refCountReg <= pack(countOnes(destThreads));
        refCountSlot <= slot;
        setRefCount <= True;
      end else
        recvFlitCount <= recvFlitCount + 1;
    end
  endrule

  rule updateRefCount (setRefCount);
    refCount.putA(True, refCountSlot, refCountReg);
  endrule

  // Populate multicast queues
  // -------------------------

  // Multicast queues (one per thread)
  Vector#(`CoresPerMailbox,
    QueueArray#(`LogThreadsPerCore,
                  `LogMsgPtrQueueSize,
                    Bit#(`LogMsgsPerMailbox))) mcastQueues <-
      replicateM(mkQueueArray);

  for (Integer i = 0; i < `CoresPerMailbox; i=i+1) begin
    // State
    Reg#(Bit#(2)) mcastState <- mkConfigReg(0);
    Reg#(Bit#(`ThreadsPerCore)) mcastDests <- mkConfigRegU;
    Reg#(Bit#(`LogMsgsPerMailbox)) mcastSlot <- mkConfigRegU;
    Reg#(Bit#(`LogThreadsPerCore)) mcastDest <- mkConfigRegU;
  
    // State machine to consume mcastBuffer, and update mcastQueues
    rule mcast0 (mcastState == 0);
      if (mcastBuffer[i].canPeek && mcastBuffer[i].canDeq) begin
        mcastBuffer[i].deq;
        mcastDests <= mcastBuffer[i].dataOut.dests;
        mcastSlot <= mcastBuffer[i].dataOut.slot;
        mcastState <= 1;
      end
    endrule

    rule mcast1 (mcastState == 1);
      mcastDest <= truncate(pack(countZerosLSB(mcastDests)));
      let firstHot = mcastDests & -mcastDests;
      mcastDests <= mcastDests & ~firstHot;
      mcastState <= 2;
    endrule

    rule mcast2 (mcastState == 2);
      if (mcastQueues[i].canEnq) begin
        mcastQueues[i].enq(mcastDest, mcastSlot);
        mcastState <= mcastDests == 0 ? 0 : 1;
      end
    endrule
  end

  // Serve requests to the receive unit
  // ----------------------------------

  // Response buffers for receive unit
  Vector#(`CoresPerMailbox, Queue#(ReceiveResp))
    recvRespQueues <- replicateM(mkUGQueue);

  // Serve requests to the receive unit
  for (Integer i = 0; i < `CoresPerMailbox; i=i+1) begin

    // State machine
    Reg#(Bit#(2)) serveState <- mkConfigReg(0);

    rule serveReceive (rxReqPorts[i].canGet && recvRespQueues[i].notFull);
      // View request
      ReceiveReq req = rxReqPorts[i].value;
      // Prepare response
      ReceiveResp resp;
      resp.status = req;
      resp.status.canRecv = mcastQueues[i].canDeq;
      resp.data = mcastQueues[i].itemOut;
      // State machine
      if (serveState == 0) begin
        mcastQueues[i].tryDeq(req.id);
        serveState <= req.op == RecvOp ? 1 : 2;
      end else if (serveState == 1) begin
        myAssert(mcastQueues[i].canDeq, "Receiving when not ready");
        mcastQueues[i].doDeq;
        serveState <= 2;
      end else if (serveState == 2) begin
        recvRespQueues[i].enq(resp);
        rxReqPorts[i].get;
        serveState <= 0;
      end
    endrule

  end

  // Transmit Unit
  // =============

  // Transmit flit-buffer
  SizedQueue#(`LogTransmitBufferLen, Flit) transmitBuffer <-
    mkUGShiftQueue(QueueOptFmax);

  // Track number of in-flight requests
  Count#(TAdd#(`LogTransmitBufferLen, 1)) inFlightTransmits <-
    mkCount(2 ** `LogTransmitBufferLen);

  // Transmit response-buffer
  Queue#(TransmitResp) transmitRespBuffer <- mkUGShiftQueue(QueueOptFmax);

  // Flit count
  Reg#(MsgLen) transmitFlitCount <- mkConfigReg(0);

  // Pipeline state
  Reg#(TransmitToken) transmit2Input <- mkVReg;
  Reg#(TransmitToken) transmit3Input <- mkVReg;
  Reg#(TransmitToken) transmit4Input <- mkVReg;

  rule transmit1;
    if (txReqPort.canGet &&
          inFlightTransmits.notFull &&
            transmitRespBuffer.notFull &&
              !flitWriteWire) begin
      TransmitReq req = txReqPort.value;
      // Is this the final beat of the message?
      Bool endOfMsg = False;
      if (transmitFlitCount == req.len) begin
        endOfMsg = True;
        transmitFlitCount <= 0;
        // Consume request
        txReqPort.get;
        // Put response
        transmitRespBuffer.enq(TransmitResp { id: req.id });
      end else
        transmitFlitCount <= transmitFlitCount+1;
      // Read message from scratchpad
      flitReadIndexWire <=
        { truncate(req.id), req.msgIndex, transmitFlitCount };
      // Trigger next stage
      inFlightTransmits.inc;
      let token = TransmitToken {dest: req.dest, notFinalFlit: !endOfMsg};
      transmit2Input <= token;
    end
  endrule

  rule transmit2;
    // Trigger next stage
    transmit3Input <= transmit2Input;
  endrule

  rule transmit3;
    // Trigger next stage
    transmit4Input <= transmit3Input;
  endrule

  rule transmit4;
    TransmitToken token = transmit4Input;
    // Put flit into transmit buffer
    myAssert(transmitBuffer.notFull, "transmitBuffer overflow");
    let flit = Flit { dest:         token.dest
                    , payload:      scratchpad.dataOutA
                    , notFinalFlit: token.notFinalFlit
                    , isIdleToken:  False };
    transmitBuffer.enq(flit);
  endrule
  
  // Scratchpad
  // ==========

  // Response buffer
  `define LogScratchpadBufferLen 4
  SizedQueue#(`LogScratchpadBufferLen, ScratchpadResp) scratchpadRespBuffer <-
    mkUGSizedQueuePrefetch;

  // Track number of in-flight requests
  Count#(TAdd#(`LogScratchpadBufferLen, 1)) inFlightScratchpadReqs <-
    mkCount(2 ** `LogScratchpadBufferLen);

  // Pipeline state
  Reg#(ScratchpadReq) scratchpad2Input <- mkVReg;
  Reg#(ScratchpadReq) scratchpad3Input <- mkVReg;

  rule scratchpad1;
    // Perform scratchpad load or store
    if (spadReqPort.canGet && inFlightScratchpadReqs.notFull) begin
      spadReqPort.get;
      ScratchpadReq req = spadReqPort.value;
      MailboxWordAddr addr = req.wordAddr;
      scratchpad.putB(req.isStore, addr, req.data, req.byteEn);
      inFlightScratchpadReqs.inc;
      // Trigger next stage
      scratchpad2Input <= req;
    end
  endrule

  rule scratchpad2;
    // Trigger next stage
    scratchpad3Input <= scratchpad2Input;
  endrule

  rule scratchpad3;
    ScratchpadReq req = scratchpad3Input;
    // Issue response
    ScratchpadResp resp;
    resp.id = req.id;
    resp.isStore = req.isStore;
    resp.data = scratchpad.dataOutB;
    scratchpadRespBuffer.enq(resp);
  endrule

  // Free Unit
  // =========

  // Current state
  // Consume one request every two cycles
  Reg#(Bit#(1)) freeState <- mkReg(0);

  // This register is pulsed when the final reference
  // to a message slot is freed
  Reg#(Bit#(1)) freeDoneReg <- mkDReg(0);

  rule free (freeReqPort.canGet && freeSlotsInitDone);
    FreeReq req = freeReqPort.value;
    // Process request in two cycles
    let count = refCount.dataOutB;
    if (freeState == 0) begin
      freeState <= 1;
    end else begin
      freeReqPort.get;
      if (count == 1) begin
        myAssert(freeSlots.notFull, "Mailbox: freeSlots full!");
        freeSlots.enq(req.slot);
        freeDoneReg <= 1;
      end
      freeState <= 0;
    end
    refCount.putB(freeState == 1, req.slot, count-1);
  endrule

  // Interfaces
  // ==========

  function ReceiveReqResp mkReceiveReqResp(Integer i) =
    interface ReceiveReqResp
      interface rxReqIn = rxReqPorts[i].in;
      interface rxRespOut =
        interface BOut
          method Action get = recvRespQueues[i].deq;
          method Bool valid = recvRespQueues[i].canDeq;
          method ReceiveResp value = recvRespQueues[i].dataOut;
        endinterface;
    endinterface;

  interface In txReqIn   = txReqPort.in;
  interface In freeReqIn = freeReqPort.in;
  interface In spadReqIn = spadReqPort.in;

  interface rxReqResp = genWith(mkReceiveReqResp);
  method Bit#(1) freeDone = freeDoneReg;

  interface BOut txRespOut;
    method Action get;
      transmitRespBuffer.deq;
    endmethod
    method Bool valid = transmitRespBuffer.canDeq;
    method TransmitResp value = transmitRespBuffer.dataOut;
  endinterface

  interface BOut spadRespOut;
    method Action get;
      scratchpadRespBuffer.deq;
      inFlightScratchpadReqs.dec;
    endmethod
    method Bool valid = scratchpadRespBuffer.canDeq;
    method ScratchpadResp value = scratchpadRespBuffer.dataOut;
  endinterface

  interface MailboxNet net;
    interface In flitIn = flitInPort.in;

    interface BOut flitOut;
      method Action get;
        transmitBuffer.deq;
        inFlightTransmits.dec;
      endmethod
      method Bool valid = transmitBuffer.canDeq;
      method Flit value = transmitBuffer.dataOut;
    endinterface
  endinterface

  method Action initSendSlots(Option#(Bool) useExtra);
    useExtraSendSlot <= useExtra;
  endmethod

endmodule

// =============================================================================
// Mailbox client
// =============================================================================

// The interface between a core and a mailbox
interface MailboxClient;
  // Scratchpad
  interface Out#(ScratchpadReq) spadReqOut;
  interface In#(ScratchpadResp) spadRespIn;
  // Transmit unit
  interface Out#(TransmitReq)   txReqOut;
  interface In#(TransmitResp)   txRespIn;
  // Receive unit
  interface Out#(ReceiveReq)    rxReqOut;
  interface In#(ReceiveResp)    rxRespIn;
  // Free unit
  interface Out#(FreeReq)       freeReqOut;
  (* always_ready, always_enabled *)
  method Action freeDone(Bit#(1) done);
endinterface

// =============================================================================
// Mailbox client unit
// =============================================================================

// We capture much of a mailbox client's behaviour here,
// avoiding clutter in Tinsel core
interface MailboxClientUnit;
  // Scratchpad request & response
  interface OutPort#(ScratchpadReq) scratchpadReq;
  interface InPort#(ScratchpadResp) scratchpadResp;

  // Response port for receive requests
  interface Out#(ReceiveResp) respOut;

  // Wakeup port for sleep requests
  interface Out#(SleepStatus) wakeup;

  // For free requests
  interface OutPort#(FreeReq) freeReq;

  // Can given thread send?
  method Bool canSend(ThreadId id);

  // Trigger send
  method Action send(ThreadId id, MsgLen len,
                       NetAddr dest, MailboxMsgAddr addr);

  // Suspend thread until event(s)
  method Action sleep(ThreadId id, WakeEvent e, SleepOp op);

  // Goes high when idle release phase (stage 1) in progress
  method Bool idleReleaseInProgress;

  // Tinsel core's interface to the mailbox
  interface MailboxClient client;

  // For idle detection
  method Bool active;
  (* always_ready, always_enabled *)
  method Bool vote;
  (* always_ready, always_enabled *)
  method Action idleDetectedStage1(Bool pulse);
  (* always_ready, always_enabled *)
  method Action idleVoteStage1(Bool pulse);
  (* always_ready, always_enabled *)
  method Action idleDetectedStage2(Bool pulse);
  method Bool idleStage1Ack;
  (* always_ready, always_enabled *)
  method Bit#(1) incReceived;
endinterface

import "BDPI" function Bit#(32) getBoardId();

module mkMailboxClientUnit#(CoreId myId) (MailboxClientUnit);
  // Ports
  OutPort#(ScratchpadReq)   scratchpadReqPort  <- mkOutPort;
  InPort#(ScratchpadResp)   scratchpadRespPort <- mkInPort;
  OutPort#(TransmitReq)     transmitPort       <- mkOutPort;
  InPort#(TransmitResp)     transmitRespPort   <- mkInPort;
  OutPort#(ReceiveReq)      recvReqPort        <- mkOutPort;
  InPort#(ReceiveResp)      recvRespPort       <- mkInPort;
  OutPort#(FreeReq)         freeReqPort        <- mkOutPort;
  OutPort#(SleepStatus)     wakeupPort         <- mkOutPort;
  OutPort#(ReceiveResp)     respPort           <- mkOutPort;

  // Sleep queue (threads sit in here while waiting for events)
  SizedQueue#(`LogThreadsPerCore, SleepStatus) sleepQueue <-
    mkUGSizedQueuePrefetch;

  // Transmit logic
  // ==============

  // Transmit queue, big enough to hold one request for each thread
  SizedQueue#(`LogThreadsPerCore, TransmitReq) transmitQueue <-
    mkUGSizedQueue;

  // Track whether each thread can send a new message
  // (We allow each thread to have at most one in-flight send
  // at a time, to implement the can-send instruction)
  Vector#(TExp#(`LogThreadsPerCore), SetReset) canThreadSend <-
    replicateM(mkSetReset(True));

  rule transmit;
    // Send transmit requests
    if (transmitQueue.canDeq &&
          transmitQueue.canPeek &&
            transmitPort.canPut) begin
      TransmitReq req = transmitQueue.dataOut;
      transmitQueue.deq;
      transmitPort.put(req);
    end
    // Receive transmit responses
    if (transmitRespPort.canGet) begin
      TransmitResp resp = transmitRespPort.value;
      transmitRespPort.get;
      Bit#(`LogThreadsPerCore) tid = truncate(resp.id);
      canThreadSend[tid].set;
    end
  endrule

  // Receive logic
  // =============

  // Pass receive responses back to the core
  rule respond (recvRespPort.canGet &&
                  recvRespPort.value.status.op != WaitUntilOp &&
                    respPort.canPut);
    ReceiveResp resp = recvRespPort.value;
    respPort.put(resp);
    recvRespPort.get;
  endrule

  // Sleep unit
  // ==========
  //
  // There is a conflict between the the sleep method and the wakeup
  // unit: both would like to write to the sleep queue.  The sleep unit
  // resolves this conflict, giving priority to the sleep method.

  // Signals from the sleep method
  Wire#(Bool) doSleep <- mkDWire(False);
  Wire#(SleepStatus) sleepThread <- mkDWire(?);

  // Signals from the wakeup unit
  Wire#(Bool) doRequeue <- mkDWire(False);
  Wire#(Bool) doResumeSleep <- mkDWire(False);
  Wire#(SleepStatus) resumeSleepWire <- mkDWire(?);

  rule sleepUnit (doSleep || doResumeSleep || doRequeue);
    myAssert(sleepQueue.notFull, "MailboxClientUnit: sleep violation");
    sleepQueue.enq(doSleep ? sleepThread :
      (doResumeSleep ? resumeSleepWire : sleepQueue.dataOut));
  endrule

  // Idle detection (see IdleDetector.bsv)
  // =====================================

  // Iterface to idle detector
  Wire#(Bool) idleStage1Wire <- mkBypassWire;
  Wire#(Bool) voteStage1Wire <- mkBypassWire;
  Wire#(Bool) idleStage2Wire <- mkBypassWire;

  // Latch the pulses
  Reg#(Bool) idleStage1Reg <- mkConfigReg(False);
  Reg#(Bool) voteStage1Reg <- mkConfigReg(False);

  // Has the idle event been detected?
  Reg#(Bool) idleDetected <- mkConfigReg(False);

  // Stage 1 acknowledgement
  Wire#(Bool) stage1AckWire <- mkDWire(False);

  // Track the number of threads waiting on the idle event
  Count#(TAdd#(`LogThreadsPerCore, 1)) numIdleWaiters <-
    mkCount(2 ** `LogThreadsPerCore);

  // Track the number of threads waiting on the idle event that voted true
  Count#(TAdd#(`LogThreadsPerCore, 1)) idleVotes <-
    mkCount(2 ** `LogThreadsPerCore);

  rule updateIdleStage;
    if (idleStage1Reg && numIdleWaiters.value == 0) begin
      idleStage1Reg <= False;
      stage1AckWire <= True;
    end else if (idleStage1Wire) begin
      idleDetected <= True;
      idleStage1Reg <= True;
      voteStage1Reg <= voteStage1Wire;
    end else if (idleStage2Wire) begin
      idleDetected <= False;
    end
  endrule

  // Wakeup unit
  // ===========
  //
  // In the background, cycle through the sleep queue, waking
  // each sleeping thread if the events it is waiting for are
  // satisfied.

  // Iterate over sleep queue, looking for threads to wake up
  rule wakeup1 (sleepQueue.canPeek && sleepQueue.canDeq);
    let thread = sleepQueue.dataOut;
    let canSend = canThreadSend[thread.id].value;
    // Select only the bits of the event that match
    let eventMatch = thread.wakeEvent &
          {pack(idleStage1Reg && voteStage1Reg),
           pack(idleStage1Reg),
           pack(thread.canRecv),
           pack(canSend)};
    // Should a wakeup be sent?
    Bool wakeupCond = eventMatch != 0;
    if (thread.op != WaitUntilOp) begin
      if (recvReqPort.canPut) begin
        recvReqPort.put(thread);
        sleepQueue.deq;
      end
    end else if (wakeupCond) begin
      // When wakeup port is ready
      if (wakeupPort.canPut) begin
        // Send wakeup
        if (thread.wakeEvent[2] == 1) numIdleWaiters.dec;
        if (thread.wakeEvent[3] == 1) idleVotes.dec;
        thread.wakeEvent = eventMatch;
        wakeupPort.put(thread);
        sleepQueue.deq;
      end 
    // If thread can't receive but wants to,
    // we need to make a new receive request
    end else if (!thread.canRecv && thread.wakeEvent[1] == 1) begin
      if (recvReqPort.canPut) begin
        recvReqPort.put(thread);
        sleepQueue.deq;
      end
    end else if (!doSleep && !doResumeSleep) begin
      // Retry later
      doRequeue <= True;
      sleepQueue.deq;
    end
  endrule

  // Pass receive responses for wakeup unit back into sleep queue
  rule wakeup2 (recvRespPort.canGet &&
                  recvRespPort.value.status.op == WaitUntilOp && !doSleep);
    ReceiveResp resp = recvRespPort.value;
    resumeSleepWire <= resp.status;
    doResumeSleep <= True;
    recvRespPort.get;
  endrule
 
  // Decrement receive count (for idle-detection)
  Wire#(Bit#(1)) incReceivedWire <- mkBypassWire;

  // Methods
  // =======

  method Bool canSend(ThreadId id) = canThreadSend[id].value;

  method Action send(ThreadId id, MsgLen len,
                       NetAddr dest, MailboxMsgAddr addr);
    myAssert(canThreadSend[id].value, "MailboxClientUnit: send violation");
    // Construct transmit request
    TransmitReq req;
    req.id = {truncate(myId), id};
    req.msgIndex = addr;
    req.len = len;
    req.dest = dest;
    // Put in queue
    myAssert(transmitQueue.notFull, "MailboxClientUnit: transmitQueue full");
    transmitQueue.enq(req);
    // Mark thread as busy
    canThreadSend[id].clear;
  endmethod

  method Action sleep(ThreadId id, WakeEvent e, SleepOp op);
    if (e[2] == 1) numIdleWaiters.inc;
    if (e[3] == 1) idleVotes.inc;
    doSleep <= True;
    sleepThread <= SleepStatus
      { id: id, wakeEvent: e, canRecv: False, op: op };
  endmethod

  method Bool idleReleaseInProgress = idleDetected;

  method Bool active = numIdleWaiters.notFull;

  method Bool vote = !idleVotes.notFull;

  method Action idleDetectedStage1(Bool pulse);
    idleStage1Wire <= pulse;
  endmethod

  method Action idleVoteStage1(Bool pulse);
    voteStage1Wire <= pulse;
  endmethod

  method Action idleDetectedStage2(Bool pulse);
    idleStage2Wire <= pulse;
  endmethod

  method Bool idleStage1Ack = stage1AckWire;

  method Bit#(1) incReceived = incReceivedWire;

  // Interfaces
  // ==========

  interface scratchpadReq  = scratchpadReqPort;
  interface scratchpadResp = scratchpadRespPort;
  interface respOut        = respPort.out;
  interface wakeup         = wakeupPort.out;
  interface freeReq        = freeReqPort;

  interface MailboxClient client;
    // Scratchpad
    interface spadReqOut = scratchpadReqPort.out;
    interface spadRespIn = scratchpadRespPort.in;
    // Transmit unit
    interface txReqOut = transmitPort.out;
    interface txRespIn = transmitRespPort.in;
    // Receive unit
    interface rxReqOut = recvReqPort.out;
    interface rxRespIn = recvRespPort.in;
    // Free unit
    interface freeReqOut = freeReqPort.out;
    method Action freeDone(Bit#(1) done);
      incReceivedWire <= done;
    endmethod
  endinterface

endmodule

// =============================================================================
// Mailbox connections
// =============================================================================

// Connect a vector of mailbox clients to a mailbox
module connectCoresToMailbox#(
         Vector#(`CoresPerMailbox, MailboxClient) clients,
         Mailbox server) ();

  // Connect scratchpad requests
  function spadReqOut(client) = client.spadReqOut;
  let spadReqs <- mkMergeTree(Fair,
                     mkUGShiftQueue1(QueueOptFmax),
                     map(spadReqOut, clients));
  connectUsing(mkUGQueue, spadReqs, server.spadReqIn);

  // Connect transmit requests
  function txReqOut(client) = client.txReqOut;
  let txReqs <- mkMergeTree(Fair,
                  mkUGShiftQueue1(QueueOptFmax),
                  map(txReqOut, clients));
  connectUsing(mkUGQueue, txReqs, server.txReqIn);

  // Connect receive requests
  for (Integer i = 0; i < `CoresPerMailbox; i=i+1)
    connectUsing(mkUGQueue, clients[i].rxReqOut, server.rxReqResp[i].rxReqIn);

  // Connect free requests
  function freeReqOut(client) = client.freeReqOut;
  let freeReqs <- mkMergeTree(Fair,
                    mkUGShiftQueue1(QueueOptFmax),
                    map(freeReqOut, clients));
  connectUsing(mkUGQueue, freeReqs, server.freeReqIn);

  // Connect scratchpad responses
  function Bit#(`LogCoresPerMailbox) spadRespKey(ScratchpadResp resp) =
    truncateLSB(resp.id);
  function spadRespIn(client) = client.spadRespIn;
  let spadResps <- mkResponseDistributor(
                     spadRespKey,
                     mkUGShiftQueue1(QueueOptFmax),
                     map(spadRespIn, clients));
  connectDirect(server.spadRespOut, spadResps);

  // Connect transmit responses
  function Bit#(`LogCoresPerMailbox) txRespKey(TransmitResp resp) =
    truncateLSB(resp.id);
  function txRespIn(client) = client.txRespIn;
  let txResps <- mkResponseDistributor(
                   txRespKey,
                    mkUGShiftQueue1(QueueOptFmax),
                   map(txRespIn, clients));
  connectDirect(server.txRespOut, txResps);

  // Connect receive responses
  for (Integer i = 0; i < `CoresPerMailbox; i=i+1)
    connectDirect(server.rxReqResp[i].rxRespOut, clients[i].rxRespIn);

  // Connect free responses
  rule freeOne;
    clients[0].freeDone(server.freeDone);
  endrule
  for (Integer i = 1; i < `CoresPerMailbox; i=i+1) begin
    rule freeRest;
      clients[i].freeDone(0);
    endrule
  end
endmodule

// =============================================================================
// Custom accelerators
// =============================================================================

// An optional custom accelerator sits alongside each mailbox in the
// design. We provide a module mkMailboxAcc with same interface as a
// Mailbox which also implicitly interfaces to an externally defined
// custom accelerator module written in Verilog:
//
//   +--------- mkMailboxAcc -----------+
//   | +-----------+  +---------------+ |
//   | | mkMailbox |  | mkAccelerator | |
//   | +-----------+  +---------------+ |
//   +----------------------------------+
//
// Outgoing packets from the accelerator and the mailbox are merged
// into a single stream.  Incoming packets from the network are routed
// either to the mailbox or the accelerator depending on the
// accelerator bit in the network address.
//
// This is the bare minimum needed to support custom accelerators.
// Note that termination detection is not yet supported in the
// presence of custom accelerators.

interface TinselAccelerator;
  method Action put(Flit flit);
  method Bool canPut;
  method Action get;
  method Bool canGet;
  method Flit data;
  (* always_ready, always_enabled *)
  method Action setBoardId(Bit#(`MeshXBits) boardX, Bit#(`MeshYBits) boardY);
endinterface

import "BVI" ExternalTinselAccelerator =
  module mkTinselAccelerator#(
           BoardId boardId, Integer tileX, Integer tileY) (TinselAccelerator);

    parameter TILE_X = tileX;
    parameter TILE_Y = tileY;

    method put(in_data) enable (in_valid);
    method in_ready canPut;

    method get() enable (out_ready);
    method out_valid canGet;
    method out_data data;

    method setBoardId (board_x, board_y) enable ((*inhigh*) en);

    default_clock clk(clk);
    default_reset rst(rst_n);

    schedule (data, canGet, canPut, setBoardId, get) CF
             (data, canGet, canPut, put);
    schedule (setBoardId) CF (get);
    schedule (setBoardId) C (setBoardId);
    schedule (put) C (put);
    schedule (get) C (get);
  endmodule

// ========================
// Mailbox with accelerator
// ========================

`ifndef UseCustomAccelerator

module mkMailboxAcc#(BoardId boardId,
         Integer tileX, Integer tileY) (Mailbox);
  Mailbox mbox <- mkMailbox;
  return mbox;
endmodule

`else

module mkMailboxAcc#(BoardId boardId,
         Integer tileX, Integer tileY) (Mailbox);
  // Instantiate standard mailbox
  Mailbox mbox <- mkMailbox;

  // Instantiate custom accelerator
  TinselAccelerator acc <- mkTinselAccelerator(boardId, tileX, tileY);

  // Set board id
  rule setBoardId;
    acc.setBoardId(boardId.x, boardId.y);
  endrule

  // Feed input to mailbox or accelerator
  // ------------------------------------

  InPort#(Flit) inPort <- mkInPort;
  OutPort#(Flit) toMailbox <- mkOutPort;

  // Connect to mailbox
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    toMailbox.out, mbox.net.flitIn);

  // To accelerator
  rule connectToAcc (inPort.canGet && inPort.value.dest.acc && acc.canPut);
    acc.put(inPort.value);
    inPort.get;
  endrule

  // To mailbox
  rule connectToMailbox (inPort.canGet && toMailbox.canPut &&
                           !inPort.value.dest.acc);
    toMailbox.put(inPort.value);
    inPort.get;
  endrule

  // Consume output from mailbox or accelerator
  // ------------------------------------------

  // Accelerator output
  Queue1#(Flit) accOutQueue <- mkUGShiftQueue1(QueueOptFmax);
  OutPort#(Flit) accOutPort <- mkOutPort;

  rule fillAccOutQueue (acc.canGet && accOutQueue.notFull);
    acc.get;
    accOutQueue.enq(acc.data);
  endrule

  rule writeToAccOutPort (accOutPort.canPut && accOutQueue.canDeq);
    accOutQueue.deq;
    accOutPort.put(accOutQueue.dataOut);
  endrule

  // Mailbox output
  Out#(Flit) mboxOut <- convertBOutToOut(mbox.net.flitOut);

  // Overall output
  Out#(Flit) out <- mkFlitMerger(mboxOut, accOutPort.out);
  Queue1#(Flit) outQueue <- mkUGShiftQueue1(QueueOptFmax);
  connectToQueue(out, outQueue);

  interface In   spadReqIn   = mbox.spadReqIn;
  interface BOut spadRespOut = mbox.spadRespOut;
  interface In   txReqIn     = mbox.txReqIn;
  interface BOut txRespOut   = mbox.txRespOut;
  interface rxReqResp        = mbox.rxReqResp;
  interface In   freeReqIn   = mbox.freeReqIn;

  interface MailboxNet net;
    interface In flitIn = inPort.in;
    interface BOut flitOut;
      method Action get;
        outQueue.deq;
      endmethod
      method Bool valid = outQueue.canDeq;
      method Flit value = outQueue.dataOut;
    endinterface
  endinterface

endmodule

`endif

endpackage
