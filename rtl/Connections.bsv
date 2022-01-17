package Connections;

import Vector      :: *;
import OffChipRAM  :: *;
import Interface   :: *;
import DRAM        :: *;
import Queue       :: *;
import DCache      :: *;
import DCacheTypes :: *;
import Util        :: *;
`ifdef DefinedIfProgRouterEnabled
import ProgRouter :: *;
`else
import Router :: *;
`endif
import Core        :: *;

// ============================================================================
// DCache <-> Core connections
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

  // Connect performance-counter wires
  rule connectPerfCountWires;
    clients[0].incMissCount(dcache.incMissCount);
    clients[0].incHitCount(dcache.incHitCount);
    clients[0].incWritebackCount(dcache.incWritebackCount);
    for (Integer i = 1; i < `CoresPerDCache; i=i+1) begin
      clients[i].incMissCount(False);
      clients[i].incHitCount(False);
      clients[i].incWritebackCount(False);
    end
  endrule

endmodule

// ============================================================================
// Off-chip RAM connections
// ============================================================================

module connectClientsToOffChipRAM#(
  // Data caches
  Vector#(`DCachesPerDRAM, DCache) caches,
  // Reqs and resps from ProgRouter's fetchers
  Vector#(`FetchersPerProgRouter, BOut#(DRAMReq)) routerReqs,
  Vector#(`FetchersPerProgRouter, In#(DRAMResp)) routerResps,
  // Off-chip memory
`ifdef StratixV
  OffChipRAMStratixV ram
`endif
`ifdef Stratix10
  OffChipRAMStratix10 ram
`endif
  ) ();

  // Count the number of outstanding fetcher requests
  // Used to throttle the fetcher requests to avoid starving/blocking
  // the cache requests
  Integer throttleCount = 2 ** (`DRAMLogMaxInFlight - 1);
  Count#(`DRAMLogMaxInFlight) fetcherCount <- mkCount(throttleCount);

  // Merge cache requests
  function getReqOut(cache) = cache.reqOut;
  Out#(DRAMReq) cacheReqs <-
    mkMergeTreeB(Fair,
      mkUGShiftQueue1(QueueOptFmax),
      map(getReqOut, caches));
  Queue#(DRAMReq) cacheReqsQueue <- mkUGQueue;
  connectToQueue(cacheReqs, cacheReqsQueue);
  BOut#(DRAMReq) cacheReqsB = queueToBOut(cacheReqsQueue);

  // Merge router requests
  Out#(DRAMReq) fetcherReqs <-
    mkMergeTreeB(Fair,
      mkUGShiftQueue1(QueueOptFmax),
      routerReqs);
  Queue#(DRAMReq) fetcherReqsQueue <- mkUGQueue;
  connectToQueue(fetcherReqs, fetcherReqsQueue);
  BOut#(DRAMReq) fetcherReqsB = queueToBOut(fetcherReqsQueue);

  // Update count on router request
  BOut#(DRAMReq) fetcherReqsIncCountB =
    interface BOut
      method Action get =
        action
          fetcherReqsB.get;
          fetcherCount.incBy(zeroExtend(fetcherReqsB.value.burst));
        endaction;
      method Bool valid = fetcherReqsB.valid &&
        zeroExtend(fetcherReqsB.value.burst) <= fetcherCount.available;
      method DRAMReq value = fetcherReqsB.value;
    endinterface;

  // Merge cache and router requests, and connect to off-chip RAM
  let reqs <- mkMergeTwoB(Fair, cacheReqsB, fetcherReqsIncCountB);
  connectUsing(mkUGQueue, reqs, ram.reqIn);

  // Connect load responses
  function DRAMClientId getRespKey(DRAMResp resp) = resp.id;
  function getRespIn(cache) = cache.respIn;
  let ramResps <- mkResponseDistributor(
                    getRespKey,
                    mkUGShiftQueue2(QueueOptFmax),
                    append(map(getRespIn, caches), routerResps));

  // Update count on respose
  BOut#(DRAMResp) ramRespOutDecCount =
    interface BOut
      method Action get =
        action
          ram.respOut.get;
          if (ram.respOut.value.id >= fromInteger(`DCachesPerDRAM))
            fetcherCount.dec;
        endaction;
      method Bool valid = ram.respOut.valid;
      method DRAMResp value = ram.respOut.value;
    endinterface;

  // Connect responses from off-chip RAM
  connectDirect(ramRespOutDecCount, ramResps);

endmodule

// ============================================================================
// ProgRouter performance counter connections
// ============================================================================

module connectProgRouterPerfCountersToCores#(
         ProgRouterPerfCounters counters, Vector#(n, Core) cores) (Empty);
  rule connect;
    // Only core zero can access the ProgRouter perf counters
    cores[0].progRouterPerfClient.incSent(counters.incSent);
    cores[0].progRouterPerfClient.incSentInterBoard(counters.incSentInterBoard);
    for (Integer i = 1; i < valueOf(n); i=i+1) begin
      cores[i].progRouterPerfClient.incSent(?);
      cores[i].progRouterPerfClient.incSentInterBoard(?);
    end
  endrule
endmodule

endpackage
