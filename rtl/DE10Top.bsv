import NOCInterfaces::*;
import MacSyncroniser::*;

import Avalon2ServerSingleMaster::*;
import Avalon2ClientServer::*;
import ClientServer::*;
import GetPut::*;

import ReliableLink::*;
import AXI4 :: *;
import AXI4Lite :: *;


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

`ifdef SIMULATE

typedef Empty DE10Ifc;
import "BDPI" function Bit#(32) getBoardId();

`else

interface DE10Ifc;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;

  // Interface to the PCIe BAR
  interface PCIeBAR controlBAR;
  // Interface to host PCIe bus
  // (Use for DMA to/from host memory)
  interface PCIeHostBus pcieHostBus;

  interface AvalonSlaveSingleMasterIfc#(4) tester; // AvalonSlave physical interface
  interface JtagUartAvalon jtagIfc;

  (* always_ready, always_enabled *)
  method Action setBoardId(Bit#(4) id);
  (* always_ready, always_enabled *)
  method Bool resetReq; // from PCIeStream; but should reset the entire design.

  interface MacDataIfc macA;
  interface MacDataIfc macB;
endinterface


`endif

interface LinkTestIfc;
  interface AvalonSlaveSingleMasterIfc#(4) av_peripheral; // AvalonSlave physical interface
  interface AvalonMacIfc toMac;
endinterface


module mkID(LinkTestIfc);

  Bit#(512) testval = 512'hCAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE;

  AvalonSlave2ClientSingleMasterIfc#(4) avalon <- mkAvalonSlave2ClientSingleMaster;

  rule handle_avalon_requests;
    let req <- avalon.client.request.get();
    ReturnedDataT rtn = tagged Invalid;
    case(tuple2(req.addr, req.rw))
      tuple2(0, MemRead) : rtn = tagged Valid 32'hDE100001; // DE10 top level v1
    endcase
    avalon.client.response.put(rtn);
  endrule

  interface av_peripheral = avalon.avs;
  interface AvalonMacIfc toMac;

    method ActionValue#(AvalonSTFlit) send() if (False);
      let testflit = AvalonSTFlit{data: testval,
                                    startofPacket:False,
                                    endofPacket:False,
                                    empty:0};
      return testflit;
    endmethod

    method Action recv(AvalonSTFlit beat);
    endmethod

  endinterface
endmodule

// module mkFakeRAM#(RAMId base) (OffChipRAMStratix10);
//
// endmodule
//
module mkDE10Top(Clock rx_390_A, Clock tx_390_A,
                  Reset rx_rst_A, Reset tx_rst_A,
                  Clock rx_390_B, Clock tx_390_B,
                  Reset rx_rst_B, Reset tx_rst_B,
                  Clock mgmt, Reset mgmt_reset,
                  DE10Ifc ifc);

  Clock default_clock <- exposeCurrentClock();
  Reset default_reset <- exposeCurrentReset();
  // LinkTestIfc idmod <- mkID();

  // MacSyncIfc syncA <- mkMacSyncroniser(default_clock, rx_390_A, tx_390_A, default_reset, rx_rst_A, tx_rst_A);
  // MacSyncIfc syncB <- mkMacSyncroniser(default_clock, rx_390_B, tx_390_B, default_reset, rx_rst_B, tx_rst_B);
  //
  // rule out;
  //   let flit <- idmod.toMac.send();
  //   syncA.nocToSync.recv(flit);
  // endrule
  //
  // rule in;
  //   let flit <- syncA.nocToSync.send();
  //   idmod.toMac.recv(flit);
  // endrule

  `ifdef SIMULATE
  Bit#(4) localBoardId = truncate(getBoardId());
  `else
  Wire#(Bit#(4)) localBoardId <- mkDWire(?);
  `endif

  // Temperature register
  Reg#(Bit#(8)) temperature <- mkReg(128);

  // Create off-chip RAMs
  Vector#(`DRAMsPerBoard, OffChipRAMStratix10) rams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    rams[i] <- mkOffChipRAMStratix10(fromInteger(i*3));
    // rams[i] <- mkFakeRAM(fromInteger(i*3));

  // Create data caches
  Vector#(`DRAMsPerBoard,
    Vector#(`DCachesPerDRAM, DCache)) dcaches = newVector;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
      dcaches[i][j] <- mkDCache(fromInteger(j)); // (* synthesize *)

  // Create cores
  Integer coreCount = 0;
  Vector#(`DRAMsPerBoard,
    Vector#(`DCachesPerDRAM,
      Vector#(`CoresPerDCache, Core))) cores = newVector;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    for (Integer j = 0; j < `DCachesPerDRAM; j=j+1)
      for (Integer k = 0; k < `CoresPerDCache; k=k+1) begin
        cores[i][j][k] <- mkCore(fromInteger(coreCount)); // (* synthesize *)
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
    fpus[i] <- mkFPU; // (* synthesize *)

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
    mkDebugLink(mgmt, mgmt_reset,
      localBoardId, temperature,
      map(getDebugLinkClient, vecOfCores));

  // Create idle-detector
  IdleDetector idle <- mkIdleDetector;

  // Create mailboxes
  Vector#(`MailboxMeshYLen,
    Vector#(`MailboxMeshXLen, Mailbox)) mailboxes =
      Vector::replicate(newVector());
  for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen; x=x+1)
      mailboxes[y][x] <- mkMailboxAcc(debugLink.getBoardId(), x, y); // (* synthesize *)

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

  // HostLink

  // Create PCIeStream instance
  PCIeStream pcie <- mkPCIeStream;

  connectUsing(mkUGShiftQueue1(QueueOptFmax), hostlink.streamToHost, pcie.streamIn);
  connectDirect(pcie.streamOut, hostlink.streamFromHost);

  // In simulation, display start-up message
  `ifdef SIMULATE
  rule displayStartup;
    let t <- $time;
    if (t == 0) begin
      $display("\nSimulator for board %d started", localBoardId);
      $dumpvars();
    end
  endrule
  `endif

  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(OffChipRAMStratix10 ram) = ram.extDRAM;
  interface dramIfcs = map(getDRAMExtIfc, rams);
  interface jtagIfc  = debugLink.jtagAvalon;
  interface controlBAR  = pcie.external.controlBAR;
  interface pcieHostBus  = pcie.external.hostBus;
  method Bool resetReq = pcie.external.resetReq;
  // interface northMac = noc.north;
  // interface southMac = noc.south;
  // interface eastMac  = noc.east;
  // interface westMac  = noc.west;
  method Action setBoardId(Bit#(4) id);
    localBoardId <= id;
  endmethod
  // method Action setTemperature(Bit#(8) temp);
  //   temperature <= temp;
  // endmethod
  `endif


  // interface tester = idmod.av_peripheral;

  // interface MacDataIfc macA = syncA.syncToMac;
  // interface MacDataIfc macB = syncB.syncToMac;

endmodule
