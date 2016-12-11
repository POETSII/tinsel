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
