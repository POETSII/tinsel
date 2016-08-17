// Copyright (c) Matthew Naylor

package DCache;

// ============================================================================
// Design overview
// ============================================================================
//
// This is an N-way set-associative write-back cache.  It will
// serve one or more highly-threaded cores, thus high throughput and
// high Fmax are much more important than low latency.  It employs a
// hash function that appends the thread id and some number of address
// bits, thus lines are not shared between threads.
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
// Pipeline structure
// ------------------
//
//            +-------------+
// req  ----->| tag lookup  |<---------------------+
//            +-------------+                      |
//                 ||                              |
//                 \/                              |
//            +-------------+      +------+        |
//            | data lookup |----->| miss |        |
//            +-------------+      | unit |        |
//                 ||              +------+        |
//                 \/                              |
//            +----------+         +----------+    |
// resp <-----| hit unit |-------->| memory   |----+
//            +----------+  retry  | response |
//                                 +----------+
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
//   a. consume feedback request from the "external request" stage, if present
//   b. otherwise, consume a fresh client request instead
//   c. send BRAM request for the tag
//
// 2. data lookup:
//   a. determine correct way
//   b. on read hit: send BRAM request for word data
//   c. on write hit: write word data to BRAM
//   d. on miss: send request to miss unit, if ready
//      (if not, mark request to be retried)
// 
// 3. hit unit:
//   a. on hit: enqueue response FIFO, if ready
//   b. on miss: update tag
//   c. update meta data
//   d. if (a) not firing or request marked for retry, then
//      send retry request to (4)
//
// 4. memory response:
//   a. consume response:
//        * receive new line data from external memory, if ready
//        * write new line data to BRAM
//        * put a fresh request, which will definitely hit if it starts
//          again from (1), into "hit buffer"
//   b. receive retry request from (3):
//        * if "retry buffer" not full and "hit buffer" not empty,
//          dequeue request from "hit buffer" and send to (1) and
//          enqueue retry request into "retry buffer"
//        * otherwise, send retry request to (1)
//   c. if no retry request from (3) then dequeue request from "hit
//      buffer" or "retry buffer" (with that priority) and send to (1)
//
//   NOTE: a request is only put into the retry buffer when another is
//   taken from the hit buffer.  This hit request will create a pipeline
//   bubble that will eventually be filled by dequeueing an element from
//   the retry buffer.  Thus the retry buffer cannot be full when the
//   pipeline is full of retries.
//
// 5. miss unit:
//    a. if miss unit ready: accept a new miss from the data lookup
//       stage, if one is available, and move to busy state
//    b. if miss unit busy:
//         * if old line is dirty: write each beat to memory
//         * request new line data from memory
//         * move to ready state

// ============================================================================
// Imports
// ============================================================================

import BlockRam  :: *;
import Queue     :: *;
import Mem       :: *;
import Util      :: *;
import Vector    :: *;
import DReg      :: *;
import Assert    :: *;
import ConfigReg :: *;

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
  Bool retry;
  SetMetaData metaData;
} DCacheToken deriving (Bits);

// State of the miss unit
typedef enum {
  MissUnitReady, MissUnitBusy
} MissUnitState deriving (Bits, Eq);

// Beat
typedef Bit#(`LogBeatsPerLine) Beat;

// Memory request from miss unit
typedef struct {
  Bool isStore;
  MemLineAddr addr;
} MissMemReq deriving (Bits);

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
  method Bool canPut;
  method Action putReq(DCacheReq req);
  method Bool canGet;
  method ActionValue#(DCacheResp) getResp;
endinterface

// ============================================================================
// Implementation
// ============================================================================

// This is the main data cache module.

module mkDCache#(Integer myId, MemDualResp extMem) (DCache);
  // Tag block RAM
  Vector#(DCacheNumWays, BlockRam#(SetIndex, Tag)) tagMem <-
    replicateM(mkBlockRam);

  // True dual-port mixed-width data block RAM
  // (One bus-sized port and one word-sized port)
  BlockRamTrueMixedBE#(BeatIndex, Bit#(`BusWidth), WordIndex, Bit#(32))
    dataMem <- mkBlockRamTrueMixedBE;

  // Meta data for each set
  BlockRam#(SetIndex, SetMetaData) metaData <- mkBlockRam;
  
  // Client request & response queues
  Queue#(DCacheReq) reqQueue <- mkUGShiftQueue(QueueOptFmax);
  Queue#(DCacheResp) respQueue <- mkUGShiftQueue(QueueOptFmax);

  // The fill queue (16 elements) stores requests that have missed
  // while waiting for external memory to fetch the data.
  SizedQueue#(4, Fill) fillQueue <- mkUGSizedQueue;

  // State of the miss unit
  Reg#(MissUnitState) missUnitState <- mkConfigReg(MissUnitReady);

  // Pipeline state and control
  Reg#(DCacheToken) tagLookup2Input    <- mkVReg;
  Reg#(DCacheToken) dataLookup1Input   <- mkVReg;
  Reg#(DCacheToken) dataLookup2Input   <- mkConfigRegU;
  Reg#(Bool)        dataLookup2Trigger <- mkDReg(False);
  Reg#(DCacheToken) dataLookup3Input   <- mkVReg;
  Reg#(DCacheToken) hitUnitInput       <- mkVReg;
  Reg#(DCacheReq)   memResponseInput   <- mkConfigRegU;
  Reg#(Bool)        memResponseTrigger <- mkDReg(False);
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
  Wire#(Bool) lineReadReqWire <- mkDWire(False);
  Wire#(BeatIndex) lineReadIndexWire <- mkDWire(0);
  Wire#(Bool) lineWriteReqWire <- mkDWire(False);
  Reg#(Bool) lineWriteReqReg <- mkReg(False);
  Wire#(BeatIndex) lineWriteIndexWire <- mkDWire(0);
  Reg#(Bit#(`BusWidth)) lineWriteDataReg <- mkConfigRegU;
  Reg#(BeatIndex) lineIndexReg <- mkRegU;

  // Use wires to issue line access in dataMem
  rule lineAccessUnit;
    lineWriteReqReg <= lineWriteReqWire;
    lineIndexReg <= lineReadIndexWire | lineWriteIndexWire;
    dataMem.putA(
      lineWriteReqReg,
      lineIndexReg,
      lineWriteDataReg);
  endrule

  // Tag lookup stage
  // ----------------

  rule tagLookup1 (feedbackTrigger || reqQueue.canDeq);
    // Select fresh client request or feedback request
    DCacheReq req = feedbackTrigger ? feedbackReq : reqQueue.dataOut;
    // Dequeue request
    if (! feedbackTrigger) reqQueue.deq;
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
    // At the moment, request does not need to be retried
    token.retry = False;
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
      if (missUnitState != MissUnitReady)
        // Need to retry this request as miss unit is busy
        token.retry = True;
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
    // Does current request need to be retried?
    Bool retry = True;
    if (token.isHit) begin
      // On hit: enqueue response queue, if ready
      DCacheResp resp;
      resp.id = token.req.id;
      resp.data = dataMem.dataOutB;
      if (respQueue.notFull) begin
        setDirtyBit = token.req.cmd.isStore;
        for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1)
          if (token.matching[i]) newDirtyBits[i] = True;
        retry = False;
        respQueue.enq(resp);
      end
    end else if (!token.retry) begin
      retry = False;
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
    // Retry request, if necessary
    memResponseInput <= token.req;
    if (retry) memResponseTrigger <= True;
  endrule

  // Memory response stage
  // ---------------------

  // 1-element buffer for requests that will hit
  Queue1#(DCacheReq) hitBuffer <- mkUGShiftQueue(QueueOptFmax);

  // 1-element buffer for requests to be retried
  Queue1#(DCacheReq) retryBuffer <- mkUGShiftQueue(QueueOptFmax);

  // Beat counter for responses
  Reg#(Beat) respBeat <- mkReg(0);

  rule memResponse;
    // If new line data available from external memory, then:
    if (extMem.loadResp.canGet && fillQueue.canDeq
          && fillQueue.canPeek && hitBuffer.notFull) begin
      // Remove item from fill queue and put associated request (which
      // will definitely hit if it starts again from the beginning of
      // the pipeline) into the hit buffer
      let fill = fillQueue.dataOut;
      if (allHigh(respBeat)) begin
        fillQueue.deq;
        hitBuffer.enq(fill.req);
      end
      // Write new line data to dataMem
      MemLoadResp resp <- extMem.loadResp.get;
      lineWriteReqWire   <= True;
      lineWriteIndexWire <= beatIndex(respBeat, fill.req.id,
                              fill.req.addr, fill.way);
      lineWriteDataReg <= resp.data;
      respBeat <= respBeat+1;
    end
    // Receive retry request from external request stage
    if (memResponseTrigger) begin
      // If retry buffer not full and hit buffer can be dequeued
      if (retryBuffer.notFull && hitBuffer.canDeq) begin
        // Dequeue request from hit buffer and feedback to tag lookup stage
        hitBuffer.deq;
        feedbackTrigger <= True;
        feedbackReq <= hitBuffer.dataOut;
        // Put retry request into buffer
        retryBuffer.enq(memResponseInput);
      end else begin
        // Otherwise, feedback retry request to tag lookup stage
        feedbackTrigger <= True;
        feedbackReq <= memResponseInput;
      end
    end else begin
      // If there's no retry from the external request stage then
      // dequeue a request from hit buffer or retry buffer
      // (with that priority) and feedback to tag lookup stage
      if (hitBuffer.canDeq) begin
        hitBuffer.deq;
        feedbackTrigger <= True;
        feedbackReq <= hitBuffer.dataOut;
      end else if (retryBuffer.canDeq) begin
        retryBuffer.deq;
        feedbackTrigger <= True;
        feedbackReq <= retryBuffer.dataOut;
      end
    end
  endrule

  // Miss unit
  // ---------

  // Token currently being processed by miss unit
  Reg#(DCacheToken) missUnitToken <- mkConfigRegU;

  // Request beat delayed by one, two, and three cycles
  Reg#(Beat) reqBeat <- mkReg(0);
  Reg#(Bool) reqBeat1 <- mkDReg(False);
  Reg#(Bool) reqBeat2 <- mkDReg(False);
  Reg#(Bool) reqBeat3 <- mkDReg(False);

  // Control signals for miss unit
  Reg#(Bool) requestedAllBeats <- mkReg(False);
  Reg#(Bool) storeReqEnqueued <- mkReg(False);
  Reg#(Bool) loadReqEnqueued <- mkReg(False);

  // Address buffer (addresses for lines being written out or read in)
  Queue#(MissMemReq) missMemReqs <- mkUGShiftQueue(QueueOptFmax);

  // Data buffer (data values for beats being written out)
  SizedQueue#(`LogDCacheWriteBufferSize, Bit#(`BusWidth)) beatBuffer <-
    mkUGShiftQueue(QueueOptFmax);

  // Used to allocate space in the beat buffer
  Count#(TAdd#(`LogDCacheWriteBufferSize, 1)) beatBufferCount <-
    mkCount(2 ** `LogDCacheWriteBufferSize);

  // Next beat to write to memory
  Reg#(Beat) writebackBeat <- mkReg(0);

  rule missUnit1;
    DCacheToken token = missUnitToken;
    case (missUnitState)
      MissUnitReady: begin
        if (dataLookup2Trigger && !dataLookup2Input.isHit) begin
          // A new miss to handle
          missUnitState <= MissUnitBusy;
          missUnitToken <= dataLookup2Input;
          requestedAllBeats <= !dataLookup2Input.evictDirty;
          storeReqEnqueued <= !dataLookup2Input.evictDirty;
          loadReqEnqueued <= False;
        end
      end
      MissUnitBusy: begin
        // If the memResponse stage is not writing line data, then:
        if (!lineWriteReqWire && !requestedAllBeats &&
               beatBufferCount.notFull) begin
          // Read old line data from dataMem
          lineReadReqWire <= True;
          lineReadIndexWire <= beatIndex(reqBeat, token.req.id,
                                 token.req.addr, token.evictWay);
          reqBeat <= reqBeat+1;
          reqBeat1 <= True;
          beatBufferCount.inc;
          if (allHigh(reqBeat)) requestedAllBeats <= True;
        end
        // Put read or write request into address buffer
        Bool loadReqEnqueuedWire = False;
        if (missMemReqs.notFull) begin
          if (!storeReqEnqueued) begin
            // Enqueue store request (for old line data)
            MissMemReq req;
            req.isStore = True;
            req.addr = truncateLSB(reconstructAddr(
                             token.evictTag.key, token.req.addr));
            missMemReqs.enq(req);
            storeReqEnqueued <= True;
          end else if (fillQueue.notFull && !loadReqEnqueued) begin
            // Enqueue load request (for new line data)
            MissMemReq req;
            req.isStore = False;
            req.addr = truncateLSB(token.req.addr);
            missMemReqs.enq(req);
            // Put request in fill queue
            Fill fill;
            fill.req = token.req;
            fill.way = token.evictWay;
            fillQueue.enq(fill);
            loadReqEnqueued <= True;
            loadReqEnqueuedWire = True;
          end
        end
        if (requestedAllBeats && (loadReqEnqueued || loadReqEnqueuedWire))
          missUnitState <= MissUnitReady;
      end
    endcase
    reqBeat2 <= reqBeat1;
    reqBeat3 <= reqBeat2;
    if (reqBeat3) beatBuffer.enq(dataMem.dataOutA);
  endrule

  rule missUnit2;
    if (missMemReqs.canDeq && extMem.req.canPut) begin
      MissMemReq miss = missMemReqs.dataOut;
      MemReq memReq;
      memReq.isStore = miss.isStore;
      memReq.id = fromInteger(myId);
      memReq.addr = {miss.addr, writebackBeat};
      memReq.data = beatBuffer.dataOut;
      memReq.burst = miss.isStore ? 1 : `BeatsPerLine;
      // If a store, dequeue the beat buffer 
      if (miss.isStore && beatBuffer.canDeq) begin
        beatBuffer.deq;
        beatBufferCount.dec;
        writebackBeat <= writebackBeat+1;
      end
      // Send request to memory
      if (!miss.isStore || beatBuffer.canDeq)
        extMem.req.put(memReq);
      // Deqeue the request buffer
      if (!miss.isStore || (beatBuffer.canDeq && allHigh(writebackBeat))) begin
        missMemReqs.deq;
      end
    end
  endrule

  // Discard store responses
  // -----------------------

  // Until the cache supports explicit flushing,
  // ignore all store responses from external memory
  rule discardStoreResps;
    if (extMem.storeResp.canGet) begin
      let _ <- extMem.storeResp.get;
    end
  endrule

  // Methods
  // -------

  method Bool canPut = reqQueue.notFull;
  method Action putReq(DCacheReq req);
    reqQueue.enq(req);
  endmethod

  method Bool canGet = respQueue.canDeq;
  method ActionValue#(DCacheResp) getResp;
    respQueue.deq;
    return respQueue.dataOut;
  endmethod
endmodule

endpackage
