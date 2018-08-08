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
import WideSRAM   :: *;
import Util       :: *;
import Socket     :: *;

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
  interface AvalonMac northMac;
  interface AvalonMac southMac;
  interface AvalonMac eastMac;
  interface AvalonMac westMac;
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

  // Create DRAMs
  Vector#(`DRAMsPerBoard, DRAM) drams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    drams[i] <- mkDRAM(fromInteger(i));

  // Create SRAMs
  Vector#(`SRAMsPerBoard, DRAM) srams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    srams[i] <- mkWideSRAM(fromInteger(`DRAMsPerBoard+i));

  // Ports
  OutPort#(DRAMReq) reqOut <- mkOutPort;
  InPort#(DRAMResp) respIn <- mkInPort;

  // Connect ports to SRAM
  connectUsing(mkUGQueue, reqOut.out, srams[0].reqIn);
  connectDirect(srams[0].respOut, respIn.in);

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
  Bit#(27) addr = 0;

  // Testbench state
  Reg#(Bit#(4)) state <- mkReg(0);
  Reg#(Bit#(64)) displayVal <- mkRegU;
  Reg#(Bit#(4)) displayCount <- mkReg(0);

  rule state0 (state == 0);
    if (fromJtag.canGet && toJtag.canPut) begin
      fromJtag.get;
      toJtag.put(88);
      state <= 1;
    end
  endrule

  // State of write
  Reg#(Bit#(256)) writeData <- mkReg(
    256'h0001234567abcd03_0001234567abcd02_0001234567abcd01_0001234567abcd00);
  Reg#(Bit#(4)) writeCount <- mkReg(0);

  // Send multiple stores to same address
  rule state1 (state == 1);
    if (reqOut.canPut) begin
      DRAMReq req;
      req.isStore = True;
      req.id = 0;
      req.addr = addr;
      req.data = writeData;
      req.burst = 1;
      reqOut.put(req);
      writeCount <= writeCount+1;
      if (writeCount == ~0) state <= 3;
    end
  endrule

  Reg#(Bit#(4)) delay <- mkReg(0);
  rule state2 (state == 2);
    delay <= delay + 1;
    if (delay == ~0) state <= 3;
  endrule

  rule state3 (state == 3);
    if (reqOut.canPut) begin
      DRAMReq req;
      req.isStore = False;
      req.id = 0;
      req.addr = addr;
      req.data = ?;
      req.burst = 1;
      reqOut.put(req);
      state <= 4;
    end
  endrule

  rule state4 (state == 4);
    if (respIn.canGet) begin
      respIn.get;
      displayVal <= truncate(respIn.value.data);
      state <= 5;
    end
  endrule

  rule display (state == 5 && toJtag.canPut);
    Bit#(8) digit = hexDigit(truncateLSB(displayVal));
    displayVal <= displayVal << 4;
    toJtag.put(digit);
    displayCount <= displayCount+1;
    if (displayCount == 15) state <= 0;
  endrule


  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(DRAM dram) = dram.external;
  interface dramIfcs = map(getDRAMExtIfc, drams);
  function DRAMExtIfc getSRAMExtIfc(WideSRAM sram) = sram.external;
  interface sramIfcs = map(getSRAMExtIfc, srams);
  interface jtagIfc  = uart.jtagAvalon;
  function AvalonMac getMac(BoardLink link) = link.avalonMac;
  interface north = Vector::map(getMac, northLink);
  interface south = Vector::map(getMac, southLink);
  interface east = Vector::map(getMac, eastLink);
  interface west = Vector::map(getMac, westLink);
  method Action setBoardId(BoardId id);
    boardId <= id;
  endmethod
  `endif
endmodule

endpackage
