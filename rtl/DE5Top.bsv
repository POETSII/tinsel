package DE5Top;

// ============================================================================
// Imports
// ============================================================================

import Core         :: *;
import DCache       :: *;
import Globals      :: *;
import DRAM         :: *;
import Interface    :: *;
import Queue        :: *;
import Vector       :: *;
import Mailbox      :: *;
import Network      :: *;
import DebugLink    :: *;
import JtagUart     :: *;
import Mac          :: *;
import FPU          :: *;
import InstrMem     :: *;
import NarrowSRAM   :: *;
import OffChipRAM   :: *;
import IdleDetector :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

import "BDPI" function Bit#(32) getBoardId();

`else

interface DE5Top;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface Vector#(`SRAMsPerBoard, SRAMExtIfc) sramIfcs;
  interface Vector#(1, AvalonMac) northMac;
  interface Vector#(1, AvalonMac) southMac;
  interface Vector#(1, AvalonMac) eastMac;
  interface Vector#(1, AvalonMac) westMac;
  interface JtagUartAvalon jtagIfc;
  (* always_ready, always_enabled *)
  method Action setBoardId(Bit#(4) id);
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module de5Top (DE5Top);
  // Board Id
  `ifdef SIMULATE
  Bit#(4) localBoardId = truncate(getBoardId());
  `else
  Wire#(Bit#(4)) localBoardId <- mkDWire(?);
  `endif

  // Create off-chip RAMs
  Vector#(`DRAMsPerBoard, OffChipRAM) rams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    rams[i] <- mkOffChipRAM(fromInteger(i*3));

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

  // Create instruction memories
  `ifdef SharedInstrMem
    for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
      for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
        for (Integer k = 0; k < `CoresPerDCache; k=k+2) begin
          if (k+1 < `CoresPerDCache)
            mkDualInstrMem(cores[i][j][k].instrMemClient,
                           cores[i][j][k+1].instrMemClient);
          else
            mkInstrMem(cores[i][j][k].instrMemClient);
        end
  `else
    for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
      for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
        for (Integer k = 0; k < `CoresPerDCache; k=k+1)
          mkInstrMem(cores[i][j][k].instrMemClient);
  `endif

  // Connect cores to data caches
  function dcacheClient(core) = core.dcacheClient;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
      connectCoresToDCache(map(dcacheClient, cores[i][j]), dcaches[i][j]);

  // Connect data caches to DRAM
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    connectDCachesToOffChipRAM(dcaches[i], rams[i]);

  // Create FPUs
  Vector#(`FPUsPerBoard, FPU) fpus;
  for (Integer i = 0; i < `FPUsPerBoard; i=i+1)
    fpus[i] <- mkFPU;

  // Connect cores to FPUs
  let vecOfCores = concat(concat(cores));
  for (Integer i = 0; i < `FPUsPerBoard; i=i+1) begin
    // Get sub-vector of cores to be connected to FPU i
    Vector#(`CoresPerFPU, Core) cs =
      takeAt(`CoresPerFPU*i, vecOfCores);
    function fpuClient(core) = core.fpuClient;
    // Connect sub-vector of cores to FPU
    connectCoresToFPU(map(fpuClient, cs), fpus[i]);
  end

  // Create DebugLink interface
  function DebugLinkClient getDebugLinkClient(Core core) = core.debugLinkClient;
  DebugLink debugLink <-
    mkDebugLink(localBoardId, map(getDebugLinkClient, vecOfCores));

  // Create idle-detector
  IdleDetector idle <- mkIdleDetector;

  // Connect cores to idle-detector
  function idleClient(core) = core.idleClient;
  connectCoresToIdleDetector(map(idleClient, vecOfCores), idle);

  // Create mailboxes
  Vector#(`MailboxMeshYLen,
    Vector#(`MailboxMeshXLen, Mailbox)) mailboxes =
      Vector::replicate(newVector());
  for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen; x=x+1)
      mailboxes[y][x] <- mkMailbox;

  // Connect cores to mailboxes
  for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen; x=x+1) begin
      // Get sub-vector of cores to be connected to mailbox
      Integer i = y*`MailboxMeshXLen+x;
      Vector#(`CoresPerMailbox, Core) cs =
        takeAt(`CoresPerMailbox*i, vecOfCores);
      function mailboxClient(core) = core.mailboxClient;
      // Connect sub-vector of cores to mailbox
      connectCoresToMailbox(map(mailboxClient, cs), mailboxes[y][x]);
    end

  // Create mesh of mailboxes
  function MailboxNet mailboxNet(Mailbox mbox) = mbox.net;
  ExtNetwork net <- mkMailboxMesh(
                      debugLink.getBoardId(),
                      map(map(mailboxNet), mailboxes),
                      idle);

  // Set board ids
  rule setBoardIds;
    for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
      for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
        for (Integer k = 0; k < `CoresPerDCache; k=k+1)
          cores[i][j][k].setBoardId(debugLink.getBoardId());
  endrule

  // In simulation, display start-up message
  `ifdef SIMULATE
  rule displayStartup;
    let t <- $time;
    if (t == 0) begin
      $display("\nSimulator for board %d started", localBoardId);
    end
  endrule
  `endif

  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(OffChipRAM ram) = ram.extDRAM;
  function Vector#(2, SRAMExtIfc) getSRAMExtIfcs(OffChipRAM ram) = ram.extSRAM;
  interface dramIfcs = map(getDRAMExtIfc, rams);
  interface sramIfcs = concat(map(getSRAMExtIfcs, rams));
  interface jtagIfc  = debugLink.jtagAvalon;
  interface northMac = net.north;
  interface southMac = net.south;
  interface eastMac  = net.east;
  interface westMac  = net.west;
  method Action setBoardId(Bit#(4) id);
    localBoardId <= id;
  endmethod
  `endif
endmodule

endpackage
