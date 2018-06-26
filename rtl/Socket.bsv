// Copyright (c) Matthew Naylor

package Socket;

// Access named sockets on the file system in simulation.

import Vector :: *;
import List   :: *;
import Util   :: *;

// Socket identifier
typedef Bit#(32) SocketId;

// Socket ids used in tinsel
SocketId             uartSocket  = 0;
Vector#(2, SocketId) northSocket = vector(1, 2);
Vector#(2, SocketId) southSocket = vector(3, 4);
Vector#(4, SocketId) eastSocket  = vector(5, 6, 7, 8);
Vector#(4, SocketId) westSocket  = vector(9, 10, 11, 12);
SocketId             pcieSocket  = 13;

`ifdef SIMULATE

// Imports from C
// --------------

import "BDPI" function ActionValue#(Bit#(32)) socketGet8(SocketId id);

import "BDPI" function ActionValue#(Bool) socketPut8(SocketId id, Bit#(8) b);

import "BDPI" function ActionValue#(Bit#(n))
  socketGetN(SocketId id, Bit#(32) nbytes);

import "BDPI" function ActionValue#(Bool)
  socketPutN(SocketId id, Bit#(32) nbytes, Bit#(n) b);

// Wrappers
// --------

function ActionValue#(Maybe#(Vector#(n, Bit#(8)))) socketGet(SocketId id) =
  actionvalue
    Bit#(TMul#(TAdd#(n, 1), 8)) tmp <-
      socketGetN(id, fromInteger(valueOf(n)));
    Vector#(TAdd#(n, 1), Bit#(8)) res = unpack(tmp);
    if (res[valueOf(n)] == 0)
      return Valid(init(res));
    else
      return Invalid;
  endactionvalue;
  
function ActionValue#(Bool) socketPut(SocketId id, Vector#(n, Bit#(8)) data) =
  socketPutN(id, fromInteger(valueOf(n)), pack(data));

`endif
endpackage
