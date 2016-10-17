package Globals;

// Global types and interfaces defined here.

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
  Bit#(`BusWidth) data;
  Bit#(`BurstWidth) burst;
} MemReq deriving (Bits);

// Load response
typedef struct {
  DCacheId id;
  Bit#(`BusWidth) data;
} MemLoadResp deriving (Bits);

// Store response
typedef struct {
  DCacheId id;
} MemStoreResp deriving (Bits);

// General response
typedef struct {
  Bool isStore;
  DCacheId id;
  Bit#(`BusWidth) data;
} MemResp deriving (Bits);

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

// Memory interface with seperate load & store response streams
interface MemDualResp;
  interface Req#(MemReq) req;
  interface Resp#(MemLoadResp) loadResp;
  interface Resp#(MemStoreResp) storeResp;
endinterface

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
