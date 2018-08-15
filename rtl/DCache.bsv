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
//
// Note the feedback loop in the pipeline due to the link from the
// memory response stage to the tag lookup stage.  On a miss, after the
// new line data has been fetched, the original request is fed back to
// the first pipeline stage, and is now guaranteed to hit.
//
// We also provide a cache flush operation that allows a client to
// flush a given line.  A full cache flush can then be programmed in
// software.

// ============================================================================
// Imports
// ============================================================================

import BlockRam    :: *;
import Queue       :: *;
import Globals     :: *;
import Util        :: *;
import Vector      :: *;
import DReg        :: *;
import Assert      :: *;
import ConfigReg   :: *;
import Interface   :: *;
import DRAM        :: *;
import OffChipRAM  :: *;
import PseudoLRU   :: *;
import DCacheTypes :: *;
import Mailbox     :: *;

// ============================================================================
// Types  
// ============================================================================

// Number of ways
typedef TExp#(`DCacheLogNumWays) DCacheNumWays;

// Client response structure
typedef struct {
  DCacheClientId id;
  Bit#(32) data;
} DCacheResp deriving (Bits);

// Flush request
typedef struct {
  DCacheReq req;  // The request leading to the flush
  Way way;        // The way to flush
} FlushReq deriving (Bits);

// Details of a DCache flush request: the 'addr' field specifies the
// line to evict and the 'data' field specifies the way.

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
typedef TSub#(`LogLinesPerMem, `DCacheLogSetsPerThread) KeyNumBits;
typedef Bit#(KeyNumBits) Key;

// Meta data per set
typedef struct {
  PLRUState plruState; // For psuedo LRU replacement policy
  Vector#(DCacheNumWays, Bool) dirty;
} SetMetaData deriving (Bits);

// Data cache pipeline token
typedef struct {
  DCacheReq req;
  Vector#(DCacheNumWays, Bool) matching;
  Bool isHit;
  Way matchingWay;
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
function Key getKey(Bit#(32) addr);
  Bit#(`LogBytesPerMem) byteAddr = truncate(addr);
  return truncateLSB(byteAddr);
endfunction

// Reconstruct line address from an aliasing address and a tag
function Bit#(`LogLinesPerMem) reconstructLineAddr(Key key, Bit#(32) addr) =
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

  // Meta data for each set
  BlockRam#(SetIndex, SetMetaData) metaData <- mkBlockRam;
  
  // Request & response ports
  InPort#(DCacheReq) reqPort  <- mkInPort;
  InPort#(DRAMResp)  respPort <- mkInPort;

  // The flush queue (flush requests waiting to re-enter the pipeline)
  Queue#(FlushReq) flushQueue <- mkUGShiftQueue2(QueueOptFmax);

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
    // Force a miss in the case of flush request
    // Force a hit if were sending a flush response
    token.isHit = any(id, matching) && !token.req.cmd.isFlush ||
                    token.req.cmd.isFlushResp;
    // In case of a miss, choose a way to evict and remember the old tag
    // On a flush, the way is specified in the data field of the request
    token.evictWay   = token.req.cmd.isFlush ?
                         truncate(token.req.data) :
                         plru(metaData.dataOut.plruState);
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
    token.matchingWay = encode(token.matching);
    // Handle hit or miss
    if (token.isHit) begin
      // On read hit: read word data from dataMem
      // On write hit: write word data to dataMem
      dataMem.putB(token.req.cmd.isStore,
                   wordIndex(token.req.id, token.req.addr, token.matchingWay),
                   token.req.data, token.req.byteEn);
    end else begin
      // Put miss requests into miss queue
      myAssert(missQueue.notFull, "DCache: miss queue full");
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
    // New dirty bits
    Vector#(DCacheNumWays, Bool) newDirtyBits = token.metaData.dirty;
    if (token.isHit) begin
      // On hit: enqueue response queue
      myAssert(respQueue.notFull, "DCache: response queue full");
      DCacheResp resp;
      resp.id = token.req.id;
      resp.data = dataMem.dataOutB;
      for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1)
        if (token.matching[i] && token.req.cmd.isStore)
          newDirtyBits[i] = True;
      respQueue.enq(resp);
    end else begin
      // On miss: update tag
      Tag newTag;
      // On a flush: invalidate the line
      newTag.valid = !token.req.cmd.isFlush;
      newTag.key = getKey(token.req.addr);
      for (Integer i = 0; i < valueOf(DCacheNumWays); i=i+1)
        if (token.evictWay == fromInteger(i)) begin
          newDirtyBits[i] = False;
          tagMem[i].write(setIndex(token.req.id, token.req.addr), newTag);
        end
    end
    // Update meta data
    SetMetaData newMetaData;
    newMetaData.plruState = plruNext(
      token.isHit ? token.matchingWay : token.evictWay,
      token.metaData.plruState);
    newMetaData.dirty = newDirtyBits;
    metaData.write(setIndex(token.req.id, token.req.addr), newMetaData);
  endrule

  // Memory response stage
  // ---------------------

  rule memResponse;
    // This rule either consumes a flush request or a memory response
    let flush = flushQueue.dataOut;
    let resp = respPort.value;
    lineWriteDataWire <= resp.data;
    lineWriteIndexWire <= beatIndex(resp.info.beat, resp.info.req.id,
                            resp.info.req.addr, resp.info.way);
    // Ready to consume flush queue?
    if (flushQueue.canDeq && flushQueue.canPeek) begin
      flush.req.cmd.isFlush = False;
      flush.req.cmd.isFlushResp = True;
      flushQueue.deq;
      feedbackTrigger <= True;
      // Set feedback request
      feedbackReq <= flush.req;
    // Otherwise, is new line data available from external memory?
    end else if (respPort.canGet) begin
      // Remove item from fill queue and feed associated request (which
      // will definitely hit if it starts again from the beginning of
      // the pipeline) back to beginning of the pipeline
      if (allHigh(resp.info.beat))
        feedbackTrigger <= True;
      // Write new line data to dataMem
      // (The write parameters are set outside condition for better timing)
      lineWriteReqWire <= True;
      respPort.get;
      // Set feedback request
      feedbackReq <= resp.info.req;
    end
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
    Bool isLoad = !miss.evictDirty || writebackDone;
    // Determine line address
    let writeLineAddr =
      reconstructLineAddr(miss.evictTag.key, miss.req.addr);
    let readLineAddr = 
      miss.req.addr[`LogBytesPerDRAM:`LogBytesPerLine];
    // Create inflight request info
    InflightDCacheReqInfo info;
    info.req = miss.req;
    info.way = miss.evictWay;
    info.beat = ?;
    // Create memory request
    DRAMReq memReq;
    memReq.isStore = !isLoad;
    memReq.id = {myId, 1'b0};
    memReq.addr = {isLoad ? readLineAddr : writeLineAddr, reqBeat};
    memReq.data = isLoad ? {?, pack(info)} : dataMem.dataOutA;
    memReq.burst = isLoad ? `BeatsPerLine : 1;
    // Are we going to send a memory request to the next stage?
    Bool sendMemReq = False;
    if (missQueue.canPeek && missQueue.canDeq && memReqQueue.notFull) begin
      if (missUnitState == 0) begin
        // Are we ready to request the new line?
        if (isLoad) begin
          if (miss.req.cmd.isFlush) begin
            if (flushQueue.notFull) begin
              writebackDone <= False;
              FlushReq flush;
              flush.req = miss.req;
              flush.way = miss.evictWay;
              flushQueue.enq(flush);
              missQueue.deq;
            end
          end else begin
            missQueue.deq;
            writebackDone <= False;
            sendMemReq = True;
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

module connectDCachesAndMailboxesToOffChipRAM#(
         Vector#(`DCachesPerDRAM, DCache) caches,
         Vector#(`DCachesPerDRAM, Mailbox) mboxes,
         OffChipRAM ram) ();

  // Connect requests
  function getReqOutC(cache) = cache.reqOut;
  function getReqOutM(mbox) = mbox.ramReqOut;
  let reqs <- mkMergeTreeB(Fair,
                mkUGShiftQueue1(QueueOptFmax),
                interleave(map(getReqOutC, caches),
                           map(getReqOutM, mboxes)));
  connectUsing(mkUGQueue, reqs, ram.reqIn);

  // Connect load responses
  function DRAMReqId getRespKey(DRAMResp resp) = resp.id;
  function getRespInC(cache) = cache.respIn;
  function getRespInM(mbox) = mbox.ramRespIn;
  let ramResps <- mkResponseDistributor(
                    getRespKey,
                    mkUGQueue,
                    interleave(map(getRespInC, caches),
                               map(getRespInM, mboxes)));
  connectDirect(ram.respOut, ramResps);

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
