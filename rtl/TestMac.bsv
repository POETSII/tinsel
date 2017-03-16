package TestMac;

// =============================================================================
// Imports
// =============================================================================

import DRAM         :: *;
import JtagUart     :: *;
import Vector       :: *;
import Interface    :: *;
import Queue        :: *;
import ReliableLink :: *;
import Mac          :: *;

// =============================================================================
// Interface
// =============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

`else

interface DE5Top;
  interface AvalonMac macIfc;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface JtagUartAvalon jtagIfc;
endinterface

`endif

// =============================================================================
// Implementation
// =============================================================================

module de5Top (DE5Top);
 
  // Create JTAG UART
  JtagUart uart <- mkJtagUart;

  // Create 10G link
  Mac link <- mkMac;
 
  // Ports
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;
  InPort#(MacBeat) rxPort <- mkInPort;
  OutPort#(MacBeat) txPort <- mkOutPort;

  // Create DRAMs
  Vector#(`DRAMsPerBoard, DRAM) drams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    drams[i] <- mkDRAM(fromInteger(i));

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Connect ports to links
  connectUsing(mkUGShiftQueue1(QueueOptFmax), txPort.out, link.fromUser);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), link.toUser, rxPort.in);

  Reg#(Bit#(2)) state <- mkReg(0);
  Reg#(Bit#(16)) timer <- mkReg(0);
  Reg#(Bit#(2)) displayCount <- mkReg(0);

  rule transmit (state == 0 || state == 1);
    if (state == 0) begin
      if (fromJtag.canGet && txPort.canPut) begin
        fromJtag.get;
        txPort.put(macBeat(True, False, 0));
        state <= 1;
      end
    end else if (state == 1) begin
      if (txPort.canPut) begin
        txPort.put(macBeat(False, True, 0));
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

  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(DRAM dram) = dram.external;
  interface dramIfcs = map(getDRAMExtIfc, drams);
  interface jtagIfc = uart.jtagAvalon;
  interface macIfc = link.avalonMac;
  `endif

endmodule

endpackage
