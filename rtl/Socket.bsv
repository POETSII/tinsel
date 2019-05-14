// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package Socket;

// Access named sockets on the file system in simulation.

import Vector :: *;
import List   :: *;
import Util   :: *;

// Socket identifier
typedef Bit#(32) SocketId;

// Socket ids used in tinsel
SocketId uartSocket  = 0;
SocketId pcieSocket  = 1;
Vector#(`NumNorthSouthLinks, SocketId) northSocket =
  Vector::map(fromInteger, genVectorFrom(4));
Vector#(`NumNorthSouthLinks, SocketId) southSocket =
  Vector::map(fromInteger, genVectorFrom(8));
Vector#(`NumEastWestLinks, SocketId) eastSocket =
  Vector::map(fromInteger, genVectorFrom(12));
Vector#(`NumEastWestLinks, SocketId) westSocket =
  Vector::map(fromInteger, genVectorFrom(16));

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
