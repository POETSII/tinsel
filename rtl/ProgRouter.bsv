// SPDX-License-Identifier: BSD-2-Clause
// Functions, data types, and modules for programmable routers
package ProgRouter;

// =============================================================================
// Routing keys and beats
// =============================================================================

// A routing record is either 40 bits or 80 bits in size (aligned on a
// 40-bit or 80-bit boundary respectively). Multiple records are
// packed into a 256-bit DRAM beat (aligned on a 256-bit boundary).
// The most significant 16 bits of the beat contain a count of the
// number of records in the beat (in the range 1 to 6 inclusive). The
// remaining 240 bits contain records. The first record lies in the
// least-significant bits of the beat. The size portion of the routing
// key contains the number of contiguous DRAM beats holding all
// records for the key.

// 256-bit routing beat
typedef struct {
  // Number of 40-bit record chunks present
  Bit#(16) size;
  // The 40-bit record chunks
  Vector#(6, Bit#(40)) chunks;
} RoutingBeat deriving (Bits);

// 32-bit routing key
typedef struct {
  // Which off-chip RAM?
  Bit#(`LogDRAMsPerBoard) ram
  // Pointer to array of routing beats containing routing records
  Bit#(`LogBeatsPerDRAM) ptr;
  // Number of beats in the array
  Bit#(`LogRoutingEntryLen) numBeats;
} RoutingKey deriving (Bits);

// Extract routing key from an address
function RoutingKey getRoutingKey(NetAddr addr) =
  unpack(getRoutingKeyRaw(addr));

// =============================================================================
// Types of routing record
// =============================================================================

typedef enum {
  URM1 = 3'd0, // 40-bit Unicast Router-to-Mailbox
  URM2 = 3'd1, // 80-bit Unicast Router-to-Mailbox
  RR   = 3'd2, // 40-bit Router-to-Router
  MRM  = 3'd3, // 80-bit Multicast Router-to-Mailbox
  IND  = 3'd4  // 40-bit Indirection
} RoutingRecordTag;

// 40-bit Unicast Router-to-Mailbox (URM1) record
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Mailbox-local thread identifier
  Bit#(6) thread;
  // Local key. The first word of the message
  // payload is overwritten with this.
  Bit#(27) localKey;
} URM1Record deriving (Bits);

// 80-bit Unicast Router-to-Mailbox (URM2) record
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Mailbox-local thread identifier
  Bit#(6) thread;
  // Currently unused
  Bit#(3) unused;
  // Local key. The first two words of the message
  // payload is overwritten with this.
  Bit#(64) localKey;
} URM2Record deriving (Bits);

// 40-bit Router-to-Router (RR) record
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Direction (N, S, E, or W)
  Bit#(2) dir;
  // Currently unused
  Bit#(3) unused;
  // New 32-bit routing key that will replace the one in the
  // current message for the next hop of the message's journey
  Bit#(32) newKey;
} RRRecord deriving (Bits);

// 80-bit Multicast Router-to-Mailbox (MRM) record
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Mailbox destination
  Bit#(4) mbox;
  // Currently unused
  Bit#(9) unused;
  // Mailbox-local destination mask
  Bit#(64) destMask;
} MRMRecord deriving (Bits);

// 40-bit Indirection (IND) record:
typedef struct {
  // Record type
  RoutingRecordTag tag;
  // Currently unused
  Bit#(5) unused;
  // New 32-bit routing key for new set of records on current router
  Bit#(32) newKey;
} INDRecord deriving (Bits);

// =============================================================================
// Design
// =============================================================================

// In the following diagram N/S/E/W are the inter-FPGA links and
// L0..L3 are links at one edge of the NoC.  Depending on the NoC
// dimensions, there may be more or less than four links on a single
// NoC edge, but the diagram assumes four.

//
//               N     S     E     W     L0..L3/Loop   Input flits
//               |     |     |     |     |       |
//             +---+ +---+ +---+ +---+ +---+     |
//             | F | | F | | F | | F | | F |     |     Fetchers
//             +---+ +---+ +---+ +---+ +---+     |
//               |     |     |     |     |       |
//             +---------------------------+     |
//             |          Crossbar         |     |     Preliminary routing
//             +---------------------------+     |
//               |     |     |     |     |       |
//              N/L0  S/L1  E/L2  W/L3   Ind-----+     Output queues
//               |     |     |     |
//             +---------------------------+
//             |          Expander         |           Final expansion
//             +---------------------------+
//               |  |  |  |  |  |  |  |
//               N  S  E  W  L0 L1 L2 L3               Output flits
//

// The core functionality is implemented in the fetchers, which:
//   (1) extract routing keys from incoming flits;
//   (2) lookup the keys in RAM;
//   (3) interpret the resulting routing records; and
//   (4) emit the interpreted flits.

// The key property of these fetchers is that they act entirely
// indepdedently of each other: each one can make progress even if
// another is blocked.  Unfortunately, this leads to a duplicated
// logic resources, but is necessary to avoid deadlock.

// Note that, as the routers are fully programmable, it is possible
// for the programmer to introduce deadlock using an ill-defined
// routing scheme, e.g. where a flit arrives in on (say) link N and
// requires a flit to be sent back along the same direction N.
// However, the hardware does guarantee deadlock-freedom if the
// routing scheme is based on dimension-ordered routing.

// After the fetchers have interpreted the flits, they are fed to a
// fair crossbar which organises them by destination into output
// queues.  To reduce logic, we allow each inter-board link to share
// an output queue with a local link, as this does not compromise
// forward progress.  Finally the queues are expanded to provide an
// output stream for each possible destination.

// =============================================================================
// Fetcher
// =============================================================================

// Flit address in a fetcher's flit buffer
typedef Bit#(`FetcherLogFlitBufferSize) FetcherFlitBufferAddr;

// Message address in a fetcher's flit buffer
typedef Bit#(`FetcherLogMsgsPerFlitBuffer) FetcherFlitBufferMsgAddr;

// This structure contains information about an in-flight memory
// request from a fetcher.  When a fetcher issues a memory load
// request, this info is packed into the unused data field of the
// request.  When the memory subsystem responds, it passes back the
// same info in an extra field inside the memory response structure.
// Maintaining info about an inflight request inside the request
// itself provides an easy way to handle out-of-order responses from
// memory.
typedef struct {
  // Message address in the fetcher's flit buffer
  FetcherFlitBufferMsgAddr msgAddr;
  // How many beats in the burst?
  Bit#(`BeatBurstWidth) burst;
  // Is this the final burst of routing records for the current key?
  Bool finalBurst;
} InflightFetcherReqInfo deriving (Bits);

// Fetcher interface
interface Fetcher;
  // Incoming and outgoing flits
  interface In#(Flit) flitIn;
  interface Out#(Flit) flitOut;
  // Off-chip RAM connections
  Vector#(`DRAMsPerBoard, BOut#(DRAMReq)) ramReqs;
  Vector#(`DRAMsPerBoard, In#(DRAMResp)) ramResps;
endinterface

// Fetcher module
module mkFetcher;

  // Flit input port
  InPort#(Flit)) flitInPort <- mkInPort;

  // RAM response ports
  Vector#(`DRAMsPerBoard, InPort#(DRAMResp)) ramRespPort <-
    replicateM(mkInPort);

  // RAM request queues
  Vector#(`DRAMsPerBoard, Queue1#(DRAMReq)) ramReqQueue <-
    replicateM(mkUGShiftQueue(QueueOptFmax));

  // Flit buffer
  BlockRamOpts flitBufferOpts =
    BlockRamOpts {
      readDuringWrite: DontCare,
      style: "AUTO",
      registerDataOut: False,
      initFile: Invalid
    };
  BlockRam#(FetcherFlitBufferAddr, Flit) flitBuffer <- mkBlockRam;

  // Beat buffer
  SizedQueue#(`LogProgRouterBeatBufferSize, RoutingBeat)) beatBuffer <-
    replicateM(mkUGSizedQueue);

  // Stage 1: consume input message
  // ------------------------------

  // Consumer state
  // State 0: pass through flits that don't contain routing keys
  // State 1: buffer flits that do contain routing keys
  // State 2: fetch routing beats
  Reg#(Bit#(2)) consumeState <- mkReg(0);

  // Count number of flits of message consumed so far
  Reg#(Bit#(`LogFlitsPerMsg)) consumeFlitCount <- mkReg(0);

  // Flit slot allocator
  Vector#(`FetcherMsgsPerFlitBuffer, SetReset) flitBufferUsedSlots <-
    mkSetReset(False);

  // Chosen message slot
  Reg#(FetcherFlitBufferMsgAddr) chosenReg <- mkRegU;

  // Routing key of message consumed
  Reg#(RoutingKey) consumeKey <- mkRegU;

  // Maintain count of routing beats fetched so far
  Reg#(Bit#(`LogRoutingEntryLen)) fetchBeatCount <- mkReg(0);

  // State 0: pass through flits that don't contain routing keys
  rule consumeMessage0 (consumeState == 0);
    Flit flit = flitInPort.value;
    // Find unused message slot
    Bool found = False;
    FetcherFlitBufferMsgAddr chosen = ?;
    for (Integer i = 0; i < `FetcherMsgsPerFlitBuffer; i=i+1) 
      if (flitBufferUsedSlots[i].value == 0) begin
        found = True;
        chosen = fromInteger(i);
      end
    chosenReg <= chosen;
    // Initialise counters for subsequent states
    flitCount <= 0;
    fetchBeatCount<= 0;
    // Consume flit
    if (flitInPort.canGet) begin
      if (flit.dest.addr.isKey) begin
        if (found) begin
          consumeState <= 1;
        end
      end else if (flitQueue.notFull) begin
        // TODO: avoid conflict with interpreter stage
        flitOutQueue.enq(flit);
        flitInPort.get;
      end
    end
  endrule

  // State 1: buffer flits that do contain routing keys
  rule consumeMessage1 (consumeState == 1);
    Flit flit = flitInPort.value;
    if (flitInPort.canGet) begin
      flitInPort.get;
      consumeKey <= getRoutingKey(flit.dest.addr);
      // Write to flit buffer
      flitBuffer.write({chosenReg, consumeFlitCount}, flit);
      consumeFlitCount <= consumeFlitCount + 1;
      // On final flit, move to fetch state
      if (! flit.notFinalFlit) begin
        consumeState <= 2;
        // Claim chosen slot
        flitBufferUsedSlots[chosenReg].set;
      end
    end
  endrule

  // State 2: fetch routing beats
  rule consumeMessage2 (consumeState == 2);
    // Have we finished fetching beats?
    Bool finished = fetchBeatCount + `ProgRouterMaxBurst >= consumeKey.len;
    // Prepare inflight RAM request info
    // (to handle out of order resps from the RAMs)
    InflightFetcherReqInfo info;
    info.msgAddr = chosenReg;
    info.burst = min(consumeKey.len - fetchBeatCount, `ProgRouterMaxBurst);
    info.finalBurst = finished;
    // Prepare RAM request
    DRAMReq req;
    req.isStore = False;
    req.id = fromInteger(`DCachesPerDRAM + myId);
    req.addr = {1'b0, consumeKey.ptr + fetchBeatCount};
    req.data = zeroExtend(pack(info));
    req.burst = info.burst;
    // Don't overfetch (beat buffer has finite size)
    if (ramReqQueue[consumeKey.ram].notFull &&
          beatBufferLen.available >= zeroExtend(req.burst)) begin
        ramReqQueue[consumeKey.ram].enq(req);
        fetchBeatCount <= fetchBeatCount + req.burst;
        beatBufferLen.incBy(zeroExtend(req.burst));
        if (finished) consumeState <= 0;
      end
    end
  endrule

  // Stage 2: consume RAM responses
  // ------------------------------



endmodule

// =============================================================================
// Programmable router
// =============================================================================

interface ProgRouter;
  // Incoming and outgoing flits
  interface Vector#(`FetchersPerProgRouter, In#(Flit) flitIn);
  interface Vector#(`FetchersPerProgRouter, Out#(Flit) flitOut);

  // Interface to off-chip memory
  interface Vector#(`DRAMsPerBoard,
    Vector#(`FetchersPerProgRouter, BOut#(DRAMReq))) ramReqs;
  interface Vector#(`DRAMsPerBoard,
    Vector#(`FetchersPerProgRouter, In#(DRAMResp))) ramResps;
endinterface

module mkProgRouter (ProgRouter);

  // Flit input ports
  Vector#(`FetchersPerProgRouter, InPort#(Flit)) flitInPort <-
    replicateM(mkInPort);

endmodule

endpackage
