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
  interface Vector#(`NumDRAMPorts, DRAMExtIfc) dramIfcs;
  (* always_enabled *)
  method Bit#(32) coreOut;
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module sockitTop (SoCKitTop);
  // Several ports to DRAM
  Vector#(`NumDRAMPorts, DRAM) dramPorts <- replicateM(mkDRAM);
  
  // One DCache per DRAM port
  // (In future, we might generalise this)
  Vector#(`NumDRAMPorts, DCache) dcaches;
  for (Integer i = 0; i < `NumDRAMPorts; i=i+1)
    dcaches[i] <- mkDCache(0);

  // Several cores per DCache
  Vector#(`NumDRAMPorts, Vector#(`CoresPerDCache, Core)) cores = newVector;
  Integer coreCount = 0;
  for (Integer i = 0; i < `NumDRAMPorts; i=i+1)
    for (Integer j = 0; j < `CoresPerDCache; j=j+1) begin
      cores[i][j] <- mkCore(fromInteger(coreCount));
      coreCount = coreCount+1;
    end
  
  // Connect cores to data cache request line
  function getDCacheReqOut(core) = core.dcacheReqOut;
  for (Integer i = 0; i < `NumDRAMPorts; i=i+1) begin
    let dcacheReqs <- mkMergeTree(Fair,
                        mkUGShiftQueue1(QueueOptFmax),
                        map(getDCacheReqOut, cores[i]));
    connectUsing(mkUGQueue, dcacheReqs, dcaches[i].reqIn);
  end

  // Connect data cache response line to cores
  function Bit#(`LogCoresPerDCache) getDCacheRespKey(DCacheResp resp) =
    truncateLSB(resp.id);
  function getDCacheRespIn(core) = core.dcacheRespIn;
  for (Integer i = 0; i < `NumDRAMPorts; i=i+1) begin
    let dcacheResps <- mkResponseDistributor(
                        getDCacheRespKey,
                        mkUGShiftQueue1(QueueOptFmax),
                        map(getDCacheRespIn, cores[i]));
    connectUsing(mkUGQueue, dcaches[i].respOut, dcacheResps);
  end

  // Connect data cache to DRAM
  for (Integer i = 0; i < `NumDRAMPorts; i=i+1) begin
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   dcaches[i].reqOut, dramPorts[i].reqIn);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   dramPorts[i].loadResp, dcaches[i].loadRespIn);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   dramPorts[i].storeResp, dcaches[i].storeRespIn);
  end

  rule display;
    Integer coreId = 0;
    for (Integer i = 0; i < `NumDRAMPorts; i=i+1)
      for (Integer j = 0; j < `CoresPerDCache; j=j+1) begin
        $display(coreId, " @ ", $time, ": ", cores[i][j].out);
        coreId = coreId+1;
      end
  endrule

  `ifndef SIMULATE
  function Bit#(32) getOut(Core core) = core.out;
  Vector#(`NumDRAMPorts, DRAMExtIfc) ifcs;
  for (Integer i = 0; i < `NumDRAMPorts; i=i+1)
    ifcs[i] = dramPorts[i].external;
  interface dramIfcs = ifcs;
  method Bit#(32) coreOut = cores[0][0].out;
  `endif
endmodule

endpackage
