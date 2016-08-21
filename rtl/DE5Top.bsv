package DE5Top;

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

typedef Empty DE5Top;

`else

interface DE5Top;
  interface DRAMExtIfc dramIfc;
  (* always_enabled *)
  method Vector#(`CoresPerTile, Bit#(32)) coreOut;
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module de5Top (DE5Top);
  // Components
  let dram   <- mkDRAM;
  let dcache <- mkDCache(0);
  Vector#(`CoresPerTile, Core) cores;
  for (Integer i = 0; i < `CoresPerTile; i=i+1)
    cores[i] <- mkCore(fromInteger(i));
  
  // Connect cores to data cache request line
  function getDCacheReqOut(core) = core.dcacheReqOut;
  let dcacheReqs <- mkMergeTree(Fair,
                      mkUGShiftQueue1(QueueOptFmax),
                      map(getDCacheReqOut, cores));
  connectUsing(mkUGQueue, dcacheReqs, dcache.reqIn);

  // Connect data cache response line to cores
  function Bit#(`LogCoresPerTile) getDCacheRespKey(DCacheResp resp) =
    truncateLSB(resp.id);
  function getDCacheRespIn(core) = core.dcacheRespIn;
  let dcacheResps <- mkResponseDistributor(
                      getDCacheRespKey,
                      mkUGShiftQueue1(QueueOptFmax),
                      map(getDCacheRespIn, cores));
  connectUsing(mkUGQueue, dcache.respOut, dcacheResps);

  // Connect data cache to DRAM
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 dcache.reqOut, dram.reqIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 dram.loadResp, dcache.loadRespIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 dram.storeResp, dcache.storeRespIn);

  rule display;
    for (Integer i = 0; i < `CoresPerTile; i=i+1)
      $display(i, " @ ", $time, ": ", cores[i].out);
  endrule

  `ifndef SIMULATE
  interface DRAMExtIfc dramIfc = dram.external;
  function Bit#(32) getOut(Core core) = core.out;
  method Vector#(`CoresPerTile, Bit#(32)) coreOut = map(getOut, cores);
  `endif
endmodule

endpackage
