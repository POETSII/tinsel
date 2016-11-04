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
// Packets
// ============================================================================

// We use the term "message" to refer to packet payload
typedef TMul#(`WordsPerMsg, 32) MsgBits;
typedef Bit#(MsgBits) Msg;

// Desination address of a packet
typedef Bit#(`LogMaxThreads) PacketDest;

// Packet type
typedef struct {
  // Destination address
  PacketDest dest;
  // Payload
  Msg payload;
} Packet deriving (Bits);

endpackage
