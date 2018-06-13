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
import BRAMCore  :: *;

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

// In simulation, use a BSV block RAM
module mkDualInstrMem#(InstrMemClient clientA, InstrMemClient clientB) (Empty);
  // Instruction memory
  BRAM_DUAL_PORT#(InstrIndex, Bit#(32)) instrMem <-
    mkBRAMCore2Load(2**`LogInstrsPerCore, False, "InstrMem.hex", False);

  // Connect to clients
  rule connectA;
    instrMem.a.put(clientA.write, clientA.addr, clientA.writeData);
    clientA.resp(instrMem.a.read);
  endrule

  rule connectB;
    instrMem.b.put(clientB.write, clientB.addr, clientB.writeData);
    clientB.resp(instrMem.b.read);
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
