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
// Byte-enables are maintained to avoid fetch-on-write.
//
// A per-thread cache flush operation is supported.
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
//
// Note the feedback loop in the pipeline due to the link from the
// memory response stage to the tag lookup stage.  On a miss, after the
// new line data has been fetched, the original request is fed back to
// the first pipeline stage, and is now guaranteed to hit.
//
// We support a per-thread cache flush mechanism that exploits the
// feedback loop.  A flush request loops round the pipeline,
// invalidating evicting a line on each iteration.  After the final
// line is evicted, we issue a dummy load request, the response of
// which is used to guarantee that all writes have reached DRAM.
// Finally, a flush response is issued to the client.

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
import DRAM      :: *;

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
  Bool isLoad;      // Load
  Bool isStore;     // Store
  Bool isFlush;     // Perform cache flush
  Bool isFlushResp; // Send cache flush response
} DCacheReqCmd deriving (Bits);

// Client request structure
typedef struct {
  DCacheClientId id;
  DCacheReqCmd cmd;
  Bit#(32) addr;
  Bit#(32) data;
  Bit#(4) byteEn;
} DCacheReq deriving (Bits);

// Details of flush request:
//   * 'addr' field contains the set index to evict
//   * 'data' field contains the way to evict
//   * client should ensure that these are both 0 initially

// Client response structure
typedef struct {
  DCacheClientId id;
  Bit#(32) data;
} DCacheResp deriving (Bits);

// Fill request
typedef struct {
  DCacheReq req;  // The request leading to the fill
  Way way;        // The way to fill
  Bool flushDone; // Goes high when the final line is being flushed
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
  Bool dirty;
  Bool writeOnly;
  Key key;
} Tag deriving (Bits);

// A key holds the upper bits of an address
typedef TSub#(`LogLinesPerDRAM, `DCacheLogSetsPerThread) KeyNumBits;
typedef Bit#(KeyNumBits) Key;

// Data cache pipeline token
typedef struct {
  DCacheReq req;
  Vector#(DCacheNumWays, Bool) matching;
  Bool isHit;
  Way way;
  Tag tag;
} DCacheToken deriving (Bits);

// Miss request (input to miss unit)
typedef struct {
  DCacheReq req;
  Way evictWay;
  Tag evictTag;
  Bool flushDone;
} MissReq deriving (Bits);

// Beat
typedef Bit#(`LogBeatsPerLine) Beat;

// Max number of in-flight cache requests
`define DCacheLogMaxInflight 5

// For flushing
typedef Bit#(`DCacheLogSetsPerThread) SetNum;
function SetNum addrToSetNum(Bit#(32) addr) =
  truncate(addr[31:`LogBytesPerLine]);
function Bit#(32) setNumToAddr(SetNum set);
  Bit#(`LogBytesPerLine) bottom = 0;
  return {0, set, bottom};
endfunction

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
function Key getKey(Bit#(32) addr);
  Bit#(`LogBytesPerDRAM) byteAddr = truncate(addr);
  return truncateLSB(byteAddr);
endfunction

// Reconstruct line address from an aliasing address and a tag
function Bit#(`LogLinesPerDRAM) reconstructLineAddr(Key key, Bit#(32) addr) =
  {key, truncate(addr[31:`LogBytesPerLine])};

// ============================================================================
// Interface
// ============================================================================

interface DCache;
  interface In#(DCacheReq)    reqIn;
  interface BOut#(DCacheResp) respOut;
  interface In#(DRAMResp)     respIn;
  interface BOut#(DRAMReq)    reqOut;
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

  // Byte enables for beats in dataMem
  BlockRamTrueMixed#(BeatIndex, Bit#(`BytesPerBeat), WordIndex, Bit#(4))
    dataMemBE <- mkBlockRamTrueMixed;

  // Track the oldest way in each set
  BlockRam#(SetIndex, Way) oldestWay <- mkBlockRam;
  
  // Request & response ports
  InPort#(DCacheReq) reqPort  <- mkInPort;
  InPort#(DRAMResp)  respPort <- mkInPort;

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
  Wire#(BeatIndex) lineReadIndexWire <- mkBypassWire;
  Wire#(Bool) lineWriteReqWire <- mkDWire(False);
  Wire#(BeatIndex) lineWriteIndexWire <- mkBypassWire;
  Wire#(Bit#(`BeatWidth)) lineWriteDataWire <- mkBypassWire;

  // Use wires to issue line access in dataMem
  rule lineAccessUnit;
    dataMem.putA(
      lineWriteReqWire,
      lineWriteReqWire ? lineWriteIndexWire : lineReadIndexWire,
      lineWriteDataWire);
    dataMemBE.putA(
      lineWriteReqWire,
      lineWriteReqWire ? lineWriteIndexWire : lineReadIndexWire,
      0);
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
    // Send read request needed for eviction policy
    oldestWay.read(setIndex(req.id, req.addr));
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
    Vector#(DCacheNumWays, Bool) writeOnly;
    for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1) begin
      tags[i]     = tagMem[i].dataOut;
      matching[i] = tags[i].valid && tags[i].key == getKey(token.req.addr);
      writeOnly[i] = tags[i].writeOnly;
    end
    token.matching = matching;
    // Was there a match?
    Bool existsMatch = any(id, matching);
    token.isHit = existsMatch;
    // Force a miss in the case of flush request
    if (token.req.cmd.isFlush) token.isHit = False;
    // Force a hit if sending a flush response
    if (token.req.cmd.isFlushResp) token.isHit = True;
    // Force a miss if reading from a write-only line
    if (token.req.cmd.isLoad && oneHotSelect(matching, writeOnly))
      token.isHit = False;
    // Convert matching way from one-hot to binary
    Way matchingWay = encode(token.matching);
    // In case of a miss, choose a way to evict and remember the old tag
    // On a flush, the way is specified in the data field of the request
    token.way = token.req.cmd.isFlush ? truncate(token.req.data) :
                  (existsMatch ? matchingWay : oldestWay.dataOut);
    token.tag = tags[token.way];
    // Trigger next stage
    dataLookup2Trigger <= True;
    dataLookup2Input <= token;
  endrule

  rule dataLookup2 (dataLookup2Trigger);
    DCacheToken token = dataLookup2Input;
    // Handle hit or miss
    if (token.isHit) begin
      // On read hit: read word data from dataMem
      // On write hit: write word data to dataMem
      dataMem.putB(token.req.cmd.isStore,
                   wordIndex(token.req.id, token.req.addr, token.way),
                   token.req.data, token.req.byteEn);
      dataMemBE.putB(token.req.cmd.isStore,
                     wordIndex(token.req.id, token.req.addr, token.way),
                     token.req.byteEn);
    end else begin
      // Put miss requests into miss queue
      myAssert(missQueue.notFull, "DCache: miss queue full");
      MissReq miss;
      miss.req = token.req;
      miss.evictWay = token.way;
      miss.evictTag = token.tag;
      // Extract current set index and way of flush
      SetNum set = addrToSetNum(token.req.addr);
      Way way = truncate(token.req.data);
      // Determine if we're flushing the final line
      miss.flushDone = allHigh(set) && allHigh(way);
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
    // New tag
    Tag newTag;
    newTag.key = getKey(token.req.addr);
    // Hit or miss?
    if (token.isHit) begin
      // On hit: enqueue response queue and update tag
      myAssert(respQueue.notFull, "DCache: response queue full");
      DCacheResp resp;
      resp.id = token.req.id;
      resp.data = dataMem.dataOutB;
      newTag.valid = True;
      newTag.dirty = token.tag.dirty || token.req.cmd.isStore;
      newTag.writeOnly = token.tag.writeOnly;
      respQueue.enq(resp);
    end else begin
      // On miss: update tag
      // On a flush: invalidate the line
      newTag.valid = !token.req.cmd.isFlush;
      newTag.dirty = False;
      newTag.writeOnly = token.req.cmd.isStore;
      // Update oldest way
      if (any(id, token.matching))
        oldestWay.write(setIndex(token.req.id, token.req.addr), token.way+1);
    end
    // Update tag
    for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1)
      if (token.way == fromInteger(i))
        tagMem[i].write(setIndex(token.req.id, token.req.addr), newTag);
  endrule

  // Memory response stage
  // ---------------------

  // Beat counter for responses
  Reg#(Beat) respBeat <- mkReg(0);

  rule memResponse;
    let fill = fillQueue.dataOut;
    // This rule may write new line data to dataMem
    // If so, here are the parameters for the write
    lineWriteDataWire <= respPort.value.data;
    lineWriteIndexWire <= beatIndex(respBeat, fill.req.id,
                            fill.req.addr, fill.way);
    // Ready to consume fill queue?
    if (fillQueue.canDeq && fillQueue.canPeek) begin
      // Is it a flush request?
      if (fill.req.cmd.isFlush) begin
        // Extract current set index and way of flush
        SetNum set = addrToSetNum(fill.req.addr);
        Way way = truncate(fill.req.data);
        // Increment set index
        if (allHigh(way)) fill.req.addr = setNumToAddr(set+1);
        // Increment way
        fill.req.data = {?, way+1};
        // Is flush complete?
        if (fill.flushDone) begin
          // Flush command morphs into flush-response command
          fill.req.cmd.isFlush = False;
          fill.req.cmd.isFlushResp = True;
        end
        // We need to wait for a response only on the final line of
        // the flush (this response signifies that all evictions due
        // to the flush have reached DRAM)
        Bool step = fill.flushDone ? respPort.canGet : True;
        if (step) begin
          // Feed request back to beginning of pipeline
          fillQueue.deq;
          feedbackTrigger <= True;
          if (fill.flushDone) respPort.get;
        end
      // Otherwise, is new line data available from external memory?
      end else if (respPort.canGet) begin
        // Remove item from fill queue and feed associated request (which
        // will definitely hit if it starts again from the beginning of
        // the pipeline) back to beginning of the pipeline
        if (allHigh(respBeat)) begin
          fillQueue.deq;
          feedbackTrigger <= True;
        end
        // Write new line data to dataMem
        // (The write parameters are set outside condition for better timing)
        lineWriteReqWire <= True;
        respPort.get;
        respBeat <= respBeat+1;
      end
    end
    // Set feedback request
    feedbackReq <= fill.req;
  endrule

  // Miss unit
  // ---------

  // Memory request queue
  Queue#(DRAMReq) memReqQueue <- mkUGShiftQueue(QueueOptFmax);

  // Index of next beat to read
  Reg#(Beat) reqBeat <- mkReg(0);

  // Has the writeback been completed?
  Reg#(Bool) writebackDone <- mkReg(False);

  // Is a beat ready on the output of dataMem?
  Reg#(Bit#(2)) missUnitState <- mkReg(0);

  rule missUnit;
    MissReq miss = missQueue.dataOut;
    // Send a load request?
    Bool isLoad = !miss.evictTag.dirty || writebackDone;
    // Determine line address
    Bit#(`LogLinesPerDRAM) writeLineAddr =
      reconstructLineAddr(miss.evictTag.key, miss.req.addr);
    Bit#(`LogLinesPerDRAM) readLineAddr = 
      miss.req.addr[`LogBytesPerDRAM-1:`LogBytesPerLine];
    // Create memory request
    DRAMReq memReq;
    memReq.isStore = !isLoad;
    memReq.id = myId;
    memReq.addr = {isLoad ? readLineAddr : writeLineAddr, reqBeat};
    memReq.data = dataMem.dataOutA;
    memReq.burst = isLoad ? (miss.req.cmd.isFlush ? 1 : `BeatsPerLine) : 1;
    memReq.byteEn = dataMemBE.dataOutA;
    // Are we going to send a memory request to the next stage?
    Bool sendMemReq = False;
    if (missQueue.canPeek && missQueue.canDeq && memReqQueue.notFull) begin
      if (missUnitState == 0) begin
        // Are we ready to request the new line?
        if (isLoad) begin
          if (fillQueue.notFull) begin
            missQueue.deq;
            writebackDone <= False;
            // When flushing, only send a mem request after eviction of
            // final line, the response of which will indicate that
            // all evictions due to the flush have reached DRAM
            sendMemReq = miss.req.cmd.isFlush ? miss.flushDone : True;
            // Put request in fill queue
            Fill fill;
            fill.req = miss.req;
            fill.way = miss.evictWay;
            fill.flushDone = miss.flushDone;
            fillQueue.enq(fill);
          end
        // Or are we still writing back old data?
        end else if (!lineWriteReqWire) begin
          // Assignment to lineReadIndexWire is performed below
          // (outside complex condition) to improve timing
          missUnitState <= 1;
        end
      end else if (missUnitState == 2) begin
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

  // Interface
  // ---------

  interface In reqIn  = reqPort.in;
  interface In respIn = respPort.in;

  interface BOut reqOut;
    method Action get;
      memReqQueue.deq;
    endmethod
    method Bool valid = memReqQueue.canDeq;
    method DRAMReq value = memReqQueue.dataOut;
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

module connectDCachesToDRAM#(
         Vector#(`DCachesPerDRAM, DCache) caches, DRAM dram) ();

  // Connect requests
  function getReqOut(cache) = cache.reqOut;
  let reqs <- mkMergeTreeB(Fair,
                mkUGShiftQueue1(QueueOptFmax),
                map(getReqOut, caches));
  connectUsing(mkUGQueue, reqs, dram.reqIn);

  // Connect load responses
  function DCacheId getRespKey(DRAMResp resp) = resp.id;
  function getRespIn(cache) = cache.respIn;
  let dramResps <- mkResponseDistributor(
                    getRespKey,
                    mkUGShiftQueue1(QueueOptFmax),
                    map(getRespIn, caches));
  connectDirect(dram.respOut, dramResps);

endmodule

// ============================================================================
// Dummy cache
// ============================================================================

// This data cache ignores its inputs and doesn't generate any outputs
module mkDummyDCache (DCache);

  // Ports
  BOut#(DCacheResp) respOutNull <- mkNullBOut;
  In#(DRAMResp)     respInNull  <- mkNullIn;
  BOut#(DRAMReq)    memReqNull  <- mkNullBOut;

  interface In reqIn =
    error("Request input to dummy cache must be unconnected");

  interface BOut respOut = respOutNull;
  interface In   respIn  = respInNull;
  interface BOut reqOut  = memReqNull;

endmodule

endpackage
