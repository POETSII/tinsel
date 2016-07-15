package TraceGen;

// ============================================================================
// Imports
// ============================================================================

import Mem     :: *;
import FIFOF   :: *;
import DCache  :: *;
import DRAM    :: *;
import Vector  :: *;
import RegFile :: *;
import Assert  :: *;

// Interface to C functions
import "BDPI" function ActionValue#(Bit#(32)) getUInt32();
import "BDPI" function ActionValue#(Bit#(8)) getChar();

// ============================================================================
// Constants
// ============================================================================

// Operator encoding
Bit#(8) opEND   = 69;  // 'E': end of operation stream
Bit#(8) opDELAY = 68;  // 'D': delay for some number of cycles
Bit#(8) opLW    = 76;  // 'L': load word
Bit#(8) opSW    = 83;  // 'S': store word
Bit#(8) opBACK  = 66;  // 'B': apply back-pressure for some number of cycles

// ============================================================================
// Types
// ============================================================================

// TraceGen request
// (Fields read straight from stdin are all 32 bits)
typedef struct {
  Bit#(8)  op;
  Bit#(32) threadId;
  Bit#(32) addr;
  Bit#(32) data;
  Bit#(32) delay;
} TraceGenReq deriving (Bits);

// ============================================================================
// Implementation
// ============================================================================

module traceGen ();
  // State
  // -----

  // DRAM instance
  let dram <- mkDRAM;

  // Data cache instance (id 0) connected directly to DRAM instance
  let dcache <- mkDCache(0, dram);

  // Record in-flight requests (max of one outstanding request per thread)
  RegFile#(DCacheClientId, TraceGenReq) inFlight <-
    mkRegFileWCF(minBound, maxBound);
  Vector#(TExp#(SizeOf#(DCacheClientId)), Reg#(Bool)) inFlightValid <-
    replicateM(mkReg(False));

  // Raw requests from stdin
  FIFOF#(TraceGenReq) rawReqs <- mkFIFOF;

  // Goes high when the opEND operation is encountered
  Reg#(Bool) allReqsGathered <- mkReg(False);

  // Countdown timer
  Reg#(Bit#(32)) countdown <- mkReg(0);

  // Back-pressure
  Reg#(Bit#(32)) backPressure <- mkReg(0);
  RWire#(Bit#(32)) backPressureWire <- mkRWire;

  // Constants
  // ---------

  Integer maxThreadId = 2**valueOf(SizeOf#(DCacheClientId))-1;

  // Rules
  // -----

  // Read raw requests from stdin and enqueue to rawReqs
  rule gatherRequests (! allReqsGathered);
    TraceGenReq req = ?;
    let op <- getChar();
    req.op = op;
    if (op == opEND)
      allReqsGathered <= True;
    else if (op == opDELAY || op == opBACK) begin
      let delay <- getUInt32();
      req.delay = delay;
      rawReqs.enq(req);
    end else begin
      let threadId <- getUInt32();
      dynamicAssert(threadId <= fromInteger(maxThreadId),
                      "TraceGen.bsv: thread id too large");
      req.threadId = threadId;
      let addr <- getUInt32();
      req.addr = addr;
      if (op == opSW) begin
        let data <- getUInt32();
        req.data = data;
      end
      rawReqs.enq(req);
    end
  endrule

  // Both the request and responses handlers update the in-flight
  // valid bits, but they never both assign to the same bit
  (* conflict_free = "issueRequests, receiveResponses" *)

  // Issue requests to data cache
  rule issueRequests (countdown == 0);
    TraceGenReq rawReq = rawReqs.first;
    if (rawReq.op == opDELAY) begin
      rawReqs.deq;
      countdown <= rawReq.delay;
    end else if (rawReq.op == opBACK) begin
      rawReqs.deq;
      backPressureWire.wset(rawReq.delay);
    end else begin
      DCacheClientId id = truncate(rawReq.threadId);
      if (!inFlightValid[id] && dcache.canPut) begin
        rawReqs.deq;
        DCacheReq req = ?;
        req.id = id;
        req.cmd.isLoad  = rawReq.op == opLW ? True : False;
        req.cmd.isStore = rawReq.op == opSW ? True : False;
        req.addr = rawReq.addr;
        req.data = rawReq.data;
        req.byteEn = -1;
        dcache.putReq(req);
        inFlightValid[id] <= True;
        inFlight.upd(id, rawReq);
      end
    end
  endrule

  // Receive responses from data cache
  rule receiveResponses (dcache.canGet && backPressure == 0);
    DCacheResp resp <- dcache.getResp;
    DCacheClientId id = resp.id;
    TraceGenReq req = inFlight.sub(id);
    dynamicAssert(inFlightValid[id],
                    "TraceGen.bsv: response has no associated request");
    if (req.op == opLW)
      $display("%d: M[%d] == %d", id, req.addr, resp.data);
    else if (req.op == opSW)
      $display("%d: M[%d] := %d", id, req.addr, req.data);
    inFlightValid[id] <= False;
  endrule

  // Delay for some number of cycles
  rule countdownTimer (countdown != 0);
    countdown <= countdown-1;
  endrule

  // Apply back-pressure for some number of cycles
  rule applyBackPressure;
    case (backPressureWire.wget()) matches
      tagged Invalid:
        if (backPressure != 0) backPressure <= backPressure-1;
      tagged Valid .delay: begin
        backPressure <= delay;
      end
    endcase
  endrule

  // Termination condition
  rule terminate ( allReqsGathered
                && !any(id, readVReg(inFlightValid))
                && !rawReqs.notEmpty );
    $finish(0);
  endrule
endmodule

endpackage
