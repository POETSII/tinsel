// Copyright (c) Matthew Naylor

package SFU;

// Shared Function Unit
// ====================
//
// Provides functions sharable by any number of cores.
// Currently, this only provides a 33-bit multiplier.

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

// =============================================================================
// Types
// =============================================================================

// A single SFU may be shared my several multi-threaded cores
typedef Bit#(`LogThreadsPerSFU) SFUClientId;

typedef enum {
  MulLower, // Compute bits [31:0] of 66-bit result
  MulUpper  // Compute bits [63:32] of 66-bit result
} SFUOp deriving (Bits, Eq);

typedef struct {
  SFUClientId id;
  SFUOp op;
  Bit#(33) argA;
  Bit#(33) argB;
} SFUReq deriving (Bits);

typedef struct {
  SFUClientId id;
  Bit#(32) data;
} SFUResp deriving (Bits);

// Token for the multipler pipeline
typedef struct {
  SFUClientId id;
  SFUOp op;
  Bit#(66) data;
} SFUMulToken deriving (Bits);

// =============================================================================
// Interface
// =============================================================================

interface SFU;
  interface In#(SFUReq) reqIn;
  interface BOut#(SFUResp) respOut;
endinterface

// =============================================================================
// Implementation
// =============================================================================

(* synthesize *)
module mkSFU (SFU);

  // Request & response ports
  InPort#(SFUReq) reqPort <- mkInPort;
  OutPort#(SFUResp) respPort <- mkOutPort;

  // Multiplier
  Mult#(33) mult <- mkSignedMult;

  // Pipeline stages for multiplier
  Reg#(Bool) mult2Fire <- mkDReg(False);
  Reg#(SFUMulToken) mult2Input <- mkConfigRegU;
  Reg#(Bool) mult3Fire <- mkDReg(False);
  Reg#(SFUMulToken) mult3Input <- mkConfigRegU;
  Reg#(Bool) mult4Fire <- mkDReg(False);
  Reg#(SFUMulToken) mult4Input <- mkConfigRegU;

  // Response buffer
  `define LogResultBufferLen 4
  SizedQueue#(`LogResultBufferLen, SFUResp) respBuffer <-
    mkUGSizedQueuePrefetch;

  // Track the number of in-flight requests
  Count#(TAdd#(`LogResultBufferLen, 1)) inflightCount <-
    mkCount(2 ** `LogResultBufferLen);

  rule mulStage1 (reqPort.canGet && inflightCount.notFull);
    reqPort.get;
    SFUReq req = reqPort.value;
    SFUMulToken token;
    token.id = req.id;
    token.op = req.op;
    token.data = mult.mult(req.argA, req.argB);
    inflightCount.inc;
    mult2Input <= token;
    mult2Fire <= True;
  endrule

  rule mulStage2;
    mult3Input <= mult2Input;
    mult3Fire <= mult2Fire;
  endrule

  rule mulStage3;
    mult4Input <= mult3Input;
    mult4Fire <= mult3Fire;
  endrule

  rule mulStage4 (mult4Fire);
    SFUMulToken token = mult4Input;
    myAssert(respBuffer.notFull, "SFU response buffer overflow");
    Bit#(32) ans = token.op == MulLower ? token.data[31:0]
                                        : token.data[63:32];
    SFUResp resp;
    resp.id = token.id;
    resp.data = ans;
    respBuffer.enq(resp);
  endrule

  interface In reqIn = reqPort.in;
  interface BOut respOut;
    method Action get;
      respBuffer.deq;
      inflightCount.dec;
    endmethod
    method Bool valid = respBuffer.canDeq && respBuffer.canPeek;
    method SFUResp value = respBuffer.dataOut;
  endinterface

endmodule

// ============================================================================
// SFU client
// ============================================================================

interface SFUClient;
  interface Out#(SFUReq) sfuReqOut;
  interface In#(SFUResp) sfuRespIn;
endinterface

// ============================================================================
// Connections
// ============================================================================

module connectCoresToSFU#(
    Vector#(`CoresPerSFU, SFUClient) clients, SFU sfu) ();

  // Connect requests
  function getSFUReqOut(client) = client.sfuReqOut;
  let sfuReqs <- mkMergeTree(Fair,
                      mkUGShiftQueue1(QueueOptFmax),
                      map(getSFUReqOut, clients));
  connectUsing(mkUGQueue, sfuReqs, sfu.reqIn);

  // Connect responses
  function Bit#(`LogCoresPerSFU) getSFURespKey(SFUResp resp) =
    truncateLSB(resp.id);
  function getSFURespIn(client) = client.sfuRespIn;
  let sfuResps <- mkResponseDistributor(
                      getSFURespKey,
                      mkUGShiftQueue1(QueueOptFmax),
                      map(getSFURespIn, clients));
  connectDirect(sfu.respOut, sfuResps);

endmodule

endpackage
