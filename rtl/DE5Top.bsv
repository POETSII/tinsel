package DE5Top;

// ============================================================================
// Imports
// ============================================================================

import Core      :: *;
import DCache    :: *;
import Globals   :: *;
import DRAM      :: *;
import Interface :: *;
import Queue     :: *;
import Vector    :: *;
import Mailbox   :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

`else

interface DE5Top;
  interface DRAMExtIfc dramIfc;
  (* always_enabled *)
  method Bit#(32) coreOut;
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module de5Top (DE5Top);
  // Create DRAM
  let dram <- mkDRAM;

  // Create data cache
  let dcache <- mkDCache(0);

  // Create mailbox
  let mailbox <- mkMailbox;

  // Create cores
  Vector#(`CoresPerDCache, Core) cores;
  for (Integer i = 0; i < `CoresPerDCache; i=i+1)
    cores[i] <- mkCore(fromInteger(i));
  
  // Connect cores to data cache
  function dcacheClient(core) = core.dcacheClient;
  connectCoresToDCache(map(dcacheClient, cores), dcache);

  // Connect cores to mailbox
  `ifdef MailboxEnabled
  function mailboxClient(core) = core.mailboxClient;
  connectCoresToMailbox(map(mailboxClient, cores), mailbox);
  `endif

  // Connect data cache to DRAM
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 dcache.reqOut, dram.reqIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 dram.loadResp, dcache.loadRespIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 dram.storeResp, dcache.storeRespIn);

  rule display;
    for (Integer i = 0; i < `CoresPerDCache; i=i+1)
      $display(i, " @ ", $time, ": ", cores[i].out);
  endrule

  `ifndef SIMULATE
  function Bit#(32) getOut(Core core) = core.out;
  interface DRAMExtIfc dramIfc = dram.external;
  method Bit#(32) coreOut = cores[0].out;
  `endif
endmodule

endpackage
