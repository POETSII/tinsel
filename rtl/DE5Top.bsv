package DE5Top;

// ============================================================================
// Imports
// ============================================================================

import Tinsel :: *;
import DCache :: *;
import Mem    :: *;
import DRAM   :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

`else

interface DE5Top;
  interface DRAMExtIfc dramIfc;
  interface Tinsel tinselIfc;
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module de5Top (DE5Top);
  let dram   <- mkDRAM;
  let dcache <- mkDCache(0, dram.internal);
  let tinsel <- tinselCore(0, dcache);

  rule display;
    $display($time, ": ", tinsel.out);
  endrule

  `ifndef SIMULATE
  interface DRAMExtIfc dramIfc = dram.external;
  interface Tinsel tinselIfc = tinsel;
  `endif
endmodule

endpackage
