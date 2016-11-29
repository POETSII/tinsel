// Copyright (c) Matthew Naylor

package JtagUart;

// This is a wrapper for Altera's memory-mapped JTAG UART component
// that provides a stream-based input/output interface.

// =============================================================================
// Imports
// =============================================================================

import Interface :: *;
import ConfigReg :: *;
import Util      :: *;

// =============================================================================
// Interfaces
// =============================================================================

// Avalon memory-mapped interface to Altera's JTAG UART.
interface JtagUartAvalon;
  (* always_ready *)
  method Bit#(3)  uart_address;
  (* always_ready *)
  method Bit#(32) uart_writedata;
  (* always_ready *)
  method Bool     uart_write;
  (* always_ready *)
  method Bool     uart_read;
  (* always_enabled *)
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

module mkJtagUart (JtagUart);

  // Create input and output ports
  InPort#(Bit#(8)) inPort <- mkInPort;
  OutPort#(Bit#(8)) outPort <- mkOutPort;

  // This register is used to toggle between reading and writing
  Reg#(Bool) toggle <- mkConfigReg(False);

  // Current state of state machine
  Reg#(JtagUartState) state <- mkConfigReg(JTAG_IDLE);

  // Avalon memory-mapped interface
  interface JtagUartAvalon jtagAvalon;
    method Bit#(3) uart_address =
      state == JTAG_READ_DATA || state == JTAG_WRITE_DATA ? 0 : 4;

    method Bit#(32) uart_writedata = 
      zeroExtend(inPort.value);

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
            if (inPort.canGet && toggle)
              state <= JTAG_READ_WSPACE;
            else if (outPort.canPut)
              state <= JTAG_READ_DATA;
          end
        JTAG_READ_DATA:
          if (!uart_waitrequest) begin
            if (uart_readdata[15] == 1) begin
              outPort.put(uart_readdata[7:0]);
            end
            state <= JTAG_IDLE;
          end
        JTAG_READ_WSPACE:
          if (!uart_waitrequest)
            state <= uart_readdata[31:16] > 0 ? JTAG_WRITE_DATA : JTAG_IDLE;
        JTAG_WRITE_DATA:
          if (!uart_waitrequest) begin
            inPort.get;
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

// In simulation, bytes are read and written via named pipes on the
// filesystem, instead of via the JTAG UART.

`ifdef SIMULATE
import "BDPI" function ActionValue#(Bit#(32)) uartGetByte();
import "BDPI" function ActionValue#(Bool) uartPutByte(Bit#(8) b);

module mkJtagUart (JtagUart);

  InPort#(Bit#(8))  inPort  <- mkInPort;
  OutPort#(Bit#(8)) outPort <- mkOutPort;

  rule connect;
    if (inPort.canGet) begin
      Bool ok <- uartPutByte(inPort.value);
      if (ok) inPort.get;
    end
    if (outPort.canPut) begin
      Bit#(32) b <- uartGetByte();
      if (b[31] == 0) outPort.put(b[7:0]);
    end
  endrule

  interface jtagIn = inPort.in;
  interface jtagOut = outPort.out;
endmodule
`endif

endpackage
