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
//    Group        |       +---------------+          |<----- Packet in
//     of          |                                  | 
//    cores        |       +--------------+           |-----> Packet out
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
// on the core side and a message-sized bus on the network side (we use
// the term "message" to mean "packet payload".)  The scratchpad is
// partitioned by thread id.  The number of 32-bit words available to
// each thread is 2^LogMsgsPerThread * 2^LogWordsPerMsg.
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
// messages in the scrachpad, giving the Receive Unit permission to
// overwrite the values at these locations.

// =============================================================================
// Imports
// =============================================================================

import Vector       :: *;
import Queue        :: *;
import Interface    :: *;
import BlockRam     :: *;
import ArrayOfSet   :: *;
import ArrayOfQueue :: *;
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
typedef TAdd#(`LogMsgsPerThread, `LogWordsPerMsg) MailboxThreadWordAddrBits;
typedef Bit#(MailboxThreadWordAddrBits) MailboxThreadWordAddr;

// Thread-local message address in scratchpad memory
typedef Bit#(`LogMsgsPerThread) MailboxThreadMsgAddr;

// Word address in scratchpad memory
typedef TAdd#(`LogThreadsPerMailbox, MailboxThreadWordAddrBits)
          MailboxWordAddrBits;
typedef Bit#(MailboxWordAddrBits) MailboxWordAddr;

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
  // Destination thread
  PacketDest dest;
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
  Bit#(`LogWordsPerMsg) wordOffset = 0;
  return {0, msgAddr, wordOffset, 2'b0};
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
  interface In#(Packet)           packetIn;
  interface BOut#(Packet)         packetOut;
endinterface

// =============================================================================
// Implementation
// =============================================================================

(* synthesize *)
module mkMailbox (Mailbox);
  // True dual-port mixed-width scratchpad
  // (One message-sized port and one word-sized port)
  BlockRamTrueMixedBE#(MailboxMsgAddr, Msg, MailboxWordAddr, Bit#(32))
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
  InPort#(Packet)          packetInPort  <- mkInPort;
  InPort#(AllocReq)        allocReqPort  <- mkInPort;

  // Message access unit
  // ===================

  // There is a conflict between the transmit and receive pipelines:
  // "receive" needs to write a message to the scratchpad while
  // "transmit" needs to read a message.  The message access unit
  // resolves this conflict: read takes priorty over write and the
  // write wire must only be asserted when the read wire is low.

  // Control wires for modifying messages in scratchpad
  Wire#(Bool) msgReadWire  <- mkDWire(False);
  Wire#(Bool) msgWriteWire <- mkDWire(False);
  Reg#(Bool) msgWriteReg <- mkRegU;
  Wire#(MailboxMsgAddr) msgReadIndexWire <- mkDWire(0);
  Wire#(MailboxMsgAddr) msgWriteIndexWire <- mkDWire(0);
  Reg#(Msg) msgWriteDataReg <- mkConfigRegU;
  Reg#(MailboxMsgAddr) msgIndexReg <- mkRegU;

  // Use wires to issue message access in scratchpad
  rule msgAccessUnit;
    msgWriteReg <= msgWriteWire;
    msgIndexReg <= msgReadIndexWire | msgWriteIndexWire;
    scratchpad.putA(
      msgWriteReg,
      msgIndexReg,
      msgWriteDataReg);
  endrule

  // Receive pipeline
  // ================

  // Note that this pipeline contains a retry loop and hence it may
  // reorder incoming packets.  However, only one packet for a
  // particular thread may be in the pipeline at any time.  Hence,
  // packets to the same destination are not reorderd.

  // Pipeline stages for receive unit
  Reg#(Bool)         receiveRetryFire <- mkDReg(False);
  Reg#(Bool)         receive2Fire     <- mkDReg(False);
  Reg#(Packet)       receive2Input    <- mkConfigRegU;
  Reg#(Bool)         receive3Fire     <- mkDReg(False);
  Reg#(Packet)       receive3Input    <- mkConfigRegU;
  Reg#(Bool)         receive4Fire     <- mkDReg(False);
  Reg#(Packet)       receive4Input    <- mkConfigRegU;
  Reg#(ReceiveAlert) receive5Input    <- mkVReg;

  // Keep track of the number of in-flight packets being received
  Count#(TAdd#(`LogAlertBufferLen, 1)) inFlightRecvs <-
    mkCount(2 ** `LogAlertBufferLen);

  rule receive1 (packetInPort.canGet || receiveRetryFire);
    // To begin, assume packet out of stage 3 cannot be written to
    // scratchpad and hence needs to be retried
    let pkt = receive4Input;
    // Determine whether a new packet can be inserted into the
    // pipeline without introducing a hazard. A hazard occurs
    // when multiple packets for the same destination are present in
    // the pipeline at the same time. See ArrayOfSet.bsv to
    // understand why such hazards need to be avoided.
    MailboxClientId dest = truncate(packetInPort.value.dest);
    Bool stall = receive2Fire && truncate(receive2Input.dest) == dest
              || receive3Fire && truncate(receive3Input.dest) == dest
              || receive4Fire && truncate(receive4Input.dest) == dest;
    // Retry failed receive or insert a new packet into the pipeline
    Bool triggerNextStage = False;
    if (receiveRetryFire) begin
      // Retry
      triggerNextStage = True;
    end else if (inFlightRecvs.notFull && !stall) begin
      // Insert new packet into the pipeline
      inFlightRecvs.inc;
      packetInPort.get;
      pkt = packetInPort.value;
      triggerNextStage = True;
    end
    // Tigger next stage
    if (triggerNextStage) begin
      receive2Fire <= True;
      // Extract a bit from the status vector
      statusMem.tryGet(truncate(pkt.dest));
    end
    // Prepare input for next stage
    receive2Input <= pkt;
  endrule

  rule receive2 (receive2Fire);
    // Trigger next stage
    receive3Fire <= True;
    receive3Input <= receive2Input;
  endrule

  rule receive3 (receive3Fire);
    let pkt = receive3Input;
    // Prepare inputs for next stage
    receive4Input <= pkt;
    // Has destination for packet been determined?
    if (statusMem.canGet) begin
      statusMem.get;
      // Trigger final pipeline stage
      receive4Fire <= True;
    end else begin
      // No space available for incoming message, retry
      receiveRetryFire <= True;
    end
  endrule
      
  rule receive4 (receive4Fire);
    let pkt = receive4Input;
    let index = statusMem.itemOut;
    // Update scratchpad
    msgWriteWire      <= True;
    msgWriteIndexWire <= { truncate(pkt.dest), index };
    msgWriteDataReg   <= pkt.payload;
    // Trigger next stage
    ReceiveAlert alert;
    alert.id = truncate(pkt.dest);
    alert.index = index;
    receive5Input <= alert;
  endrule

  rule receive5;
    ReceiveAlert alert = receive5Input;
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

  // Transmit packet-buffer
  `define LogTransmitBufferLen 1
  SizedQueue#(`LogTransmitBufferLen, Packet) transmitBuffer <-
    mkUGShiftQueue(QueueOptFmax);

  // Transmit response-buffer
  SizedQueue#(`LogTransmitBufferLen, TransmitResp) transmitRespBuffer <-
    mkUGShiftQueue(QueueOptFmax);

  // Track number of in-flight requests
  Count#(TAdd#(`LogTransmitBufferLen, 1)) inFlightTransmits <-
    mkCount(2 ** `LogTransmitBufferLen);
  Count#(TAdd#(`LogTransmitBufferLen, 1)) inFlightTransmitResps <-
    mkCount(2 ** `LogTransmitBufferLen);

  // Pipeline state
  Reg#(TransmitReq) transmit2Input <- mkVReg;
  Reg#(TransmitReq) transmit3Input <- mkVReg;
  Reg#(TransmitReq) transmit4Input <- mkVReg;

  rule transmit1;
    if (txReqPort.canGet &&
          inFlightTransmits.notFull &&
            inFlightTransmitResps.notFull &&
              !msgWriteWire) begin
      TransmitReq req = txReqPort.value;
      // Consume request
      txReqPort.get;
      inFlightTransmits.inc;
      inFlightTransmitResps.inc;
      // Read message from scratchpad
      msgReadWire      <= True;
      msgReadIndexWire <= { truncate(req.id), req.msgIndex };
      // Trigger next stage
      transmit2Input <= req;
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
    TransmitReq req = transmit4Input;
    // Message available on scratchpad output bus
    Msg msg = scratchpad.dataOutA;
    // Put packet into transmit buffer
    myAssert(transmitBuffer.notFull, "transmitBuffer overflow");
    transmitBuffer.enq(Packet { dest: zeroExtend(req.dest), payload: msg });
    // Put response
    myAssert(transmitRespBuffer.notFull, "transmitRespBuffer overflow");
    transmitRespBuffer.enq(TransmitResp { id: req.id });
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
  interface In packetIn   = packetInPort.in;

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
      inFlightTransmitResps.dec;
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

  interface BOut packetOut;
    method Action get;
      transmitBuffer.deq;
      inFlightTransmits.dec;
    endmethod
    method Bool valid = transmitBuffer.canDeq;
    method Packet value = transmitBuffer.dataOut;
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
  method Action send(ThreadId id, PacketDest dest, MailboxThreadMsgAddr addr);
  // Scratchpad address of message received
  // (Valid on 2nd cycle after call to "recv")
  method Bit#(32) recvAddr;
  // Tinsel core's interface to the mailbox
  interface MailboxClient client;
endinterface

module mkMailboxClientUnit#(CoreId myId) (MailboxClientUnit);
  // Ports
  OutPort#(ScratchpadReq) scratchpadReqPort  <- mkOutPort;
  InPort#(ScratchpadResp) scratchpadRespPort <- mkInPort;
  OutPort#(AllocReq)      allocReqPort       <- mkOutPort;
  OutPort#(TransmitReq)   transmitPort       <- mkOutPort;
  InPort#(TransmitResp)   transmitRespPort   <- mkInPort;
  InPort#(ReceiveAlert)   alertPort          <- mkInPort;

  // Transmit logic
  // ==============

  // Transmit queue, big enough to hold one request for each thread
  SizedQueue#(`LogThreadsPerCore, TransmitReq) transmitQueue <-
    mkUGSizedQueue;

  // Track whether each thread can send a new message
  // (We allow each thread to have at most one in-flight send
  // at a time, to implement the can-send instruction)
  Vector#(`LogThreadsPerCore, SetReset) canThreadSend <-
    replicateM(mkSetReset(True));

  // Flag indicating whether client thread can send
  Reg#(Bool) canSendReg1 <- mkConfigReg(False);
  Reg#(Bool) canSendReg2 <- mkConfigReg(False);

  rule transmit;
    canSendReg2 <= canSendReg1;
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

  // One queue of unread message-pointers per thread
  ArrayOfQueue#(`LogThreadsPerCore,
                `LogMsgsPerThread,
                MailboxThreadMsgAddr) unread;
  `ifdef MailboxClientUseSet
  unread <- mkArrayOfSetCompat;
  `else
  unread <- mkArrayOfQueue;
  `endif
  
  // Receive unit state
  Reg#(Bit#(2))      recvState <- mkConfigReg(0);
  Reg#(ReceiveAlert) alertReg  <- mkConfigRegU;

  rule receive0 (recvState == 0);
    if (alertPort.canGet) begin
      alertPort.get;
      alertReg <= alertPort.value;
      recvState <= 1;
    end
  endrule

  rule receive1 (recvState == 1);
    if (unread.canEnq) begin
      unread.enq(truncate(alertReg.id), alertReg.index);
      recvState <= 2;
    end
  endrule

  rule receive2 (recvState == 2);
    recvState <= 3;
  endrule

  rule receive3 (recvState == 3);
    if (unread.didEnq) begin
      if (alertPort.canGet) begin
        alertPort.get;
        alertReg <= alertPort.value;
        recvState <= 1;
      end else
        recvState <= 0;
    end else
      recvState <= 1;
  endrule

  // Methods
  // =======

  method Action prepare(ThreadId id);
    myAssert(unread.canTryDeq(id), "MailboxClientUnit: prepare vilation");
    unread.tryDeq(id);
    canSendReg1 <= canThreadSend[id].value;
  endmethod

  method Bool canRecv = unread.canDeq;

  method Action recv;
    myAssert(unread.canDeq, "MailboxClientUnit: recv violation");
    unread.deq;
  endmethod

  method Bool canSend = canSendReg2;

  method Action send(ThreadId id, PacketDest dest, MailboxThreadMsgAddr addr);
    myAssert(canSendReg2, "MailboxClientUnit: send violation");
    // Construct transmit request
    TransmitReq req;
    req.id = {truncate(myId), id};
    req.msgIndex = addr;
    req.dest = dest;
    // Put in queue
    myAssert(transmitQueue.notFull, "MailboxClientUnit: transmitQueue full");
    transmitQueue.enq(req);
    // Mark thread as busy
    canThreadSend[id].clear;
  endmethod

  method Bit#(32) recvAddr = msgAddrToByteAddr(unread.itemOut);

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
