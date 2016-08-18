package DE5Top;

// ============================================================================
// Imports
// ============================================================================

import Tinsel    :: *;
import DCache    :: *;
import Mem       :: *;
import DRAM      :: *;
import Interface :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

`else

interface DE5Top;
  interface DRAMExtIfc dramIfc;
  (* always_enabled *)
  method Bit#(32) tinselOut;
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module de5Top (DE5Top);
  // Components
  let dram   <- mkDRAM;
  let dcache <- mkDCache(0);
  let tinsel <- tinselCore(0);

  // Connect core to data cache
  connectRegFmax(tinsel.dcacheReqOut, dcache.reqIn);
  connectRegFmax(dcache.respOut, tinsel.dcacheRespIn);

  // Connect data cache to DRAM
  connectRegFmax(dcache.reqOut, dram.reqIn);
  connectRegFmax(dram.loadResp, dcache.loadRespIn);
  connectRegFmax(dram.storeResp, dcache.storeRespIn);

  rule display;
    $display($time, ": ", tinsel.out);
  endrule

  `ifndef SIMULATE
  interface DRAMExtIfc dramIfc = dram.external;
  method Bit#(32) tinselOut = tinsel.out;
  `endif
endmodule

endpackage
