// import NOCInterfaces::*;
// import MacSyncroniser::*;

// import Avalon2ServerSingleMaster::*;
// import Avalon2ClientServer::*;
import ClientServer::*;
import GetPut::*;

import ReliableLink::*;
// import AXI4 :: *;
// import AXI4Lite :: *;


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
import Connections  :: *;
import PCIeStream   :: *;
import HostLink   :: *;
import Clocks   :: *;
import Util   :: *;

`ifdef SIMULATE

typedef Empty DE10Ifc;
import "BDPI" function Bit#(32) getBoardId();

`else

interface DE10Ifc;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) northMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) southMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) eastMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) westMac;

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
module mkDE10Top(DE10Ifc ifc);

  Clock defaultClock <- exposeCurrentClock();
  Reset externalReset <- exposeCurrentReset();
  MakeResetIfc hostReset <- mkReset(1, False, defaultClock);
  Reset combinedReset <- mkResetEither(externalReset, hostReset.new_rst);



  `ifdef SIMULATE
  DE10Ifc top <- mkDE10Top_inner();

  `endif

  `ifndef SIMULATE
  DE10Ifc top <- mkDE10Top_inner(reset_by combinedReset);

  (* fire_when_enabled, no_implicit_conditions *)
  rule pcieReset;
    if (top.resetReq) hostReset.assertReset();
  endrule

  interface dramIfcs = top.dramIfcs;
  interface jtagIfc  = top.jtagIfc;
  interface controlBAR  = top.controlBAR;
  interface pcieHostBus  = top.pcieHostBus;
  method Bool resetReq = !top.resetReq;
  method Action setBoardId(Bit#(4) id) = top.setBoardId(id);
  method Action setTemperature(Bit#(8) temp) = top.setTemperature(temp);
  interface northMac = top.north;
  interface southMac = top.south;
  interface eastMac  = top.east;
  interface westMac  = top.west;

  `endif

endmodule


module mkDE10Top_inner(DE10Ifc ifc);

  // Board Id
  `ifdef SIMULATE
  Bit#(4) localBoardId = truncate(getBoardId());
  `else
  Wire#(Bit#(4)) localBoardId <- mkDWire(?);
  `endif

  // Temperature register
  Reg#(Bit#(8)) temperature <- mkReg(128);

  // Create off-chip RAMs
  Vector#(`DRAMsPerBoard, OffChipRAMStratix10) rams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1) begin
    let ram = mkOffChipRAMStratix10(fromInteger(i));
    rams[i] <- ram;
  end


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
  PCIeStream pcie <- mkPCIeStream();

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

  // HostLink
  HostLinkPCIeAdaptorIfc hostlink <- mkHostLink();
  connectUsing(mkUGShiftQueue1(QueueOptFmax), hostlink.streamToHost, pcie.streamIn);
  connectDirect(pcie.streamOut, hostlink.streamFromHost);

  // Create network-on-chip
  function MailboxNet mailboxNet(Mailbox mbox) = mbox.net;
  NoC noc <- mkNoCDE10(
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
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1) begin
    let dcache = dcaches[i];
    let dram = rams[i];
    connectClientsToOffChipRAMs10(dcache, dram);
  end


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


  // In simulation, display start-up message
  // `ifdef SIMULATE

  Reg#(Bool) started <- mkReg(False);
  rule displayStartup;
    let t <- $time;
    if (!started) begin
      $display("\nSimulator for board %d started at time ", localBoardId, t);
      // $dumpvars();
      temperature <= 0;
      started <= True;
    end
  endrule
  // `endif

  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(OffChipRAMStratix10 ram) = ram.extDRAM;
  interface dramIfcs = map(getDRAMExtIfc, rams);
  interface jtagIfc  = debugLink.jtagAvalon;

  interface northMac = noc.north;
  interface southMac = noc.south;
  interface eastMac  = noc.east;
  interface westMac  = noc.west;
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
