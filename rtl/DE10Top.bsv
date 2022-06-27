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
// import Globals      :: *;
// import DRAM         :: *;
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
// import IdleDetector :: *;
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
  interface Vector#(`NumNorthSouthLinks, AvalonMac) northMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) southMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) eastMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) westMac;

  interface JtagUartAvalon jtagIfc;
  (* always_ready, always_enabled *)
  method Action setTemperature(Bit#(8) temp);

endinterface


`endif


interface PrintableStringIfc;
  method Action show;
  method Bool done;
endinterface

module mkPrintStr(String s, OutPort#(Bit#(8)) out, PrintableStringIfc ifc);

  List#(Char) s_l = stringToCharList(s);
  Reg#(UInt#(32)) s_len <- mkReg(fromInteger(stringLength(s)));
  Reg#(UInt#(32)) i <- mkReg(0);

  Stmt printStmt = seq
    i<=0;
    for (i<=0; i<s_len; i<=i+1) seq
      await(out.canPut); out.put(fromInteger(charToInteger(s_l[i])));
    endseq
  endseq;

  FSM fsm <- mkFSM(printStmt);

  method show = fsm.start;
  method done = fsm.done;
endmodule

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
  InPort#(Bit#(64)) fromLinkNorth <- mkInPort;
  OutPort#(Bit#(64)) toLinkNorth <- mkOutPort;
  InPort#(Bit#(64)) fromLinkSouth <- mkInPort;
  OutPort#(Bit#(64)) toLinkSouth <- mkOutPort;



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


  connectUsing(mkUGQueue, toLinkNorth.out, linkNorth.streamIn);
  connectDirect(linkNorth.streamOut, fromLinkNorth.in);
  connectUsing(mkUGQueue, toLinkSouth.out, linkSouth.streamIn);
  connectDirect(linkSouth.streamOut, fromLinkSouth.in);


  `ifdef SINGLE_BOARD
  // in simulation, we may want to connect north and south on a single board with a loopback cable...?
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
  `endif // SIMULATE_INTERNALLOOPBACK

  `else // not SIMULATE
  ReliableLink linkNorth <- mkReliableLink;
  ReliableLink linkSouth <- mkReliableLink;
  connectUsing(mkUGQueue, toLinkNorth.out, linkNorth.streamIn);
  connectDirect(linkNorth.streamOut, fromLinkNorth.in);
  connectUsing(mkUGQueue, toLinkSouth.out, linkSouth.streamIn);
  connectDirect(linkSouth.streamOut, fromLinkSouth.in);

  AvalonCCIfc northCC <- mkAvalonStreamConverter(linkNorth.avalonMac,
                                                defaultClock, northTxClk, northRxClk,
                                                 defaultReset, northTxRst, northRxRst);
  AvalonCCIfc southCC <- mkAvalonStreamConverter(linkSouth.avalonMac,
                                                defaultClock, southTxClk, southRxClk,
                                                defaultReset, southTxRst, southRxRst);


  `endif

  // Ports
  // Create DRAMs
  // Vector#(`DRAMsPerBoard, DRAM) drams;
  // for (Integer i = 0; i < `DRAMsPerBoard; i=i+1)
  //   drams[i] <- mkDRAM(fromInteger(i));

  // Connect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);



  Reg#(Bit#(3))  error_rate_state <- mkReg(0); // 0: waiting; 1: running.
  Reg#(Bit#(3))  echo_state <- mkReg(0); // 0: waiting; 1: running.
  Reg#(Bit#(3))  mirror_state <- mkReg(0); // 0: waiting; 1: running.

  Reg#(Bit#(64)) recvCount <- mkReg(0);
  Reg#(Bit#(64)) sendCount <- mkReg(0);
  Reg#(Bit#(64)) timer <- mkReg(0);
  Reg#(Bit#(4))  displayCount <- mkReg(0);
  Reg#(Bit#(32)) numTimeouts <- mkReg(0);
  Reg#(Bit#(64)) lastReceived <- mkRegU;

  Reg#(Int#(32)) stdoutCtr <- mkReg(0);
  PrintableStringIfc start_msg <- mkPrintStr("Starting_reliable_link_tester.\n", toJtag);
  PrintableStringIfc startT_msg <- mkPrintStr("starting_test.\n", toJtag);
  PrintableStringIfc timer_msg <- mkPrintStr("timer:", toJtag);
  PrintableStringIfc sent_msg <- mkPrintStr("sent:", toJtag);
  PrintableStringIfc recv_msg <- mkPrintStr("recv:", toJtag);
  PrintableStringIfc timeouts_msg <- mkPrintStr("timeouts:", toJtag);

  PrintableStringIfc sent_one_msg <- mkPrintStr("sent the first flit\n", toJtag);
  PrintableStringIfc recv_one_msg <- mkPrintStr("correctly recv first flit\n", toJtag);

  Reg#(Bool) sent_inital_send_msg <- mkReg(False);
  Reg#(Bool) sent_inital_recv_msg <- mkReg(False);


  Reg#(Bit#(8)) inchar <- mkReg(0);

  /////////////////////// loopback plug link tester ////////////////////////////
  rule transmit (error_rate_state == 1 && sendCount <= numVals);
    if (toLinkNorth.canPut) begin
      toLinkNorth.put(sendCount);
      sendCount <= sendCount+1;
    end
  endrule

  rule send_inital_sent_msg (!sent_inital_send_msg && error_rate_state == 1 && toJtag.canPut && sendCount > 0);
    toJtag.put(charToAscii("S"));
    sent_inital_send_msg <= True;
  endrule

  rule send_inital_recv_msg (!sent_inital_recv_msg && error_rate_state == 1 && toJtag.canPut && recvCount > 0);
    toJtag.put(charToAscii("R"));
    sent_inital_recv_msg <= True;
  endrule


  rule receive (error_rate_state == 1);
    if (fromLinkSouth.canGet && toJtag.canPut) begin
      fromLinkSouth.get;
      lastReceived <= fromLinkSouth.value;
      if (recvCount == fromLinkSouth.value) begin
        if (recvCount == numVals) begin
          toJtag.put(charToAscii("D"));
          error_rate_state <= 0;
        end else begin
          recvCount <= recvCount+1;
        end
      end else begin
        // out of order or corrupted packet; bail.
        toJtag.put(charToAscii("X"));
        // error_rate_state <= 0;
      end
    end
  endrule

  rule incTimer (error_rate_state == 1);
    timer <= timer+1;
  endrule


  Stmt error_rate_test_stmts = seq

    startT_msg.show();
    await(startT_msg.done);

    action
      sendCount <= 0;
      timer <= 0;
      numTimeouts <= 0;
      sent_inital_send_msg <= False;
      sent_inital_recv_msg <= False;
    endaction

    error_rate_state <= 1;

    while (error_rate_state != 0) seq
      delay(1);
    endseq

    await(toJtag.canPut); toJtag.put(charToAscii((error_rate_state == 0) ? "0" : "1"));

    toJtag.put(10);

    timer_msg.show; await(timer_msg.done);

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
    sent_msg.show; await(sent_msg.done);

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
    recv_msg.show; await(recv_msg.done);



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
    timeouts_msg.show; await(timeouts_msg.done);


    numTimeouts <= linkNorth.numTimeouts;


    displayCount <= 0;
    while (displayCount < 7) action
      if (toJtag.canPut) begin
        Bit#(8) digit = hexDigit(truncateLSB(numTimeouts));
        toJtag.put(digit);
        numTimeouts <= numTimeouts << 4;
        if (displayCount == 7) begin
          displayCount <= 0;
          error_rate_state <= 0;
          recvCount <= 0;
          sendCount <= 0;
        end else begin
          displayCount <= displayCount+1;
        end
      end
    endaction
    toJtag.put(10);

  endseq;

  //////////////////////////// serial port echo tester /////////////////////////
  rule transmit_echo (echo_state == 1);
    if (fromJtag.canGet && toLinkNorth.canPut) begin
      toLinkNorth.put(extend(fromJtag.value));
      fromJtag.get;
    end
  endrule


  rule receive_echo (echo_state == 1);
    if (fromLinkNorth.canGet && toJtag.canPut) begin
      toJtag.put(truncate(fromLinkNorth.value));
      fromLinkNorth.get;
    end
  endrule

  Stmt test_echo_stmts = seq
    toJtag.put(charToAscii("E"));
    echo_state <= 1;
    await(echo_state == 0);
  endseq;

  FSM test_error_rate <- mkFSM(error_rate_test_stmts);

  FSM test_echo <- mkFSM(test_echo_stmts);

  //////////////////////////// internal mirroring //////////////////////////////
  rule southToNorthMirror (mirror_state == 1);
    if (fromLinkSouth.canGet && toLinkNorth.canPut) begin
      toLinkNorth.put(fromLinkNorth.value);
      fromLinkSouth.get;
    end
  endrule


  rule northToSouthMirror (mirror_state == 1);
    if (fromLinkNorth.canGet && toLinkSouth.canPut) begin
      toLinkSouth.put(fromLinkNorth.value);
      fromLinkNorth.get;
    end
  endrule
  //

  Stmt test_mirror_stmts = seq
    mirror_state <= 1;
    await(mirror_state == 0);
  endseq;

  // FSM test_error_rate <- mkFSM(error_rate_test_stmts);
  //
  FSM test_mirror <- mkFSM(test_mirror_stmts);



  Stmt test_top_stmts = seq
    start_msg.show();
    await(start_msg.done);

    await(fromJtag.canGet);
    action
      inchar <= fromJtag.value;
      fromJtag.get();
    endaction

    while (fromJtag.canGet) seq
      fromJtag.get();
    endseq

    if (inchar == charToAscii("l")) seq
      test_error_rate.start();
      delay(5);
      await(test_error_rate.done);
    endseq

    // echo typed chars
    if (inchar == charToAscii("e")) seq
      test_echo.start();
      delay(5);
      await(test_echo.done);
    endseq

    // mirror
    if (inchar == charToAscii("m")) seq
      test_mirror.start();
      delay(5);
      await(test_mirror.done);
    endseq

  endseq;

  FSM test_top <- mkFSM(test_top_stmts);

  rule run;
    // infinite loop
    test_top.start();
  endrule

  `ifndef SIMULATE

  interface jtagIfc = uart.jtagAvalon;
  interface northMac = replicate(northCC.external);
  interface southMac = replicate(southCC.external);
  `endif

endmodule
