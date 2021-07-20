// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package JtagUart;

// This is a wrapper for Altera's memory-mapped JTAG UART component
// that provides a stream-based input/output interface.

// =============================================================================
// Imports
// =============================================================================

import Clocks    :: *;
import FIFOF    :: *;
import Interface :: *;
import ConfigReg :: *;
import Util      :: *;
import Socket    :: *;

// =============================================================================
// Interfaces
// =============================================================================

// Avalon memory-mapped interface to Altera's JTAG UART.
(* always_ready, always_enabled *)
interface JtagUartAvalon;
  method Bit#(1)  uart_address;
  method Bit#(32) uart_writedata;
  method Bool     uart_write;
  method Bool     uart_read;
  method Action   uart(Bool uart_waitrequest,
                       Bit#(32) uart_readdata);
endinterface

// The interface provided by the wrapper
interface JtagUart;
`ifndef SIMULATE
  interface JtagUartAvalon jtagAvalon;
`endif
  interface In#(Bit#(8))   jtagIn;
  interface Out#(Bit#(8))  jtagOut;
endinterface

// =============================================================================
// Implementation
// =============================================================================

`ifndef SIMULATE

// State of the wrapper
typedef enum {
  JTAG_IDLE,
  JTAG_READ_DATA,   // Consume char from UART, if available
  JTAG_READ_WSPACE, // Read UART's control register to determine write space
  JTAG_WRITE_DATA   // Write char to UART's data register
} JtagUartState deriving (Bits, Eq);

module mkJtagUart (Clock axi_clk, Reset axi_rst, JtagUart ifc);

  // Create input and output ports
  // clocked by default
  InPort#(Bit#(8)) inPort <- mkInPort;
  OutPort#(Bit#(8)) outPort <- mkOutPort; // out means out of this module

  SyncFIFOIfc#(Bit#(8)) designToUartFIFO <- mkSyncFIFOFromCC(1, axi_clk);
  FIFOF#(Bit#(8)) designToUartFIFOUnGuarded <- mkUGFIFOF1(clocked_by axi_clk, reset_by axi_rst);
  SyncFIFOIfc#(Bit#(8)) uartToDesignFIFO <- mkSyncFIFOToCC(1, axi_clk, axi_rst);
  FIFOF#(Bit#(8)) uartToDesignFIFOUnGuarded <- mkUGFIFOF1(clocked_by axi_clk, reset_by axi_rst);

  rule uartToDesignTxfr;
    outPort.put(uartToDesignFIFO.first);
    uartToDesignFIFO.deq();
  endrule

  rule designToUartTxfr (inPort.canGet);
    designToUartFIFO.enq(inPort.value);
    inPort.get();
  endrule

  // UGFIFOs for rule scheduling. For the axi address, we, re, and data lines to
  // be driven in a single rule, we need to allow that rule to fire even if
  // we don't have the abilty to operate on both CC fifos.
  rule uartToDesignTxfrGuardCopy (uartToDesignFIFOUnGuarded.notEmpty);
    uartToDesignFIFOUnGuarded.deq();
    uartToDesignFIFO.enq(uartToDesignFIFOUnGuarded.first);
  endrule

  rule designToUartTxfrGuardCopy (designToUartFIFOUnGuarded.notFull);
    designToUartFIFO.deq();
    designToUartFIFOUnGuarded.enq(designToUartFIFO.first);
  endrule


  // This register is used to toggle between reading and writing
  // accessed by axi, so on the axi_clk domain
  Reg#(Bool) toggle <- mkConfigReg(False, clocked_by axi_clk, reset_by axi_rst);

  // Current state of state machine
  Reg#(JtagUartState) state <- mkConfigReg(JTAG_IDLE, clocked_by axi_clk, reset_by axi_rst);

  // Avalon memory-mapped interface
  interface JtagUartAvalon jtagAvalon;
    method Bit#(1) uart_address =
      state == JTAG_READ_DATA || state == JTAG_WRITE_DATA ? 0 : 1; // 32b symbol address

    method Bit#(32) uart_writedata =
      zeroExtend(designToUartFIFOUnGuarded.notEmpty ? designToUartFIFOUnGuarded.first : 0);

    method Bool uart_write =
      state == JTAG_WRITE_DATA;

    method Bool uart_read =
      state == JTAG_READ_DATA || state == JTAG_READ_WSPACE;

    method Action uart(Bool uart_waitrequest,
                       Bit#(32) uart_readdata);
      case (state)
        JTAG_IDLE:
          begin
            toggle <= !toggle;
            if (designToUartFIFOUnGuarded.notEmpty && toggle)
              state <= JTAG_READ_WSPACE;
            else if (uartToDesignFIFOUnGuarded.notFull)
              state <= JTAG_READ_DATA;
          end
        JTAG_READ_DATA:
          if (!uart_waitrequest) begin
            if (uart_readdata[15] == 1) begin
              uartToDesignFIFOUnGuarded.enq(uart_readdata[7:0]);
            end
            state <= JTAG_IDLE;
          end
        JTAG_READ_WSPACE:
          if (!uart_waitrequest)
            state <= (uart_readdata[31:16] > 0 && designToUartFIFOUnGuarded.notEmpty) ? JTAG_WRITE_DATA : JTAG_IDLE;
        JTAG_WRITE_DATA:
          if (!uart_waitrequest) begin
            designToUartFIFOUnGuarded.deq;
            state <= JTAG_IDLE;
          end
      endcase
    endmethod
  endinterface

  // Streaming interface
  interface In  jtagIn  = inPort.in;
  interface Out jtagOut = outPort.out;

endmodule

`endif

// =============================================================================
// Simulation
// =============================================================================

// In simulation, bytes are read and written via UNIX domain sockets.

`ifdef SIMULATE

module mkJtagUart (Clock axi_clk, Reset axi_rst, JtagUart ifc);

  InPort#(Bit#(8))  inPort  <- mkInPort;
  OutPort#(Bit#(8)) outPort <- mkOutPort;

  rule connect;
    if (inPort.canGet) begin
      Bool ok <- socketPut8(uartSocket, inPort.value);
      if (ok) inPort.get;
    end
    if (outPort.canPut) begin
      Bit#(32) b <- socketGet8(uartSocket);
      if (b[31] == 0) begin
        $display("JTAG sending byte ", b);
        outPort.put(b[7:0]);
      end
    end
  endrule

  interface jtagIn = inPort.in;
  interface jtagOut = outPort.out;
endmodule
`endif

endpackage
