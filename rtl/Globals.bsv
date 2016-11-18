package Globals;

// Global types and interfaces defined here.

// ============================================================================
// Cores
// ============================================================================

// Global core id
typedef Bit#(`LogMaxCores) CoreId;

// Core-local thread id
typedef Bit#(`LogThreadsPerCore) ThreadId;

// ============================================================================
// Caches
// ============================================================================

// Unique identifier per data cache
typedef Bit#(`LogDCachesPerDRAM) DCacheId;

// ============================================================================
// External memory
// ============================================================================

// Memory address
typedef TSub#(30, `LogWordsPerBeat) MemAddrNumBits;
typedef Bit#(MemAddrNumBits) MemAddr;

// Cache line address
typedef Bit#(TSub#(MemAddrNumBits, `LogBeatsPerLine)) MemLineAddr;

// General request
typedef struct {
  Bool isStore;
  DCacheId id;
  MemAddr addr;
  Bit#(`BeatWidth) data;
  Bit#(`BeatBurstWidth) burst;
} MemReq deriving (Bits);

// Load response
typedef struct {
  DCacheId id;
  Bit#(`BeatWidth) data;
} MemLoadResp deriving (Bits);

// Store response
typedef struct {
  DCacheId id;
} MemStoreResp deriving (Bits);

// ============================================================================
// Messages
// ============================================================================

// Message length in flits
// (A length of N corresponds to N+1 flits)
typedef Bit#(`LogMaxFlitsPerMsg) MsgLen;

// Flit payload
typedef Bit#(TMul#(`WordsPerFlit, 32)) FlitPayload;

// Desination address of a message
typedef Bit#(`LogMaxThreads) FlitDest;

// Flit type
typedef struct {
  // Destination address
  FlitDest dest;
  // Payload
  FlitPayload payload;
  // Is this the final flit in the message? (Active-low)
  Bool notFinalFlit;
} Flit deriving (Bits);

endpackage
