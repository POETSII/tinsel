// Copyright (c) Matthew Naylor

package Pipe;

// Access named pipes on the file system in simulation.

// Pipe identifier
typedef Bit#(32) PipeId;

// Pipe ids used in tinsel
PipeId uartPipe  = 0;
PipeId northPipe = 1;
PipeId southPipe = 2;
PipeId eastPipe  = 3;
PipeId westPipe  = 4;

`ifdef SIMULATE

// Imports from C
import "BDPI" function ActionValue#(Bit#(32)) pipeGet8(PipeId id);
import "BDPI" function ActionValue#(Bool) pipePut8(PipeId id, Bit#(8) b);
import "BDPI" function ActionValue#(Bit#(80)) pipeGet72(PipeId id);
import "BDPI" function ActionValue#(Bool) pipePut72(PipeId id, Bit#(72) b);

`endif
endpackage
