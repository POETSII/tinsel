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
//   QueryIn: tag (1 byte), board offset (1 byte)
//   --------------------------------------------
//
//   Sets the X offset (offset[3:0]) and the Y offset (offset[7:4])
//   of the board id (to support multiple boxes).
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
// Commands sent from DebugLink to the host PC over the JTAG UART are
// as follows.
//
//   QueryOut: tag (1 byte), payload (1 byte)
//   ----------------------------------------
//
//   Response to QueryIn command. Payload contains 1 + Board Id.  By
//   returning non-zero, this response can be used by the host to
//   detect a tinsel board (distingushing it, for example, from a host
//   board which returns a 0 payload) and also to discover the board id.
//
//
//   StdOut: tag (1 byte), thread id (1 byte),
//           core id (1 byte), payload (1 byte)
//   ------------------------------------------
//
//   A byte sent by a thread is forwarded to the host.
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

// =============================================================================
// DebugLink commands
// =============================================================================

typedef Bit#(2) DebugLinkCmd;
DebugLinkCmd cmdQueryIn  = 0;
DebugLinkCmd cmdQueryOut = 0;
DebugLinkCmd cmdSetDest  = 1;
DebugLinkCmd cmdStdIn    = 2;
DebugLinkCmd cmdStdOut   = 2;

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
  (* always_ready, always_enabled *)
  method BoardId getBoardId();
endinterface

module mkDebugLink#(
    Bit#(4) boardIdWithinBox,
    Vector#(n, DebugLinkClient) cores) (DebugLink);

  // The board offset (in a multi-box setup) received via DebugLink
  Reg#(Bit#(8)) boardOffset <- mkReg(0);

  // The board id combines the Y offset, received via DebugLink,
  // with the box-local board id (set via DIP switches) to give
  // an overall board id.
  Reg#(BoardId) boardId <- mkReg(unpack(0));

  // The board id will be distributed across the chip,
  // so let's use a shift register to help timing
  Vector#(3, Reg#(BoardId)) boardIdVec <- replicateM(mkReg(unpack(0)));

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

  // Board id shift register
  // -----------------------

  rule setBoardId;
    BoardId id;
    id.y = truncate(boardOffset[7:4] + zeroExtend(boardIdWithinBox[3:2]));
    id.x = truncate(boardOffset[3:0] + zeroExtend(boardIdWithinBox[1:0]));
    boardId <= id;
  endrule

  rule distributeBoardId;
    // Put board id through shift register to improve timing
    boardIdVec[2] <= boardId;
    for (Integer i = 0; i < 2; i=i+1)
      boardIdVec[i] <= boardIdVec[i+1];
  endrule

  // Receive commands over UART
  // --------------------------

  // State
  Reg#(Bit#(2)) recvState <- mkConfigReg(0);

  // Destination core id
  Reg#(Bit#(8)) recvDestCore <- mkConfigReg(0);

  // Destination thread id
  Reg#(Bit#(8)) recvDestThread <- mkConfigReg(0);

  // Command
  Reg#(DebugLinkCmd) recvCmd <- mkConfigReg(0);

  // Respond to query command?
  Reg#(Bool) respondQuery <- mkConfigReg(False);

  rule uartRecv (fromJtag.canGet && toBusPort.canPut && !respondQuery);
    fromJtag.get;
    if (recvState == 0) begin
      DebugLinkCmd cmd = truncate(fromJtag.value);
      recvCmd <= cmd;
      recvState <= 1;
    end else if (recvState == 1) begin
      if (recvCmd == cmdQueryIn) begin
        respondQuery <= True;
        boardOffset <= fromJtag.value;
        recvState <= 0;
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
      recvDestCore <= fromJtag.value;
      recvState <= 0;
    end
  endrule

  // Send commands over UART
  // -----------------------

  // State
  Reg#(Bit#(2)) sendState <- mkConfigReg(0);

  // Flit being forwarded
  Reg#(DebugLinkFlit) sendFlit <- mkConfigRegU;

  // Send QueryOut command
  rule uartSendQueryOut (toJtag.canPut && respondQuery);
    if (sendState == 0) begin
      // Send QueryOut
      toJtag.put(zeroExtend(cmdQueryOut));
      sendState <= 1;
    end else begin
      // Send QueryOut payload
      toJtag.put(1 + zeroExtend(pack(boardId)));
      sendState <= 0;
      respondQuery <= False;
    end
  endrule

  // Send StdOut command
  rule uartSendStdOut (toJtag.canPut && !respondQuery);
    if (sendState == 0) begin
      if (fromBusPort.canGet) begin
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


  `ifndef SIMULATE
  interface jtagAvalon = uart.jtagAvalon;
  `endif

  method BoardId getBoardId() = boardIdVec[0];

endmodule

endpackage
