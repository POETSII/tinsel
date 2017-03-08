package AvalonStream;

// =============================================================================
// Imports
// =============================================================================

import Interface :: *;
import ConfigReg :: *;
import Util      :: *;

// =============================================================================
// Types
// =============================================================================

typedef struct {
  Bool  start; // Mark start of packet
  Bool  stop;  // Mark end of packet
  dataT data;  // Payload
} AvalonBeat#(type dataT) deriving (Bits);

function AvalonBeat#(dataT) avalonBeat(Bool start, Bool stop, dataT data) =
  AvalonBeat { start: start, stop: stop, data: data };

// =============================================================================
// Interfaces
// =============================================================================

// Avalon streaming source interface
interface AvalonSource#(type dataT);
  (* always_ready *)
  method dataT source_data;
  (* always_ready *)
  method Bool source_valid;
  (* always_ready *)
  method Bool source_startofpacket;
  (* always_ready *)
  method Bool source_endofpacket;
  (* always_enabled *)
  method Action source(Bool source_ready);
endinterface

interface Source#(type dataT);
  interface AvalonSource#(dataT) avalonSource;
  interface In#(AvalonBeat#(dataT)) in;
endinterface

// Avalon streaming sink interface
interface AvalonSink#(type dataT);
  (* always_ready *)
  method Bool sink_ready;
  (* always_enabled *)
  method Action sink(dataT sink_data, Bool sink_valid, 
                       Bool sink_startofpacket, Bool sink_endofpacket);
endinterface

interface Sink#(type dataT);
  interface AvalonSink#(dataT) avalonSink;
  interface Out#(AvalonBeat#(dataT)) out;
endinterface

// =============================================================================
// Modules
// =============================================================================

module mkSource (Source#(dataT)) provisos(Bits#(dataT, dataTW));

  // Create input port
  InPort#(AvalonBeat#(dataT)) inPort <- mkInPort;

  // Avalon streaming source interface
  interface AvalonSource avalonSource;
    method dataT source_data = inPort.value.data;
    method Bool source_valid = inPort.canGet;
    method Bool source_startofpacket = inPort.value.start;
    method Bool source_endofpacket = inPort.value.stop;
    method Action source(Bool source_ready);
      if (source_ready && inPort.canGet) inPort.get;
    endmethod
  endinterface

  // Port interface
  interface In in = inPort.in;

endmodule

module mkSink (Sink#(dataT)) provisos(Bits#(dataT, dataTW));

  // Create output port
  OutPort#(AvalonBeat#(dataT)) outPort <- mkOutPort;

  // Avalon streaming source interface
  interface AvalonSink avalonSink;
    method Bool sink_ready = outPort.canPut;
    method Action sink(dataT sink_data, Bool sink_valid, 
                         Bool sink_startofpacket, Bool sink_endofpacket);
      AvalonBeat#(dataT) beat;
      beat.data = sink_data;
      beat.start = sink_startofpacket;
      beat.stop = sink_endofpacket;
      if (sink_valid && outPort.canPut) outPort.put(beat);
    endmethod
  endinterface

  // Port interface
  interface Out out = outPort.out;

endmodule

endpackage
