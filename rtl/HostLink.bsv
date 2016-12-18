// Copyright (c) Matthew Naylor

package HostLink;

// HostLink connects the host PC to the tinsel cores.
//
// Communication between the host and the FPGA(s) takes the form of a
// byte-stream over a JTAG UART. Commands can be sent from the host to
// the FPGA and viceversa.  All commands sent from the host consist of
// 5 bytes: a 1-byte command tag and a 4-byte argument.
//
//   SetDest (1 byte), core id (4 bytes)
//   -----------------------------------
//
//   Sets the destination core id for all subsequent commands (until
//   the next SetDest command).  This is a meta-command in that it is
//   not actually sent to any core.  The MSB of core id is the
//   broadcast bit.
//
//   StdIn (1 byte), payload (4 bytes)
//   ---------------------------------
//
//   The payload is sent to the destination core(s).
//
// All commands sent to the host consist of 9 bytes: a 1-byte command
// tag, a 4-byte source core id, and a 4-byte argument.
//
//   StdOut (1 byte), core id (4 bytes), payload (4 bytes)
//   -----------------------------------------------------
//
//   The payload is sent to the host with the given source core id.

// =============================================================================
// Imports
// =============================================================================

import JtagUart  :: *;
import Interface :: *;
import Vector    :: *;
import Queue     :: *;
import Util      :: *;

// =============================================================================
// HostLink commands
// =============================================================================

typedef Bit#(2) HostLinkCmd;
HostLinkCmd cmdSetDest = 0;
HostLinkCmd cmdStdIn   = 1;
HostLinkCmd cmdStdOut  = 2;

// =============================================================================
// Types
// =============================================================================

// A host-link flit (that travels along the host-link bus connecting the cores)
typedef struct {
  // The source or destination core
  Bit#(`LogCoresPerBoard) coreId;
  // Is every core a destination?
  Bool isBroadcast;
  // Command
  HostLinkCmd cmd;
  // Command argument
  Bit#(32) arg;
} HostLinkFlit deriving (Bits, FShow);

// Flit serialiser state machine
typedef enum {SER_IDLE, SER_SRC, SER_ARG} SerialiserState
  deriving (Bits, Eq);

// =============================================================================
// HostLink router
// =============================================================================

// The cores are connected to a serial chain of routers (a bus)
interface HostLinkRouter;
  interface In#(HostLinkFlit) busIn;
  interface Out#(HostLinkFlit) busOut;
  interface In#(HostLinkFlit) fromCore;
  interface Out#(HostLinkFlit) toCore;
endinterface

module mkHostLinkRouter#(Bit#(`LogCoresPerBoard) myId) (HostLinkRouter);

  // Ports
  InPort#(HostLinkFlit)  busInPort    <- mkInPort;
  OutPort#(HostLinkFlit) busOutPort   <- mkOutPort;
  InPort#(HostLinkFlit)  fromCorePort <- mkInPort;
  OutPort#(HostLinkFlit) toCorePort   <- mkOutPort;

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
// Main HostLink module
// =============================================================================

// Each core implements the following host link interface
interface HostLinkCore;
  interface In#(HostLinkFlit) fromHost;
  interface Out#(HostLinkFlit) toHost;
endinterface

interface HostLink;
  `ifndef SIMULATE
  interface JtagUartAvalon jtagAvalon;
  `endif
endinterface

module mkHostLink#(Vector#(n, HostLinkCore) cores) (HostLink);

  // Ports
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;

  // Create JTAG UART instance
  JtagUart uart <- mkJtagUart;

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Create serial bus
  Vector#(n, HostLinkRouter) bus;
  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    bus[i] <- mkHostLinkRouter(fromInteger(i));
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   cores[i].toHost, bus[i].fromCore);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   bus[i].toCore, cores[i].fromHost);
  end
  for (Integer i = 0; i < valueOf(n)-1; i=i+1)
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   bus[i].busOut, bus[i+1].busIn);

  // Deserialise flits from the JTAG UART
  // ------------------------------------

  // Output from deserialiser
  OutPort#(HostLinkFlit) desOutPort <- mkOutPort;

  // Connect deserialiser to bus
  connectUsing(mkUGShiftQueue1(QueueOptFmax), desOutPort.out, bus[0].busIn);

  // State of deserialiser
  Reg#(Bit#(3)) desState <- mkReg(0);

  // Deserialiser buffer
  Reg#(Bit#(32)) desBuffer <- mkRegU;

  // Destination address
  Reg#(Bit#(32)) destCore <- mkRegU;

  rule deserialise (fromJtag.canGet && desOutPort.canPut);
    fromJtag.get;
    Bit#(32) newBuffer = { fromJtag.value, truncateLSB(desBuffer) };
    desBuffer <= newBuffer;
    if (desState == 4) begin
      HostLinkCmd cmd = truncate(desBuffer);
      if (cmd == cmdSetDest) begin
        destCore <= newBuffer;
      end else begin
        HostLinkFlit flit;
        flit.coreId = truncate(destCore);
        flit.isBroadcast = unpack(destCore[31]);
        flit.cmd = cmd;
        flit.arg = newBuffer;
        desOutPort.put(flit);
      end
      desState <= 0;
    end else
      desState <= desState + 1;
  endrule

  // Serialise flits to JTAG UART
  // ----------------------------

  // Input to the serialiser
  InPort#(HostLinkFlit) serInPort <- mkInPort;

  // Connect bus to serialiser
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 bus[valueOf(n)-1].busOut, serInPort.in);

  // State of serialiser
  Reg#(SerialiserState) serState <- mkReg(SER_IDLE);

  // Shift registers
  Reg#(Bit#(32)) serSrc <- mkRegU;
  Reg#(Bit#(32)) serArg <- mkRegU;

  // Byte counters
  Reg#(Bit#(2)) serSrcCount <- mkReg(0);
  Reg#(Bit#(2)) serArgCount <- mkReg(0);

  rule serialiser (toJtag.canPut);
    case (serState)
      SER_IDLE:
        if (serInPort.canGet) begin
          serInPort.get;
          serSrc <= zeroExtend(serInPort.value.coreId);
          serArg <= serInPort.value.arg;
          if (! serInPort.value.isBroadcast) begin
            toJtag.put(zeroExtend(serInPort.value.cmd));
            serState <= SER_SRC;
          end
        end
      SER_SRC:
        begin
          toJtag.put(truncate(serSrc));
          serSrc <= { ?, serSrc[31:8] };
          serSrcCount <= serSrcCount+1;
          if (allHigh(serSrcCount)) serState <= SER_ARG;
        end
      SER_ARG:
        begin
          toJtag.put(truncate(serArg));
          serArg <= { ?, serArg[31:8] };
          serArgCount <= serArgCount+1;
          if (allHigh(serArgCount)) serState <= SER_IDLE;
        end
    endcase
  endrule

  `ifndef SIMULATE
  interface jtagAvalon = uart.jtagAvalon;
  `endif

endmodule

endpackage
