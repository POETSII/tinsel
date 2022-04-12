// SPDX-License-Identifier: BSD-2-Clause
package Mac;

// Wrapper for Altera 10G Ethernet MAC.
//
// There are three aims of this wrapper:
//   1. Convert between avalon and bluespec interfaces.
//   2. Filter out erroneous packets (e.g. that fail the CRC check).
//   3. Provide a convenient point to implement simulation behaviour.

// =============================================================================
// Imports
// =============================================================================

import Interface    :: *;
import ConfigReg    :: *;
import Util         :: *;
import Queue        :: *;
import BlockRam     :: *;
import Socket       :: *;
import Vector       :: *;

// =============================================================================
// Types
// =============================================================================

typedef struct {
  Bool start;    // Mark start of packet
  Bool stop;     // Mark end of packet
  Bit#(64) data; // Payload
} MacBeat deriving (Bits);

function MacBeat macBeat(Bool start, Bool stop, Bit#(64) data) =
  MacBeat { start: start, stop: stop, data: data };

// =============================================================================
// Interfaces
// =============================================================================

(* always_ready, always_enabled *)
interface AvalonMac;
  // TX connection to 10G MAC
  method Bit#(64) source_data;
  method Bool source_valid;
  method Bool source_startofpacket;
  method Bool source_endofpacket;
  method Bit#(1) source_error;
  method Bit#(3) source_empty;
  method Action source(Bool source_ready);
  // RX connection to 10G MAC
  method Bool sink_ready;
  method Action sink(Bit#(64) sink_data, Bool sink_valid,
                       Bool sink_startofpacket, Bool sink_endofpacket,
                         Bit#(6) sink_error, Bit#(3) sink_empty);
endinterface

interface Mac;
  // Avalon streaming interface
  `ifndef SIMULATE
  interface AvalonMac avalonMac;
  `endif
  // Connections to user logic
  interface Out#(MacBeat) toUser;
  interface In#(MacBeat) fromUser;
endinterface

// =============================================================================
// Receive Buffer
// =============================================================================

// We'd like to drop erroneous packets (e.g. due to CRC error or
// buffer overflow), but errors are only reported on the last beat of
// a packet so we need a receive buffer.  The receive buffer is a
// queue with two back pointers:
//
//   1. The "near back pointer" points one location past the last item
//      of a valid packet in the buffer.
//
//   2. The "far back pointer" points to the next location to be
//      enqueued.  When a valid end-of-packet is received, the
//      "near back pointer" gets updated to the "far back pointer".
//      When an invalid end-of-packet is received, the "far back
//      pointer" gets reset to the "near back pointer", i.e. the
//      packet is dropped.

typedef Bit#(`LogMacRecvBufferSize) ReceiveBufferPtr;

interface ReceiveBuffer;
  method Bool canEnq;
  method Action enq(MacBeat beat, Bit#(6) err);
  interface Out#(MacBeat) out;
endinterface

module mkReceiveBuffer (ReceiveBuffer);

  // Output port
  OutPort#(MacBeat) outPort <- mkOutPort;

  // Contents of the buffer
  BlockRamOpts bufferOpts = defaultBlockRamOpts;
  bufferOpts.registerDataOut = False;
  BlockRam#(ReceiveBufferPtr, MacBeat) buffer <-
    mkBlockRamOpts(bufferOpts);

  // Is the output of the buffer valid?
  Reg#(Bool) dataValid <- mkConfigReg(False);

  // Pointers
  Reg#(ReceiveBufferPtr) frontPtr <- mkConfigReg(0);
  Reg#(ReceiveBufferPtr) nearBackPtr <- mkConfigReg(0);
  Reg#(ReceiveBufferPtr) farBackPtr <- mkConfigReg(0);

  // Drop packet when this register is high
  Reg#(Bool) drop <- mkConfigReg(False);

  // Output port
  rule writeToOutputPort;
    ReceiveBufferPtr ptr = frontPtr;
    if (outPort.canPut && dataValid) begin
      outPort.put(buffer.dataOut);
      ptr = ptr+1;
    end
    buffer.read(ptr);
    frontPtr <= ptr;
    dataValid <= nearBackPtr != ptr;
  endrule

  method Action enq(MacBeat beat, Bit#(6) err);
    buffer.write(farBackPtr, beat);
    // Increment pointer
    ReceiveBufferPtr ptr = farBackPtr+1;
    // Look for CRC or overflow errors
    if (beat.stop && (err[1] == 1 || err[5] == 1 || err[0] == 1 || drop)) begin
      // Drop packet
      farBackPtr <= nearBackPtr;
      drop <= False;
    end else if (beat.stop) begin
      nearBackPtr <= ptr;
      farBackPtr <= ptr;
    end else begin
      if (farBackPtr+2 == nearBackPtr) begin
        // Drop oversized packet
        drop <= True;
      end else
        farBackPtr <= ptr;
    end
  endmethod

  method Bool canEnq = (farBackPtr+1) != frontPtr;

  interface out = outPort.out;

endmodule

// =============================================================================
// FPGA Implementation
// =============================================================================

`ifndef SIMULATE

module mkMac (Mac);

  // Ports
  InPort#(MacBeat) inPort <- mkInPort;
  OutPort#(MacBeat) outPort <- mkOutPort;

  // Receive buffer
  ReceiveBuffer buffer <- mkReceiveBuffer;

  interface AvalonMac avalonMac;
    // Avalon streaming source interface
    method Bit#(64) source_data = inPort.value.data;
    method Bool source_valid = inPort.canGet;
    method Bool source_startofpacket = inPort.value.start;
    method Bool source_endofpacket = inPort.value.stop;
    method Bit#(1) source_error = 0;
    method Bit#(3) source_empty = 0;
    method Action source(Bool source_ready);
      if (source_ready && inPort.canGet) inPort.get;
    endmethod

    // Avalon streaming sink interface
    method Bool sink_ready = buffer.canEnq;
    method Action sink(Bit#(64) sink_data, Bool sink_valid,
                         Bool sink_startofpacket, Bool sink_endofpacket,
                           Bit#(6) sink_error, Bit#(3) sink_empty);
      MacBeat beat;
      beat.data = sink_data;
      beat.start = sink_startofpacket;
      beat.stop = sink_endofpacket;
      if (sink_valid && buffer.canEnq) buffer.enq(beat, sink_error);
    endmethod
  endinterface

  // Interfaces
  interface fromUser = inPort.in;
  interface toUser = buffer.out;

endmodule

`endif

// =============================================================================
// Simulation
// =============================================================================

`ifdef SIMULATE

// A simulation MAC using UNIX domain sockets
// (Inserts padding and drops random packets)
module mkMac#(SocketId id) (Mac);

  // Ports
  OutPort#(MacBeat) outPort <- mkOutPort;
  InPort#(MacBeat) inPort <- mkInPort;

  // Count of number of beats received
  Reg#(Bit#(10)) count <- mkReg(0);

  // In this state, emit padding to comply with min ethernet frame size
  Reg#(Bool) emitPadding <- mkReg(False);

  // Random number, used to drop packets
  Reg#(Bit#(32)) random <- mkReg(0);

  // Drop packet while in this state
  Reg#(Bool) drop <- mkReg(False);

  // Generate a random number
  // (For random dropping of packets)
  rule genRandom;
    random <= random*1103515245 + 12345;
  endrule

  Reg#(Bool) linkrecvalive <- mkReg(False);


  // Consume input stream
  // (Insert padding and drop random packets)
  rule consume;
    MacBeat beat = inPort.value;
    if (count == 0) begin
      if (drop && inPort.canGet) begin
        // Drop packet
        inPort.get;
        if (beat.stop) drop <= False;
      end else if (random[31:28] == 0 && inPort.canGet) begin
        // Move to drop-packet state
        drop <= True;
        inPort.get;
      end else if (inPort.canGet) begin
        Bit#(72) tmp = zeroExtend(pack(beat));
        Bool ok <- socketPut(id, unpack(tmp));
        if (ok) begin
          // if (!linkrecvalive && beat.start && beat.data != 0) begin
          //   $display($time, "Sending flit on port ", id, " value %x", beat.data);
          //   // linkrecvalive <= True;
          // end
          // Receive first beat (start of packet)
          inPort.get;
          myAssert(beat.start, "MAC: missing start-of-packet");
          count <= 1;
        end
      end
    end else if (count > 0) begin
      if (emitPadding) begin
        // Insert padding
        MacBeat outBeat = macBeat(False, count == 7, 0);
        Bit#(72) tmp = zeroExtend(pack(outBeat));
        Bool ok <- socketPut(id, unpack(tmp));
        if (ok) begin
          if (count == 7) begin
            emitPadding <= False;
            count <= 0;
          end else
            count <= count+1;
        end
      end else if (inPort.canGet) begin
        // Receive remaining beats
        MacBeat outBeat = beat;
        if (beat.stop && count < 7) outBeat.stop = False;
        Bit#(72) tmp = zeroExtend(pack(outBeat));
        Bool ok <- socketPut(id, unpack(tmp));
        if (ok) begin
          inPort.get;
          if (beat.stop && count < 7) begin
            count <= count+1;
            emitPadding <= True;
          end else if (beat.stop) begin
            count <= 0;
          end else begin
            count <= count+1;
          end
        end
      end
    end
  endrule

  Reg#(Bool) linksendalive <- mkReg(False);

  // Produce output stream
  rule produce (outPort.canPut);
    Maybe#(Vector#(9, Bit#(8))) m <- socketGet(id);
    if (isValid(m)) begin

      // if (!linksendalive) begin
      //   $display($time, "Recvd flit from port ", id);
      //   linksendalive <= True;
      // end

      Bit#(72) tmp = pack(fromMaybe(?, m));
      outPort.put(unpack(truncate(tmp)));
    end
  endrule

  // Interfaces
  interface fromUser = inPort.in;
  interface toUser = outPort.out;
endmodule


// A simple loopback MAC
// (Inserts padding, drops random packets, and inserts latency)
module mkMacLoopback (Mac);

  // Ports
  OutPort#(MacBeat) outPort <- mkOutPort;
  InPort#(MacBeat) inPort <- mkInPort;

  // Buffer to introduce latency
  SizedQueue#(`MacLatency, MacBeat) buffer <-
    mkUGShiftQueueCore(QueueOptFmax);

  // Count of number of 64-bit words received
  Reg#(Bit#(10)) count <- mkReg(0);

  // In this state, emit padding to comply with min ethernet frame size
  Reg#(Bool) emitPadding <- mkReg(False);

  // Random number, used to drop packets
  Reg#(Bit#(32)) random <- mkReg(0);

  // Drop packet while in this state
  Reg#(Bool) drop <- mkReg(False);

  // Fill buffer
  rule introduceLatency (inPort.canGet && buffer.notFull);
    inPort.get;
    buffer.enq(inPort.value);
  endrule

  // Generate a random number
  // (For random dropping of packets)
  rule genRandom;
    random <= random*1103515245 + 12345;
  endrule

  rule loopback;
    MacBeat beat = buffer.dataOut;
    if (count == 0) begin
      if (drop && buffer.canDeq) begin
        // Drop packet
        buffer.deq;
        if (beat.stop) drop <= False;
      end else if (random[31:28] == 0 && buffer.canDeq) begin
        // Move to drop-packet state
        drop <= True;
        buffer.deq;
      end else if (buffer.canDeq && outPort.canPut) begin
        // Receive first beat (start of packet)
        buffer.deq;
        myAssert(beat.start, "Loopback MAC: missing start-of-packet");
        outPort.put(beat);
        count <= 1;
      end
    end else if (count > 0) begin
      if (emitPadding) begin
        // Insert padding
        if (outPort.canPut) begin
          outPort.put(macBeat(False, count == 7, 0));
          if (count == 7) begin
            emitPadding <= False;
            count <= 0;
          end else
            count <= count+1;
        end
      end else if (buffer.canDeq && outPort.canPut) begin
        // Receive remaining beats
        buffer.deq;
        if (beat.stop && count < 7) begin
          beat.stop = False;
          count <= count+1;
          emitPadding <= True;
        end else if (beat.stop) begin
          count <= 0;
        end else begin
          count <= count+1;
        end
        outPort.put(beat);
      end
    end
  endrule

  // Interfaces
  interface fromUser = inPort.in;
  interface toUser = outPort.out;
endmodule

`endif

// =============================================================================
// MAC Switch
// =============================================================================

function AvalonMac macMux(Bool sel, AvalonMac macA, AvalonMac macB) =
  interface AvalonMac;
    method source_data =
      sel ? macB.source_data : macA.source_data;
    method source_valid =
      sel ? macB.source_valid : macA.source_valid;
    method source_startofpacket =
      sel ? macB.source_startofpacket : macA.source_startofpacket;
    method Bool source_endofpacket =
      sel ? macB.source_endofpacket : macA.source_endofpacket;
    method Bit#(1) source_error =
      sel ? macB.source_error : macA.source_error;
    method Bit#(3) source_empty =
      sel ? macB.source_empty : macA.source_empty;
    method Action source(Bool source_ready);
      if (sel) macB.source(source_ready);
      else macA.source(source_ready);
    endmethod
    method Bool sink_ready =
      sel ? macB.sink_ready : macA.sink_ready;
    method Action sink(Bit#(64) sink_data, Bool sink_valid,
                         Bool sink_startofpacket, Bool sink_endofpacket,
                           Bit#(6) sink_error, Bit#(3) sink_empty);
      if (sel)
        macB.sink(sink_data, sink_valid, sink_startofpacket,
                  sink_endofpacket, sink_error, sink_empty);
      else
        macA.sink(sink_data, sink_valid, sink_startofpacket,
                  sink_endofpacket, sink_error, sink_empty);
    endmethod
  endinterface;

endpackage
