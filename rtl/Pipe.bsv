// Copyright (c) Matthew Naylor

package Pipe;

// Access named pipes on the file system in simulation.

import Vector :: *;

// Pipe identifier
typedef Bit#(32) PipeId;

// Pipe ids used in tinsel
PipeId uartPipe  = 0;
PipeId pciePipe  = 0;
PipeId northPipe = 1;
PipeId southPipe = 2;
PipeId eastPipe  = 3;
PipeId westPipe  = 4;

`ifdef SIMULATE

// Imports from C
// --------------

import "BDPI" function ActionValue#(Bit#(32)) pipeGet8(PipeId id);

import "BDPI" function ActionValue#(Bool) pipePut8(PipeId id, Bit#(8) b);

import "BDPI" function ActionValue#(Bit#(n))
  pipeGetN(PipeId id, Bit#(32) nbytes);

import "BDPI" function ActionValue#(Bool)
  pipePutN(PipeId id, Bit#(32) nbytes, Bit#(n) b);

// Wrappers
// --------

function ActionValue#(Maybe#(Vector#(n, Bit#(8)))) pipeGet(PipeId id) =
  actionvalue
    Bit#(TMul#(TAdd#(n, 1), 8)) tmp <-
      pipeGetN(id, fromInteger(valueOf(n)));
    Vector#(TAdd#(n, 1), Bit#(8)) res = unpack(tmp);
    if (res[valueOf(n)] == 0)
      return Valid(init(res));
    else
      return Invalid;
  endactionvalue;
  
function ActionValue#(Bool) pipePut(PipeId id, Vector#(n, Bit#(8)) data) =
  pipePutN(id, fromInteger(valueOf(n)), pack(data));

`endif
endpackage
