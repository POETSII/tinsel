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
  interface AvalonMac mac;
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
  OutPort#(Flit) toLink <- mkOutPort;
  InPort#(Flit) fromLink <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Flit) toDetector <- mkOutPort;
  InPort#(Flit) fromDetector <- mkInPort;

  // Create JTAG UART instance
  JtagUart uart <- mkJtagUart;

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Create off-board link
  BoardLink link <- mkBoardLink(northSocket);

  // Connect ports to off-board link
  connectUsing(mkUGQueue, toLink.out, link.flitIn);
  connectUsing(mkUGQueue, link.flitOut, fromLink.in);

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

  // Is the idle detected enabled
  Reg#(Bool) idleDetectedEnabled <- mkConfigReg(False);

  // Connect PCIe stream and 10G link
  // --------------------------------

  Reg#(Bit#(32)) fromPCIeDA    <- mkConfigRegU;
  Reg#(Bit#(32)) fromPCIeNM    <- mkConfigRegU;
  Reg#(Bit#(8))  fromPCIeFM    <- mkConfigRegU;
  Reg#(Bit#(1))  toLinkState   <- mkConfigReg(0);

  Reg#(Bit#(32)) messageCount  <- mkConfigReg(0);
  Reg#(Bit#(8))  flitCount     <- mkConfigReg(0);

  rule toLink0 (toLinkState == 0);
    if (fromDetector.canGet) begin
      if (toLink.canPut) begin
        toLink.put(fromDetector.value);
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
      if (fromPCIe.canGet && toLink.canPut) begin
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
        toLink.put(flit);
        fromPCIe.get;
        if (flitCount == 0) detector.incCount;
      end
    end
  endrule

  // Connect 10G link to PCIe stream and idle detector
  rule fromLinkRule (fromLink.canGet);
    Flit flit = fromLink.value;
    if (flit.isIdleToken) begin
      if (toDetector.canPut) begin
        toDetector.put(flit);
        fromLink.get;
      end
    end else begin
      if (toPCIe.canPut) begin
        toPCIe.put(flit.payload);
        fromLink.get;
        if (!flit.notFinalFlit) detector.decCount;
      end
    end
  endrule

  // Dimensions of the board mesh (received over the UART)
  Reg#(Bit#(`MeshXBits)) meshXLen <- mkConfigReg(0);
  Reg#(Bit#(`MeshYBits)) meshYLen <- mkConfigReg(0);
  Reg#(Bit#(TAdd#(`MeshXBits, `MeshYBits))) meshBoards <- mkConfigReg(0);

  // Is idle-detection currently enabled
  Reg#(Bool) idleDetectorEnabled <- mkConfigReg(False);

  // Pass idle-detector options to idle-detector
  rule enabler;
    detector.enabled(idleDetectorEnabled, meshXLen, meshYLen, meshBoards);
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

  // On the 2nd query command, enable the idle detector.
  // This is to allow the ids of all boards to be set before
  // enabling the idle detector.
  //
  // The parameter byte of the second query command contains
  // the dimensions of the board mesh: Y = byte[7:4], X = byte[3:0].

  Reg#(Bit#(8)) boardIdWithinBox <- mkConfigReg(0);
  Reg#(Bit#(2)) uartState <- mkConfigReg(0);

  rule uartReceive0 (fromJtag.canGet && uartState == 0);
    fromJtag.get;
    uartState <= 1;
  endrule

  rule uartReceive1 (fromJtag.canGet && uartState == 1);
    fromJtag.get;
    enumerated <= True;
    if (enumerated) begin
      idleDetectorEnabled <= True;
      Bit#(`MeshXBits) xLen = truncate(fromJtag.value[3:0]);
      Bit#(`MeshYBits) yLen = truncate(fromJtag.value[7:4]);
      meshYLen <= yLen;
      meshXLen <= xLen;
      meshBoards <= zeroExtend(xLen) * zeroExtend(yLen);
    end
    uartState <= 2;
  endrule

  rule uartRespond0 (toJtag.canPut && uartState == 2);
    toJtag.put(0);
    uartState <= 3;
  endrule

  rule uartRespond1 (toJtag.canPut && uartState == 3);
    toJtag.put(0);
    uartState <= 0;
  endrule

  `ifndef SIMULATE
  interface controlBAR = pcie.external.controlBAR;
  interface pcieHostBus = pcie.external.hostBus;
  method Bool resetReq = pcie.external.resetReq;
  interface mac = link.avalonMac;
  interface jtagAvalon = uart.jtagAvalon;
  `endif

endmodule

endpackage
