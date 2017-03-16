// Copyright (c) Matthew Naylor

package Pipe;
`ifdef SIMULATE

// Access named pipes on the file system in simulation.

// Pipe identifier
typedef Bit#(32) PipeId;

// Pipe ids used in tinsel
PipeId uartPipe = 0;

// Imports from C
import "BDPI" function ActionValue#(Bit#(32)) pipeGet8(PipeId id);
import "BDPI" function ActionValue#(Bool) pipePut8(PipeId id, Bit#(8) b);

`endif
endpackage
