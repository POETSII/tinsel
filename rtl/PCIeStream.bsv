// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor
//
// This module provides a bidirectional host<->FPGA stream abstraction
// on top of PCIe.  Two double-buffers residing in host memory are
// used to transfer data in each direction.  The FPGA uses DMA to
// access the buffers efficiently. The buffers are managed using a set
// of control/status registers (CSRs) accessible via a PCIe BAR:
//
// CSR        Offset    Host Access    Description
// -------    ------    -----------    -----------
// AddrRxA    0         W              Address of RxA buffer in host memory
// AddrRxB    16        W              Address of RxB buffer in host memory
// AddrTxA    32        W              Address of TxA buffer in host memory
// AddrTxB    48        W              Address of TxB buffer in host memory
// LenRxA     64        R/W            Num 128-bit words in RxA buffer
// LenRxB     80        R/W            Num 128-bit words in RxB buffer
// LenTxA     96        R/W            Num 128-bit words in TxA buffer
// LenTxB     112       R/W            Num 128-bit words in TxB buffer
// Enable     128       R/W            Enable/disable the double-buffers
// -          144       -              -
// Inflight   160       R              Are there any inflight requests?
// Reset      176       W              Trigger board reset
//
// Semantics of the Rx double-buffer from host viewpoint:
//  * When the host wants to read, it waits until LenRxA is non-zero.
//  * When the host has finished reading, it sets LenRxA to zero.
//  * Ditto for LenRxB.
//  * LenRxA and LenRxB form a double-buffer: when the host has
//    finished reading the RxA buffer, it moves on to the RxB
//    buffer, and vice-versa.
//
// Semantics of the Tx doublebuffer from the host PC viewpoint:
//  * When the host wants to write, it waits until LenTxA is zero.
//  * When the host has finished writing, it sets LenTxA to non-zero.
//  * Ditto for LenTxB.
//  * LenTxA and LenTxB form a double-buffer: when the host has
//    finished writing the TxA buffer, it moves on to the TxB
//    buffer, and vice-versa.

// =============================================================================
// Imports
// =============================================================================

import Queue     :: *;
import Interface :: *;
import BlockRam  :: *;
import DReg      :: *;
import ConfigReg :: *;
import Util      :: *;
import Vector    :: *;
import Socket    :: *;

// =============================================================================
// Constants
// =============================================================================

// Number of 128-bit words in each Rx buffer
`define LogBufferSize 16 /* 1MByte */

// =============================================================================
// Types
// =============================================================================

typedef Bit#(`LogBufferSize) BufferIndex;
typedef Bit#(TAdd#(`LogBufferSize, 1)) BufferLen;

// Type for requests on the host bus
typedef enum {HostWriteOp, HostReadOp, HostFlushOp} HostOp deriving (Eq, Bits);
typedef struct {
  HostOp    op;     // Operation
  Bit#(64)  addr;   // Address in host memory
  Bit#(128) data;   // Data to write
  Bit#(4)   burst;  // Size of burst
  Bit#(1)   buffer; // Buffer to flush
  BufferLen count;  // Number of datums in buffer
} HostReq deriving (Bits);

// =============================================================================
// Interfaces
// =============================================================================

// Avalon memory-mapped BAR
(* always_ready, always_enabled *)
interface PCIeBAR;
  method Action s(
    Bit#(128) writedata,
    Bit#(4) address,
    Bool read,
    Bool write,
    Bit#(16) byteenable
  );
  method Bit#(128) s_readdata;
  method Bool s_readdatavalid;
  method Bool s_waitrequest;
endinterface

// Avalon memory-mapped host-bus master
(* always_ready, always_enabled *)
interface PCIeHostBus;
  method Action m(
    Bit#(128) readdata,
    Bool readdatavalid,
    Bool waitrequest
  );
  method Bit#(128) m_writedata;
  method Bit#(64) m_address;
  method Bool m_read;
  method Bool m_write;
  method Bit#(4) m_burstcount;
  method Bit#(16) m_byteenable;
endinterface

// External interface
interface PCIeStreamExt;
  // Interface to the BAR
  interface PCIeBAR controlBAR;
  // Interface to host PCIe bus
  // (Use for DMA to/from host memory)
  interface PCIeHostBus hostBus;
  // Reset request
  (* always_ready, always_enabled *)
  method Bool resetReq;
endinterface

// Top-level interface
interface PCIeStream;
  `ifndef SIMULATE
  // Interface to PCIe core
  interface PCIeStreamExt external;
  `endif
  // Interface to application logic
  interface In#(Bit#(128)) streamIn;
  interface BOut#(Bit#(128)) streamOut;
  // Has the stream been enabled by the host?
  method Bool en;
endinterface

// =============================================================================
// Implementation (Synthesis)
// =============================================================================

`ifndef SIMULATE

module mkPCIeStream (PCIeStream);

  // Ports
  InPort#(Bit#(128)) inPort <- mkInPort;

  // Buffer size constant
  Integer bufferSize = 2 ** `LogBufferSize;

  // Is the stream enabled?
  Reg#(Bool) enabled <- mkConfigReg(False);

  // Rx double-buffer
  // ----------------
  //
  // Holds data to be read by the host PC

  // Which buffer is currently being read by the host?
  Reg#(Bit#(1)) rxReadBuffer <- mkConfigReg(0);

  // Which buffer is currently being written by the FPGA?
  Reg#(Bit#(1)) rxWriteBuffer <- mkConfigReg(0);

  // Number of 128-bit items in each buffer
  Reg#(BufferLen) lenRxA <- mkConfigReg(0);
  Reg#(BufferLen) lenRxB <- mkConfigReg(0);

  // Addresses of buffers in host memory
  Reg#(Bit#(64)) addrRxA <- mkConfigReg(0);
  Reg#(Bit#(64)) addrRxB <- mkConfigReg(0);

  // Tx double-buffer
  // ----------------
  //
  // Holds data written by the host PC

  // Which buffer is currently being read by the FPGA?
  Reg#(Bit#(1)) txReadBuffer <- mkConfigReg(0);

  // Number of 128-bit items in each buffer
  Reg#(BufferLen) lenTxA <- mkConfigReg(0);
  Reg#(BufferLen) lenTxB <- mkConfigReg(0);

  // Addresses of buffers in host memory
  Reg#(Bit#(64)) addrTxA <- mkConfigReg(0);
  Reg#(Bit#(64)) addrTxB <- mkConfigReg(0);

  // Board reset
  Reg#(Bool) resetReg <- mkConfigReg(False);
  PulseWire resetWire <- mkPulseWire;

  // Avalon-MM state & wires
  // -----------------------

  // Avalon-MM state
  Reg#(Bit#(64))  ctrlReadData      <- mkConfigReg(0);
  Reg#(Bool)      ctrlReadDataValid <- mkDReg(False);
  Reg#(Bit#(4))   burstCount        <- mkConfigReg(1);
  Reg#(Bool)      waitRequest       <- mkConfigReg(True);

  // Avalon-MM wires
  PulseWire setLenRxA   <- mkPulseWire;
  PulseWire setLenRxB   <- mkPulseWire;
  PulseWire resetLenRxA <- mkPulseWire;
  PulseWire resetLenRxB <- mkPulseWire;
  PulseWire setLenTxA   <- mkPulseWire;
  PulseWire setLenTxB   <- mkPulseWire;
  PulseWire resetLenTxA <- mkPulseWire;
  PulseWire resetLenTxB <- mkPulseWire;
  Wire#(BufferLen) newLenTx <- mkDWire(?);

  // Queues
  // ------

  // Stream input buffer & length
  SizedQueue#(3, Bit#(128)) inBuffer <- mkUGSizedQueuePrefetch;
  Count#(4) inBufferLen <- mkCount(8);

  // Stream output buffer & length
  SizedQueue#(6, Bit#(128)) outBuffer <- mkUGSizedQueuePrefetch;
  Count#(7) outBufferLen <- mkCount(64);

  // Requests on the host bus
  Queue#(HostReq) hostReqs <- mkUGQueue;

  // Pending flush requests
  Queue1#(HostReq) pendingFlushes <- mkUGShiftQueue(QueueOptFmax);

  // Count of pending reads
  Count#(7) pendingReads <- mkCount(64);

  // Fill the input buffer
  // ---------------------

  rule fillInputBuffer;
    if (inPort.canGet && inBufferLen.notFull) begin
      inPort.get;
      inBuffer.enq(inPort.value);
      inBufferLen.inc;
    end
  endrule

  // Fill the Rx buffers
  // -------------------

  // In this mode we fill the Rx buffers
  Reg#(Bool) fillRxMode <- mkConfigReg(False);

  // Number of idle cycles used to determine when to make a underfull
  // buffer available to the host.
  Reg#(Bit#(5)) rxIdle <- mkConfigReg(0);

  // Count the number of words written to the buffer
  Reg#(BufferLen) rxCount <- mkConfigReg(0);

  // Is there a burst-write in progress?
  Reg#(Bool) burstInProgress <- mkConfigReg(False);

  rule fillRxBuffers (fillRxMode && (enabled || burstInProgress));
    // Base address for Rx write buffer
    Bit#(64) baseAddr = rxWriteBuffer == 0 ? addrRxA : addrRxB;
    // Time to advance the write buffer?
    Bool advance = !burstInProgress &&
                     (rxCount == fromInteger(bufferSize) ||
                        (rxIdle == ~0 && rxCount != 0));
    // If we've just finished a burst write then
    // give control to the consumeTxBuffers rule
    if (burstInProgress && burstCount == 1) begin
      burstInProgress <= False;
      fillRxMode <= False;
    end else if (rxWriteBuffer == rxReadBuffer && advance) begin
      // When the host is ready to read the write buffer, and the
      // write buffer is ready to be advanced, try to advance the
      // write buffer and issue a flush request.
      if (hostReqs.notFull && pendingFlushes.notFull) begin
        HostReq req = ?;
        req.op = HostFlushOp;
        req.addr = baseAddr;
        req.buffer = rxWriteBuffer;
        req.count = rxCount;
        req.burst = 1;
        hostReqs.enq(req);
        pendingFlushes.enq(req);
        rxWriteBuffer <= rxWriteBuffer+1;
        rxCount <= 0;
        rxIdle <= 0;
        fillRxMode <= False;
      end
    end else if (inBuffer.canDeq && rxCount != fromInteger(bufferSize)) begin
      // Otherwise, write item(s) to host memory
      if (hostReqs.notFull) begin
        inBuffer.deq;
        inBufferLen.dec;
        HostReq req = ?;
        req.op = HostWriteOp;
        req.addr = baseAddr + zeroExtend({rxCount, 4'b0000});
        req.data = inBuffer.dataOut;
        req.burst = burstCount;
        if (burstCount == 1) begin
          burstInProgress <= True;
          BufferLen left = fromInteger(bufferSize) - rxCount;
          req.burst = truncate(min(left, zeroExtend(inBufferLen.value)));
          burstCount <= req.burst;
        end else
          burstCount <= burstCount-1;
        hostReqs.enq(req);
        rxCount <= rxCount+1;
        rxIdle <= 0;
      end
    end else if (!burstInProgress) begin
      fillRxMode <= False;
      if (rxIdle != ~0) rxIdle <= rxIdle+1;
    end
  endrule

  // Use wires to update the lengths of the receive buffers
  rule rxLenUpdate;
    if (resetLenRxA)
      lenRxA <= 0;
    else if (setLenRxA)
      lenRxA <= pendingFlushes.dataOut.count;

    if (resetLenRxB)
      lenRxB <= 0;
    else if (setLenRxB)
      lenRxB <= pendingFlushes.dataOut.count;
  endrule

  // Consume the Tx buffers
  // ----------------------

  // Count the number of words read from the buffer
  Reg#(BufferLen) txCount <- mkConfigReg(0);

  rule consumeTxBuffers (enabled && !fillRxMode);
    // Base address for Tx read buffer
    Bit#(64) baseAddr = txReadBuffer == 0 ? addrTxA : addrTxB;
    // Size of buffer
    let len = txReadBuffer == 0 ? lenTxA : lenTxB;
    // Is there something to read?
    if (len != 0) begin
      // Have we read it all?
      if (txCount == len) begin
        // Advance to the next buffer
        txCount <= 0;
        txReadBuffer <= txReadBuffer+1;
        if (txReadBuffer == 0) resetLenTxA.send; else resetLenTxB.send;
        fillRxMode <= True;
      end else begin
        // Otherwise, read from host memory
        if (!pendingFlushes.notEmpty &&
               pendingReads.value < 56 &&
                 outBufferLen.value < 56) begin
          if (hostReqs.notFull) begin
            HostReq req = ?;
            req.op = HostReadOp;
            req.addr = baseAddr + zeroExtend({txCount, 4'b0000});
            req.burst = truncate(min(8, len-txCount));
            hostReqs.enq(req);
            pendingReads.incBy(zeroExtend(req.burst));
            outBufferLen.incBy(zeroExtend(req.burst));
            txCount <= txCount + zeroExtend(req.burst);
            fillRxMode <= True;
          end
        end else
          fillRxMode <= True;
      end
    end else
      fillRxMode <= True;
  endrule

  // Use wires to update the lengths of the transmit buffers
  rule txLenUpdate;
    if (resetLenTxA)
      lenTxA <= 0;
    else if (setLenTxA)
      lenTxA <= newLenTx;

    if (resetLenTxB)
      lenTxB <= 0;
    else if (setLenTxB)
      lenTxB <= newLenTx;
  endrule

  // Drive the waitrequest signal on the BAR
  // ---------------------------------------

  // High on first cycle after reset
  Reg#(Bool) init <- mkConfigReg(True);

  rule onFirstCycle (init);
    init <= False;
  endrule

  // Control waitRequest signal on BAR
  // (We want to keep waitRequest high during reset)
  rule updateWaitRequest;
    if (init)
      waitRequest <= False;
    else if (resetWire)
      waitRequest <= True;
  endrule


  // Interfaces
  // ----------

  interface PCIeStreamExt external;

    // Control BAR Slave
    interface PCIeBAR controlBAR;
      method Action s(
          Bit#(128) writedata,
          Bit#(4) address,
          Bool read,
          Bool write,
          Bit#(16) byteenable);
       if (! waitRequest) begin
        case (address[3:0])
          0: addrRxA <= truncate(writedata);
          1: addrRxB <= truncate(writedata);
          2: addrTxA <= truncate(writedata);
          3: addrTxB <= truncate(writedata);
          4: begin
               ctrlReadData <= zeroExtend(lenRxA);
               if (write) begin
                 resetLenRxA.send;
                 rxReadBuffer <= 1;
               end
             end
          5: begin
               ctrlReadData <= zeroExtend(lenRxB);
               if (write) begin
                 resetLenRxB.send;
                 rxReadBuffer <= 0;
               end
             end
          6: begin
               ctrlReadData <= zeroExtend(lenTxA);
               if (write) begin
                 setLenTxA.send;
                 newLenTx <= truncate(writedata);
               end
             end
          7: begin
               ctrlReadData <= zeroExtend(lenTxB);
               if (write) begin
                 setLenTxB.send;
                 newLenTx <= truncate(writedata);
               end
             end
          8: if (write) enabled <= unpack(writedata[0]);
          10: begin
                Bool inflight = pendingReads.value != 0 ||
                                  pendingFlushes.canDeq ||
                                    burstInProgress ||
                                      hostReqs.notEmpty;
                ctrlReadData <= zeroExtend(pack(inflight));
              end
          11: if (write) begin
                resetReg <= True;
                resetWire.send;
              end
        endcase
        ctrlReadDataValid <= read;
       end
      endmethod
      method Bit#(128) s_readdata = {ctrlReadData, ctrlReadData};
      method Bool s_readdatavalid = ctrlReadDataValid;
      method Bool s_waitrequest   = waitRequest;
    endinterface

    // Host bus master
    interface PCIeHostBus hostBus;
      method Action m(
        Bit#(128) readdata,
        Bool readdatavalid,
        Bool waitrequest
      );
        if (!waitrequest && hostReqs.canDeq) hostReqs.deq;
        if (readdatavalid) begin
          if (pendingReads.value == 0) begin
            if (pendingFlushes.dataOut.buffer == 0)
              setLenRxA.send;
            else
              setLenRxB.send;
            pendingFlushes.deq;
          end else begin
            pendingReads.dec;
            outBuffer.enq(readdata);
          end
        end
      endmethod
      method Bit#(128) m_writedata = hostReqs.dataOut.data;
      method Bit#(64) m_address = hostReqs.dataOut.addr;
      method Bool m_read = hostReqs.canDeq &&
                             hostReqs.dataOut.op != HostWriteOp;
      method Bool m_write = hostReqs.canDeq &&
                              hostReqs.dataOut.op == HostWriteOp;
      method Bit#(4) m_burstcount = hostReqs.dataOut.burst;
      method Bit#(16) m_byteenable = 16'hffff;
    endinterface

    method Bool resetReq = resetReg;
  endinterface

  // Interface to application logic
  interface In streamIn = inPort.in;
  interface BOut streamOut;
    method Action get = action outBuffer.deq; outBufferLen.dec; endaction;
    method Bool valid = outBuffer.canPeek && outBuffer.canDeq;
    method Bit#(128) value = outBuffer.dataOut;
  endinterface
  method Bool en = enabled;

endmodule

`else

// =============================================================================
// Implementation (Simulation)
// =============================================================================

module mkPCIeStream (PCIeStream);

  // Ports
  InPort#(Bit#(128)) inPort <- mkInPort;

  // Output queue
  Queue#(Bit#(128)) outQueue <- mkUGQueue;
 
  // Input rule
  rule in (inPort.canGet);
    Bool ok <- socketPut(pcieSocket, unpack(inPort.value));
    if (ok) inPort.get;
  endrule
 
  // Output rule
  rule out (outQueue.notFull);
    Maybe#(Vector#(16, Bit#(8))) m <- socketGet(pcieSocket);
    if (isValid(m)) outQueue.enq(pack(fromMaybe(?, m)));
  endrule

  // Interface to application logic
  interface In streamIn = inPort.in;
  interface BOut streamOut;
    method Action get = action outQueue.deq; endaction;
    method Bool valid = outQueue.canDeq;
    method Bit#(128) value = outQueue.dataOut;
  endinterface
  method Bool en = True;

endmodule

`endif
