package TestPCIe3xMboard;

// ============================================================================
// Imports
// ============================================================================

import Globals    :: *;
import Interface  :: *;
import Queue      :: *;
import Vector     :: *;
import Network    :: *;
import JtagUart   :: *;
import Mac        :: *;
import InstrMem   :: *;
import DRAM       :: *;

// ============================================================================
// Interface
// ============================================================================

interface DE5Top;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface AvalonMac northMac;
  interface AvalonMac southMac;
  interface AvalonMac eastMac;
  interface AvalonMac westMac;
  interface JtagUartAvalon jtagIfc;
  (* always_ready, always_enabled *)
  method Action setBoardId(BoardId id);
endinterface

// ============================================================================
// Implementation
// ============================================================================

module de5Top (DE5Top);
  // Board Id
  Wire#(BoardId) boardId <- mkDWire(?);

  // Create JTAG UART
  JtagUart uart <- mkJtagUart;

  // Create DRAMs
  Vector#(`DRAMsPerBoard, DRAM) drams;
  for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
    drams[i] <- mkDRAM(fromInteger(i));

  // Create off-board links
  BoardLink northLink <- mkBoardLink(0);
  BoardLink southLink <- mkBoardLink(1);
  BoardLink eastLink  <- mkBoardLink(2);
  BoardLink westLink  <- mkBoardLink(3);

  // Ports
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;
  InPort#(Bit#(64)) fromLink <- mkInPort;
  OutPort#(Bit#(64)) toLink <- mkOutPort;
  InPort#(Flit) fromEast <- mkInPort;
  OutPort#(Flit) toEast <- mkOutPort;
  InPort#(Flit) fromWest <- mkInPort;
  OutPort#(Flit) toWest <- mkOutPort;
  InPort#(Flit) fromNorth <- mkInPort;
  OutPort#(Flit) toNorth <- mkOutPort;
  InPort#(Flit) fromSouth <- mkInPort;
  OutPort#(Flit) toSouth <- mkOutPort;

  // Connect ports
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  connectUsing(mkUGShiftQueue1(QueueOptFmax), toEast.out, eastLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toWest.out, westLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toNorth.out, northLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toSouth.out, southLink.flitIn);

  connectUsing(mkUGShiftQueue1(QueueOptFmax), eastLink.flitOut, fromEast.in);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), westLink.flitOut, fromWest.in);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), northLink.flitOut, fromNorth.in);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), southLink.flitOut, fromSouth.in);

  Reg#(Bit#(8)) state <- mkReg(0);

  rule test0 (fromJtag.canGet && state == 0 && toJtag.canPut);
    fromJtag.get;
    //toJtag.put(48);
    state <= 1;
  endrule

  rule test1 (state == 1 && toEast.canPut && toJtag.canPut);
    toEast.put(unpack(65));
    toJtag.put(49);
    state <= 2;
  endrule

  rule test2 (state == 2 && fromWest.canGet && toJtag.canPut);
    fromWest.get;
    //toJtag.put(truncate(pack(fromWest.value))); 
    toJtag.put(50);
    state <= 0;
  endrule

  rule forward (state == 0 && fromWest.canGet &&
                  toEast.canPut && toJtag.canPut);
    fromWest.get;
    toJtag.put(51);
    toEast.put(fromWest.value);
  endrule

  // Using the PCIe motherboard, the east and west lanes differ
  // depending on which slot we're in:
  //   slot id 0 (C) ==> swap
  //   slot id 1 (B) ==> no swap
  //   slot id 2 (A) ==> no swap
  Bool swap = boardId.x == 0;
  AvalonMac east = macMux(swap, eastLink.avalonMac, westLink.avalonMac);
  AvalonMac west = macMux(swap, westLink.avalonMac, eastLink.avalonMac);

  `ifndef SIMULATE
  function DRAMExtIfc getDRAMExtIfc(DRAM dram) = dram.external;
  interface dramIfcs = map(getDRAMExtIfc, drams);
  interface jtagIfc  = uart.jtagAvalon;
  interface northMac = northLink.avalonMac;
  interface southMac = southLink.avalonMac;
  interface eastMac  = east;
  interface westMac  = west;
  method Action setBoardId(BoardId id);
    boardId <= id;
  endmethod
  `endif
endmodule

endpackage
