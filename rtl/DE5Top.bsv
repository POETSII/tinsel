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
import Ring      :: *;

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

  // Create cores
  Integer coreCount = 0;
  Vector#(`DCachesPerDRAM, Vector#(`CoresPerDCache, Core)) cores = newVector;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1)
    for (Integer j = 0; j < `CoresPerDCache; j=j+1) begin
      cores[i][j] <- mkCore(fromInteger(coreCount));
      coreCount = coreCount+1;
    end
  
  // Connect cores to data caches
  function dcacheClient(core) = core.dcacheClient;
  for (Integer i = 0; i < `DCachesPerDRAM; i=i+1) begin
    connectCoresToDCache(map(dcacheClient, cores[i]), dcaches[i]);
  end

  // Connect data caches to DRAM
  connectDCachesToDRAM(dcaches, dram);

  // Mailboxes
  `ifdef MailboxEnabled

  // Create mailboxes
  Vector#(`RingSize, Mailbox) mailboxes;
  for (Integer i = 0; i < `RingSize; i=i+1)
    mailboxes[i] <- mkMailbox;

  // Connect cores to mailboxes
  for (Integer i = 0; i < `RingSize; i=i+1) begin
    // Get sub-vector of cores to be connected to mailbox i
    Vector#(`CoresPerMailbox, Core) cs =
      takeAt(`CoresPerMailbox*i, concat(cores));
    function mailboxClient(core) = core.mailboxClient;
    // Connect sub-vector of cores to mailbox
    connectCoresToMailbox(map(mailboxClient, cs), mailboxes[i]);
  end

  // Create ring of mailboxes
  mkRing(mailboxes);

  `endif

  rule display;
    Integer id = 0;
    for (Integer i = 0; i < `DCachesPerDRAM; i=i+1)
      for (Integer j = 0; j < `CoresPerDCache; j=j+1) begin
        $display(id, " @ ", $time, ": ", cores[i][j].out);
        id = id+1;
      end
  endrule

  `ifndef SIMULATE
  function Bit#(32) getOut(Core core) = core.out;
  interface DRAMExtIfc dramIfc = dram.external;
  method Bit#(32) coreOut = cores[0][0].out;
  `endif
endmodule

endpackage
