package Connections;

import Vector      :: *;
import OffChipRAM  :: *;
import Interface   :: *;
import DRAM        :: *;
import Queue       :: *;
import DCache      :: *;
import DCacheTypes :: *;

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
  // Programmable per-board router, reqs and resps
  BOut#(DRAMReq) routerReqs, In#(DRAMResp) routerResps,
  // Off-chip memory
  OffChipRAM ram) ();

  // Connect requests
  function getReqOut(cache) = cache.reqOut;
  let reqs <- mkMergeTreeB(Fair,
                mkUGShiftQueue1(QueueOptFmax),
                append(map(getReqOut, caches),
                  cons(routerReqs, nil)));
  connectUsing(mkUGQueue, reqs, ram.reqIn);

  // Connect load responses
  function DRAMClientId getRespKey(DRAMResp resp) = resp.id;
  function getRespIn(cache) = cache.respIn;
  let ramResps <- mkResponseDistributor(
                    getRespKey,
                    mkUGShiftQueue2(QueueOptFmax),
                    append(map(getRespIn, caches), 
                      cons(routerResps, nil)));
  connectDirect(ram.respOut, ramResps);

endmodule

endpackage
