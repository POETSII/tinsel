package Mem;

// ============================================================================
// Imports
// ============================================================================

import Queue :: *;

// ============================================================================
// Types
// ============================================================================

// Unique identifier per data cache
typedef Bit#(`LogNumDCaches) DCacheId;

// Memory address
typedef TSub#(30, `LogWordsPerLine) MemAddrNumBits;
typedef Bit#(MemAddrNumBits) MemAddr;

// Load request
typedef struct {
  DCacheId id;
  MemAddr addr;
} MemLoadReq deriving (Bits);

// Store request
typedef struct {
  DCacheId id;
  MemAddr addr;
  Bit#(`LineSize) data;
} MemStoreReq deriving (Bits);

// General request
typedef struct {
  Bool isStore;
  DCacheId id;
  MemAddr addr;
  Bit#(`LineSize) data;
} MemReq deriving (Bits);

// Load response
typedef struct {
  DCacheId id;
  Bit#(`LineSize) data;
} MemLoadResp deriving (Bits);

// Store response
typedef struct {
  DCacheId id;
} MemStoreResp deriving (Bits);

// ============================================================================
// Interfaces
// ============================================================================

// Request interface
interface Req#(type t);
  method Bool canPut;
  method Action put(t req);
endinterface

// Response interface
interface Resp#(type t);
  method Bool canGet;
  method ActionValue#(t) get();
endinterface

// Memory interface with seperate load & store, request & response streams
interface MemDualReqResp;
  interface Req#(MemLoadReq) loadReq;
  interface Req#(MemStoreReq) storeReq;
  interface Resp#(MemLoadResp) loadResp;
  interface Resp#(MemStoreResp) storeResp;
endinterface

// Memory interface with seperate load & store response streams
interface MemDualResp;
  interface Req#(MemReq) req;
  interface Resp#(MemLoadResp) loadResp;
  interface Resp#(MemStoreResp) storeResp;
endinterface

// ============================================================================
// Merge load and store requests into single stream
// ============================================================================

module mkMergeLoadStoreReqs#(MemDualResp mem) (MemDualReqResp);
  // Output queue
  Queue1#(MemReq) outQueue <- mkUGQueue1;

  // Load-request queue
  Queue1#(MemLoadReq) loadQueue <- mkUGQueue1;

  // Wires
  PulseWire putStoreWire <- mkPulseWire;
  Wire#(MemStoreReq) putStoreReqWire <- mkDWire(?);
  
  // Emit output requests from output queue
  rule emit (mem.req.canPut && outQueue.notEmpty);
    outQueue.deq;
    mem.req.put(outQueue.dataOut);
  endrule

  // Merge requests
  rule merge (outQueue.notFull);
    // Enqueue a new output request?
    Bool enqOut = False;
    // Merged reqest
    MemReq req = ?;
    // Give priority to store requests
    if (putStoreWire) begin
      enqOut = True;
      req.isStore = True;
      req.id = putStoreReqWire.id;
      req.addr = putStoreReqWire.addr;
      req.data = putStoreReqWire.data;
    end else if (loadQueue.notEmpty) begin
      enqOut = True;
      req.isStore = False;
      req.id = loadQueue.dataOut.id;
      req.addr = loadQueue.dataOut.addr;
      loadQueue.deq;
    end
    if (enqOut) outQueue.enq(req);
  endrule

  interface Req loadReq;
    method Bool canPut = loadQueue.notFull;
    method Action put(MemLoadReq req);
      loadQueue.enq(req);
    endmethod
  endinterface

  interface Req storeReq;
    method Bool canPut = outQueue.notFull;
    method Action put(MemStoreReq req);
      putStoreWire.send;
      putStoreReqWire <= req;
    endmethod
  endinterface

  interface Resp loadResp = mem.loadResp;
  interface Resp storeResp = mem.storeResp;
endmodule

endpackage
