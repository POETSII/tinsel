// SPDX-License-Identifier: BSD-2-Clause
// Functions, data types, and modules for routers (non-programmable)
package Router;

import Globals    :: *;
import Util       :: *;
import DRAM       :: *;
import Vector     :: *;
import Queue      :: *;
import Interface  :: *;
import BlockRam   :: *;
import Assert     :: *;
import Util       :: *;
import DReg       :: *;
import Reserved   :: *;
// import ProgRouter :: *;

// =============================================================================
// Routing keys and beats
// =============================================================================

// A routing record is either 48 bits or 96 bits in size (aligned on a
// 48-bit or 96-bit boundary respectively). Multiple records are
// packed into a 256-bit DRAM beat (aligned on a 256-bit boundary).
// The most significant 16 bits of the beat contain a count of the
// number of records in the beat (in the range 1 to 5 inclusive). The
// remaining 240 bits contain records. The first record lies in the
// least-significant bits of the beat. The size portion of the routing
// key contains the number of contiguous DRAM beats holding all
// records for the key.

// // 256-bit routing beat
// typedef struct {
//   // Number of records present
//   Bit#(16) size;
//   // The 48-bit record chunks
//   Vector#(5, Bit#(48)) chunks;
// } RoutingBeat deriving (Bits, FShow);
//
// // 32-bit routing key
// typedef struct {
//   // Which off-chip RAM?
//   Bit#(`LogDRAMsPerBoard) ram;
//   // Pointer to array of routing beats containing routing records
//   Bit#(`LogBeatsPerDRAM) ptr;
//   // Number of beats in the array
//   Bit#(`LogRoutingEntryLen) numBeats;
//   Reserved#(TSub#(32, TAdd#(TAdd#(`LogDRAMsPerBoard, `LogBeatsPerDRAM), `LogRoutingEntryLen))) unused;
// } RoutingKey deriving (Bits, FShow);
//
// // Extract routing key from an address
// function RoutingKey getRoutingKey(NetAddr addr) =
//   unpack(getRoutingKeyRaw(addr));

// =============================================================================
// Types of routing record
// =============================================================================

typedef enum {
  URM1 = 3'd0, // 48-bit Unicast Router-to-Mailbox
  URM2 = 3'd1, // 96-bit Unicast Router-to-Mailbox
  RR   = 3'd2, // 48-bit Router-to-Router
  MRM  = 3'd3, // 96-bit Multicast Router-to-Mailbox
  IND  = 3'd4  // 48-bit Indirection
} RoutingRecordTag deriving (Bits, Eq, FShow);

typedef enum {
  NORTH = 2'd0,
  SOUTH = 2'd1,
  EAST  = 2'd2,
  WEST  = 2'd3
} RoutingDir deriving (Bits, Eq);

// Records missing; we don't store a indirection table.

// =============================================================================
// Internal types
// =============================================================================

// It is sometimes convenient (though redundant) to record a routing
// decision for a flit internally within the programmable router
typedef struct {
  // Normal flit
  Flit flit;
  // Routing decision for flit
  RoutingDecision decision;
} RoutedFlit deriving (Bits, FShow);

// Routing decision
typedef enum {
  RouteNorth,
  RouteSouth,
  RouteEast,
  RouteWest,
  RouteNoC
} RoutingDecision deriving (Bits, Eq, FShow);

// // Elements of the indirection queue inside each fetcher
// typedef struct {
//   // The indirection
//   RoutingKey key;
//   // The location of the message in the flit buffer
//   FetcherFlitBufferMsgAddr addr;
// } IndQueueEntry deriving (Bits, FShow);


// =============================================================================
// Design
// =============================================================================

// In the following diagram N/S/E/W are the inter-FPGA links and
// L0..L3 are links at one edge of the NoC.  Depending on the NoC
// dimensions, there may be more or less than four links on a single
// NoC edge, but the diagram assumes four.

//
//   N     S     E     W    L0     L1    L2    L3     Input flits
//   |     |     |     |     |     |     |     |
// +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+
// | F | | F | | F | | F | | F | | F | | F | | F |    Fetchers
// +---+ +---+ +---+ +---+ +---+ +---+ +---+ +---+
//   |     |     |     |     |     |     |     |
// +-------------------------------------------+
// |                 Crossbar                  |      Routing
// +-------------------------------------------+
//   |     |     |     |     |     |     |     |
//   N     S     E     W     L0    L1    L2    L3     Output queues

// The core functionality is implemented in the fetchers, which:
//   (1) extract routing keys from incoming flits;
//   (2) lookup the keys in RAM;
//   (3) interpret the resulting routing records; and
//   (4) emit the interpreted flits.

// The key property of these fetchers is that they act entirely
// indepdedently of each other: each one can make progress even if
// another is blocked.  This leads to duplicated logic resources, but
// is necessary to avoid deadlock.

// As the routers are fully programmable, it is possible for the
// programmer to introduce deadlock using an ill-defined routing
// scheme, e.g. where a flit arrives in on (say) link N and requires a
// flit to be sent back along the same direction N.  However, the
// hardware does guarantee deadlock-freedom if the routing scheme is
// based on dimension-ordered routing.

// After the fetchers have interpreted the flits, they are fed to a
// fair crossbar which organises them by destination into output
// queues.

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
// typedef struct {
//   // Message address in the fetcher's flit buffer
//   FetcherFlitBufferMsgAddr msgAddr;
//   // How many beats in the burst?
//   Bit#(`BeatBurstWidth) burst;
//   // Is this the final burst of routing records for the current key?
//   Bool finalBurst;
//   // Are we processing a max-sized key (which must contain an IND record)?
//   Bool isMaxSizedKey;
// } InflightFetcherReqInfo deriving (Bits, FShow);

// Routing beat, tagged with the beat number in the DRAM burst
// typedef struct {
//   // Beat
//   RoutingBeat beat;
//   // Beat number
//   Bit#(`BeatBurstWidth) beatNum;
//   // Inflight request info
//   InflightFetcherReqInfo info;
// } NumberedRoutingBeat deriving (Bits, FShow);

// Fetcher interface
interface Fetcher;
  // Incoming and outgoing flits
  interface In#(Flit) flitIn;
  interface BOut#(RoutedFlit) flitOut;
  // Activity
  interface FetcherActivity activity;
endinterface

// Fetcher activity for performance counters and termination detection
(* always_ready *)
interface FetcherActivity;
  // Increment number of sent messages
  method Bit#(1) incSent;
  // Increment number of messages sent to another board
  method Bit#(1) incSentInterBoard;
  // Increment number of received messages
  method Bit#(1) incReceived;
  // Active (in the termination-detection sense)?
  method Bool active;
endinterface

// Fetcher module
module mkPassthroughFetcher#(BoardId boardId, Integer fetcherId) (Fetcher);

  // Flit input port
  InPort#(Flit) flitInPort <- mkInPort;

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
  BlockRam#(FetcherFlitBufferAddr, Flit) flitBuffer <-
    mkBlockRamOpts(flitBufferOpts);

  // Beat buffer
  // SizedQueue#(`FetcherLogBeatBufferSize, NumberedRoutingBeat)
  //   beatBuffer <- mkUGSizedQueue;

  // Track length of beat buffer, so that we don't overfetch
  Count#(TAdd#(`FetcherLogBeatBufferSize, 1)) beatBufferLen <-
      mkCount(2 ** `FetcherLogBeatBufferSize);

  // For flits whose destinations are *not* routing keys
  Queue1#(RoutedFlit) flitBypassQueue <- mkUGShiftQueue(QueueOptFmax);

  // For flits whose destinations are routing keys
  Queue1#(RoutedFlit) flitProcessedQueue <- mkUGShiftQueue(QueueOptFmax);

  // Final output queue for flits
  Queue1#(RoutedFlit) flitOutQueue <- mkUGShiftQueue(QueueOptFmax);

  // Indirection queue and size
  // SizedQueue#(`FetcherLogIndQueueSize, IndQueueEntry) indQueue <-
  //   mkUGShiftQueue(QueueOptFmax);
  // Count#(TAdd#(`FetcherLogIndQueueSize, 1)) indQueueLen <-
  //     mkCount(2 ** `FetcherLogIndQueueSize);

  // Activity
  Reg#(Bit#(1)) incSentReg <- mkDReg(0);
  Reg#(Bit#(1)) incSentInterBoardReg <- mkDReg(0);
  Reg#(Bit#(1)) incReceivedReg <- mkDReg(0);

  // Stage 1: consume input message
  // ------------------------------

  // Consumer state
  // State 0: pass through flits that don't contain routing keys
  // State 1: buffer flits that do contain routing keys
  // State 2: fetch routing beats
  Reg#(Bit#(2)) consumeState <- mkReg(0);

  // Count number of flits of message consumed so far
  Reg#(Bit#(`LogMaxFlitsPerMsg)) consumeFlitCount <- mkReg(0);

  // Flit slot allocator
  Vector#(`FetcherMsgsPerFlitBuffer, SetReset) flitBufferUsedSlots <-
    replicateM(mkSetReset(False));

  // Chosen message slot
  Reg#(FetcherFlitBufferMsgAddr) chosenReg <- mkRegU;

  // Routing key of message consumed
  // Reg#(RoutingKey) consumeKey <- mkRegU;

  // Maintain count of routing beats fetched so far
  Reg#(Bit#(`LogRoutingEntryLen)) fetchBeatCount <- mkReg(0);

  // Track when messages are bypassing fetcher, to keep the bypass atomic
  Reg#(Bool) bypassInProgress <- mkReg(False);

  // State 0: pass through flits that don't contain routing keys
  rule consumeMessage0 (consumeState == 0);
    Flit flit = flitInPort.value;
    // Find unused message slot
    if (flitInPort.canGet) begin
      if (flitOutQueue.notFull) begin
        flitInPort.get;
        bypassInProgress <= flit.notFinalFlit;
        // Make routing decision
        RoutingDecision decision = RouteNoC;
        MailboxNetAddr addr = flit.dest.addr;
        if (addr.board.y < boardId.y) decision = RouteSouth;
        else if (addr.board.y > boardId.y) decision = RouteNorth;
        else if (addr.host.valid)
          decision = addr.host.value == 0 ? RouteWest : RouteEast;
        else if (addr.board.x < boardId.x) decision = RouteWest;
        else if (addr.board.x > boardId.x) decision = RouteEast;
        // Insert into bypass queue
        flitOutQueue.enq(RoutedFlit { decision: decision, flit: flit});
      end
    end

  endrule


  // Stage 3: merge output queues
  // ----------------------------

  // We want to merge messages, not flits
  // Are we in the middle of consuming a message?
  // Reg#(Bool) mergeInProgress <- mkReg(False);
  // Reg#(Bool) prevFromBypass <- mkReg(False);
  //
  // rule merge (flitOutQueue.notFull);
  //   // We only have the bypass queue
  //   if (flitBypassQueue.canDeq) begin
  //     flitBypassQueue.deq;
  //     flitOutQueue.enq(flitBypassQueue.dataOut);
  //     mergeInProgress <= flitBypassQueue.dataOut.flit.notFinalFlit;
  //     prevFromBypass <= True;
  //   end
  // endrule

  // Interfaces
  // -----------

  interface flitIn = flitInPort.in;
  interface flitOut = queueToBOut(flitOutQueue);

  interface FetcherActivity activity;
    method Bit#(1) incSent = incSentReg;
    method Bit#(1) incSentInterBoard = incSentInterBoardReg;
    method Bit#(1) incReceived = incReceivedReg;
    method Bool active =
      beatBufferLen.value != 0 || consumeState != 0;
  endinterface

endmodule


// =============================================================================
// Crossbar
// =============================================================================

// Selector function for a mux in the programmable router crossbar
typedef function Bool selector(RoutedFlit flit) SelectorFunc;

module mkProgRouterCrossbar#(
         Vector#(numOut, SelectorFunc) f,
         Vector#(numIn, BOut#(RoutedFlit)) out)
           (Vector#(numOut, BOut#(RoutedFlit)))
             provisos(Add#(a__, 1, numIn));

  // Input ports
  Vector#(numIn, InPort#(RoutedFlit)) inPort <- replicateM(mkInPort);

  // Connect up input ports
  for (Integer i = 0; i < valueOf(numIn); i=i+1)
    connectDirect(out[i], inPort[i].in);

  // Cosume wires, for each input port
  Vector#(numIn, PulseWire) consumeWire <- replicateM(mkPulseWireOR);

  // Keep track of service history for flit sources (for fair selection)
  Vector#(numOut, Reg#(Bit#(numIn))) hist <- replicateM(mkReg(0));

  // Current choice of flit source
  Vector#(numOut, Reg#(Bit#(numIn))) choiceReg <- replicateM(mkReg(0));

  // Output queues
  Vector#(numOut, Queue#(RoutedFlit)) outQueue <-
    replicateM(mkUGShiftQueue(QueueOptFmax));

  // Selector mux for each out queue
  for (Integer i = 0; i < valueOf(numOut); i=i+1) begin

    rule select;
      // Vector of input flits and available flits
      Vector#(numIn, RoutedFlit) flits = newVector;
      Vector#(numIn, Bool) nextAvails = newVector;
      Bool avail = False;
      for (Integer j = 0; j < valueOf(numIn); j=j+1) begin
        flits[j] = inPort[j].value;
        nextAvails[j] = inPort[j].canGet && f[i](inPort[j].value)
                          && choiceReg[i][j] == 0;
        avail = avail || (choiceReg[i][j] == 1 && inPort[j].canGet);
      end
      Bit#(numIn) nextAvail = pack(nextAvails);
      // Choose a new source using fair scheduler
      match {.newHist, .nextChoice} = sched(hist[i], nextAvail);
      // Select a flit
      RoutedFlit flit = oneHotSelect(unpack(choiceReg[i]), flits);
      // Consume a flit
      if (avail) begin
        if (outQueue[i].notFull) begin
          // Pass chosen flit to out queue
          outQueue[i].enq(flit);
          // On final flit of message
          if (!flit.flit.notFinalFlit) begin
            choiceReg[i] <= nextChoice;
            hist[i] <= newHist;
          end
        end
      end else if (choiceReg[i] == 0) begin
        choiceReg[i] <= nextChoice;
        hist[i] <= newHist;
      end
      // Consume from chosen source
      for (Integer j = 0; j < valueOf(numIn); j=j+1)
        if (inPort[j].canGet && choiceReg[i][j] == 1 && outQueue[i].notFull)
          consumeWire[j].send;
    endrule

  end

  // Consume from flit sources
  rule consumeFlitSources;
    for (Integer j = 0; j < valueOf(numIn); j=j+1)
      if (consumeWire[j]) inPort[j].get;
  endrule

  return map(queueToBOut, outQueue);
endmodule


// =============================================================================
// Bypass router
// =============================================================================

// Enough bits to store a count of the number of fetchers
typedef TLog#(TAdd#(`FetchersPerProgRouter, 1)) LogFetchersPerProgRouter;


// ProgRouter's performance counters
(* always_ready, always_enabled *)
interface ProgRouterPerfCounters;
  method Bit#(LogFetchersPerProgRouter) incSent;
  method Bit#(LogFetchersPerProgRouter) incSentInterBoard;
endinterface


interface ProgRouter;
  // Incoming and outgoing flits
  interface Vector#(`FetchersPerProgRouter, In#(Flit)) flitIn;
  interface Vector#(`FetchersPerProgRouter, BOut#(Flit)) flitOut;

  // // Interface to off-chip memory
  // interface Vector#(`DRAMsPerBoard,
  //   Vector#(`FetchersPerProgRouter, BOut#(DRAMReq))) ramReqs;
  // interface Vector#(`DRAMsPerBoard,
  //   Vector#(`FetchersPerProgRouter, In#(DRAMResp))) ramResps;

  // Activities & performance counters
  interface Vector#(`FetchersPerProgRouter, FetcherActivity) activities;
  interface ProgRouterPerfCounters perfCounters;
endinterface


// (* synthesize *)
module mkBypassRouter#(BoardId boardId) (ProgRouter);

  // Fetchers
  Vector#(`FetchersPerProgRouter, Fetcher) fetchers = newVector;
  for (Integer i = 0; i < `FetchersPerProgRouter; i=i+1)
    fetchers[i] <- mkPassthroughFetcher(boardId, i);

  // Crossbar routing functions
  function Bit#(`MailboxMeshXBits) xcoord(RoutedFlit rf) =
    zeroExtend(rf.flit.dest.addr.mbox.x);
  function Bool routeN(RoutedFlit rf) = rf.decision == RouteNorth;
  function Bool routeS(RoutedFlit rf) = rf.decision == RouteSouth;
  function Bool routeE(RoutedFlit rf) = rf.decision == RouteEast;
  function Bool routeW(RoutedFlit rf) = rf.decision == RouteWest;
  function Bool routeL(Bit#(`MailboxMeshXBits) x, RoutedFlit rf) =
    rf.decision == RouteNoC && xcoord(rf) == x;
  Vector#(`FetchersPerProgRouter, SelectorFunc) funcs;
  funcs[0] = routeN; funcs[1] = routeS;
  funcs[2] = routeE; funcs[3] = routeW;
  for (Integer i = 0; i < `MailboxMeshXLen; i=i+1)
    funcs[4+i] = routeL(fromInteger(i));

  // Crossbar
  function BOut#(RoutedFlit) getFetcherFlitOut(Fetcher f) = f.flitOut;
  Vector#(`FetchersPerProgRouter, BOut#(RoutedFlit)) fetcherOuts =
    map(getFetcherFlitOut, fetchers);
  Vector#(`FetchersPerProgRouter, BOut#(RoutedFlit))
    crossbarOuts <- mkProgRouterCrossbar(funcs, fetcherOuts);
  Vector#(`FetchersPerProgRouter, BOut#(Flit)) crossbarOutFlits;
  function Flit toFlit (RoutedFlit rf) = rf.flit;
  for (Integer i = 0; i < `FetchersPerProgRouter; i=i+1)
    crossbarOutFlits[i] <- onBOut(toFlit, crossbarOuts[i]);

  // Flit input interfaces
  Vector#(`FetchersPerProgRouter, In#(Flit)) flitInIfc = newVector;
  for (Integer i = 0; i < `FetchersPerProgRouter; i=i+1)
    flitInIfc[i] = fetchers[i].flitIn;

  // Performance counters
  Vector#(TExp#(TLog#(`FetchersPerProgRouter)),
    Bit#(LogFetchersPerProgRouter)) incSents = replicate(0);
  Vector#(TExp#(TLog#(`FetchersPerProgRouter)),
    Bit#(LogFetchersPerProgRouter)) incSentsInterBoard = replicate(0);
  for (Integer i = 0; i < `FetchersPerProgRouter; i=i+1) begin
    incSents[i] = zeroExtend(fetchers[i].activity.incSent);
    incSentsInterBoard[i] =
      zeroExtend(fetchers[i].activity.incSentInterBoard);
  end
  Bit#(LogFetchersPerProgRouter) numSent <-
    mkPipelinedReductionTree( \+ , 0, toList(incSents));
  Bit#(LogFetchersPerProgRouter) numSentInterBoard <-
    mkPipelinedReductionTree( \+ , 0, toList(incSentsInterBoard));

  function FetcherActivity getActivity(Fetcher f) = f.activity;
  interface flitIn = flitInIfc;
  interface flitOut = crossbarOutFlits;
  interface activities = map(getActivity, fetchers);
  interface ProgRouterPerfCounters perfCounters;
    method incSent = numSent;
    method incSentInterBoard = numSentInterBoard;
  endinterface

endmodule

// For core(s) to access ProgRouter's performance counters
(* always_ready, always_enabled *)
interface ProgRouterPerfClient;
  method Action incSent(Bit#(LogFetchersPerProgRouter) amount);
  method Action incSentInterBoard(Bit#(LogFetchersPerProgRouter) amount);
endinterface

endpackage
