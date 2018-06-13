// Copyright (c) Matthew Naylor

package InstrMem;

// Provides an instruction memory with two interfaces,
// allowing it to be shared by up to two cores.

// ============================================================================
// Imports
// ============================================================================

import BlockRam  :: *;
import RegFile   :: *;
import Assert    :: *;
import Util      :: *;
import ConfigReg :: *;

// An index to instruction memory
typedef Bit#(`LogInstrsPerCore) InstrIndex;

(* always_ready, always_enabled *)
interface InstrMemClient;
  method Bool write;
  method InstrIndex addr;
  method Bit#(32) writeData;
  method Action resp(Bit#(32) readData);
endinterface

// Single-port verison
// ===================

module mkInstrMem#(InstrMemClient client) (Empty);
  // Block RAM
  BlockRamOpts instrMemOpts = defaultBlockRamOpts;
  instrMemOpts.initFile = Valid("InstrMem");
  instrMemOpts.registerDataOut = False;
  BlockRam#(InstrIndex, Bit#(32)) instrMem <-
    mkBlockRamOpts(instrMemOpts);

  // Connect to client
  rule connect;
    if (client.write) instrMem.write(client.addr, client.writeData);
    else instrMem.read(client.addr);
    client.resp(instrMem.dataOut);
  endrule
endmodule

// Dual-port version
// =================

`ifdef SIMULATE

// In simulation, use a RegFile
module mkDualInstrMem#(InstrMemClient clientA, InstrMemClient clientB) (Empty);
  // Instruction memory
  RegFile#(InstrIndex, Bit#(32)) instrMem <- mkRegFileFullLoad("InstrMem");

  // Data out registers
  Reg#(Bit#(32)) dataA <- mkRegU;
  Reg#(Bit#(32)) dataB <- mkRegU;

  // Connect to clients
  rule connectA;
    if (clientA.write) instrMem.upd(clientA.addr, clientA.writeData);
    else dataA <= instrMem.sub(clientA.addr);
    clientA.resp(dataA);
  endrule

  rule connectB;
    if (clientB.write) instrMem.upd(clientB.addr, clientB.writeData);
    else dataB <= instrMem.sub(clientB.addr);
    clientB.resp(dataB);
  endrule
endmodule

`else

// On FPGA, use a true dual port block RAM
module mkDualInstrMem#(InstrMemClient clientA, InstrMemClient clientB) (Empty);
  // Instruction memory
  BlockRamOpts instrMemOpts = defaultBlockRamOpts;
  instrMemOpts.initFile = Valid("InstrMem");
  instrMemOpts.registerDataOut = False;
  BlockRamTrueMixed#(InstrIndex, Bit#(32),
                     InstrIndex, Bit#(32)) instrMem <- 
    mkBlockRamTrueMixedOpts(instrMemOpts);

  // Connect to clients
  rule connectA;
    instrMem.putA(clientA.write, clientA.addr, clientA.writeData);
    clientA.resp(instrMem.dataOutA);
  endrule

  rule connectB;
    instrMem.putB(clientB.write, clientB.addr, clientB.writeData);
    clientB.resp(instrMem.dataOutB);
  endrule

endmodule

`endif

endpackage
