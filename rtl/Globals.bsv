package Globals;

// Core-local thread id
typedef Bit#(`LogThreadsPerCore) ThreadId;

// Board-local core id
typedef Bit#(`LogCoresPerBoard) CoreId;

// Board id
typedef struct {
  Bit#(`LogMeshXLen) x;
  Bit#(`LogMeshYLen) y;
} BoardId deriving (Eq, Bits);

// Network address
typedef struct {
  // MSB denotes one of two address spaces:
  //   (0) thread address space
  //   (1) management address space
  Bit#(1) space;
  BoardId board;
  CoreId core;
  ThreadId thread;
} NetAddr deriving (Bits, Eq);

// ============================================================================
// Messages
// ============================================================================

// Message length in flits
// (A length of N corresponds to N+1 flits)
typedef Bit#(`LogMaxFlitsPerMsg) MsgLen;

// Flit payload
typedef Bit#(TMul#(`WordsPerFlit, 32)) FlitPayload;

// Flit type
typedef struct {
  // Destination address
  NetAddr dest;
  // Payload
  FlitPayload payload;
  // Is this the final flit in the message?
  Bool notFinalFlit;
} Flit deriving (Bits);

// A padded flit is a multiple of 64 bits
// (i.e. the data width of the 10G MAC interface)
typedef TMul#(TDiv#(SizeOf#(Flit), 64), 64) PaddedFlitNumBits;
typedef Bit#(PaddedFlitNumBits) PaddedFlit;

// Padding functions
function PaddedFlit padFlit(Flit flit) = {?, pack(flit)};
function Flit unpadFlit(PaddedFlit flit) = unpack(truncate(pack(flit)));

// ============================================================================
// Caches
// ============================================================================

// Unique identifier per data cache
typedef Bit#(`LogDCachesPerDRAM) DCacheId;

endpackage
