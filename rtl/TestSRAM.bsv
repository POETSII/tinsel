// SPDX-License-Identifier: BSD-2-Clause
package TestSRAM;

// ============================================================================
// Imports
// ============================================================================

import Core       :: *;
import DCache     :: *;
import Globals    :: *;
import DRAM       :: *;
import Interface  :: *;
import Queue      :: *;
import Vector     :: *;
import Mailbox    :: *;
import Network    :: *;
import DebugLink  :: *;
import JtagUart   :: *;
import Mac        :: *;
import FPU        :: *;
import NarrowSRAM :: *;
import WideSRAM   :: *;
import OffChipRAM :: *;
import Util       :: *;
import Socket     :: *;
import ConfigReg  :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

import "BDPI" function Bit#(32) getBoardId();

`else

interface DE5Top;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface Vector#(`SRAMsPerBoard, SRAMExtIfc) sramIfcs;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) northMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) southMac;
  interface Vector#(`NumEastWestLinks, AvalonMac) eastMac;
  interface Vector#(`NumEastWestLinks, AvalonMac) westMac;
  interface JtagUartAvalon jtagIfc;
  (* always_ready, always_enabled *)
  method Action setBoardId(BoardId id);
endinterface

`endif

// ============================================================================
// Implementation
// ============================================================================

module de5Top (DE5Top);
  // Board Id
  `ifdef SIMULATE
  BoardId boardId = unpack(truncate(getBoardId()));
  `else
  Wire#(BoardId) boardId <- mkDWire(?);
  `endif

  // Create RAMs
  Vector#(`DRAMsPerBoard, OffChipRAM) rams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    rams[i] <- mkOffChipRAM(fromInteger(i*3));

  // Ports
  OutPort#(DRAMReq) reqOutA <- mkOutPort;
  InPort#(DRAMResp) respInA <- mkInPort;
  OutPort#(DRAMReq) reqOutB <- mkOutPort;
  InPort#(DRAMResp) respInB <- mkInPort;

  // Connect ports to SRAM
  connectUsing(mkUGQueue, reqOutA.out, rams[0].reqIn);
  connectDirect(rams[0].respOut, respInA.in);
  connectUsing(mkUGQueue, reqOutB.out, rams[1].reqIn);
  connectDirect(rams[1].respOut, respInB.in);

  // Create inter-FPGA links
  Vector#(`NumNorthSouthLinks, BoardLink) northLink <-
    mapM(mkBoardLink, northSocket);
  Vector#(`NumNorthSouthLinks, BoardLink) southLink <-
    mapM(mkBoardLink, southSocket);
  Vector#(`NumEastWestLinks, BoardLink) eastLink <-
    mapM(mkBoardLink, eastSocket);
  Vector#(`NumEastWestLinks, BoardLink) westLink <-
    mapM(mkBoardLink, westSocket);

  // Create JTAG UART instance
  JtagUart uart <- mkJtagUart;

  // Ports
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Address accessed by test bench
  Reg#(Bit#(27)) addr <- mkRegU;
  Reg#(Bit#(27)) baseAddr <- mkRegU;

  // Testbench state
  Reg#(Bit#(4)) state <- mkReg(0);
  Reg#(Bit#(64)) displayVal <- mkRegU;
  Reg#(Bit#(4)) displayCount <- mkReg(0);

  // Time the load and store sequences
  Reg#(Bit#(32)) loadTime <- mkReg(0);
  Reg#(Bit#(32)) storeTime <- mkReg(0);

  rule state0 (state == 0);
    if (fromJtag.canGet && toJtag.canPut) begin
      fromJtag.get;
      Bit#(27) x;
      if (fromJtag.value == 65)
        x = fromInteger(8388608/32);
      else if (fromJtag.value == 66)
        x = fromInteger(16777216/32);
      else
        x = fromInteger(25165824/32);
      baseAddr <= x;
      addr <= x;
      toJtag.put(88);
      state <= 1;
      loadTime <= 0;
      storeTime <= 0;
    end
  endrule

  // State of access
  Reg#(Bit#(256)) writeData <- mkReg(0);
  Reg#(Bit#(16)) writeCount <- mkReg(0);
  Reg#(Bit#(16)) readCount <- mkConfigReg(0);
  Reg#(Bit#(16)) respCount <- mkConfigReg(0);
  PulseWire gotAllResps <- mkPulseWire;

  // Send multiple stores to same address
  rule state1 (state == 1);
    if (reqOutA.canPut && reqOutB.canPut) begin
      DRAMReq req;
      req.isStore = True;
      req.id = 0;
      req.addr = addr;
      req.data = writeData;
      req.burst = 1;
      reqOutA.put(req);
      reqOutB.put(req);
      writeCount <= writeCount+1;
      if (writeCount == ~0) state <= 3;
      writeData <= {0, writeData[31:0] + 1};
    end
    storeTime <= storeTime + 1;
  endrule

  rule state3 (state == 3);
    if (reqOutA.canPut) begin
      DRAMReq req;
      req.isStore = False;
      req.id = 0;
      req.addr = addr;
      req.data = ?;
      req.burst = 1;
      reqOutA.put(req);
      if (readCount == ~0) state <= 4;
      readCount <= readCount+1;
    end
  endrule

  rule state4a (state != 5);
    if (respInA.canGet) begin
      respInA.get;
      respCount <= respCount+1;
      //displayVal <= truncate(respInA.value.data);
      displayVal <= { storeTime, loadTime };
      if (respCount == ~0) gotAllResps.send;
    end
  endrule

  rule state4b (state == 4 && gotAllResps);
    state <= 5;
  endrule

  rule count (state == 3 || state == 4);
    loadTime <= loadTime+1;
  endrule

  rule display (state == 5 && toJtag.canPut);
    Bit#(8) digit = hexDigit(truncateLSB(displayVal));
    displayVal <= displayVal << 4;
    toJtag.put(digit);
    displayCount <= displayCount+1;
    if (displayCount == 15) state <= 0;
  endrule

  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(OffChipRAM ram) = ram.extDRAM;
  function Vector#(2, SRAMExtIfc) getSRAMExtIfcs(OffChipRAM ram) = ram.extSRAM;
  function AvalonMac getMac(BoardLink link) = link.avalonMac;
  interface dramIfcs = map(getDRAMExtIfc, rams);
  interface sramIfcs = concat(map(getSRAMExtIfcs, rams));
  interface jtagIfc  = uart.jtagAvalon;
  interface northMac = Vector::map(getMac, northLink);
  interface southMac = Vector::map(getMac, southLink);
  interface eastMac = Vector::map(getMac, eastLink);
  interface westMac = Vector::map(getMac, westLink);
  method Action setBoardId(BoardId id);
    boardId <= id;
  endmethod
  `endif
endmodule

endpackage
