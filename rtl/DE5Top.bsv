// SPDX-License-Identifier: BSD-2-Clause
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
// import NarrowSRAM   :: *;
import OffChipRAM   :: *;
import IdleDetector :: *;
import Connections  :: *;
import PCIeStream   :: *;
import HostLink     :: *;
import Clocks       :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

import "BDPI" function Bit#(32) getBoardId();

`else

interface DE5Top;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  //interface Vector#(`SRAMsPerBoard, SRAMExtIfc) sramIfcs;
  //interface Vector#(`NumNorthSouthLinks, AvalonMac) northMac;
  //interface Vector#(`NumNorthSouthLinks, AvalonMac) southMac;
  //interface Vector#(`NumEastWestLinks, AvalonMac) eastMac;
  //interface Vector#(`NumEastWestLinks, AvalonMac) westMac;
  interface JtagUartAvalon jtagIfc;
  (* always_ready, always_enabled *)
  method Action setBoardId(Bit#(4) id);
  (* always_ready, always_enabled *)
  method Action setTemperature(Bit#(8) temp);

  // Interface to the PCIe BAR
  interface PCIeBAR controlBAR;
  // Interface to host PCIe bus
  // (Use for DMA to/from host memory)
  interface PCIeHostBus pcieHostBus;
  // Reset request
  (* always_enabled, always_ready *)
  method Bool resetReq;

endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

// mkDE10Top wrapper ensures the entire design is reset correctly when requested by the host
module de5Top (DE5Top);

  Clock defaultClock <- exposeCurrentClock();
  Reset externalReset <- exposeCurrentReset();
  MakeResetIfc bufferedResetMod <- mkReset(1, False, defaultClock);
  Reset bufferedReset = bufferedResetMod.new_rst;

  Reg#(Bool) rst_0 <- mkReg(True, reset_by externalReset);
  Reg#(Bool) rst_1 <- mkReg(False, reset_by externalReset);
  Reg#(Bool) rst_2 <- mkReg(False, reset_by externalReset);

  rule deassert_reset (rst_2);
    rst_0 <= False;
    rst_1 <= False;
    rst_2 <= False;
  endrule

  rule assert_internal_reset (rst_2);
    bufferedResetMod.assertReset();
  endrule

  rule propagate_reset;
    rst_1 <= rst_0;
    rst_2 <= rst_1;
  endrule



  // MakeResetIfc hostReset <- mkReset(1, False, defaultClock);
  // Reset combinedReset <- mkResetEither(externalReset, hostReset.new_rst);

  DE5Top top <- de5Top_inner(reset_by bufferedReset);

  `ifndef SIMULATE
  // (* fire_when_enabled, no_implicit_conditions *)
  // rule pcieReset;
  //   if (top.resetReq) hostReset.assertReset();
  // endrule

  interface dramIfcs = top.dramIfcs;
  interface jtagIfc  = top.jtagIfc;
  interface controlBAR  = top.controlBAR;
  interface pcieHostBus  = top.pcieHostBus;
  method Bool resetReq = !top.resetReq;
  method Action setBoardId(Bit#(4) id) = top.setBoardId(id);
  method Action setTemperature(Bit#(8) temp) = top.setTemperature(temp);
  `endif

endmodule


module de5Top_inner (DE5Top);
  // Board Id
  `ifdef SIMULATE
  Bit#(4) localBoardId = truncate(getBoardId());
  `else
  Wire#(Bit#(4)) localBoardId <- mkDWire(?);
  `endif

  // Temperature register
  Reg#(Bit#(8)) temperature <- mkReg(128);

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
    mkDebugLink(localBoardId, temperature,
      map(getDebugLinkClient, vecOfCores));

  // Create PCIeStream instance
  PCIeStream pcie <- mkPCIeStream;

  // Create idle-detector
  IdleDetector idle <- mkIdleDetector;

  // Create mailboxes
  Vector#(`MailboxMeshYLen,
    Vector#(`MailboxMeshXLen, Mailbox)) mailboxes =
      Vector::replicate(newVector());
  for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen; x=x+1)
      mailboxes[y][x] <- mkMailboxAcc(debugLink.getBoardId(), x, y);

  // Initialise mailbox send slots
  rule initSendSlots;
    for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
      for (Integer x = 0; x < `MailboxMeshXLen; x=x+1)
        mailboxes[y][x].initSendSlots(debugLink.useExtraSendSlot);
  endrule

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

  HostLinkPCIeAdaptorIfc hostlink <- mkHostLink();
  connectUsing(mkUGShiftQueue1(QueueOptFmax), hostlink.streamToHost, pcie.streamIn);
  connectDirect(pcie.streamOut, hostlink.streamFromHost);

  function MailboxNet mailboxNet(Mailbox mbox) = mbox.net;
  // Create network-on-chip
  NoC noc <- mkNoC(
    debugLink.getBoardId(),
    debugLink.linkEnable,
    map(map(mailboxNet), mailboxes),
    hostlink.mbox,
    idle);

  // Connect cores and ProgRouter fetchers to idle-detector
  function idleClient(core) = core.idleClient;
  connectClientsToIdleDetector(
    map(idleClient, vecOfCores), noc.activities, idle);

  // Connections to off-chip RAMs
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    connectClientsToOffChipRAM(dcaches[i],
      noc.dramReqs[i], noc.dramResps[i], rams[i]);

  // Connects ProgRouter performance counters to cores
  connectProgRouterPerfCountersToCores(noc.progRouterPerfCounters,
    concat(concat(cores)));

  // Set board ids
  rule setBoardIds;
    for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
      for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
        for (Integer k = 0; k < `CoresPerDCache; k=k+1)
          cores[i][j][k].setBoardId(debugLink.getBoardId());
  endrule

  // NoC rim unused
  `ifndef SIMULATE
  mkNullAvalonMac(noc.north[0]);
  mkNullAvalonMac(noc.south[0]);
  mkNullAvalonMac(noc.east[0]);
  mkNullAvalonMac(noc.west[0]);
  `endif

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
  //function Vector#(2, SRAMExtIfc) getSRAMExtIfcs(OffChipRAM ram) = ram.extSRAM;
  interface dramIfcs = map(getDRAMExtIfc, rams);
  //interface sramIfcs = concat(map(getSRAMExtIfcs, rams));
  interface jtagIfc  = debugLink.jtagAvalon;
  //interface northMac = noc.north;
  //interface southMac = noc.south;
  //interface eastMac  = noc.east;
  //interface westMac  = noc.west;
  method Action setBoardId(Bit#(4) id);
    localBoardId <= id;
  endmethod
  method Action setTemperature(Bit#(8) temp);
    temperature <= temp;
  endmethod

  interface controlBAR  = pcie.external.controlBAR;
  interface pcieHostBus  = pcie.external.hostBus;
  method Bool resetReq = pcie.external.resetReq;
  `endif
endmodule

endpackage
