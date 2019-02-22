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
// scratchpad within a mailbox.  
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
//    cores        |       +--------------+           |-----> Flit out
//              <--------->| Receive Unit |           |
//                 |       +--------------+           |
//                 |                                  |
//                 |       +---------------+          | 
//              <--------->| Allocate Unit |          | 
//                 |       +---------------+          |
//                 |                                  | 
//                 +----------------------------------+
//                                              
//
// Scratchpad
// ----------
// 
// The scratchpad is a mixed-width dual-port block RAM with a 32-bit bus
// on the core side and a flit-sized bus on the network side (a
// message is comprised of several flits).  The scratchpad is
// partitioned by thread id.  The number of 32-bit words available to
// each thread is:
//
//   2^LogMsgsPerThread * 2^LogMaxFlitsPerMsg * 2^LogWordsPerFlit.
// 
// One attraction of using a scratchpad is that a message can be forwarded
// (recieved and sent) without serialising it through the 32-bit core.
// 
// Transmit Unit
// -------------
//
// The Transmit Unit accepts requests for a message-sized block
// (aligned) in the scratchpad to be sent to a given destination.
// When the Transmit Unit eventually sends the message, it will
// produce a response, notifying the thread that made the request.
//
// Receive Unit
// ------------
// 
// The Receive Unit contains a status register for each thread served by the
// mailbox.  The status register contains a bit-vector specifying which
// message-sized blocks (aligned) in the scratchpad can be used to store
// an incoming message.  When a message arrives for a given thread:
// 
//   1. the first hot bit in that thread's status vector is used to determine 
//      the location for the message in the scratchpad;
//   2. the message is written to the scratchpad and
//      the first hot bit in the status vector is cleared;
//   3. a notification (or alert) is sent to the thread indicating
//      the location of the new message in the scratchpad.
//
// If there are no hot bits in the status vector for the receiving
// thread, backpressure is applied to the network.
//
// Allocate Unit
// -------------
//
// A thread can set bits in its status vector by sending an "allocate"
// request.  In this way, a thread can allocate locations for incoming
// messages in the scratchpad, giving the Receive Unit permission to
// overwrite the values at these locations.

// =============================================================================
// Imports
// =============================================================================

import Vector       :: *;
import Queue        :: *;
import Interface    :: *;
import BlockRam     :: *;
import ArrayOfSet   :: *;
import ConfigReg    :: *;
import Util         :: *;
import Globals      :: *;
import DReg         :: *;

// =============================================================================
// Types
// =============================================================================

// A single Mailbox may be shared my several multi-threaded cores
typedef Bit#(`LogThreadsPerMailbox) MailboxClientId;

// Thread-local word address in scratchpad memory
typedef TAdd#(`LogMsgsPerThread,
          TAdd#(`LogMaxFlitsPerMsg, `LogWordsPerFlit))
            MailboxThreadWordAddrBits;
typedef Bit#(MailboxThreadWordAddrBits) MailboxThreadWordAddr;

// Thread-local flit address in scratchpad memory
typedef Bit#(`LogFlitsPerThread) MailboxThreadFlitAddr;

// Thread-local message address in scratchpad memory
typedef Bit#(`LogMsgsPerThread) MailboxThreadMsgAddr;

// Word address in scratchpad memory
typedef TAdd#(`LogThreadsPerMailbox, MailboxThreadWordAddrBits)
          MailboxWordAddrBits;
typedef Bit#(MailboxWordAddrBits) MailboxWordAddr;

// Flit address in scratchpad memory
typedef TAdd#(`LogThreadsPerMailbox, `LogFlitsPerThread) MailboxFlitAddrBits;
typedef Bit#(MailboxFlitAddrBits) MailboxFlitAddr;

// Message address in scratchpad memory
typedef TAdd#(`LogThreadsPerMailbox, `LogMsgsPerThread) MailboxMsgAddrBits;
typedef Bit#(MailboxMsgAddrBits) MailboxMsgAddr;

// Status memory
// (The status vector for a thread indicates the message-aligned
// locations in the scractpad that can be used to store incoming
// messages)
typedef Bit#(`LogThreadsPerMailbox) StatusVectorIndex;
typedef Bit#(TExp#(`LogMsgsPerThread)) StatusVector;
typedef Bit#(TAdd#(`LogThreadsPerMailbox, `LogMsgsPerThread)) StatusBitIndex;

// Scratchpad request
typedef struct {
  // Source of request
  MailboxClientId id;
  // Operation
  Bool isStore;
  // Thread-local word address
  MailboxThreadWordAddr wordAddr;
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
  // Thread-local message address
  MailboxThreadMsgAddr msgIndex;
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

// Allocation request
// (Request to allocate space for a message)
typedef struct {
  // Source of request
  MailboxClientId id;
  // Thread-local address for message
  MailboxThreadMsgAddr msgIndex;
} AllocReq deriving (Bits);

// Receive notification
// (Notification that a message has been received)
typedef struct {
  // Message destination
  MailboxClientId id;
  // Index of message scratchpad
  Bit#(`LogMsgsPerThread) index;
} ReceiveAlert deriving (Bits);

// Transmit pipeline token
typedef struct {
  NetAddr dest;
  Bool notFinalFlit;
} TransmitToken deriving (Bits);

// =============================================================================
// Functions
// =============================================================================

// Convert byte address to message index
function MailboxThreadMsgAddr byteAddrToMsgIndex(Bit#(32) addr);
  MailboxThreadMsgAddr msgAddr = truncate(addr[31:`LogBytesPerMsg]);
  return msgAddr;
endfunction

// Convert message address to byte address
function Bit#(32) msgAddrToByteAddr(MailboxThreadMsgAddr msgAddr);
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
  // Core-side interfaces to receive unit
  interface In#(AllocReq)         allocReqIn;
  interface BOut#(ReceiveAlert)   rxAlertOut;
  // Network-side interface
  interface MailboxNet            net;
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

  // Alert buffer (to notify threads about received messages)
  `define LogAlertBufferLen 4
  SizedQueue#(`LogAlertBufferLen, ReceiveAlert) alertBuffer <-
    mkUGSizedQueuePrefetch;

  // Receive status: bit-vector per thread specifying which message-sized
  // blocks (aligned) in the scratchpad can be used to hold incoming messages
  ArrayOfSet#(`LogThreadsPerMailbox, `LogMsgsPerThread) statusMem <-
    mkArrayOfSet;

  // Request & response ports
  InPort#(ScratchpadReq)   spadReqPort   <- mkInPort;
  InPort#(TransmitReq)     txReqPort     <- mkInPort;
  InPort#(Flit)            flitInPort    <- mkInPort;
  InPort#(AllocReq)        allocReqPort  <- mkInPort;

  // Message access unit
  // ===================

  // There is a conflict between the transmit and receive pipelines:
  // "receive" needs to write a message to the scratchpad while
  // "transmit" needs to read a message.  The message access unit
  // resolves this conflict: read takes priorty over write and the
  // write wire must only be asserted when the read wire is low.

  // Control wires for modifying messages in scratchpad
  Wire#(Bool) flitReadWire  <- mkDWire(False);
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

  // State
  Reg#(Bit#(1))      recvState      <- mkConfigReg(0);
  Reg#(Flit)         flitBuffer2    <- mkConfigRegU;
  Reg#(Flit)         flitBuffer3    <- mkConfigRegU;
  Reg#(MsgLen)       recvFlitCount  <- mkConfigReg(0);
  Reg#(ReceiveAlert) receive4Input  <- mkVReg;

  // Keep track of the number of in-flight flits being received
  Count#(TAdd#(`LogAlertBufferLen, 1)) inFlightRecvs <-
    mkCount(2 ** `LogAlertBufferLen);

  // Are we processing the first flit of a message?
  Reg#(Bool) firstFlit <- mkConfigReg(True);

  // Inputs to receive stage 2
  Reg#(Bool) receive2Fire <- mkDReg(False);
  Reg#(Bool) receive2FirstFlit <- mkConfigRegU;

  // Inputs to receive stage 3
  Reg#(Bool) receive3Fire <- mkDReg(False);
  Reg#(Bool) receive3FirstFlit <- mkConfigRegU;

  // Receive pipeline stall registers
  Reg#(Maybe#(Bit#(`LogThreadsPerMailbox))) recvStall1 <- mkDReg(Invalid);
  Reg#(Maybe#(Bit#(`LogThreadsPerMailbox))) recvStall2 <- mkDReg(Invalid);

  // The index in the scratchpad that the message is being written to
  Reg#(Bit#(`LogMsgsPerThread)) destIndex <- mkConfigRegU;

  // Stages 0 and 1
  // Stage 1 only invoked on first flit of each messages
  rule receive0and1;
    Bool consumeFlit = False;
    let flit = flitInPort.value;
    if (recvState == 0) begin
      // Try to consume an incoming flit
      if (flitInPort.canGet && inFlightRecvs.notFull) begin
        if (firstFlit) begin
          // Stall condition
          Bool stall = recvStall1 == Valid(truncate(pack(flit.dest)))
                    || recvStall2 == Valid(truncate(pack(flit.dest)));
          if (!stall) begin
            recvState <= 1;
            statusMem.tryGet(truncate(pack(flit.dest)));
          end
        end else begin
          consumeFlit = True;
        end
      end
    end else if (recvState == 1) begin
      // Do we have somewhere to put the incoming message?
      if (statusMem.canGetEarly) begin
        inFlightRecvs.inc;
        consumeFlit = True;
        recvStall1 <= Valid(truncate(pack(flit.dest)));
      end
      recvState <= 0;
    end
    if (consumeFlit) begin
      // Consume flit
      flitInPort.get;
      // Trigger receive stage 2
      receive2Fire <= True;
      // We may have finished processing this message,
      // in which case reset firstFlit back to True
      firstFlit <= !flit.notFinalFlit;
    end
    flitBuffer2 <= flit;
    receive2FirstFlit <= firstFlit;
  endrule

  // Stage 2
  rule receive2 (receive2Fire);
    if (receive2FirstFlit) begin
      recvStall2 <= Valid(truncate(pack(flitBuffer2.dest)));
      statusMem.get;
    end
    flitBuffer3 <= flitBuffer2;
    receive3FirstFlit <= receive2FirstFlit;
    receive3Fire <= True;
  endrule

  // Stage 3
  rule receive3 (receive3Fire);
    let flit = flitBuffer3;
    let index = receive3FirstFlit ? statusMem.itemOut : destIndex;
    if (receive3FirstFlit) destIndex <= statusMem.itemOut;
    // Update scratchpad
    flitWriteWire      <= True;
    flitWriteIndexWire <= { truncate(pack(flit.dest)), index, recvFlitCount };
    flitWriteDataReg   <= flit.payload;
    // Update flit count
    if (!flit.notFinalFlit) begin
      recvFlitCount <= 0;
      // Trigger next stage
      ReceiveAlert alert;
      alert.id = truncate(pack(flit.dest));
      alert.index = index;
      receive4Input <= alert;
    end else
      recvFlitCount <= recvFlitCount+1;
  endrule

  // Stage 4
  rule receive4;
    ReceiveAlert alert = receive4Input;
    // Issue response
    myAssert(alertBuffer.notFull, "Mailbox: alertBuffer overflow");
    alertBuffer.enq(alert);
  endrule

  // Allocation request & response
  // =============================
  //
  // Allocate space in scratchpad for a message

  rule allocHandler;
    if (allocReqPort.canGet && statusMem.canPut) begin
      AllocReq req = allocReqPort.value;
      statusMem.put(req.id, req.msgIndex);
      allocReqPort.get;
    end
  endrule

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
      flitReadWire      <= True;
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
  
  // Scratchpad interface
  // ====================

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
      MailboxWordAddr addr = {truncate(req.id), req.wordAddr};
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

  // Interfaces
  // ==========

  interface In txReqIn    = txReqPort.in;
  interface In allocReqIn = allocReqPort.in;
  interface In spadReqIn  = spadReqPort.in;

  interface BOut rxAlertOut;
    method Action get;
      alertBuffer.deq;
      inFlightRecvs.dec;
    endmethod
    method Bool valid = alertBuffer.canDeq;
    method ReceiveAlert value = alertBuffer.dataOut;
  endinterface

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

endmodule

// =============================================================================
// Mailbox client
// =============================================================================

// The interface implemented by a mailbox client (e.g. a Tinsel core)
interface MailboxClient;
  // Scratchpad
  interface Out#(ScratchpadReq) spadReqOut;
  interface In#(ScratchpadResp) spadRespIn;
  // Transmit unit
  interface Out#(TransmitReq)   txReqOut;
  interface In#(TransmitResp)   txRespIn;
  // Receive unit
  interface Out#(AllocReq)      allocReqOut;
  interface In#(ReceiveAlert)   rxAlertIn;
endinterface

// A bit vector of events that can cause a sleeping thread to wake up
// Bit 0: can-send
// Bit 1: can-receive
// Bit 2: idle, vote false
// Bit 3: idle, vote true
typedef Bit#(4) WakeEvent;

// Pair containing thread id and wake event
typedef struct {
  ThreadId id;
  WakeEvent wakeEvent;
} ThreadEventPair deriving (Bits);

// =============================================================================
// Mailbox client unit
// =============================================================================

// We capture much of a mailbox client's behaviour here,
// avoiding clutter in Tinsel core
interface MailboxClientUnit;
  // Scratchpad request & response
  interface OutPort#(ScratchpadReq) scratchpadReq;
  interface InPort#(ScratchpadResp) scratchpadResp;

  // Allocate request
  interface OutPort#(AllocReq) allocateReq;

  // Prepare for mailbox access by given thread
  method Action prepare(ThreadId id);
  // Is a send/receive possible on prepared thread?
  // (Valid on 2nd cycle after call to "prepare")
  method Bool canSend;
  method Bool canRecv;
  // Trigger send/receive
  // (Must only be called on 2nd cycle after call to "prepare")
  method Action recv;
  method Action send(ThreadId id, MsgLen len,
                       NetAddr dest, MailboxThreadMsgAddr addr);
  // Scratchpad address of message received
  // (Valid on 2nd cycle after call to "recv")
  method Bit#(32) recvAddr;

  // Suspend thread until event(s)
  method Action sleep(ThreadId id, WakeEvent e);
  // Port used to indicate when a thread should be woken
  interface Out#(ThreadEventPair) wakeup;

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
endinterface

import "BDPI" function Bit#(32) getBoardId();

module mkMailboxClientUnit#(CoreId myId) (MailboxClientUnit);
  // Ports
  OutPort#(ScratchpadReq)   scratchpadReqPort  <- mkOutPort;
  InPort#(ScratchpadResp)   scratchpadRespPort <- mkInPort;
  OutPort#(AllocReq)        allocReqPort       <- mkOutPort;
  OutPort#(TransmitReq)     transmitPort       <- mkOutPort;
  InPort#(TransmitResp)     transmitRespPort   <- mkInPort;
  InPort#(ReceiveAlert)     alertPort          <- mkInPort;
  OutPort#(ThreadEventPair) wakeupPort         <- mkOutPort;

  // Sleep queue (threads sit in here while waiting for events)
  SizedQueue#(`LogThreadsPerCore, ThreadEventPair) sleepQueue <- mkUGSizedQueue;

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

  // One set of unread message-pointers per thread
  ArrayOfSet#(`LogThreadsPerCore,
              `LogMsgsPerThread) unread <- mkArrayOfSet;

  // Receive unit state
  Reg#(Bit#(1))      recvState <- mkConfigReg(0);
  Reg#(ReceiveAlert) alertReg  <- mkConfigRegU;

  rule receive0 (recvState == 0);
    if (alertPort.canGet) begin
      alertPort.get;
      alertReg <= alertPort.value;
      recvState <= 1;
    end
  endrule

  rule receive1 (recvState == 1);
    if (unread.canPut) begin
      unread.put(truncate(alertReg.id), alertReg.index);
      recvState <= 0;
    end
  endrule

  // Status unit
  // ===========
  //
  // Determine can-send and can-recv status for each thread.  There is a
  // conflict between the call to "prepare" and the wakeup unit.  The
  // status unit resolves this conflict, giving priority to the
  // "prepare" method.

  // When read unit is triggered by call to "prepare"
  Wire#(Bool)     doPrepare <- mkDWire(False);
  Wire#(ThreadId) prepareId <- mkDWire(?);

  // Flag indicating whether client thread can send
  Reg#(Bool) canSendReg1 <- mkConfigReg(False);
  Reg#(Bool) canSendReg2 <- mkConfigReg(False);

  rule statusUnit;
    ThreadId id = doPrepare ? prepareId : sleepQueue.dataOut.id;
    unread.tryGet(id);
    canSendReg1 <= canThreadSend[id].value;
    canSendReg2 <= canSendReg1;
  endrule

  // Sleep unit
  // ==========
  //
  // There is a conflict between the the sleep method and the wakeup
  // unit: both would like to write to the sleep queue.  The sleep unit
  // resolves this conflict, giving priority to the sleep method.

  // Signals from the sleep method
  Wire#(Bool) doSleep <- mkDWire(False);
  Wire#(ThreadEventPair) sleepThread <- mkDWire(?);

  // Signals from the wakeup unit
  Wire#(Bool) doRequeue <- mkDWire(False);
  Reg#(ThreadEventPair) wakeupReg <- mkRegU;

  rule sleepUnit (doSleep || doRequeue);
    myAssert(sleepQueue.notFull, "MailboxClientUnit: sleep violation");
    sleepQueue.enq(doSleep ? sleepThread : wakeupReg);
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

  // Buffer for the active events
  Reg#(WakeEvent) eventMatchReg <- mkRegU;

  // State machine control
  Reg#(Bit#(2)) wakeupState <- mkReg(0);
  Reg#(Bool) wakeup2Fire <- mkDReg(False);

  rule wakeup0 (!doPrepare && wakeupState == 0 &&
                  sleepQueue.canPeek && sleepQueue.canDeq);
    let thread = sleepQueue.dataOut;
    sleepQueue.deq;
    wakeupReg <= thread;
    wakeupState <= 1;
  endrule

  rule wakeup1 (wakeupState == 1);
    wakeupState <= 2;
    wakeup2Fire <= True;
  endrule

  rule wakeup2 (wakeupState == 2);
    // Select only the bits of the event that match
    let eventMatchNow = wakeupReg.wakeEvent &
          {pack(idleStage1Reg && voteStage1Reg),
           pack(idleStage1Reg),
           pack(unread.canGet),
           pack(canSendReg2)};
    let eventMatch = wakeup2Fire ? eventMatchNow : eventMatchReg;
    eventMatchReg <= eventMatch;
    // Should a wakeup be sent?
    Bool wakeupCond = eventMatch != 0;
    // When wakeup port is ready
    if (wakeupPort.canPut) begin
      let wakeup = wakeupReg;
      wakeup.wakeEvent = eventMatch;
      if (wakeupCond) begin
        // Send wakeup
        wakeupPort.put(wakeup);
        wakeupState <= 0;
        if (wakeupReg.wakeEvent[2] == 1) numIdleWaiters.dec;
        if (wakeupReg.wakeEvent[3] == 1) idleVotes.dec;
      end else if (!doSleep) begin
        // Retry later
        doRequeue <= True;
        wakeupState <= 0;
      end
    end
  endrule

  // Methods
  // =======

  method Action prepare(ThreadId id);
    doPrepare <= True;
    prepareId <= id;
  endmethod

  method Bool canRecv = unread.canGet;

  method Action recv;
    myAssert(unread.canGet, "MailboxClientUnit: recv violation");
    unread.get;
  endmethod

  method Bool canSend = canSendReg2;

  method Action send(ThreadId id, MsgLen len,
                       NetAddr dest, MailboxThreadMsgAddr addr);
    myAssert(canSendReg2, "MailboxClientUnit: send violation");
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

  method Bit#(32) recvAddr = msgAddrToByteAddr(unread.itemOut);

  method Action sleep(ThreadId id, WakeEvent e);
    if (e[2] == 1) numIdleWaiters.inc;
    if (e[3] == 1) idleVotes.inc;
    doSleep <= True;
    sleepThread <= ThreadEventPair { id: id, wakeEvent: e };
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


  // Interfaces
  // ==========

  interface scratchpadReq  = scratchpadReqPort;
  interface scratchpadResp = scratchpadRespPort;
  interface allocateReq    = allocReqPort;

  interface MailboxClient client;
    // Scratchpad
    interface spadReqOut  = scratchpadReqPort.out;
    interface spadRespIn  = scratchpadRespPort.in;
    // Transmit unit
    interface txReqOut    = transmitPort.out;
    interface txRespIn    = transmitRespPort.in;
    // Receive unit
    interface allocReqOut = allocReqPort.out;
    interface rxAlertIn   = alertPort.in;
  endinterface

  interface wakeup = wakeupPort.out;

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

  // Connect allocation requests
  function allocReqOut(client) = client.allocReqOut;
  let allocReqs <- mkMergeTree(Fair,
                     mkUGShiftQueue1(QueueOptFmax),
                     map(allocReqOut, clients));
  connectUsing(mkUGQueue, allocReqs, server.allocReqIn);

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

  // Connect receive-alerts
  function Bit#(`LogCoresPerMailbox) alertRespKey(ReceiveAlert alert) =
    truncateLSB(alert.id);
  function rxAlertIn(client) = client.rxAlertIn;
  let rxAlerts <- mkResponseDistributor(
                    alertRespKey,
                    mkUGShiftQueue1(QueueOptFmax),
                    map(rxAlertIn, clients));
  connectDirect(server.rxAlertOut, rxAlerts);
endmodule

endpackage
