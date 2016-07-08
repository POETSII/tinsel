package Mem;

// ============================================================================
// Types
// ============================================================================

// Unique identifier per data cache
typedef Bit#(`LogNumDCaches) DCacheId;

// Load request
typedef struct {
  DCacheId id;
  Bit#(32) addr;
} MemLoadReq deriving (Bits);

// Store request
typedef struct {
  DCacheId id;
  Bit#(32) addr;
  Bit#(`LineSize) data;
} MemStoreReq deriving (Bits);

// Load response
typedef struct {
  DCacheId id;
  Bit#(`LineSize) data;
} MemLoadResp deriving (Bits);

// Store response
typedef struct {
  DCacheId id;
  Bit#(`LineSize) data;
} MemStoreResp deriving (Bits);

// Memory request
typedef union tagged {
  MemLoadReq LoadReq;
  MemStoreReq StoreReq;
} MemReq deriving (Bits);

// ============================================================================
// Interface
// ============================================================================

interface Mem;
  method Bool canPutLoad;
  method Action putLoadReq(MemLoadReq req);
  method Bool canPutStore;
  method Action putStoreReq(MemStoreReq req);
  method Bool canGetLoad;
  method ActionValue#(MemLoadResp) getLoadResp;
  method Bool canGetStore;
  method ActionValue#(MemStoreResp) getStoreResp;
endinterface

endpackage
