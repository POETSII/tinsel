package Globals;

// Core-local thread id
typedef Bit#(`LogThreadsPerCore) ThreadId;

// Board-local core id
typedef Bit#(`LogCoresPerBoard) CoreId;

// Board id
typedef struct {
  Bit#(`MeshYBits) y;
  Bit#(`MeshXBits) x;
} BoardId deriving (Eq, Bits);

// Network address
typedef struct {
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

// ============================================================================
// Address mapping
// ============================================================================

// Map a tinsel memory address to a DRAM address.
// The bottom and top halves of memory both map to the same DRAM memory.
// But the interleaving translation is applied to the upper top
// quarter of memory.
function Bit#(32) mapAddress(Bit#(32) memAddr);
  Bit#(`LogBytesPerDRAM) addr = truncate(memAddr);
  // Separate DRAM address into MSB and rest
  Bit#(1) msb = truncateLSB(addr);
  Bit#(TSub#(`LogBytesPerDRAM, 1)) rest = truncate(addr);
  // The bottom bits address bytes within a line
  Bit#(`LogBytesPerLine) bottom = truncate(rest);
  Bit#(TSub#(TSub#(`LogBytesPerDRAM, 1), `LogBytesPerLine)) middle =
    truncateLSB(rest);
  // Separate upper half of address space into partition index and offset
  Bit#(`LogThreadsPerDRAM) partIndex = truncateLSB(middle);
  let partOffset = truncate(middle);
  // Produce DRAM address
  let res = memAddr[`LogBytesPerDRAM] == 0 ? addr :
           (msb == 0 ? addr : {msb, partOffset, partIndex, bottom});
  return {1'b0, res};
endfunction

endpackage
