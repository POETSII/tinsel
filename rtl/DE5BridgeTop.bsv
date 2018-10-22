// This module implements a "bridge board", i.e. an FPGA board that acts
// as a proxy between a host PC (connected over PCIe) and an FPGA
// mesh (connected via a 10G link).
//
// The basic idea is that messages received from the host PC over PCIe
// are inserted onto the mesh's 10G network.  Likewise, messages
// from the mesh's network are sent to the host PC over PCIe.
//
// The format of the data stream in the PC->FPGA direction is:
//
//   1. DA: Destination address (4 bytes)
//   2. NM: Number of messages that follow minus one (4 bytes)
//   3. FM: Number of flit payloads per message minus one (1 byte)
//   4. Padding (7 bytes)
//   5. (NM+1)*(FM+1) flit payloads ((NM+1)*(FM+1)*BytesPerFlit bytes)
//   6. Goto step 1
//
// The format of the data stream in the FPGA->PC direction is simply
// raw flit payloads.
//
// This module assumes that BytesPerFlit is 16.  This restriction
// should be removed in future, if necessary.

package DE5BridgeTop;

// ============================================================================
// Imports
// ============================================================================

import Globals      :: *;
import DRAM         :: *;
import Interface    :: *;
import Queue        :: *;
import Vector       :: *;
import Mailbox      :: *;
import Network      :: *;
import Mac          :: *;
import PCIeStream   :: *;
import Socket       :: *;
import ConfigReg    :: *;
import JtagUart     :: *;
import DebugLink    :: *;
import IdleDetector :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5BridgeTop;

`else

interface DE5BridgeTop;
  // Interface to the PCIe BAR
  interface PCIeBAR controlBAR;
  // Interface to host PCIe bus
  // (Use for DMA to/from host memory)
  interface PCIeHostBus pcieHostBus;
  // Interface to host over a JTAG UART
  interface JtagUartAvalon jtagAvalon;
  // Connection to FPGA cluster
  interface Vector#(2, AvalonMac) mac;
  // Reset request
  (* always_enabled, always_ready *)
  method Bool resetReq;
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module de5BridgeTop (DE5BridgeTop);

  // Ports
  OutPort#(Bit#(128)) toPCIe <- mkOutPort;
  InPort#(Bit#(128)) fromPCIe <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Flit) toDetector <- mkOutPort;
  InPort#(Flit) fromDetector <- mkInPort;

  Vector#(2, OutPort#(Flit)) toLink <- replicateM(mkOutPort);

  // Create JTAG UART instance
  JtagUart uart <- mkJtagUart;

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Create off-board links
  Vector#(2, BoardLink) link;
  for (Integer i = 0; i < 2; i=i+1)
    link[i] <- mkBoardLink(northSocket[i]);

  // Connect ports to off-board links
  for (Integer i = 0; i < 2; i=i+1)
    connectUsing(mkUGQueue, toLink[i].out, link[i].flitIn);

  // Create PCIeStream instance
  PCIeStream pcie <- mkPCIeStream;

  // Connect ports to PCIeStream
  connectUsing(mkUGQueue, toPCIe.out, pcie.streamIn);
  connectDirect(pcie.streamOut, fromPCIe.in);

  // Create idle detector master
  IdleDetectMaster detector <- mkIdleDetectMaster;

  // Connect ports to idle detect master
  connectUsing(mkUGQueue, toDetector.out, detector.flitIn);
  connectUsing(mkUGQueue, detector.flitOut, fromDetector.in);

  // Has board been enumerated over JTAG yet?
  Reg#(Bool) enumerated <- mkConfigReg(False);

  // Connect PCIe stream and 10G links
  // ---------------------------------

  Reg#(Bit#(32)) fromPCIeDA    <- mkConfigRegU;
  Reg#(Bit#(32)) fromPCIeNM    <- mkConfigRegU;
  Reg#(Bit#(8))  fromPCIeFM    <- mkConfigRegU;
  Reg#(Bit#(1))  toLinkState   <- mkConfigReg(0);

  Reg#(Bit#(32)) messageCount  <- mkConfigReg(0);
  Reg#(Bit#(8))  flitCount     <- mkConfigReg(0);

  InPort#(Flit)   inPort       <- mkInPort;

  rule toLink0 (toLinkState == 0);
    if (fromDetector.canGet) begin
      if (toLink[0].canPut) begin
        toLink[0].put(fromDetector.value);
        fromDetector.get;
      end
    end else begin
      if (fromPCIe.canGet) begin
        Bit#(128) data = fromPCIe.value;
        fromPCIeDA <= data[31:0];
        fromPCIeNM <= data[63:32];
        fromPCIeFM <= data[95:88];
        toLinkState <= 1;
        fromPCIe.get;
      end
    end
  endrule

  rule toLink1 (toLinkState == 1);
    if (flitCount == 0 && detector.disableHostMsgs) begin
      // Hold off sending
    end else begin
      if (fromPCIe.canGet && toLink[0].canPut) begin
        Flit flit;
        flit.dest = unpack(truncate(fromPCIeDA));
        flit.payload = fromPCIe.value;
        flit.notFinalFlit = True;
        flit.isIdleToken = False;
        if (flitCount == fromPCIeFM) begin
          flitCount <= 0;
          flit.notFinalFlit = False;
          if (messageCount == fromPCIeNM) begin
            messageCount <= 0;
            toLinkState <= 0;
          end else
            messageCount <= messageCount+1;
        end else
          flitCount <= flitCount+1;
        toLink[0].put(flit);
        fromPCIe.get;
        if (flitCount == 0) detector.incCount;
      end
    end
  endrule

  // Connect 10G link to PCIe stream and idle detector
  rule fromLinkRule (inPort.canGet);
    Flit flit = inPort.value;
    if (flit.isIdleToken) begin
      if (toDetector.canPut) begin
        toDetector.put(flit);
        inPort.get;
      end
    end else begin
      if (toPCIe.canPut) begin
        toPCIe.put(flit.payload);
        inPort.get;
        if (!flit.notFinalFlit) detector.decCount;
      end
    end
  endrule

  // Join input links to inPort
  Out#(Flit) flits <- mkFlitMerger(link[0].flitOut, link[1].flitOut);
  connectUsing(mkUGQueue, flits, inPort.in);

  // Enable idle detector
  rule enabler;
    detector.enabled(enumerated);
  endrule

  // In simulation, display start-up message
  `ifdef SIMULATE
  rule displayStartup;
    let t <- $time;
    if (t == 0) begin
      $display("\nSimulator for bridge board started");
    end
  endrule
  `endif

  // JTAG UART Handler
  // -----------------

  // Respond to the Query command with a zero byte.  The host uses the
  // query command to distinguish this bridge board from a worker board,
  // which returns non-zero.

  Reg#(Bit#(2)) uartState <- mkConfigReg(0);

  rule uartReceive (fromJtag.canGet && uartState == 0);
    fromJtag.get;
    enumerated <= True;
    uartState <= 1;
  endrule

  rule uartRespond (toJtag.canPut && uartState != 0);
    toJtag.put(0);
    uartState <= uartState == 1 ? 2 : 0;
  endrule

  `ifndef SIMULATE
  function AvalonMac getMac(BoardLink lnk) = lnk.avalonMac;

  interface controlBAR = pcie.external.controlBAR;
  interface pcieHostBus = pcie.external.hostBus;
  method Bool resetReq = pcie.external.resetReq;
  interface mac = map(getMac, link);
  interface jtagAvalon = uart.jtagAvalon;
  `endif

endmodule

endpackage
