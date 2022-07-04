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
// import FPU          :: *;
// import InstrMem     :: *;
// import NarrowSRAM   :: *;
// import IdleDetector :: *;
import Clocks   :: *;
import Util   :: *;
import StmtFSM :: * ;
import StringPack :: * ;
import BlockRam :: *;

`ifdef SIMULATE

typedef Empty DE10Ifc;
import "BDPI" function Bit#(32) getBoardId();

`else

interface DE10Ifc;
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

typedef UInt#(4) AddrA; // 2 A's
typedef UInt#(6) AddrB; // 16 B's
typedef Bit#(64) DataA;
typedef Bit#(16) DataB;


module mkDE10Top(DE10Ifc);


  // Create JTAG UART
  JtagUart uart <- mkJtagUart;
  InPort#(Bit#(8)) fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag <- mkOutPort;

  // Connect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  PrintableStringIfc start_msg <- mkPrintStr("Starting_reliable_link_tester.\n", toJtag);
  Reg#(Bit#(8)) inchar <- mkReg(0);


  // Default options
  BlockRamOpts opts =
    BlockRamOpts {
      readDuringWrite: DontCare,
      style: "AUTO",
      registerDataOut: False,
      initFile: Invalid
    };


  BlockRamTrueMixedBE#(AddrA, DataA, AddrB, DataB) bram <- mkBlockRamTrueMixedBEOpts_S10(opts);

  Reg#(AddrA) addr_a <- mkReg(0);
  Reg#(AddrB) addr_b <- mkReg(0);

  PrintableStringIfc zeroed_msg <- mkPrintStr("zeroed BRAM via port A.\n", toJtag);
  PrintableStringIfc read_zero_fail_msg <- mkPrintStr("port A read zero fail, addr.\n", toJtag);
  PrintableStringIfc read_zero_test_done_msg <- mkPrintStr("port A read all zero test complete.\n", toJtag);

  PrintableStringIfc read_zero_B_fail_msg <- mkPrintStr("port B read zero fail, addr.\n", toJtag);
  PrintableStringIfc read_zero_B_test_done_msg <- mkPrintStr("port B read all zero test complete.\n", toJtag);

  PrintableStringIfc write_B_byte_read_A_fail_zero <- mkPrintStr("port A read zero fail after writing to B.\n", toJtag);
  PrintableStringIfc write_B_byte_read_A_fail_F <- mkPrintStr("port A read F fail after writing to B.\n", toJtag);

  PrintableStringIfc write_B_read_A_done <- mkPrintStr("write partial B read A pass.\n", toJtag);


  Stmt test_BRAM_stmts = seq
    for (addr_a<=0; addr_a<1<<valueOf(SizeOf#(AddrA)); addr_a<=addr_a+1) seq
      bram.putA(True, addr_a, 0);
    endseq
    delay(1);
    zeroed_msg.show();
    await(zeroed_msg.done);

    for (addr_a<=0; addr_a<1<<valueOf(SizeOf#(AddrA)); addr_a<=addr_a+1) seq
      bram.putA(False, addr_a, 0);
      delay(1);
      if (bram.dataOutA != 0) seq
        read_zero_fail_msg.show;
        await(read_zero_fail_msg.done);
      endseq
    endseq

    read_zero_test_done_msg.show;
    await(read_zero_test_done_msg.done);

    for (addr_b<=0; addr_b<1<<valueOf(SizeOf#(AddrB)); addr_b<=addr_b+1) seq
      action
        Bit#(2) mask = ~0;
        bram.putB(False, addr_b, 0, mask );
      endaction
      delay(1);
      if (bram.dataOutB != 0) seq
        read_zero_B_fail_msg.show;
        await(read_zero_B_fail_msg.done);
      endseq
    endseq

    read_zero_B_test_done_msg.show;
    await(read_zero_B_test_done_msg.done);

    // write to the mask and check on port A

    action
      bram.putB(True, 2, 16'hFF, 2'b1 ); // write a single 0xF to addr 2 byte 1
      bram.putA(False, 0, 0); // and write the same address to port A so we can read it
    endaction

    par
      action
        bram.putA(False, 0, 0); // and write the same address to port A so we can read it
      endaction

      if (bram.dataOutA != 0) seq
        $display("%h", bram.dataOutA);
        write_B_byte_read_A_fail_zero.show;
        await(write_B_byte_read_A_fail_zero.done);
      endseq
    endpar

    if (bram.dataOutA != 64'h000000FF00000000) seq
      $display("%h", bram.dataOutA);
      write_B_byte_read_A_fail_F.show;
      await(write_B_byte_read_A_fail_F.done);
    endseq

    write_B_read_A_done.show;
    await(write_B_read_A_done.done);

  endseq;

  FSM test_BRAM <- mkFSM(test_BRAM_stmts);

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

    if (inchar == charToAscii("s")) seq
      test_BRAM.start();
      delay(5);
      await(test_BRAM.done);
    endseq

  endseq;

  FSM test_top <- mkFSM(test_top_stmts);

  rule run;
    // infinite loop
    test_top.start();
  endrule

  `ifndef SIMULATE
  interface jtagIfc = uart.jtagAvalon;
  `endif

endmodule
