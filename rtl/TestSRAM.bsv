package TestSRAM;

// ============================================================================
// Imports
// ============================================================================

import Core       :: *;
import DCache     :: *;
import Globals    :: *;
import DRAM       :: *;
import SRAMx4     :: *;
import Interface  :: *;
import Queue      :: *;
import Vector     :: *;
import Mailbox    :: *;
import Network    :: *;
import DebugLink  :: *;
import JtagUart   :: *;
import Mac        :: *;
import FPU        :: *;
import OffChipRAM :: *;
import Util       :: *;

// ============================================================================
// Interface
// ============================================================================

`ifdef SIMULATE

typedef Empty DE5Top;

import "BDPI" function Bit#(32) getBoardId();

`else

interface DE5Top;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface AvalonMac northMac;
  interface AvalonMac southMac;
  interface AvalonMac eastMac;
  interface AvalonMac westMac;
  interface JtagUartAvalon jtagIfc;
  interface SRAMExtIfc sramIfc;
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

  Vector#(`DRAMsPerBoard, DRAM) drams;
  OffChipRAM offChipRAM <- mkOffChipRAM;
  drams[0] = offChipRAM.dram0;
  drams[1] = offChipRAM.dram1;

  // Ports
  OutPort#(DRAMReq) reqOutA <- mkOutPort;
  OutPort#(DRAMReq) reqOutB <- mkOutPort;
  InPort#(DRAMResp) respInA <- mkInPort;
  InPort#(DRAMResp) respInB <- mkInPort;

  // Connect ports to SRAM
  connectUsing(mkUGQueue, reqOutA.out, drams[0].reqIn);
  connectUsing(mkUGQueue, reqOutB.out, drams[1].reqIn);
  connectDirect(drams[0].respOut, respInA.in);
  connectDirect(drams[1].respOut, respInB.in);

  // Create bus of mailboxes
  ExtNetwork net <- mkBus(boardId, Vector::nil);

  // Create JTAG UART instance
  JtagUart uart <- mkJtagUart;

  // Ports
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Address accessed by test bench
  Bit#(26) addr = {1'b1, 0, 1'b1};

  // Testbench state
  Reg#(Bit#(4)) state <- mkReg(0);
  Reg#(Bit#(64)) displayVal <- mkRegU;
  Reg#(Bit#(4)) displayCount <- mkRegU;

  rule state0 (state == 0);
    if (fromJtag.canGet && toJtag.canPut) begin
      fromJtag.get;
      toJtag.put(88);
      state <= 1;
    end
  endrule

  // State of write
  Reg#(Bit#(256)) writeData <- mkReg(256'h01234567abcd00);
  Reg#(Bit#(4)) writeCount <- mkReg(0);

  // Send multiple stores to same address
  rule state1 (state == 1);
    if (reqOutA.canPut) begin
      DRAMReq req;
      req.isStore = True;
      req.id = 0;
      req.addr = addr;
      req.data = writeData;
      req.burst = 1;
      reqOutA.put(req);
      writeData <= { writeData[255:64], writeData[63:0] + 1 };
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
    if (reqOutA.canPut) begin
      DRAMReq req;
      req.isStore = False;
      req.id = 0;
      req.addr = addr;
      req.data = ?;
      req.burst = 1;
      reqOutA.put(req);
      state <= 4;
    end
  endrule

  rule state4 (state == 4);
    if (respInA.canGet) begin
      respInA.get;
      displayVal <= truncate(respInA.value.data);
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
  interface jtagIfc  = uart.jtagAvalon;
  interface northMac = net.north;
  interface southMac = net.south;
  interface eastMac  = net.east;
  interface westMac  = net.west;
  interface sramIfc  = offChipRAM.sramExt;
  method Action setBoardId(BoardId id);
    boardId <= id;
  endmethod
  `endif
endmodule

endpackage
