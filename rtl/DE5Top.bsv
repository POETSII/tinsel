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

  // Create data caches
  Vector#(`DCachesPerDRAM, DCache) dcaches;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1)
    dcaches[i] <- mkDCache(fromInteger(i));

  // Create mailbox
  `ifdef MailboxEnabled
  let mailbox <- mkMailbox;
  `endif

  // Create cores
  Vector#(`DCachesPerDRAM, Vector#(`CoresPerDCache, Core)) cores = newVector;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1)
    for (Integer j = 0; j < `CoresPerDCache; j=j+1) begin
      Integer id = i * `DCachesPerDRAM + j;
      cores[i][j] <- mkCore(fromInteger(id));
    end
  
  // Connect cores to data caches
  function dcacheClient(core) = core.dcacheClient;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1) begin
    connectCoresToDCache(map(dcacheClient, cores[i]), dcaches[i]);
  end

  // Connect cores to mailbox
  `ifdef MailboxEnabled
  function mailboxClient(core) = core.mailboxClient;
  connectCoresToMailbox(map(mailboxClient, concat(cores)), mailbox);
  // Connect packet-out to packet-in (i.e. only one mailbox)
  connectDirect(mailbox.packetOut, mailbox.packetIn);
  `endif

  // Connect data caches to DRAM
  connectDCachesToDRAM(dcaches, dram);

  rule display;
    for (Integer i = 0; i < `DCachesPerDRAM; i=i+1)
      for (Integer j = 0; j < `CoresPerDCache; j=j+1) begin
        Integer id = i * `DCachesPerDRAM + j;
        $display(id, " @ ", $time, ": ", cores[i][j].out);
      end
  endrule

  `ifndef SIMULATE
  function Bit#(32) getOut(Core core) = core.out;
  interface DRAMExtIfc dramIfc = dram.external;
  method Bit#(32) coreOut = cores[0][0].out;
  `endif
endmodule

endpackage
