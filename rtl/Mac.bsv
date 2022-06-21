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
  Bit#(512) data; // Payload
  UInt#(6) empty; // bytes from the LSB that do not contain valid data
} MacBeat deriving (Bits);

function MacBeat macBeat(Bool start, Bool stop, Bit#(512) data, UInt#(6) empty) =
  MacBeat { start: start, stop: stop, data: data, empty:empty };

// =============================================================================
// Interfaces
// =============================================================================

(* always_ready, always_enabled *)
interface AvalonMac;
  // TX connection to 10G MAC
  method Bit#(512) source_data;
  method Bool source_valid;
  method Bool source_startofpacket;
  method Bool source_endofpacket;
  method Bit#(1) source_error;
  method Bit#(6) source_empty;
  method Action source(Bool source_ready);
  // RX connection to 10G MAC
  method Bool sink_ready;
  method Action sink(Bit#(512) sink_data, Bool sink_valid,
                       Bool sink_startofpacket, Bool sink_endofpacket,
                         Bit#(6) sink_error, Bit#(6) sink_empty);
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
    method Bit#(512) source_data = inPort.value.data;
    method Bool source_valid = inPort.canGet;
    method Bool source_startofpacket = inPort.value.start;
    method Bool source_endofpacket = inPort.value.stop;
    method Bit#(1) source_error = 0;
    method Bit#(6) source_empty = pack(inPort.value.empty);
    method Action source(Bool source_ready);
      if (source_ready && inPort.canGet) inPort.get;
    endmethod

    // Avalon streaming sink interface
    method Bool sink_ready = buffer.canEnq;
    method Action sink(Bit#(512) sink_data, Bool sink_valid,
                         Bool sink_startofpacket, Bool sink_endofpacket,
                           Bit#(6) sink_error, Bit#(6) sink_empty);
      MacBeat beat;
      beat.data = sink_data;
      beat.start = sink_startofpacket;
      beat.stop = sink_endofpacket;
      beat.empty = unpack(sink_empty);
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
        $display("[mac::consume] dropping packet");
        // Move to drop-packet state
        if (beat.stop) begin
          inPort.get; // 512b packet: start and end can be on the same beat.
        end else begin
          drop <= True; // if multi-beat, drop the next beats as well.
          inPort.get;
        end
      end else if (inPort.canGet) begin
        Bit#(520) tmp = zeroExtend(pack(beat));
        Bool ok <- socketPut(id, unpack(tmp));
        if (ok) begin
          // if (!linkrecvalive && beat.start && beat.data != 0) begin
          //   $display($time, "Sending flit on port ", id, " value %x", beat.data);
          //   // linkrecvalive <= True;
          // end
          // Receive first beat (start of packet)
          inPort.get;
          myAssert(beat.start, "MAC: missing start-of-packet");
          if (!beat.stop) count <= 1;
        end
      end
    end else if (count > 0) begin
      if (inPort.canGet) begin
        // Receive remaining beats
        MacBeat outBeat = beat;
        if (beat.stop) outBeat.stop = False;
        Bit#(520) tmp = zeroExtend(pack(outBeat)); // pad the 512+sop+eop to a mul of 8
        Bool ok <- socketPut(id, unpack(tmp));
        if (ok) begin
          inPort.get;
          if (beat.stop) begin
            count <= 0;
          end else begin
            count <= count+1;
          end
        end
      end
    end
  endrule

  Reg#(Bool) linksendalive <- mkReg(False);
  Reg#(Bool) split_last_flit <- mkReg(False);
  Reg#(Bit#(520)) last_flit_partially_sent <- mkReg(0);

  // Produce output stream
  rule produce (outPort.canPut && !split_last_flit);
    Maybe#(Vector#(65, Bit#(8))) m <- socketGet(id);
    if (isValid(m)) begin
      Bit#(520) tmp = pack(fromMaybe(?, m));
      last_flit_partially_sent <= tmp;
      MacBeat beat = unpack(truncate(tmp));
      Bool dosplit = random[30:29] == 2'b11;
      split_last_flit <= dosplit;
      if (dosplit) begin
        beat.empty = 6'd32;
        // $display("[mac::produce] sending half flit");
      end
      outPort.put(beat);
    end
  endrule

  rule produce_secondhalf (split_last_flit);
    split_last_flit <= False; // return to normal operation
    MacBeat beat = unpack(truncate(last_flit_partially_sent));
    beat.empty = 6'd32;
    beat.data = beat.data >> 256;
    outPort.put(beat);
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

  // Count of number of 512-bit words received
  Reg#(Bit#(10)) count <- mkReg(0);

  // Random number, used to drop packets
  Reg#(Bit#(32)) random <- mkReg(0);

  // Drop packet while in this state
  Reg#(Bool) drop <- mkReg(False);

  // split into 2 flits in this state
  Reg#(Bool) split_last_flit <- mkReg(False);
  Reg#(MacBeat) last_flit_partially_sent <- mkReg(?);


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
      end else if (split_last_flit) begin
        split_last_flit <= False;
        beat.empty = 6'd32;
        beat.data = beat.data >> 256;
        outPort.put(beat);
        buffer.deq;
      end else if (random[31:28] == 0 && buffer.canDeq) begin
        // Move to drop-packet state
        if (!beat.stop) drop <= True;
        buffer.deq;
      end else if (random[31:28] == 4'b1111 && buffer.canDeq) begin
        // $display("[mkMacLoopback] splitting flit.");
        split_last_flit <= True;
        beat.empty = 6'd32;
        outPort.put(beat);
      end else if (buffer.canDeq && outPort.canPut) begin
        // Receive first beat (start of packet)
        buffer.deq;
        myAssert(beat.start, "Loopback MAC: missing start-of-packet");
        outPort.put(beat);
        if (!beat.stop) count <= 1;
      end
    end else if (count > 0) begin
      if (buffer.canDeq && outPort.canPut) begin
        // Receive remaining beats
        buffer.deq;
        if (beat.stop) begin
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
    method Bit#(6) source_empty =
      sel ? macB.source_empty : macA.source_empty;
    method Action source(Bool source_ready);
      if (sel) macB.source(source_ready);
      else macA.source(source_ready);
    endmethod
    method Bool sink_ready =
      sel ? macB.sink_ready : macA.sink_ready;
    method Action sink(Bit#(512) sink_data, Bool sink_valid,
                         Bool sink_startofpacket, Bool sink_endofpacket,
                           Bit#(6) sink_error, Bit#(6) sink_empty);
      if (sel)
        macB.sink(sink_data, sink_valid, sink_startofpacket,
                  sink_endofpacket, sink_error, sink_empty);
      else
        macA.sink(sink_data, sink_valid, sink_startofpacket,
                  sink_endofpacket, sink_error, sink_empty);
    endmethod
  endinterface;

endpackage
