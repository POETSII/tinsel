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
import Host      :: *;
import JtagUart  :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

`else

interface DE5Top;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface JtagUartAvalon jtagIfc;
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module de5Top (DE5Top);
  // Create DRAMs
  Vector#(`DRAMsPerBoard, DRAM) drams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    drams[i] <- mkDRAM(fromInteger(i));

  // Create data caches
  Vector#(`DRAMsPerBoard,
    Vector#(`DCachesPerDRAM, DCache)) dcaches = newVector;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
      dcaches[i][j] <- mkDCache(fromInteger(j));

  // Create cores
  Integer coreCount = 0;
  Vector#(`DRAMsPerBoard,
    Vector#(`DCachesPerDRAM,
      Vector#(`CoresPerDCache, Core))) cores = newVector;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
      for (Integer k = 0; k < `CoresPerDCache; k=k+1) begin
        cores[i][j][k] <- mkCore(fromInteger(coreCount));
        coreCount = coreCount+1;
      end

  // Connect cores to data caches
  function dcacheClient(core) = core.dcacheClient;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
      connectCoresToDCache(map(dcacheClient, cores[i][j]), dcaches[i][j]);

  // Connect data caches to DRAM
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    connectDCachesToDRAM(dcaches[i], drams[i]);

  // Create mailboxes
  Vector#(`MailboxesPerBoard, Mailbox) mailboxes;
  for (Integer i = 0; i < `MailboxesPerBoard; i=i+1)
    mailboxes[i] <- mkMailbox;

  // Connect cores to mailboxes
  let vecOfCores = concat(concat(cores));
  for (Integer i = 0; i < `MailboxesPerBoard; i=i+1) begin
    // Get sub-vector of cores to be connected to mailbox i
    Vector#(`CoresPerMailbox, Core) cs =
      takeAt(`CoresPerMailbox*i, vecOfCores);
    function mailboxClient(core) = core.mailboxClient;
    // Connect sub-vector of cores to mailbox
    connectCoresToMailbox(map(mailboxClient, cs), mailboxes[i]);
  end

  // Create host interface
  Host host <- mkHost;

  // Create ring of mailboxes
  function MailboxNet mailboxNet(Mailbox mbox) = mbox.net;
  mkRing(append(map(mailboxNet, mailboxes), cons(host.net, nil)));

  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(DRAM dram) = dram.external;
  interface dramIfcs = map(getDRAMExtIfc, drams);
  interface jtagIfc = host.jtagAvalon;
  `endif
endmodule

endpackage
