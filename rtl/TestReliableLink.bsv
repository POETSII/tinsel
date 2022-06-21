// SPDX-License-Identifier: BSD-2-Clause
package TestReliableLink;

// This simple loopback test: (1) waits for a character on the JTAG
// UART; (2) sends a stream of items over the reliable link; (3) waits
// for all the responses; (4) emits a hex value over the JTAG UART
// denoting the time between the first send and the final response.

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
import Util         :: *;

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

  // Number of values to send in test
  `ifdef SIMULATE
  Bit#(64) numVals = 1000000;
  `else
  Bit#(64) numVals = 1000000000;
  `endif

  // Create JTAG UART
  JtagUart uart <- mkJtagUart;

  // Create 10G link
  `ifdef SIMULATE
  ReliableLink link <- mkReliableLinkLoopback;
  `else
  ReliableLink link <- mkReliableLink;
  `endif

  // Ports
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;
  InPort#(Bit#(64)) fromLink <- mkInPort;
  OutPort#(Bit#(64)) toLink <- mkOutPort;

  // Create DRAMs
  Vector#(`DRAMsPerBoard, DRAM) drams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    drams[i] <- mkDRAM(fromInteger(i));

  // Connect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Connect ports to reliable link
  connectUsing(mkUGQueue, toLink.out, link.streamIn);
  connectDirect(link.streamOut, fromLink.in);

  Reg#(Bit#(3))  state <- mkReg(0);
  Reg#(Bit#(64)) recvCount <- mkReg(0);
  Reg#(Bit#(64)) sendCount <- mkReg(0);
  Reg#(Bit#(64)) timer <- mkReg(0);
  Reg#(Bit#(4))  displayCount <- mkReg(0);
  Reg#(Bit#(32)) numTimeouts <- mkReg(0);
  Reg#(Bit#(64)) lastReceived <- mkRegU;

  rule start (state == 0);
    if (fromJtag.canGet) begin
      $display("starting.")
      fromJtag.get;
      state <= 1;
    end
  endrule

  rule transmit (state == 1 && sendCount <= numVals);
    if (toLink.canPut) begin
      toLink.put(sendCount);
      sendCount <= sendCount+1;
    end
  endrule

  rule receive (state == 1);
    if (fromLink.canGet && toJtag.canPut) begin
      fromLink.get;
      lastReceived <= fromLink.value;
      if (recvCount == fromLink.value) begin
        if (recvCount == numVals)
          state <= 2;
        else
          recvCount <= recvCount+1;
      end else begin
        toJtag.put(88);
        state <= 2;
      end
    end
  endrule

  rule incTimer (state == 1);
    timer <= timer+1;
  endrule

  rule display0 (state == 2 && toJtag.canPut);
    Bit#(8) digit = hexDigit(truncateLSB(timer));
    timer <= timer << 4;
    toJtag.put(digit);
    displayCount <= displayCount+1;
    if (displayCount == 15) state <= 3;
  endrule

  rule display1 (state == 3 && toJtag.canPut);
    toJtag.put(10);
    state <= 4;
    numTimeouts <= link.numTimeouts;
  endrule

  rule display2 (state == 4 && toJtag.canPut);
    Bit#(8) digit = hexDigit(truncateLSB(numTimeouts));
    toJtag.put(digit);
    numTimeouts <= numTimeouts << 4;
    if (displayCount == 7) begin
      $display(numTimeouts, " ", recvCount, " ", sendCount);

      displayCount <= 0;
      state <= 0;
      recvCount <= 0;
      sendCount <= 0;
    end else begin
      displayCount <= displayCount+1;
    end
  endrule

  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(DRAM dram) = dram.external;
  interface dramIfcs = map(getDRAMExtIfc, drams);
  interface jtagIfc = uart.jtagAvalon;
  interface macIfc = link.avalonMac;
  `endif

endmodule

endpackage
