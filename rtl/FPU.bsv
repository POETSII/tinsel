// Copyright (c) Matthew Naylor

package FPU;

// =============================================================================
// Imports
// =============================================================================

import Queue     :: *;
import Interface :: *;
import Mult      :: *;
import Util      :: *;
import Vector    :: *;
import DReg      :: *;
import ConfigReg :: *;
import FPUOps    :: *;

// =============================================================================
// Types
// =============================================================================

// A single FPU may be shared my several multi-threaded cores
typedef Bit#(`LogThreadsPerFPU) FPUClientId;

typedef enum {
  IntMult,
  FPAddSub,
  FPMult,
  FPDiv,
  FPToInt,
  FPFromInt,
  FPCompare
} FPUOpcode deriving (Bits, Eq, FShow);

// FPU request
typedef struct {
  FPUClientId id;
  FPUOpcode opcode;
  FPUOpInput in;
} FPUReq deriving (Bits, FShow);

// FPU response
typedef struct {
  FPUClientId id;
  FPUOpOutput out;
} FPUResp deriving (Bits);

// FPU pipeline token
typedef struct {
  Bool valid;
  FPUOpcode opcode;
  FPUClientId id;
} FPUToken deriving (Bits, FShow);

// Invalid token
FPUToken invalidFPUToken = FPUToken { valid : False, opcode : ?, id : ? };

// =============================================================================
// Interface
// =============================================================================

interface FPU;
  interface In#(FPUReq) reqIn;
  interface BOut#(FPUResp) respOut;
endinterface

// =============================================================================
// Implementation
// =============================================================================

(* synthesize *)
module mkFPU (FPU);

  // Request & response ports
  InPort#(FPUReq) reqPort <- mkInPort;
  OutPort#(FPUResp) respPort <- mkOutPort;

  // FPU operations
  FPUOp intMult   <- mkIntMult;
  FPUOp fpAddSub  <- mkFPAddSub;
  FPUOp fpMult    <- mkFPMult;
  FPUOp fpDiv     <- mkFPDiv;
  FPUOp fpToInt   <- mkFPToInt;
  FPUOp fpFromInt <- mkFPFromInt;
  FPUOp fpCompare <- mkFPCompare;
 
  // Response buffer
  `define LogResultBufferLen 5
  SizedQueue#(`LogResultBufferLen, FPUResp) respBuffer <-
    mkUGSizedQueuePrefetch;

  // Track the number of in-flight requests
  Count#(TAdd#(`LogResultBufferLen, 1)) inflightCount <-
    mkCount(2 ** `LogResultBufferLen);

  // A shift register of FPU tokens.
  // The value tokens[i] respresents an operation whose result is
  // available in i-1 clock cycles; tokens[0] is unused.
  Vector#(TAdd#(`FPUOpMaxLatency, 2), Reg#(FPUToken)) tokens <-
    replicateM(mkConfigReg(invalidFPUToken));
 
  // Supply inputs to FPU operations
  rule supply;
    FPUReq req = reqPort.value;
    intMult.put(req.in);
    fpAddSub.put(req.in);
    fpMult.put(req.in);
    fpDiv.put(req.in);
    fpToInt.put(req.in);
    fpFromInt.put(req.in);
    fpCompare.put(req.in);
  endrule

  // Consume request
  rule consume;
    FPUReq req = reqPort.value;
    Bool doConsume = False;
    for (Integer i = 1; i <= `FPUOpMaxLatency; i=i+1) begin
      Bool canConsume =
        reqPort.canGet && inflightCount.notFull && !tokens[i+1].valid &&
             ( i == `IntMultLatency   && req.opcode == IntMult
            || i == `FPAddSubLatency  && req.opcode == FPAddSub
            || i == `FPMultLatency    && req.opcode == FPMult
            || i == `FPDivLatency     && req.opcode == FPDiv
            || i == `FPConvertLatency && req.opcode == FPToInt
            || i == `FPConvertLatency && req.opcode == FPFromInt
            || i == `FPCompareLatency && req.opcode == FPCompare
             );
      if (canConsume) begin
        doConsume = True;
        FPUToken token;
        token.id = req.id;
        token.opcode = req.opcode;
        token.valid = True;
        tokens[i] <= token;
      end else
        tokens[i] <= tokens[i+1];
    end
    if (doConsume) begin
      reqPort.get;
      inflightCount.inc;
    end
  endrule

  // Produce response
  rule produce;
    FPUResp resp;
    resp.id = tokens[1].id;
    resp.out =
      case (tokens[1].opcode) matches
        IntMult:   intMult.out;
        FPAddSub:  fpAddSub.out;
        FPMult:    fpMult.out;
        FPDiv:     fpDiv.out;
        FPFromInt: fpFromInt.out;
        FPToInt:   fpToInt.out;
        FPCompare: fpCompare.out;
      endcase;
    if (tokens[1].valid) begin
      respBuffer.enq(resp);
    end
  endrule

  interface In reqIn = reqPort.in;
  interface BOut respOut;
    method Action get;
      respBuffer.deq;
      inflightCount.dec;
    endmethod
    method Bool valid = respBuffer.canDeq && respBuffer.canPeek;
    method FPUResp value = respBuffer.dataOut;
  endinterface

endmodule

// ============================================================================
// FPU client
// ============================================================================

interface FPUClient;
  interface Out#(FPUReq) fpuReqOut;
  interface In#(FPUResp) fpuRespIn;
endinterface

// ============================================================================
// Connections
// ============================================================================

module connectCoresToFPU#(
    Vector#(`CoresPerFPU, FPUClient) clients, FPU fpu) ();

  // Connect requests
  function getFPUReqOut(client) = client.fpuReqOut;
  let fpuReqs <- mkMergeTree(Fair,
                      mkUGShiftQueue1(QueueOptFmax),
                      map(getFPUReqOut, clients));
  connectUsing(mkUGQueue, fpuReqs, fpu.reqIn);

  // Connect responses
  function Bit#(`LogCoresPerFPU) getFPURespKey(FPUResp resp) =
    truncateLSB(resp.id);
  function getFPURespIn(client) = client.fpuRespIn;
  let fpuResps <- mkResponseDistributor(
                      getFPURespKey,
                      mkUGShiftQueue1(QueueOptFmax),
                      map(getFPURespIn, clients));
  connectDirect(fpu.respOut, fpuResps);

endmodule

endpackage
