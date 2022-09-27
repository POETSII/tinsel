// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package DebugLink;

// DebugLink connects all the cores on a single FPGA board using a
// unidirectional serial bus.  It supports commands being sent to any
// core from the host, over JTAG UART, and to the host from any core.
// The aim is to support system initialisation, debugging, and
// profiling.  At present only a small number of commands are
// available but this may grow in future.

// Commands sent from the host PC to DebugLink typically consist of a
// few bytes over the JTAG UART.
//
//   QueryIn: tag (1 byte), board id (1 byte), board offset (1 byte), config (1 byte)
//   -------------------------------------------------------------
//
//   Sets the X offset (offset[3:0]) and the Y offset (offset[7:4])
//   of the board id (to support multiple boxes).
//   also set (DE10) the local board ID out of the 8 in the server (lower 4 bits)
//   Disable the specified inter-FPGA links:
//     * config[0]: disable links on north side of box
//     * config[1]: disable links on south side of box
//     * config[2]: disable links on east side of box
//     * config[3]: disable links on west side of box
//   Enable extra send slot:
//     * config[4]: reserve extra send slot
//   Responds with a QueryOut (see below).
//
//   SetDest: tag (1 byte), thread id (1 byte), core id (1 byte)
//   -----------------------------------------------------------
//
//   Sets the destination core id for all subsequent commands (until
//   the next SetDest command). The MSB of the core id is the
//   broadcast bit, in which case the command is sent to the given
//   thread id on every core.
//
//   StdIn: tag (1 byte), payload (1 byte)
//   -------------------------------------
//
//   The 8-bit payload gets sent to the destination core(s).
//
//   TempIn: tag (1 byte)
//   --------------------
//
//   Request the FPGA temperature.
//
// Commands sent from DebugLink to the host PC over the JTAG UART are
// as follows.
//
//   QueryOut: tag (1 byte), payload (1 byte), FPGA ID (8 bytes)
//   ----------------------------------------
//
//   Response to QueryIn command. Payload contains 1 + Board Id.  By
//   returning non-zero, this response can be used by the host to
//   detect a tinsel board (distingushing it, for example, from a host
//   board which returns a 0 payload) and also to discover the board id.
//
//   StdOut: tag (1 byte), thread id (1 byte),
//           core id (1 byte), payload (1 byte)
//   ------------------------------------------
//
//   A byte sent by a thread is forwarded to the host.
//
//   TempOut: tag (1 byte), payload (1 byte)
//   ---------------------------------------
//
//   Actual temperature in celsius is payload - 128.
//
//   Overheat: tag (1 byte)
//   ----------------------
//
//   FPGA is overheating.
//

// =============================================================================
// Imports
// =============================================================================

import JtagUart  :: *;
import Interface :: *;
import Vector    :: *;
import Queue     :: *;
import Util      :: *;
import Globals   :: *;
import ConfigReg :: *;
import ChipID    :: *;
import GetPut    :: *;

// =============================================================================
// DebugLink commands
// =============================================================================

typedef Bit#(3) DebugLinkCmd;
DebugLinkCmd cmdQueryIn  = 0;
DebugLinkCmd cmdQueryOut = 0;
DebugLinkCmd cmdSetDest  = 1;
DebugLinkCmd cmdStdIn    = 2;
DebugLinkCmd cmdStdOut   = 2;
DebugLinkCmd cmdTempIn   = 4;
DebugLinkCmd cmdTempOut  = 4;
DebugLinkCmd cmdOverheat = 5;

// =============================================================================
// Temperature parameters
// =============================================================================

// If the FPGA temperature rises above this threshold we send an
// overheat message over debug link.  To convert this temperature to
// Celsuis, subtract 128.
`define TemperatureThreshold 213

// Average of the temperature over multiple samples
`define LogTemperatureSamples 10

// =============================================================================
// Types
// =============================================================================

// A DebugLink flit (that travels along the DebugLink bus connecting the cores)
typedef struct {
  // The source or destination core
  Bit#(`LogCoresPerBoard) coreId;
  // Is every core a destination?
  Bool isBroadcast;
  // The source or destination thread
  Bit#(`LogThreadsPerCore) threadId;
  // Payload
  Bit#(8) payload;
  // Command
  DebugLinkCmd cmd;
} DebugLinkFlit deriving (Bits, FShow);

// =============================================================================
// DebugLink router
// =============================================================================

// The cores are connected to a serial chain of routers (a bus)
interface DebugLinkRouter;
  interface In#(DebugLinkFlit) busIn;
  interface Out#(DebugLinkFlit) busOut;
  interface In#(DebugLinkFlit) fromCore;
  interface Out#(DebugLinkFlit) toCore;
endinterface

(* synthesize *)
module mkDebugLinkRouter#(Bit#(`LogCoresPerBoard) myId) (DebugLinkRouter);

  // Ports
  InPort#(DebugLinkFlit)  busInPort    <- mkInPort;
  OutPort#(DebugLinkFlit) busOutPort   <- mkOutPort;
  InPort#(DebugLinkFlit)  fromCorePort <- mkInPort;
  OutPort#(DebugLinkFlit) toCorePort   <- mkOutPort;

  // Is the flit a broadcast?
  Bool broadcast = busInPort.value.isBroadcast;

  // Is the flit from the bus for me?
  Bool busInForMe = busInPort.value.coreId == myId;

  // Route from the bus to the core?
  Bool routeBusToCore = busInPort.canGet && toCorePort.canPut &&
                          busInForMe && !broadcast;

  // Route from the bus to the bus?
  Bool routeBusToBus = busInPort.canGet && busOutPort.canPut &&
                         !busInForMe && !broadcast;

  // Route from the bus to the core and the bus?
  Bool routeBusToCoreAndBus = busInPort.canGet &&
                                busOutPort.canPut &&
                                  toCorePort.canPut && 
                                    broadcast;

  // Trigger busInPort.get
  PulseWire busInPortGet <- mkPulseWireOR;

  // Route flit from bus to core
  rule busToCore (routeBusToCore || routeBusToCoreAndBus);
    busInPortGet.send;
    toCorePort.put(busInPort.value);
  endrule

  // This fact used to be inferred, but is now needed for the
  // open-source version of BSC
  (* mutually_exclusive = "busToBus, coreToBus" *)

  // Route flit from bus to bus
  rule busToBus (routeBusToBus || routeBusToCoreAndBus);
    busInPortGet.send;
    busOutPort.put(busInPort.value);
  endrule

  // Route flit from core to bus
  rule coreToBus (busOutPort.canPut && fromCorePort.canGet &&
                   !(routeBusToBus || routeBusToCoreAndBus));
    fromCorePort.get;
    busOutPort.put(fromCorePort.value);
  endrule

  // Consume busInPort
  rule consume (busInPortGet);
    busInPort.get;
  endrule

  // Interface
  interface In  busIn    = busInPort.in;
  interface Out busOut   = busOutPort.out;
  interface In  fromCore = fromCorePort.in;
  interface Out toCore   = toCorePort.out;

endmodule

// =============================================================================
// Main DebugLink module
// =============================================================================

// Each core implements the following interface
interface DebugLinkClient;
  interface In#(DebugLinkFlit) fromDebugLink;
  interface Out#(DebugLinkFlit) toDebugLink;
endinterface

interface DebugLink;
  `ifndef SIMULATE
  interface JtagUartAvalon jtagAvalon;
  `endif
  // Get board id via DebugLink
  (* always_ready, always_enabled *)
  method BoardId getBoardId();
  (* always_ready, always_enabled *)
  method Bit#(4) getBoardIdWithinBox();
  // Config option: disable each inter-FPGA link via DebugLink
  // (Allows sanboxing of boxes or groups of boxes)
  (* always_ready, always_enabled *)
  method Vector#(4, Bool) linkEnable;
  // Config option: reserve extra send slot per thread in mailbox
  (* always_ready, always_enabled *)
  method Option#(Bool) useExtraSendSlot;
endinterface

module mkDebugLink#(
    Bit#(8) temperature,
    Vector#(n, DebugLinkClient) cores,
    Maybe#(Bit#(64)) chipIDReg) (DebugLink);



  // The board id combines the Y offset, received via DebugLink,
  // with the box-local board id (set via DIP switches) to give
  // an overall board id.
  Reg#(BoardId) boardId <- mkReg(unpack(0));
  // The board offset (in a multi-box setup) received via DebugLink
  Reg#(Bit#(8)) boardOffset <- mkConfigReg(0);
  // Get board id via DebugLink
  Reg#(Bit#(4)) boardIdWithinBox <- mkConfigReg(0);

  // An enable line for each inter-FPGA link on the board
  // (Initially, all disabled)
  Reg#(Vector#(4, Bool)) linkEnableReg <- mkConfigReg(replicate(False));

  // Config option: reserve extra send slot in mailbox?
  // Use a chain of registers to aid propagation on chip
  Vector#(3, Reg#(Option#(Bool))) useExtraSendSlotReg <-
     replicateM(mkConfigReg(Option {valid : False, value: False}));

  // Ports
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;
  InPort#(DebugLinkFlit) fromBusPort <- mkInPort;
  OutPort#(DebugLinkFlit) toBusPort <- mkOutPort;

  // Create JTAG UART instance
  JtagUart uart <- mkJtagUart;

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Create serial bus
  Vector#(n, DebugLinkRouter) bus;
  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    bus[i] <- mkDebugLinkRouter(fromInteger(i));
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   cores[i].toDebugLink, bus[i].fromCore);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   bus[i].toCore, cores[i].fromDebugLink);
  end
  for (Integer i = 0; i < valueOf(n)-1; i=i+1)
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   bus[i].busOut, bus[i+1].busIn);

  // Connect bus to DebugLink
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 bus[valueOf(n)-1].busOut, fromBusPort.in);

  // Connect DebugLink to bus
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toBusPort.out, bus[0].busIn);



  // Monitor temperature
  // -------------------

  // Check temperature to avoid overheating?
  Reg#(Bool) checkTemperature <- mkConfigReg(False);

  // Should we send an emergency overheat message?
  Reg#(Bool) overheatDetected <- mkConfigReg(False);

  // Have we sent an emergency overheat message?
  Reg#(Bool) overheatMsgSent <- mkConfigReg(False);

  // Sum of temperature over many samples
  Reg#(Bit#(TAdd#(`LogTemperatureSamples, 8))) tempSum <- mkConfigReg(0);

  // Number of samples taken
  Reg#(Bit#(`LogTemperatureSamples)) tempSamples <- mkConfigReg(0);

  rule monitorTemperature (checkTemperature && !overheatDetected);
    tempSamples <= tempSamples + 1;
    if (allHigh(tempSamples)) begin
      if ((tempSum >> `LogTemperatureSamples) > `TemperatureThreshold)
        overheatDetected <= True;
      else
        tempSum <= zeroExtend(temperature);
    end else
      tempSum <= tempSum + zeroExtend(temperature);
  endrule

  // Receive commands over UART
  // --------------------------

  // State
  Reg#(Bit#(3)) recvState <- mkConfigReg(0);

  // Destination core id
  Reg#(Bit#(8)) recvDestCore <- mkConfigReg(0);

  // Destination thread id
  Reg#(Bit#(8)) recvDestThread <- mkConfigReg(0);

  // Command
  Reg#(DebugLinkCmd) recvCmd <- mkConfigReg(0);

  // Respond to command?
  Reg#(Bool) respondFlag <- mkConfigReg(False);
  Reg#(DebugLinkCmd) respondCmd <- mkConfigRegU;

  rule uartRecv (fromJtag.canGet && toBusPort.canPut && !respondFlag);
    fromJtag.get;
    if (recvState == 0) begin
      DebugLinkCmd cmd = truncate(fromJtag.value);
      if (cmd == cmdTempIn) begin
        respondFlag <= True;
        respondCmd <= cmdTempIn;
      end else begin
        recvCmd <= cmd;
        recvState <= 1;
      end
    end else if (recvState == 1) begin
      if (recvCmd == cmdQueryIn) begin
        // TODO FIXME: need more bits (second state) for the board ID reg
        boardOffset <= extend(fromJtag.value[7:4]);
        boardIdWithinBox <= fromJtag.value[3:0];
        recvState <= 2;
      end else if (recvCmd == cmdSetDest) begin
        recvDestThread <= fromJtag.value;
        recvState <= 2;
      end else if (recvCmd == cmdStdIn) begin
        DebugLinkFlit flit;
        flit.coreId = truncate(recvDestCore);
        flit.isBroadcast = unpack(recvDestCore[7]);
        flit.threadId = truncate(recvDestThread);
        flit.cmd = recvCmd;
        flit.payload = truncate(fromJtag.value);
        toBusPort.put(flit);
        recvState <= 0;
      end
    end else if (recvState == 2) begin
      if (recvCmd == cmdQueryIn) begin
        $display("[mkDebugLink::uartRecv] set board id to ", boardId, " boardIdWithinBox ", boardIdWithinBox);
        $display("[mkDebugLink::uartRecv] set boardOffset to ", fromJtag.value);
        boardOffset <= fromJtag.value;
        recvState <= 3;
      end else begin
        recvDestCore <= fromJtag.value;
        recvState <= 0;
      end
    end else if (recvState == 3) begin
      if (recvCmd == cmdQueryIn) begin
        Bit#(4) edgeEn = truncate(fromJtag.value);
        Vector#(4, Bool) linkEn = replicate(True);
        $display("[mkDebugLink::uartRecv] setting link enable regs to ", fromJtag.value, " and setting respondFlag");
        // Disable north link?
        Bit#(2) y = boardIdWithinBox[3:2];
        Bit#(2) x = boardIdWithinBox[1:0];
        if (y == fromInteger(`MeshYLenWithinBox-1) &&
              edgeEn[0] == 1) linkEn[0] = False;
        // Disable south link?
         if (y == 0 && edgeEn[1] == 1) linkEn[1] = False;
        // Disable east link?
        if (x == fromInteger(`MeshXLenWithinBox-1) &&
              edgeEn[2] == 1) linkEn[2] = False;
        // Disable west link?
        if (x == 0 && edgeEn[3] == 1) linkEn[3] = False;
        linkEnableReg <= linkEn;
        // Reserve extra send slot?
        useExtraSendSlotReg[2] <=
          Option {valid: True, value: fromJtag.value[4] == 1};
        respondFlag <= True;
        respondCmd <= cmdQueryIn;
        // Start checking temperature after first query command
        checkTemperature <= True;
        recvState <= 0;
      end else begin
        $display("WARNING: debglink recv in state 3, but not processing a queryin pkt!");
      end
    end
  endrule

  // Send commands over UART
  // -----------------------

  // State
  Reg#(Bit#(4)) sendState <- mkConfigReg(0);

  // Flit being forwarded
  Reg#(DebugLinkFlit) sendFlit <- mkConfigRegU;

  // Send QueryOut command
  (* no_implicit_conditions, fire_when_enabled *)
  rule uartSendQueryOut (toJtag.canPut && respondFlag);
    if (respondCmd == cmdQueryIn) begin
      if (sendState == 0) begin
        // Send QueryOut
        toJtag.put(zeroExtend(cmdQueryOut));
        sendState <= 1;
      end else if (sendState == 1) begin
        // Send QueryOut payload
        toJtag.put(1 + zeroExtend(pack(boardIdWithinBox)));
        sendState <= 2;
      end else if (sendState > 1) begin
        // send the FPGA ID. 8 bytes
        Vector#(8, Bit#(8)) chipvec = unpack(fromMaybe(0, chipIDReg));
        toJtag.put(chipvec[sendState-2]);
        if (sendState == 9) begin
        sendState <= 0;
        respondFlag <= False;
        end else begin
          sendState <= sendState+1;
        end
      end
    end else if (respondCmd == cmdTempIn) begin
      if (sendState == 0) begin
        // Send TempOut
        toJtag.put(zeroExtend(cmdTempOut));
        sendState <= 1;
      end else begin
        // Send temperature
        toJtag.put(temperature);
        sendState <= 0;
        respondFlag <= False;
      end
    end
  endrule

  // Send StdOut command
  rule uartSendStdOut (toJtag.canPut && !respondFlag);
    if (sendState == 0) begin
      if (overheatDetected && !overheatMsgSent) begin
        overheatMsgSent <= True;
        toJtag.put(zeroExtend(cmdOverheat));
      end else if (fromBusPort.canGet) begin
        fromBusPort.get;
        if (! fromBusPort.value.isBroadcast) begin
          // Send StdOut
          sendFlit <= fromBusPort.value;
          toJtag.put(zeroExtend(cmdStdOut));
          sendState <= 1;
        end
      end
    end else if (sendState == 1) begin
      // Send StdOut thread id
      toJtag.put(zeroExtend(sendFlit.threadId));
      sendState <= 2;
    end else if (sendState == 2) begin
      // Send StdOut core id
      toJtag.put(zeroExtend(sendFlit.coreId));
      sendState <= 3;
    end else begin
      // Send StdOut payload
      toJtag.put(sendFlit.payload);
      sendState <= 0;
    end
  endrule

  (* no_implicit_conditions, fire_when_enabled *)
  rule setBoardId;
    BoardId id;
    id.y = truncate(boardOffset[7:4] + zeroExtend(boardIdWithinBox[3:2]));
    id.x = truncate(boardOffset[3:0] + zeroExtend(boardIdWithinBox[1:0]));
    boardId <= id;
  endrule
  // Propagate extra send slot option through chain of registers (for timing)
  rule chain;
    for (Integer i = 0; i < 2; i=i+1)
      useExtraSendSlotReg[i] <= useExtraSendSlotReg[i+1];
  endrule

  `ifndef SIMULATE
  interface jtagAvalon = uart.jtagAvalon;
  `endif

  method BoardId getBoardId() = boardId;
  method Bit#(4) getBoardIdWithinBox() = boardIdWithinBox;
  method Vector#(4, Bool) linkEnable = linkEnableReg;
  method Option#(Bool) useExtraSendSlot = useExtraSendSlotReg[0];
endmodule

endpackage
