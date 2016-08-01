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

// General response
typedef struct {
  Bool isStore;
  DCacheId id;
  Bit#(`LineSize) data;
} MemResp deriving (Bits);

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

endpackage
