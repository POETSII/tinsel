import JtagUart     :: *;
import ConfigReg :: *;
import Connectable::*;
import Interface::*;
import Clocks::*;
import DebugLink    :: *;
import Vector    :: *;


interface JtagUartAvalonDestination;
  method Action uart_in(Bit#(1) uart_address, Bit#(32) uart_writedata,
                        Bool uart_write, Bool uart_read);
  method Bool uart_waitrequest;
  method Bit#(32) uart_readdata;
endinterface

module mkSimJtagUart#(Vector#(n, Bit#(8)) input_stream) (JtagUartAvalonDestination);

  Reg#(Bit#(1)) addr_r <- mkConfigReg(0);
  Reg#(Bool) we_r <- mkConfigReg(False);
  Reg#(Bool) re_r <- mkConfigReg(False);
  Wire#(Bit#(32)) writedata_w <- mkWire();

  Reg#(Bit#(TAdd#(TLog#(n),1))) input_ctr <- mkReg(0);
  Reg#(Bit#(2)) requeststate <- mkConfigReg(0); // 0: no request 1: recv request 2: reply

  Reg#(Bit#(32)) configreg_0 <- mkConfigReg(0);
  Reg#(Bit#(32)) ctrl_r <- mkConfigReg(0);

  rule req_transition;
    if (requeststate == 1)
      requeststate <= 2;
    if (requeststate == 2)
      requeststate <= 0;
  endrule

  rule deque_read_data;
    if (addr_r == 0 && re_r && requeststate == 2 && input_ctr < fromInteger(valueOf(n)) ) begin
        input_ctr <= input_ctr + 1;
    end
  endrule

  rule set_reg_0;
    Bit#(32) newval = 0;
    newval[16] = input_ctr < fromInteger(valueOf(n)) ? 1 : 0; // read avail
    newval[15] = input_ctr < fromInteger(valueOf(n)) ? 1 : 0; // rvalid
    newval[7:0] = input_stream[input_ctr];
    configreg_0 <= newval;
  endrule

  rule writespace;
    ctrl_r[16] <= 1; // always have space to write
  endrule

  rule display_rule_w;
    if (addr_r == 0 && we_r == True && requeststate == 2)
      $display("uart sending t host %x ;", writedata_w);
  endrule

  rule display_rule_r;
    if (addr_r == 0 && re_r == True && requeststate == 2)
      $display("DUT read from host %x", configreg_0);
  endrule


  method Action uart_in(Bit#(1) uart_address, Bit#(32) uart_writedata,
                        Bool uart_write, Bool uart_read);
    re_r <= uart_read;
    we_r <= uart_write;
    addr_r <= uart_address;
    if ((uart_write || uart_read) && requeststate == 0)
      requeststate <= 1;

    writedata_w <= uart_writedata;
  endmethod

  method Bool uart_waitrequest = requeststate == 0 || requeststate == 1;

  method Bit#(32) uart_readdata();
    if (addr_r == 1 && re_r && requeststate <= 2) begin // control reg
      return ctrl_r;
    end else if (addr_r == 0 && re_r && requeststate <= 2) begin
      return configreg_0;
    end else begin
      return 0;
    end
  endmethod

endmodule

module mkShimDebugLinkClient(DebugLinkClient);

  interface In fromDebugLink;
    method Bool didPut = False;
    method Action tryPut(DebugLinkFlit v);
    endmethod
  endinterface

  interface Out toDebugLink;
    method Action tryGet();
    endmethod

    method Bool valid() = False;
    method DebugLinkFlit value = unpack(0);
  endinterface

endmodule

// module mkSimJtagUartTop(Empty);
//
//   Clock default_clock <- exposeCurrentClock();
//   Reset default_reset <- exposeCurrentReset();
//
//   Clock mgmt_clk <- mkAbsoluteClock(10, 200);// exposeCurrentClock();
//   Reset mgmt_reset <- mkInitialReset(2, clocked_by mgmt_clk); //exposeCurrentReset();
//
//
//   // Create JTAG UART instance
//   JtagUart uart <- mkJtagUart(mgmt_clk, mgmt_reset);
//   JtagUartAvalonDestination test <- mkSimJtagUart(clocked_by mgmt_clk, reset_by mgmt_reset);
//
//   // mkConnection(uart.jtagAvalon, test);
//   rule a;
//     uart.jtagAvalon.uart(test.uart_waitrequest, test.uart_readdata);
//   endrule
//
//   rule b;
//     test.uart_in(uart.jtagAvalon.uart_address, uart.jtagAvalon.uart_writedata,
//                 uart.jtagAvalon.uart_write, uart.jtagAvalon.uart_read);
//   endrule
//
//   Reg#(Bit#(8)) ctr <- mkReg(0);
//
//   rule write;
//     uart.jtagIn.tryPut(ctr);
//   endrule
//
//   rule count (uart.jtagIn.didPut);
//     ctr <= ctr+1;
//   endrule
//
//   // connectUsing(mkQueue, uart.jtagOut, uart.jtagIn);
//
//   // In simulation, display start-up message
//
//   rule displayStartup;
//     let t <- $time;
//     if (t == 0) begin
//       $display("\nSimulator started");
//       $dumpvars();
//     end
//   endrule
//
//   rule exit;
//     if (ctr == 100)
//       $finish();
//   endrule
//
// endmodule

module mkSimJtagUartTop(Empty);

  Clock default_clock <- exposeCurrentClock();
  Reset default_reset <- exposeCurrentReset();

  Clock mgmt_clk <- mkAbsoluteClock(10, 200);// exposeCurrentClock();
  Reset mgmt_reset <- mkInitialReset(2, clocked_by mgmt_clk); //exposeCurrentReset();


  // Create JTAG UART instance
  // JtagUart uart <- mkJtagUart(mgmt_clk, mgmt_reset);
  Vector#(1, DebugLinkClient) cores = newVector();
  cores[0] <- mkShimDebugLinkClient();

  Vector#(3, Bit#(8)) stim_input = newVector();
  stim_input[0] = 0;
  stim_input[1] = 0;
  stim_input[2] = 0;


  Bit#(4) localBoardId = 0;
  Reg#(Bit#(8)) temperature <- mkReg(0);
  DebugLink debugLink <- mkDebugLink(mgmt_clk, mgmt_reset, localBoardId, temperature, cores);
  JtagUartAvalonDestination test <- mkSimJtagUart(stim_input, clocked_by mgmt_clk, reset_by mgmt_reset);

  // mkConnection(uart.jtagAvalon, test);
  rule a;
    debugLink.jtagAvalon.uart(test.uart_waitrequest, test.uart_readdata);
  endrule

  rule b;
    test.uart_in(debugLink.jtagAvalon.uart_address, debugLink.jtagAvalon.uart_writedata,
                debugLink.jtagAvalon.uart_write, debugLink.jtagAvalon.uart_read);
  endrule

  // connectUsing(mkQueue, uart.jtagOut, uart.jtagIn);

  // In simulation, display start-up message

  rule displayStartup;
    let t <- $time;
    if (t == 0) begin
      $display("\nSimulator started");
      $dumpvars();
    end
  endrule

    Reg#(Bit#(32)) ctr <- mkReg(0);

    rule count;
      ctr <= ctr+1;
    endrule


  rule exit;
    if (ctr == 8000)
      $finish();
  endrule

endmodule
