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
  method Action source(Bool source_ready);
  // RX connection to 10G MAC
  method Bool sink_ready;
  method Action sink(Bit#(64) sink_data, Bool sink_valid,
                       Bool sink_startofpacket, Bool sink_endofpacket,
                         Bit#(6) sink_error);
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

typedef Bit#(`LogReceiveBufferSize) ReceiveBufferPtr;

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
    end else begin
      if (beat.stop) nearBackPtr <= ptr;
      if (nearBackPtr == farBackPtr+2) begin
        // Drop part of oversized packet
        farBackPtr <= nearBackPtr;
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
    method Action source(Bool source_ready);
      if (source_ready && inPort.canGet) inPort.get;
    endmethod

    // Avalon streaming sink interface
    method Bool sink_ready = buffer.canEnq;
    method Action sink(Bit#(64) sink_data, Bool sink_valid,
                         Bool sink_startofpacket, Bool sink_endofpacket,
                           Bit#(6) sink_error);
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
module mkMac (Mac);
  // For now, just implement a loopback.
  // (This includes padding, packet dropping, and latency insertion)

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

endpackage
