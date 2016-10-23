package TestMailbox;

// ============================================================================
// Imports
// ============================================================================

import FIFOF     :: *;
import Vector    :: *;
import Interface :: *;
import ConfigReg :: *;
import Mailbox   :: *;
import Queue     :: *;
import Util      :: *;

// Interface to C functions
import "BDPI" function ActionValue#(Bit#(32)) getUInt32();
import "BDPI" function ActionValue#(Bit#(8)) getChar();

// ============================================================================
// Constants
// ============================================================================

// Operator encoding
Bit#(8) opSEND  = 83;  // 'S': send message from scratchpad
Bit#(8) opEND   = 69;  // 'E': end of operation stream
Bit#(8) opDELAY = 68;  // 'D': delay
Bit#(8) opBACK  = 66;  // 'B': apply back-pressure

// ============================================================================
// Types
// ============================================================================

// Mailbox request
// (Fields read straight from stdin are all 32 bits)
typedef struct {
  Bit#(8)  op;
  Bit#(32) src;
  Bit#(32) dst;
  Bit#(32) delay;
} MailboxReq deriving (Bits);

// ============================================================================
// Implementation
// ============================================================================

module testMailbox ();

  // Create mailbox
  let mb <- mkMailbox;

  // Connect packet-out to packet-in (i.e. only one mailbox)
  connectDirect(mb.packetOut, mb.packetIn);

  // Connect test driver to mailbox
  OutPort#(ScratchpadReq) spadReq   <- mkOutPort;
  InPort#(ScratchpadResp) spadResp  <- mkInPort;
  OutPort#(TransmitReq)   txReq     <- mkOutPort;
  InPort#(TransmitResp)   txResp    <- mkInPort;
  OutPort#(AllocReq)      allocReq  <- mkOutPort;
  InPort#(ReceiveAlert)   rxAlert   <- mkInPort;

  connectUsing(mkUGQueue, spadReq.out, mb.spadReqIn);
  connectDirect(mb.spadRespOut, spadResp.in);
  connectUsing(mkUGQueue, txReq.out, mb.txReqIn);
  connectDirect(mb.txRespOut, txResp.in);
  connectUsing(mkUGQueue, allocReq.out, mb.allocReqIn);
  connectDirect(mb.rxAlertOut, rxAlert.in);

  // Constants
  // ---------

  Integer maxThreadId = 2**valueOf(SizeOf#(MailboxClientId))-1;

  // Read raw requests from stdin and enqueue to mbReqs
  // --------------------------------------------------

  // Raw requests from stdin
  FIFOF#(MailboxReq) mbReqs <- mkUGFIFOF;

  // Goes high when the opEND operation is encountered
  Reg#(Bool) allReqsGathered <- mkReg(False);

  rule gatherRequests (! allReqsGathered && mbReqs.notFull);
    MailboxReq req = ?;
    let op <- getChar();
    req.op = op;
    if (op == opEND)
      allReqsGathered <= True;
    else if (op == opDELAY) begin
      let n <- getUInt32();
      req.delay = n;
      mbReqs.enq(req);
    end else if (op == opSEND) begin
      let src <- getUInt32();
      myAssert(src <= fromInteger(maxThreadId),
                 "TestMailbox.bsv: thread id too large");
      req.src = src;
      let dst <- getUInt32();
      myAssert(dst <= fromInteger(maxThreadId),
                 "TestMailbox.bsv: thread id too large");
      req.dst = dst;
      mbReqs.enq(req);
    end
  endrule

  // Initialisation
  // --------------

  Reg#(Bool) init <- mkReg(True);
  Reg#(Bool) initPhase1Done <- mkReg(False);
  Reg#(Bit#(TAdd#(`LogThreadsPerMailbox, 1))) initState <- mkReg(0);
  Count#(8) initStores <- mkCount(128);

  rule initialise (init && allocReq.canPut && spadReq.canPut);
    if (initPhase1Done) begin
      if (initStores.value == 0) init <= False;
    end else begin
      // Allocate block for receiving
      AllocReq req;
      req.id = truncateLSB(initState);
      req.msgIndex = 1 + zeroExtend(initState[0]);
      allocReq.put(req);
      // Initialise block for sending
      ScratchpadReq wreq;
      wreq.id = req.id;
      wreq.isStore = True;
      Bit#(`LogMsgsPerThread) base = 0;
      wreq.wordAddr = { base, signExtend(initState[0]) };
      wreq.data = zeroExtend(req.id) + (initState[0] == 0 ? 0 : 100);
      wreq.byteEn = -1;
      spadReq.put(wreq);
      initStores.inc;
      // State machine
      if (initState == -1)
        initPhase1Done <= True;
      else
        initState <= initState + 1;
    end
  endrule

  rule consumeInitStoreResps (init);
    if (spadResp.canGet) begin
      spadResp.get;
      initStores.dec;
    end
  endrule

  // Issue transmit requests to mailbox
  // ----------------------------------

  rule issueSendRequests(!init &&
                           mbReqs.notEmpty &&
                             mbReqs.first.op == opSEND &&
                               txReq.canPut);
    MailboxReq mbReq = mbReqs.first;
    TransmitReq req;
    req.id = truncate(mbReq.src);
    req.dest = truncate(mbReq.dst);
    req.msgIndex = 0;
    txReq.put(req);
    mbReqs.deq;
    $display("S %d %d", req.id, req.dest);
  endrule

  // Handle delay requests
  // ---------------------

  Reg#(Bit#(32)) delayCounter <- mkReg(0);

  rule delay (!init && mbReqs.notEmpty && mbReqs.first.op == opDELAY);
    if (delayCounter == mbReqs.first.delay) begin
      delayCounter <= 0;
      mbReqs.deq;
    end else
      delayCounter <= delayCounter+1;
  endrule

  // Handle alerts
  // -------------

  FIFOF#(Bit#(`LogMsgsPerThread)) inFlightRecv <- mkUGSizedFIFOF(16);
  Reg#(Bit#(1)) alertState <- mkReg(0);

  rule handleAlerts (!init && rxAlert.canGet &&
                       spadReq.canPut && inFlightRecv.notFull);
    ReceiveAlert alert = rxAlert.value;
    ScratchpadReq req = ?;
    req.id = truncate(alert.id);
    req.isStore = False;
    req.wordAddr = { alert.index, signExtend(alertState) };
    spadReq.put(req);
    alertState <= alertState + 1;
    if (alertState == 1) begin
      inFlightRecv.enq(alert.index);
      rxAlert.get;
    end
  endrule

  // Handle responses from mailbox
  // -----------------------------

  Reg#(Bit#(1)) recvState <- mkReg(0);
  Reg#(Bit#(32)) recvReg <- mkRegU;

  rule ignoreResponses;
    // Ignore transmit responses
    if (txResp.canGet) txResp.get;
  endrule

  rule handleResponses (!init && spadResp.canGet);
    ScratchpadResp resp = spadResp.value;
    if (resp.isStore) spadResp.get;
    else if (allocReq.canPut && inFlightRecv.notEmpty) begin
      // Handle load responses
      spadResp.get;
      recvReg <= resp.data;
      recvState <= recvState + 1;
      if (recvState == 1) begin
        inFlightRecv.deq;
        Bit#(`LogMsgsPerThread) index = inFlightRecv.first;
        // Issue allocate request
        AllocReq req;
        req.id = truncate(resp.id);
        req.msgIndex = index;
        allocReq.put(req);
        $display("R %d %d %d", resp.id, recvReg, resp.data);
      end
    end
  endrule

  // Termination condition
  // ---------------------

  rule terminate (allReqsGathered && !inFlightRecv.notEmpty);
    $finish(0);
  endrule
endmodule

endpackage
