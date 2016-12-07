// Copyright (c) Matthew Naylor

package DCache;

// ============================================================================
// Design overview
// ============================================================================
//
// This is an N-way set-associative write-back cache.  It will serve
// one or more highly-threaded cores, thus high throughput and high
// Fmax are much more important than low latency, allowing deep
// pipelining.  It employs a hash function that appends the thread id
// and some number of address bits, thus lines are not shared between
// threads.
//
// We assume there is a max of one request per thread in the cache
// pipeline at any time.  Together with the no-sharing property
// (above), this implies that in-flight requests are always operating
// on different lines -- hence, there are no dependencies between
// in-flight requests.  To allow clients to meet this assumption, we
// issue store responses as well as load responses.
//
// The block RAM used to cache data is a true dual-port mixed-width
// block RAM with a bus-sized port and a word-sized port; each port
// allows either a read or a write on each cycle.
//
// Cache lines are read and written in bus-sized chunks called beats.
//
// Pipeline structure
// ------------------
//
//            +-------------+      +----------------------+
// req  ----->| tag lookup  |<-----| memory response unit |
//            +-------------+      +----------------------+
//                 ||                               
//                 \/                               
//            +-------------+      +-----------+    
//            | data lookup |----->| miss unit |    
//            +-------------+      +-----------+    
//                 ||
//                 \/
//            +----------+
// resp <-----| hit unit |
//            +----------+
//
//
// NOTE: each pipeline stage may be composed of several pipelined
// sub-stages, e.g. tag and data lookup each have at least two
// sub-stages due to 2-cycle latent BRAMs
//
// Pipeline stages
// ---------------
//
// 1. tag lookup:
//   a. consume request from the "memory response unit", if present
//   b. otherwise, consume a fresh client request instead
//   c. send BRAM request for the tag
//
// 2. data lookup:
//   a. determine correct way
//   b. on read hit: send BRAM request for word data
//   c. on write hit: write word data to BRAM
//   d. on miss: send request to miss unit
// 
// 3. hit unit:
//   a. on hit: enqueue response FIFO
//   b. on miss: update tag
//   c. update meta data
//
// 4. memory response:
//   a. if response available:
//        * receive new line data from external memory
//        * write new line data to BRAM
//        * send request to (1), which will definitely hit this time
//
// 5. miss unit:
//   a. if old line is dirty: write each beat to memory
//   b. request new line data from memory

// ============================================================================
// Imports
// ============================================================================

import BlockRam  :: *;
import Queue     :: *;
import Globals   :: *;
import Util      :: *;
import Vector    :: *;
import DReg      :: *;
import Assert    :: *;
import ConfigReg :: *;
import Interface :: *;

// ============================================================================
// Types  
// ============================================================================

// A single DCache may be shared my several multi-threaded cores
typedef TAdd#(`LogThreadsPerCore, `LogCoresPerDCache) DCacheClientIdBits;
typedef Bit#(DCacheClientIdBits) DCacheClientId;

// Number of ways
typedef TExp#(`DCacheLogNumWays) DCacheNumWays;

// Way
typedef Bit#(`DCacheLogNumWays) Way;

// Client request command (one hot encoding)
typedef struct {
  Bool isLoad;
  Bool isStore;
} DCacheReqCmd deriving (Bits);

// Client request structure
typedef struct {
  DCacheClientId id;
  DCacheReqCmd cmd;
  Bit#(32) addr;
  Bit#(32) data;
  Bit#(4) byteEn;
} DCacheReq deriving (Bits);

// Client response structure
typedef struct {
  DCacheClientId id;
  Bit#(32) data;
} DCacheResp deriving (Bits);

// Fill request
typedef struct {
  DCacheReq req; // The request leading to the fill
  Way way;       // The way to fill
} Fill deriving (Bits);

// Index for a set in the tag array and the meta-data array
typedef TAdd#(`DCacheLogSetsPerThread, DCacheClientIdBits) SetIndexNumBits;
typedef Bit#(SetIndexNumBits) SetIndex;

// Index for a beat in the data array
typedef TAdd#(SetIndexNumBits, TAdd#(`DCacheLogNumWays, `LogBeatsPerLine))
  BeatIndexNumBits;
typedef Bit#(BeatIndexNumBits) BeatIndex;

// Index for a word in the data array
typedef TAdd#(BeatIndexNumBits, `LogWordsPerBeat) WordIndexNumBits;
typedef Bit#(WordIndexNumBits) WordIndex;

// Cache line tag
typedef struct {
  Bool valid;
  Key key;
} Tag deriving (Bits);

// A key holds the upper bits of an address
typedef TSub#(30, TAdd#(`DCacheLogSetsPerThread, `LogWordsPerLine)) KeyNumBits;
typedef Bit#(KeyNumBits) Key;

// Meta data per set
typedef struct {
  Way oldestWay;
  Vector#(DCacheNumWays, Bool) dirty;
} SetMetaData deriving (Bits);

// Data cache pipeline token
typedef struct {
  DCacheReq req;
  Vector#(DCacheNumWays, Bool) matching;
  Bool isHit;
  Way evictWay;
  Tag evictTag;
  Bool evictDirty;
  SetMetaData metaData;
} DCacheToken deriving (Bits);

// Miss request (input to miss unit)
typedef struct {
  DCacheReq req;
  Way evictWay;
  Tag evictTag;
  Bool evictDirty;
} MissReq deriving (Bits);

// Beat
typedef Bit#(`LogBeatsPerLine) Beat;

// Max number of in-flight cache requests
`define DCacheLogMaxInflight 5

// ============================================================================
// Functions
// ============================================================================

// Determine the set index given the thread id and address
function SetIndex setIndex(DCacheClientId id, Bit#(32) addr) =
  {id, truncate(addr[31:`LogBytesPerLine])};

// Determine the beat index in the data array
function BeatIndex beatIndex(
  Beat beat, DCacheClientId id, Bit#(32) addr, Way way) =
    {way, id, truncate(addr[31:`LogBytesPerLine]), beat};

// Determine the word index in the data array
function WordIndex wordIndex(DCacheClientId id, Bit#(32) addr, Way way) =
  {way, id, truncate(addr[31:2])};

// Determine the bits that make up a tag
function Key getKey(Bit#(32) addr) = truncateLSB(addr);

// Reconstruct a 32-bit address from an aliasing address and a tag
function Bit#(32) reconstructAddr(Key key, Bit#(32) addr);
  Bit#(`LogBytesPerLine) low = 0;
  return {key, truncate(addr[31:`LogBytesPerLine]), low};
endfunction

// ============================================================================
// Interface
// ============================================================================

interface DCache;
  interface In#(DCacheReq)    reqIn;
  interface BOut#(DCacheResp) respOut;
  interface In#(MemLoadResp)  loadRespIn;
  interface In#(MemStoreResp) storeRespIn;
  interface BOut#(MemReq)     reqOut;
endinterface

// ============================================================================
// Implementation
// ============================================================================

(* synthesize *)
module mkDCache#(DCacheId myId) (DCache);
  // Tag block RAM
  Vector#(DCacheNumWays, BlockRam#(SetIndex, Tag)) tagMem <-
    replicateM(mkBlockRam);

  // True dual-port mixed-width data block RAM
  // (One bus-sized port and one word-sized port)
  BlockRamTrueMixedBE#(BeatIndex, Bit#(`BeatWidth), WordIndex, Bit#(32))
    dataMem <- mkBlockRamTrueMixedBE;

  // Meta data for each set
  BlockRam#(SetIndex, SetMetaData) metaData <- mkBlockRam;
  
  // Request & response ports
  InPort#(DCacheReq)    reqPort       <- mkInPort;
  InPort#(MemLoadResp)  loadRespPort  <- mkInPort;
  InPort#(MemStoreResp) storeRespPort <- mkInPort;

  // The fill queue (16 elements) stores requests that have missed
  // while waiting for external memory to fetch the data.
  SizedQueue#(4, Fill) fillQueue <- mkUGSizedQueue;

  // The response queue buffers responses to the client
  SizedQueue#(`DCacheLogMaxInflight, DCacheResp) respQueue <-
    mkUGSizedQueuePrefetch;

  // The miss queue buffers requests to the miss unit
  SizedQueue#(`DCacheLogMaxInflight, MissReq) missQueue <- mkUGSizedQueue;

  // Track the number of in-flight requests
  Count#(TAdd#(`DCacheLogMaxInflight, 1)) inflightCount <-
    mkCount(2 ** `DCacheLogMaxInflight);

  // Pipeline state and control
  Reg#(DCacheToken) tagLookup2Input    <- mkVReg;
  Reg#(DCacheToken) dataLookup1Input   <- mkVReg;
  Reg#(DCacheToken) dataLookup2Input   <- mkConfigRegU;
  Reg#(Bool)        dataLookup2Trigger <- mkDReg(False);
  Reg#(DCacheToken) dataLookup3Input   <- mkVReg;
  Reg#(DCacheToken) hitUnitInput       <- mkVReg;
  Reg#(DCacheReq)   feedbackReq        <- mkConfigRegU;
  Reg#(Bool)        feedbackTrigger    <- mkDReg(False);

  // Line access unit
  // ----------------

  // There is a pipeline conflict between the dataLookup stage and the
  // missUnit stage: dataLookup wishes to fetch the old line
  // data for writeback, and missUnit wishes to write new line
  // data for a fill.  The line access unit resolves this conflict:
  // write takes priorty over read and the read wire must only be
  // asserted when the write wire is low.

  // Control wires for modifying lines in dataMem
  Wire#(BeatIndex) lineReadIndexWire <- mkDWire(?);
  Wire#(Bool) lineWriteReqWire <- mkDWire(False);
  Reg#(Bool) lineWriteReqReg <- mkReg(False);
  Wire#(BeatIndex) lineWriteIndexWire <- mkDWire(?);
  Reg#(Bit#(`BeatWidth)) lineWriteDataReg <- mkConfigRegU;
  Reg#(BeatIndex) lineIndexReg <- mkRegU;

  // Use wires to issue line access in dataMem
  rule lineAccessUnit;
    lineWriteReqReg <= lineWriteReqWire;
    lineIndexReg <= lineWriteReqWire ? lineWriteIndexWire : lineReadIndexWire;
    dataMem.putA(
      lineWriteReqReg,
      lineIndexReg,
      lineWriteDataReg);
  endrule

  // Tag lookup stage
  // ----------------

  rule tagLookup1 (feedbackTrigger || reqPort.canGet && inflightCount.notFull);
    // Select fresh client request or feedback request
    DCacheReq req = feedbackTrigger ? feedbackReq : reqPort.value;
    // Dequeue request
    if (! feedbackTrigger) begin
      inflightCount.inc;
      reqPort.get;
    end
    // Send read request for tags
    for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1)
      tagMem[i].read(setIndex(req.id, req.addr));
    // Send read request for meta data
    metaData.read(setIndex(req.id, req.addr));
    // Trigger next stage
    DCacheToken token = ?;
    token.req = req;
    tagLookup2Input <= token;
  endrule

  rule tagLookup2;
    DCacheToken token = tagLookup2Input;
    // Trigger next stage
    dataLookup1Input <= token;
  endrule

  // Data lookup stage
  // -----------------

  rule dataLookup1;
    DCacheToken token = dataLookup1Input;
    // Compute matching way (associative lookup)
    Vector#(DCacheNumWays, Tag) tags;
    Vector#(DCacheNumWays, Bool) matching;
    for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1) begin
      tags[i]     = tagMem[i].dataOut;
      matching[i] = tags[i].valid && tags[i].key == getKey(token.req.addr);
    end
    token.matching = matching;
    token.isHit = any(id, matching);
    // In case of a miss, choose a way to evict and remember the old tag
    token.evictWay   = metaData.dataOut.oldestWay;
    token.evictTag   = tags[token.evictWay];
    token.evictDirty = metaData.dataOut.dirty[token.evictWay];
    // Remember meta data for later stages
    token.metaData = metaData.dataOut;
    // Trigger next stage
    dataLookup2Trigger <= True;
    dataLookup2Input <= token;
  endrule

  rule dataLookup2 (dataLookup2Trigger);
    DCacheToken token = dataLookup2Input;
    // Convert index of match from one-hot to binary
    Way matchingWay = encode(token.matching);
    // Handle hit or miss
    if (token.isHit) begin
      // On read hit: read word data from dataMem
      // On write hit: write word data to dataMem
      dataMem.putB(token.req.cmd.isStore,
                   wordIndex(token.req.id, token.req.addr, matchingWay),
                   token.req.data, token.req.byteEn);
    end else begin
      // Put miss requests into miss queue
      MissReq miss;
      miss.req = token.req;
      miss.evictWay = token.evictWay;
      miss.evictTag = token.evictTag;
      miss.evictDirty = token.evictDirty;
      missQueue.enq(miss);
    end
    // Trigger next stage
    dataLookup3Input <= token;
  endrule

  rule dataLookup3;
    DCacheToken token = dataLookup3Input;
    hitUnitInput <= token;
  endrule

  // Hit unit
  // --------

  rule hitUnit1;
    DCacheToken token = hitUnitInput;
    // Has a new dirty bit been set?
    Bool setDirtyBit = False;
    // New dirty bits
    Vector#(DCacheNumWays, Bool) newDirtyBits = token.metaData.dirty;
    // Will a line been evicted?
    Bool willEvict = False;
    if (token.isHit) begin
      // On hit: enqueue response queue
      myAssert(respQueue.notFull, "DCache: response queue full");
      DCacheResp resp;
      resp.id = token.req.id;
      resp.data = dataMem.dataOutB;
      setDirtyBit = token.req.cmd.isStore;
      for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1)
        if (token.matching[i]) newDirtyBits[i] = True;
      respQueue.enq(resp);
    end else begin
      // On miss: update tag
      Tag newTag;
      newTag.valid = True;
      newTag.key = getKey(token.req.addr);
      for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1)
        if (token.evictWay == fromInteger(i)) begin
          newDirtyBits[i] = False;
          tagMem[i].write(setIndex(token.req.id, token.req.addr), newTag);
        end
      // A line will be evicted
      willEvict = True;
    end
    // Update meta data
    SetMetaData newMetaData;
    newMetaData.oldestWay = token.metaData.oldestWay + (willEvict ? 1 : 0);
    newMetaData.dirty = newDirtyBits;
    if (setDirtyBit || willEvict)
      metaData.write(setIndex(token.req.id, token.req.addr), newMetaData);
  endrule

  // Memory response stage
  // ---------------------

  // Beat counter for responses
  Reg#(Beat) respBeat <- mkReg(0);

  rule memResponse;
    // If new line data available from external memory, then:
    if (loadRespPort.canGet && fillQueue.canDeq && fillQueue.canPeek) begin
      // Remove item from fill queue and put associated request (which
      // will definitely hit if it starts again from the beginning of
      // the pipeline) into the hit buffer
      let fill = fillQueue.dataOut;
      if (allHigh(respBeat)) begin
        fillQueue.deq;
        feedbackReq <= fill.req;
        feedbackTrigger <= True;
      end
      // Write new line data to dataMem
      MemLoadResp resp = loadRespPort.value;
      loadRespPort.get;
      lineWriteReqWire   <= True;
      lineWriteIndexWire <= beatIndex(respBeat, fill.req.id,
                              fill.req.addr, fill.way);
      lineWriteDataReg <= resp.data;
      respBeat <= respBeat+1;
    end
  endrule

  // Miss unit
  // ---------

  // Memory request queue
  Queue#(MemReq) memReqQueue    <- mkUGShiftQueue(QueueOptFmax);

  // Index of next beat to read
  Reg#(Beat) reqBeat <- mkReg(0);

  // Has the writeback been completed?
  Reg#(Bool) writebackDone <- mkReg(False);

  // Is a beat ready on the output of dataMem?
  Reg#(Bit#(2)) missUnitState  <- mkReg(0);

  rule missUnit;
    MissReq miss = missQueue.dataOut;
    // Send a load request?
    Bool isLoad = !miss.evictDirty || writebackDone;
    // Determine line address
    MemLineAddr writeLineAddr =
      truncateLSB(reconstructAddr(
        miss.evictTag.key, miss.req.addr));
    MemLineAddr readLineAddr = truncateLSB(miss.req.addr);
    // Create memory request
    MemReq memReq;
    memReq.isStore = !isLoad;
    memReq.id = myId;
    memReq.addr = {isLoad ? readLineAddr : writeLineAddr, reqBeat};
    memReq.data = dataMem.dataOutA;
    memReq.burst = isLoad ? `BeatsPerLine : 1;
    // Are we going to send a memory request to the next stage?
    Bool sendMemReq = False;
    if (missQueue.canPeek && missQueue.canDeq && memReqQueue.notFull) begin
      if (missUnitState == 0) begin
        // Are we ready to request the new line?
        if (isLoad) begin
          if (fillQueue.notFull) begin
            missQueue.deq;
            writebackDone <= False;
            sendMemReq = True;
            // Put request in fill queue
            Fill fill;
            fill.req = miss.req;
            fill.way = miss.evictWay;
            fillQueue.enq(fill);
          end
        // Or are we still writing back old data?
        end else if (!lineWriteReqWire) begin
          // Assignment to lineReadIndexWire is performed below
          // (outside complex condition) to improve timing
          missUnitState <= 1;
        end
      end else if (missUnitState == 3) begin
        sendMemReq = True;
        reqBeat <= reqBeat+1;
        if (allHigh(reqBeat)) writebackDone <= True;
        missUnitState <= 0;
      end else
        missUnitState <= missUnitState+1;
    end
    // If reading old line data then where from?
    lineReadIndexWire <= beatIndex(reqBeat, miss.req.id,
                           miss.req.addr, miss.evictWay);
    // Send memory request to next stage
    if (sendMemReq) memReqQueue.enq(memReq);
  endrule

  // Discard store responses
  // -----------------------

  // Until the cache supports explicit flushing,
  // ignore all store responses from external memory
  rule discardStoreResps;
    if (storeRespPort.canGet) begin
      storeRespPort.get;
    end
  endrule

  // Interface
  // ---------

  interface In  reqIn       = reqPort.in;
  interface In  loadRespIn  = loadRespPort.in;
  interface In  storeRespIn = storeRespPort.in;

  interface BOut reqOut;
    method Action get;
      memReqQueue.deq;
    endmethod
    method Bool valid = memReqQueue.canDeq;
    method MemReq value = memReqQueue.dataOut;
  endinterface

  interface BOut respOut;
    method Action get;
      respQueue.deq;
      inflightCount.dec;
    endmethod
    method Bool valid = respQueue.canDeq;
    method DCacheResp value = respQueue.dataOut;
  endinterface
endmodule

// ============================================================================
// DCache client
// ============================================================================

interface DCacheClient;
  interface Out#(DCacheReq) dcacheReqOut;
  interface In#(DCacheResp) dcacheRespIn;
endinterface

// ============================================================================
// Connections
// ============================================================================

module connectCoresToDCache#(
         Vector#(`CoresPerDCache, DCacheClient) clients,
         DCache dcache) ();

  // Connect requests
  function getDCacheReqOut(client) = client.dcacheReqOut;
  let dcacheReqs <- mkMergeTree(Fair,
                      mkUGShiftQueue1(QueueOptFmax),
                      map(getDCacheReqOut, clients));
  connectUsing(mkUGQueue, dcacheReqs, dcache.reqIn);

  // Connect responses
  function Bit#(`LogCoresPerDCache) getDCacheRespKey(DCacheResp resp) =
    truncateLSB(resp.id);
  function getDCacheRespIn(client) = client.dcacheRespIn;
  let dcacheResps <- mkResponseDistributor(
                      getDCacheRespKey,
                      mkUGShiftQueue1(QueueOptFmax),
                      map(getDCacheRespIn, clients));
  connectDirect(dcache.respOut, dcacheResps);

endmodule

// ============================================================================
// Dummy cache
// ============================================================================

// This data cache ignores its inputs and doesn't generate any outputs
module mkDummyDCache (DCache);

  // Ports
  BOut#(DCacheResp) respNull      <- mkNullBOut;
  In#(MemLoadResp)  loadRespNull  <- mkNullIn;
  In#(MemStoreResp) storeRespNull <- mkNullIn;
  BOut#(MemReq)     memReqNull    <- mkNullBOut;

  interface In reqIn =
    error("Request input to dummy cache must be unconnected");

  interface BOut respOut     = respNull;
  interface In   loadRespIn  = loadRespNull;
  interface In   storeRespIn = storeRespNull;
  interface BOut reqOut      = memReqNull;

endmodule

endpackage
