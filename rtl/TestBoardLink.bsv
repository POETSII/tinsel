package TestBoardLink;

// =============================================================================
// Imports
// =============================================================================

import DRAM         :: *;
import JtagUart     :: *;
import AvalonStream :: *;
import Vector       :: *;
import Interface    :: *;
import Queue        :: *;

// =============================================================================
// Interface
// =============================================================================


`ifdef SIMULATE

typedef Empty DE5Top;

`else

interface DE5Top;
  interface AvalonSource#(Bit#(64)) transmitIfc;
  interface AvalonSink#(Bit#(64)) receiveIfc;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface JtagUartAvalon jtagIfc;
endinterface

`endif

// =============================================================================
// Implementation
// =============================================================================

module de5Top (DE5Top);
  
  // Ports
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;
  InPort#(AvalonBeat#(Bit#(64))) rxPort <- mkInPort;
  OutPort#(AvalonBeat#(Bit#(64))) txPort <- mkOutPort;

  // Create JTAG UART instance
  JtagUart uart <- mkJtagUart;

  // Create DRAMs
  Vector#(`DRAMsPerBoard, DRAM) drams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    drams[i] <- mkDRAM(fromInteger(i));

  // Create links
  Source#(Bit#(64)) source <- mkSource;
  Sink#(Bit#(64)) sink <- mkSink;

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Connect ports to links
  connectUsing(mkUGShiftQueue1(QueueOptFmax), txPort.out, source.in);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), sink.out, rxPort.in);

  Reg#(Bit#(2)) state <- mkReg(0);
  Reg#(Bit#(16)) timer <- mkReg(0);
  Reg#(Bit#(2)) displayCount <- mkReg(0);

  rule transmit (state == 0 || state == 1);
    if (state == 0) begin
      if (fromJtag.canGet && txPort.canPut) begin
        fromJtag.get;
        txPort.put(avalonBeat(True, False, 0));
        state <= 1;
      end
    end else if (state == 1) begin
      if (txPort.canPut) begin
        txPort.put(avalonBeat(False, True, 0));
        state <= 2;
      end
    end
  endrule

  rule receive (state == 2);
    if (rxPort.canGet) begin
      rxPort.get;
      if (rxPort.value.stop) state <= 3;
    end
    timer <= timer+1;
  endrule

  rule display (state == 3 && toJtag.canPut);
    Bit#(8) digit = timer[15:12] >= 10 ?
      55 + zeroExtend(timer[15:12]) : 48 + zeroExtend(timer[15:12]);
    toJtag.put(digit);
    timer <= timer << 4;
    displayCount <= displayCount+1;
    if (displayCount == 3) state <= 0;
  endrule

  function DRAMExtIfc getDRAMExtIfc(DRAM dram) = dram.external;
  interface dramIfcs = map(getDRAMExtIfc, drams);
  interface jtagIfc = uart.jtagAvalon;
  interface transmitIfc = source.avalonSource;
  interface receiveIfc = sink.avalonSink;

endmodule

endpackage
