// SPDX-License-Identifier: BSD-2-Clause
package DCacheTypes;

// Unique identifier per data cache
typedef Bit#(`LogDCachesPerDRAM) DCacheId;

// Cache way
typedef Bit#(`DCacheLogNumWays) Way;

// A single DCache may be shared my several multi-threaded cores
typedef TAdd#(`LogThreadsPerCore, `LogCoresPerDCache) DCacheClientIdBits;
typedef Bit#(DCacheClientIdBits) DCacheClientId;

// DCache client request command (one hot encoding)
typedef struct {
  Bool isLoad;      // Load
  Bool isStore;     // Store
  Bool isFlush;     // Perform cache flush
  Bool isFlushResp; // Flush response (only used internally)
} DCacheReqCmd deriving (Bits);

// DCache client request structure
typedef struct {
  DCacheClientId id;
  DCacheReqCmd cmd;
  Bit#(32) addr;
  Bit#(32) data;
  Bit#(4) byteEn;
} DCacheReq deriving (Bits);

// Details of a flush request: the 'addr' field specifies the
// line to evict and the 'data' field specifies the way.

// This structure contains DCache-related information about an
// in-flight memory request.  When the DCache issues a memory load
// request, this info is packed into the unused data field of the
// request.  When the memory subsystem responds, it passes back the
// same info in an extra field inside the memory response structure.
// Maintaining info about an inflight request inside the request itself
// provides an easy way to handle out-of-order responses from memory.
// We try to keep this structure as small as possible so that memory
// responses do not become too big.
typedef struct {
  DCacheReq req;
  Way way;
  Bit#(`LogBeatsPerLine) beat;
} InflightDCacheReqInfo deriving (Bits);

endpackage
