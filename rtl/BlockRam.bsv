// Copyright (c) Matthew Naylor, Jon Woodruff

package BlockRam;

// =======
// Imports
// =======

import BRAMCore :: *;

// ==========
// Interfaces
// ==========

// Basic dual-port block RAM with a read port and a write port
interface BlockRam#(type addr, type data);
  method Action read(addr a);
  method Action write(addr a, data x);
  method data dataOut;
endinterface

// This version provides byte-enables
interface BlockRamByteEn#(type addr, type data, numeric type dataBytes);
  method Action read(addr a);
  method Action write(addr a, data x, Bit#(dataBytes) be);
  method data dataOut;
endinterface

// Short-hand for byte-enables version
typedef BlockRamByteEn#(addr, data, TDiv#(SizeOf#(data), 8))
  BlockRamBE#(type addr, type data);

// =======
// Options
// =======

// For simultaneous read and write to same address,
// read old data or don't care?
typedef enum {
  DontCare, OldData 
} ReadDuringWrite deriving (Eq);

// Block RAM options
typedef struct {
  ReadDuringWrite readDuringWrite;

  // Is the data output registered? (i.e. two cycle read latency)
  Bool registerDataOut;

  // If Valid, initialise to file contents
  // If Invalid, initialise to all zeros
  Maybe#(String) initFile;
} BlockRamOpts;

// Default options
BlockRamOpts defaultBlockRamOpts =
  BlockRamOpts {
    readDuringWrite: OldData,
    registerDataOut: False,
    initFile:        Invalid
  };

// =========================
// Basic dual-port block RAM
// =========================

module mkBlockRam (BlockRam#(addr, data))
    provisos(Bits#(addr, awidth), Bits#(data, dwidth), Bounded#(addr));
  let ram <- mkBlockRamOpts(defaultBlockRamOpts); return ram;
endmodule

`ifdef SIMULATE

// BSV implementation using BRAMCore

module mkBlockRamOpts#(BlockRamOpts opts) (BlockRam#(addr, data))
         provisos(Bits#(addr, addrWidth),
                  Bits#(data, dataWidth),
                  Bounded#(addr));
  // For simulation, use a BRAMCore
  BRAM_DUAL_PORT#(addr, data) ram <-
      mkBRAMCore2Load(valueOf(TExp#(addrWidth)), opts.registerDataOut,
                        fromMaybe("Zero", opts.initFile) + ".hex", False);

  method Action read(addr address);
    ram.a.put(False, address, ?);
  endmethod

  method Action write(addr address, data val);
    ram.b.put(True, address, val);
  endmethod

  method data dataOut = ram.a.read;
endmodule

`else

// Altera implementation

import "BVI" AlteraBlockRam =
  module mkBlockRamOpts#(BlockRamOpts opts) (BlockRam#(addr, data))
         provisos(Bits#(addr, addrWidth),
                  Bits#(data, dataWidth));

    parameter ADDR_WIDTH     = valueOf(addrWidth);
    parameter DATA_WIDTH     = valueOf(dataWidth);
    parameter NUM_ELEMS      = valueOf(TExp#(addrWidth));
    parameter BE_WIDTH       = 1;
    parameter RD_DURING_WR = 
      opts.readDuringWrite == OldData ? "OLD_DATA" : "DONT_CARE";
    parameter DO_REG         =
      opts.registerDataOut ? "CLOCK0" : "UNREGISTERED";
    parameter INIT_FILE      =
      case (opts.initFile) matches
        tagged Invalid: return "UNUSED";
        tagged Valid .str: return (str + ".mif");
      endcase;

    method read(RD_ADDR) enable (RE) clocked_by(clk);
    method write(WR_ADDR, DI) enable (WE) clocked_by(clk);
    method DO dataOut;

    port BE clocked_by(clk) = 1'b1;

    default_clock clk(CLK, (*unused*) clk_gate);
    default_reset no_reset;

    schedule (dataOut) CF (dataOut);
    schedule (dataOut) CF (read);
    schedule (dataOut) CF (write);
    schedule (read)    CF (write);
    schedule (write)   C  (write);
    schedule (read)    C  (read);
  endmodule

`endif

// ===========================================
// Basic dual-port block RAM with byte-enables
// ===========================================

module mkBlockRamBE (BlockRamByteEn#(addr, data, dataBytes))
    provisos(Bits#(addr, awidth), Bits#(data, dwidth),
             Bounded#(addr), Mul#(dataBytes, 8, dwidth),
             Div#(dwidth, dataBytes, 8));
  let ram <- mkBlockRamBEOpts(defaultBlockRamOpts); return ram;
endmodule

`ifdef SIMULATE

// BSV implementation using BRAMCore

module mkBlockRamBEOpts#(BlockRamOpts opts)
         (BlockRamByteEn#(addr, data, dataBytes))
         provisos(Bits#(addr, addrWidth), Bits#(data, dataWidth),
                  Bounded#(addr), Mul#(dataBytes, 8, dataWidth),
                  Div#(dataWidth, dataBytes, 8));
  // For simulation, use a BRAMCore
  BRAM_DUAL_PORT_BE#(addr, data, dataBytes) ram <-
      mkBRAMCore2BELoad(valueOf(TExp#(addrWidth)), opts.registerDataOut,
                          fromMaybe("Zero", opts.initFile) + ".hex", False);

  method Action read(addr address);
    ram.a.put(0, address, ?);
  endmethod

  method Action write(addr address, data val, Bit#(dataBytes) be);
    ram.b.put(be, address, val);
  endmethod

  method data dataOut = ram.a.read;
endmodule

`else

// Altera implementation

import "BVI" AlteraBlockRam =
  module mkBlockRamBEOpts#(BlockRamOpts opts)
         (BlockRamByteEn#(addr, data, dataBytes))
         provisos(Bits#(addr, addrWidth), Bits#(data, dataWidth),
                  Bounded#(addr), Mul#(dataBytes, 8, dataWidth),
                  Div#(dataWidth, dataBytes, 8));

    parameter ADDR_WIDTH     = valueOf(addrWidth);
    parameter DATA_WIDTH     = valueOf(dataWidth);
    parameter NUM_ELEMS      = valueOf(TExp#(addrWidth));
    parameter BE_WIDTH       = valueOf(dataBytes);
    parameter RD_DURING_WR = 
      opts.readDuringWrite == OldData ? "OLD_DATA" : "DONT_CARE";
    parameter DO_REG         =
      opts.registerDataOut ? "CLOCK0" : "UNREGISTERED";
    parameter INIT_FILE      =
      case (opts.initFile) matches
        tagged Invalid: return "UNUSED";
        tagged Valid .x: return (x + ".mif");
      endcase;

    method read(RD_ADDR) enable (RE) clocked_by(clk);
    method write(WR_ADDR, DI, BE) enable (WE) clocked_by(clk);
    method DO dataOut;

    default_clock clk(CLK, (*unused*) clk_gate);
    default_reset no_reset;

    schedule (dataOut) CF (dataOut);
    schedule (dataOut) CF (read);
    schedule (dataOut) CF (write);
    schedule (read)    CF (write);
    schedule (write)   C  (write);
    schedule (read)    C  (read);
  endmodule

`endif

endpackage
