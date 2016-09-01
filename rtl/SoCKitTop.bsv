package SoCKitTop;

// ============================================================================
// Imports
// ============================================================================

import Core      :: *;
import DCache    :: *;
import Mem       :: *;
import DRAM      :: *;
import Interface :: *;
import Queue     :: *;
import Vector    :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty SoCKitTop;

`else

interface SoCKitTop;
  interface DRAMExtIfc dramIfc;
  (* always_enabled *)
  method Bit#(32) coreOut;
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module sockitTop (SoCKitTop);
  // DRAM interface
  DRAM dram <- mkDRAM;
  
  // Several caches per DRAM
  Vector#(`DCachesPerDRAM, DCache) dcaches;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1)
    dcaches[i] <- mkDCache(fromInteger(i));

  // Several cores per DCache
  Vector#(`DCachesPerDRAM, Vector#(`CoresPerDCache, Core)) cores = newVector;
  Integer coreCount = 0;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1)
    for (Integer j = 0; j < `CoresPerDCache; j=j+1) begin
      cores[i][j] <- mkCore(fromInteger(coreCount));
      coreCount = coreCount+1;
    end
  
  // Connect cores to data cache request line
  function getDCacheReqLine(core) = core.dcacheReqOut;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1) begin
    let dcacheReqs <- mkMergeTree(Fair,
                        mkUGShiftQueue1(QueueOptFmax),
                        map(getDCacheReqLine, cores[i]));
    connectUsing(mkUGQueue, dcacheReqs, dcaches[i].reqIn);
  end

  // Connect data cache response line to cores
  function Bit#(`LogCoresPerDCache) getDCacheRespKey(DCacheResp resp) =
    truncateLSB(resp.id);
  function getDCacheRespIn(core) = core.dcacheRespIn;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1) begin
    let dcacheResps <- mkResponseDistributor(
                        getDCacheRespKey,
                        mkUGShiftQueue1(QueueOptFmax),
                        map(getDCacheRespIn, cores[i]));
    connectUsing(mkUGQueue, dcaches[i].respOut, dcacheResps);
  end

  // Connect caches to DRAM request line
  function getDRAMReqLine(dcache) = dcache.reqOut;
  let dramReqs <- mkMergeTree(Fair,
                    mkUGShiftQueue1(QueueOptFmax),
                    map(getDRAMReqLine, dcaches));
  connectUsing(mkUGQueue, dramReqs, dram.reqIn);

  // Connect DRAM load response line to caches
  function Bit#(`LogCoresPerDCache) getDRAMLoadRespKey(MemLoadResp resp) =
    truncateLSB(resp.id);
  function getDRAMLoadRespIn(dcache) = dcache.loadRespIn;
  let dramLoadResps <- mkResponseDistributor(
                         getDRAMLoadRespKey,
                         mkUGShiftQueue1(QueueOptFmax),
                         map(getDRAMLoadRespIn, dcaches));
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 dram.loadResp, dramLoadResps);

  // Connect DRAM store response line to caches
  function Bit#(`LogCoresPerDCache) getDRAMStoreRespKey(MemStoreResp resp) =
    truncateLSB(resp.id);
  function getDRAMStoreRespIn(dcache) = dcache.storeRespIn;
  let dramStoreResps <- mkResponseDistributor(
                          getDRAMStoreRespKey,
                          mkUGShiftQueue1(QueueOptFmax),
                          map(getDRAMStoreRespIn, dcaches));
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 dram.storeResp, dramStoreResps);

  rule display;
    Integer coreId = 0;
    for (Integer i = 0; i < `DCachesPerDRAM; i=i+1)
      for (Integer j = 0; j < `CoresPerDCache; j=j+1) begin
        $display(coreId, " @ ", $time, ": ", cores[i][j].out);
        coreId = coreId+1;
      end
  endrule

  `ifndef SIMULATE
  interface DRAMExtIfc dramIfc = dram.external;
  method Bit#(32) coreOut = cores[0][0].out;
  `endif
endmodule

endpackage
