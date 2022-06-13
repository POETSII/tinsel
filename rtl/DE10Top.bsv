// import NOCInterfaces::*;
// import MacSyncroniser::*;

// import Avalon2ServerSingleMaster::*;
// import Avalon2ClientServer::*;
import ClientServer::*;
import GetPut::*;

import ReliableLink::*;
// import AXI4 :: *;
// import AXI4Lite :: *;


// ============================================================================
// Imports
// ============================================================================

// import Core         :: *;
// import DCache       :: *;
import Globals      :: *;
import DRAM         :: *;
import Interface    :: *;
import Queue        :: *;
import Vector       :: *;
// import Mailbox      :: *;
// import Network      :: *;
// import DebugLink    :: *;
import JtagUart     :: *;
import Mac          :: *;
// import FPU          :: *;
// import InstrMem     :: *;
// import NarrowSRAM   :: *;
import OffChipRAM   :: *;
// import IdleDetector :: *;
import Connections  :: *;
import PCIeStream   :: *;
import HostLink   :: *;
import Clocks   :: *;
import Util   :: *;
import StmtFSM :: * ;
import StringPack :: * ;
import AvalonStreamCC :: *;

`ifdef SIMULATE

typedef Empty DE10Ifc;
import "BDPI" function Bit#(32) getBoardId();

`else

interface DE10Ifc;
  interface Vector#(`DRAMsPerBoard, DRAMExtIfc) dramIfcs;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) northMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) southMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) eastMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) westMac;

  interface JtagUartAvalon jtagIfc;
  (* always_ready, always_enabled *)
  method Action setTemperature(Bit#(8) temp);

  // Interface to the PCIe BAR
  interface PCIeBAR controlBAR;
  // Interface to host PCIe bus
  // (Use for DMA to/from host memory)
  interface PCIeHostBus pcieHostBus;
  // Reset request
  (* always_enabled, always_ready *)
  method Bool resetReq;

endinterface


`endif



// ============================================================================
// Implementation
// ============================================================================

// // mkDE10Top wrapper ensures the entire design is reset correctly when requested by the host
// module mkDE10Top(DE10Ifc ifc);
//
//   Clock defaultClock <- exposeCurrentClock();
//   Reset externalReset <- exposeCurrentReset();
//   MakeResetIfc hostReset <- mkReset(1, False, defaultClock);
//   Reset combinedReset = externalReset; // <- mkResetEither(externalReset, hostReset.new_rst);
//
//
//
//   `ifdef SIMULATE
//   // DE10Ifc top <- mkDE10Top_inner();
//   DE10Ifc top <- mkDE10_LinkTest();
//
//   `endif
//
//   `ifndef SIMULATE
//   // DE10Ifc top <- mkDE10Top_inner(reset_by externalReset);
//   DE10Ifc top <- mkDE10_LinkTest();
//
//   (* fire_when_enabled, no_implicit_conditions *)
//   rule pcieReset;
//     if (top.resetReq) hostReset.assertReset();
//   endrule
//
//   interface dramIfcs = top.dramIfcs;
//   interface jtagIfc  = top.jtagIfc;
//   interface controlBAR  = top.controlBAR;
//   interface pcieHostBus  = top.pcieHostBus;
//   method Bool resetReq = !top.resetReq;
//   method Action setTemperature(Bit#(8) temp) = top.setTemperature(temp);
//   interface northMac = top.northMac;
//   interface southMac = top.southMac;
//   interface eastMac  = top.eastMac;
//   interface westMac  = top.westMac;
//
//   `endif
//
// endmodule


// interface PrinterIfc;
//
//
// module mkJTAGPrinter(JtagUart uart, )

// function Stmt printStringToUart(JtagUart uart, String s);
//   List#(Char) s_c = stringToCharList(s);
//   Reg#()
//   Stmt fsm = seq
//     for (Integer ci=0; ci<stringLength(s); ci=ci+1) action
//       toJtag.put(pack(s_c[ci]));
//     endaction
//   endseq
//   return fsm;
// endfunction


module mkDE10Top(Clock northTxClk, Clock northRxClk, Clock southTxClk, Clock southRxClk,
                 Reset northTxRst, Reset northRxRst, Reset southTxRst, Reset southRxRst,
                 DE10Ifc ifc);

   Clock defaultClock <- exposeCurrentClock();
   Reset defaultReset <- exposeCurrentReset();

  // Number of values to send in test
  `ifdef SIMULATE
  Bit#(64) numVals = 10000;
  `else
  Bit#(64) numVals = 10000000;
  `endif

  // Create JTAG UART
  JtagUart uart <- mkJtagUart;
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;
  InPort#(Bit#(64)) fromLink <- mkInPort;
  OutPort#(Bit#(64)) toLink <- mkOutPort;



  // Create 10G link
  `ifdef SIMULATE
  ReliableLink linkNorth <- mkReliableLink;
  ReliableLink linkSouth <- mkReliableLink;

  AvalonCCIfc northCC <- mkAvalonStreamConverter(linkNorth.avalonMac,
                                              defaultClock, defaultClock, defaultClock,
                                               defaultReset, defaultReset, defaultReset);
  AvalonCCIfc southCC <- mkAvalonStreamConverter(linkSouth.avalonMac,
                                                defaultClock, defaultClock, defaultClock,
                                                defaultReset, defaultReset, defaultReset);


  connectUsing(mkUGQueue, toLink.out, linkNorth.streamIn);
  connectDirect(linkSouth.streamOut, fromLink.in);

  (* fire_when_enabled, no_implicit_conditions *)
  rule driveNorthToSouth;
    let in = linkNorth.avalonMac;
    let out = linkSouth.avalonMac;
    in.source(out.sink_ready);
    if (out.sink_ready)
      out.sink(in.source_data, in.source_valid,
               in.source_startofpacket, in.source_endofpacket,
               zeroExtend(in.source_error), in.source_empty);
    else
      out.sink(?, False, ?, ?, ?, ?);
  endrule

  (* fire_when_enabled, no_implicit_conditions *)
  rule driveSouthToNorth;
    let in = linkSouth.avalonMac;
    let out = linkNorth.avalonMac;
    in.source(out.sink_ready);
    if (out.sink_ready)
      out.sink(in.source_data, in.source_valid,
               in.source_startofpacket, in.source_endofpacket,
               zeroExtend(in.source_error), in.source_empty);
    else
      out.sink(?, False, ?, ?, ?, ?);
  endrule

  `else
  ReliableLink linkNorth <- mkReliableLink;
  ReliableLink linkSouth <- mkReliableLink;
  connectUsing(mkUGQueue, toLink.out, linkNorth.streamIn);
  connectDirect(linkSouth.streamOut, fromLink.in);


  `endif

  // Ports
  // Create DRAMs
  // Vector#(`DRAMsPerBoard, DRAM) drams;
  // for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
  //   drams[i] <- mkDRAM(fromInteger(i));

  // Connect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);



  Reg#(Bit#(3))  state <- mkReg(0); // 0: waiting; 1: running.
  Reg#(Bit#(64)) recvCount <- mkReg(0);
  Reg#(Bit#(64)) sendCount <- mkReg(0);
  Reg#(Bit#(64)) timer <- mkReg(0);
  Reg#(Bit#(4))  displayCount <- mkReg(0);
  Reg#(Bit#(32)) numTimeouts <- mkReg(0);
  Reg#(Bit#(64)) lastReceived <- mkRegU;

  Reg#(Int#(32)) stdoutCtr <- mkReg(0);
  String start = "Starting_reliable_link_tester.";
  String startT = "starting_test.";
  Reg#(Bit#(8)) inchar <- mkReg(0);

  List#(Char) start_lc = stringToCharList(start);

  // connectUsing(mkQueue, uart.jtagOut, uart.jtagIn);

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
        if (recvCount == numVals) begin
          toJtag.put(charToAscii("D"));
          state <= 0;
        end else begin
          recvCount <= recvCount+1;
        end
      end else begin
        // out of order or corrupted packet; bail.
        toJtag.put(charToAscii("X"));
        // state <= 0;
      end
    end
  endrule

  rule incTimer (state == 1);
    timer <= timer+1;
  endrule


  Stmt tststmt = seq


    // for (stdoutCtr<=0; stdoutCtr<fromInteger(stringLength(start))-1; stdoutCtr<=stdoutCtr+1) action
    //   toJtag.put(charToAscii(start_lc[stdoutCtr]));
    // endaction
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[0]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[1]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[2]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[3]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[4]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[5]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[6]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[7]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[8]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[9]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[10]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[11]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[12]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[13]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[14]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[15]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[16]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[17]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[18]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[19]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[20]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[21]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[22]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[23]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[24]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[25]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[26]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[27]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[28]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(start)[29]));
    await(toJtag.canPut); toJtag.put(10);

    await(fromJtag.canGet);
    action
      inchar <= fromJtag.value;
      fromJtag.get();
    endaction

    while (fromJtag.canGet) seq
      fromJtag.get();
    endseq


    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[0]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[1]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[2]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[3]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[4]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[5]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[6]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[7]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[8]));
    await(toJtag.canPut); toJtag.put(charToAscii(stringToCharList(startT)[9]));
    await(toJtag.canPut); toJtag.put(10);

    action
      sendCount <= 0;
      timer <= 0;
      numTimeouts <= 0;
    endaction

    // par
    //   while (sendCount <= numVals) action
    //     timer <= timer+1;
    //   endaction
    //
    //
    //   while (sendCount <= numVals) action
    //     if (toLink.canPut) action
    //       toLink.put(sendCount);
    //       sendCount <= sendCount+1;
    //     endaction
    //   endaction
    //
    //   while (sendCount <= numVals) action
    //     if (fromLink.canGet && toJtag.canPut) action
    //       fromLink.get;
    //       lastReceived <= fromLink.value;
    //       if (recvCount == fromLink.value) begin
    //         if (recvCount == numVals)
    //           state <= 2;
    //         else
    //           recvCount <= recvCount+1;
    //       end else begin
    //         toJtag.put(88);
    //         state <= 2;
    //       end
    //     endaction
    //   endaction
    //
    // endpar

    state <= 1;

    while (state != 0) seq
      delay(1);
    endseq

    await(toJtag.canPut); toJtag.put(charToAscii((state == 0) ? "0" : "1"));

    toJtag.put(10);

    await(toJtag.canPut); toJtag.put(charToAscii("t"));
    await(toJtag.canPut); toJtag.put(charToAscii("i"));
    await(toJtag.canPut); toJtag.put(charToAscii("m"));
    await(toJtag.canPut); toJtag.put(charToAscii("e"));
    await(toJtag.canPut); toJtag.put(charToAscii("r"));
    await(toJtag.canPut); toJtag.put(charToAscii(":"));

    displayCount <= 0;
    while (displayCount < 15) action
      if (toJtag.canPut) begin
        Bit#(8) digit = hexDigit(truncateLSB(timer));
        timer <= timer << 4;
        toJtag.put(digit);
        displayCount <= displayCount+1;
      end
    endaction

    toJtag.put(10);
    await(toJtag.canPut); toJtag.put(charToAscii("s"));
    await(toJtag.canPut); toJtag.put(charToAscii("e"));
    await(toJtag.canPut); toJtag.put(charToAscii("n"));
    await(toJtag.canPut); toJtag.put(charToAscii("t"));
    await(toJtag.canPut); toJtag.put(charToAscii(":"));


    displayCount <= 0;
    while (displayCount < 15) action
      if (toJtag.canPut) begin
        Bit#(8) digit = hexDigit(truncateLSB(sendCount));
        sendCount <= sendCount << 4;
        toJtag.put(digit);
        displayCount <= displayCount+1;
      end
    endaction


    toJtag.put(10);
    await(toJtag.canPut); toJtag.put(charToAscii("r"));
    await(toJtag.canPut); toJtag.put(charToAscii("e"));
    await(toJtag.canPut); toJtag.put(charToAscii("c"));
    await(toJtag.canPut); toJtag.put(charToAscii("v"));
    await(toJtag.canPut); toJtag.put(charToAscii(":"));



    displayCount <= 0;
    while (displayCount < 15) action
      if (toJtag.canPut) begin
        Bit#(8) digit = hexDigit(truncateLSB(recvCount));
        recvCount <= recvCount << 4;
        toJtag.put(digit);
        displayCount <= displayCount+1;
      end
    endaction


    toJtag.put(10);
    await(toJtag.canPut); toJtag.put(charToAscii("t"));
    await(toJtag.canPut); toJtag.put(charToAscii("i"));
    await(toJtag.canPut); toJtag.put(charToAscii("m"));
    await(toJtag.canPut); toJtag.put(charToAscii("e"));
    await(toJtag.canPut); toJtag.put(charToAscii("o"));
    await(toJtag.canPut); toJtag.put(charToAscii("u"));
    await(toJtag.canPut); toJtag.put(charToAscii("t"));
    await(toJtag.canPut); toJtag.put(charToAscii("s"));
    await(toJtag.canPut); toJtag.put(charToAscii(":"));


    numTimeouts <= linkNorth.numTimeouts;


    displayCount <= 0;
    while (displayCount < 7) action
      if (toJtag.canPut) begin
        Bit#(8) digit = hexDigit(truncateLSB(numTimeouts));
        toJtag.put(digit);
        numTimeouts <= numTimeouts << 4;
        if (displayCount == 7) begin
          displayCount <= 0;
          state <= 0;
          recvCount <= 0;
          sendCount <= 0;
        end else begin
          displayCount <= displayCount+1;
        end
      end
    endaction
    toJtag.put(10);

  endseq;


  FSM test <- mkFSM(tststmt);

  rule run;
    test.start();
  endrule

  `ifndef SIMULATE

  AvalonCCIfc northCC <- mkAvalonStreamConverter(linkNorth.avalonMac,
                                                defaultClock, northTxClk, northRxClk,
                                                 defaultReset, northTxRst, northRxRst);
 AvalonCCIfc southCC <- mkAvalonStreamConverter(linkSouth.avalonMac,
                                                defaultClock, southTxClk, southRxClk,
                                                defaultReset, southTxRst, southRxRst);

  function DRAMExtIfc getDRAMExtIfc(DRAM dram) = dram.external;
  // interface dramIfcs = map(getDRAMExtIfc, drams);
  interface jtagIfc = uart.jtagAvalon;
  interface northMac = replicate(northCC.external);
  interface southMac = replicate(southCC.external);
  `endif

endmodule
