// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor, Jon Woodruff

package BlockRam;

// =======
// Imports
// =======

import BRAMCore  :: *;
import Vector    :: *;
import Assert    :: *;
import ConfigReg :: *;

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


// True dual-port mixed-width block RAM
interface BlockRamTrueMixed#
            (type addrA, type dataA,
             type addrB, type dataB);
  // Port A
  method Action putA(Bool wr, addrA a, dataA x);
  method dataA dataOutA;
  // Port B
  method Action putB(Bool wr, addrB a, dataB x);
  method dataB dataOutB;
endinterface

// Non-mixed-width variant
typedef BlockRamTrueMixed#(addr, data, addr, data)
  BlockRamTrue#(type addr, type data);

// True dual-port mixed-width block RAM with byte-enables
// (Port B has the byte enables and must be smaller than port A)
interface BlockRamTrueMixedByteEn#
            (type addrA, type dataA,
             type addrB, type dataB,
             numeric type dataBBytes);
  // Port A
  method Action putA(Bool wr, addrA a, dataA x);
  method dataA dataOutA;
  // Port B
  method Action putB(Bool wr, addrB a, dataB x, Bit#(dataBBytes) be);
  method dataB dataOutB;
endinterface

// Short-hand for byte-enables version
typedef BlockRamTrueMixedByteEn#(
          addrA, dataA, addrB, dataB, TDiv#(SizeOf#(dataB), 8))
  BlockRamTrueMixedBE#(type addrA, type dataA, type addrB, type dataB);

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

  // Implementation style
  // Defaults to "AUTO", but can be "MLAB" or "M20K"
  String style;

  // Is the data output registered? (i.e. two cycle read latency)
  Bool registerDataOut;

  // If Valid, initialise to file contents
  // If Invalid, initialise to all zeros
  Maybe#(String) initFile;
} BlockRamOpts;

// Default options
BlockRamOpts defaultBlockRamOpts =
  BlockRamOpts {
    readDuringWrite: DontCare,
    //readDuringWrite: OldData,
    style: "AUTO",
    registerDataOut: True,
    initFile: Invalid
  };

// =========================
// Basic dual-port block RAM
// =========================

module mkBlockRam (BlockRam#(addr, data))
    provisos(Bits#(addr, awidth), Bits#(data, dwidth), Bounded#(addr));
  let ram <- mkBlockRamOpts(defaultBlockRamOpts); return ram;
endmodule

// BSV implementation using BRAMCore
module mkBlockRamOpts_SIMULATED#(BlockRamOpts opts) (BlockRam#(addr, data))
         provisos(Bits#(addr, addrWidth),
                  Bits#(data, dataWidth),
                  Bounded#(addr));
  // For simulation, use a BRAMCore
  let ram <-
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

// to support simulation, we need to define the imported BVI modules - but they should assert
`ifndef SIMULATE

import "BVI" AlteraBlockRam =
  module mkBlockRamOpts_ALTERA#(BlockRamOpts opts) (BlockRam#(addr, data))
         provisos(Bits#(addr, addrWidth),
                  Bits#(data, dataWidth));

    parameter ADDR_WIDTH     = valueOf(addrWidth);
    parameter DATA_WIDTH     = valueOf(TMul#(TDiv#(dataWidth, 8), 8));
    parameter NUM_ELEMS      = valueOf(TExp#(addrWidth));
    parameter BE_WIDTH       = valueOf(TDiv#(dataWidth, 8)); // sx10 has BE for all BRAMs
    parameter RD_DURING_WR =
      opts.readDuringWrite == OldData ? "OLD_DATA" : "DONT_CARE";
    parameter DO_REG         =
      opts.registerDataOut ? "CLOCK0" : "UNREGISTERED";
    parameter INIT_FILE      =
      case (opts.initFile) matches
        tagged Invalid: return "UNUSED";
        tagged Valid .str: return (str + ".mif");
      endcase;
    parameter DEV_FAMILY = `DeviceFamily;
    parameter STYLE = opts.style;

    method read(RD_ADDR) enable (RE) clocked_by(clk);
    method write(WR_ADDR, DI) enable (WE) clocked_by(clk);
    method DO dataOut;

    port BE clocked_by(clk) = ~0;

    default_clock clk(CLK, (*unused*) clk_gate);
    default_reset no_reset;

    schedule (dataOut) CF (dataOut);
    schedule (dataOut) CF (read);
    schedule (dataOut) CF (write);
    schedule (read)    CF (write);
    schedule (write)   C  (write);
    schedule (read)    C  (read);
  endmodule

`else

module mkBlockRamOpts_ALTERA#(BlockRamOpts opts) (BlockRam#(addr, data));
  staticAssert(False, "Altera BVI module used in sim enviroment.");
endmodule

`endif

`ifdef SIMULATE

module mkBlockRamOpts#(BlockRamOpts opts) (BlockRam#(addr, data))
         provisos(Bits#(addr, addrWidth),
                  Bits#(data, dataWidth),
                  Bounded#(addr));
  // For simulation, use a BRAMCore
  BlockRam#(addr, data) bram <- mkBlockRamOpts_SIMULATED(opts);
  return bram;
endmodule

`else

// Altera implementation
module mkBlockRamOpts#(BlockRamOpts opts) (BlockRam#(addr, data))
         provisos(Bits#(addr, addrWidth),
                  Bits#(data, dataWidth),
                  Bounded#(addr));
  // For simulation, use a BRAMCore
  BlockRam#(addr, data) bram <- mkBlockRamOpts_ALTERA(opts);
  return bram;
endmodule

`endif

// ===========================================
// Basic dual-port block RAM with byte-enables
// ===========================================

module mkBlockRamBE (BlockRamByteEn#(addr, data, dataBytes))
    provisos(Bits#(addr, awidth), Bits#(data, dwidth),
             Bounded#(addr), Mul#(dataBytes, 8, dwidth),
             Div#(dwidth, dataBytes, 8));
  BlockRamByteEn#(addr, data, dataBytes) ram <- mkBlockRamBEOpts(defaultBlockRamOpts); return ram;
endmodule

// BSV implementation using BRAMCore
module mkBlockRamBEOpts_SIMULATED#(BlockRamOpts opts)
         (BlockRamByteEn#(addr, data, dataBytes))
         provisos(Bits#(addr, addrWidth), Bits#(data, dataWidth),
                  Bounded#(addr), Mul#(dataBytes, 8, dataWidth),
                  Div#(dataWidth, dataBytes, 8));
  // For simulation, use a BRAMCore
  let ram <-
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

`ifndef SIMULATE

// Altera implementation
import "BVI" AlteraBlockRam =
  module mkBlockRamBEOpts_ALTERA#(BlockRamOpts opts)
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
    parameter DEV_FAMILY = `DeviceFamily;
    parameter STYLE = opts.style;

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

`else

module mkBlockRamBEOpts_ALTERA#(BlockRamOpts opts) (BlockRam#(addr, data));
  staticAssert(False, "Altera BVI module used in sim enviroment.");
endmodule

`endif


`ifdef SIMULATE

module mkBlockRamBEOpts#(BlockRamOpts opts)
         (BlockRamByteEn#(addr, data, dataBytes))
         provisos(Bits#(addr, addrWidth), Bits#(data, dataWidth),
                  Bounded#(addr), Mul#(dataBytes, 8, dataWidth),
                  Div#(dataWidth, dataBytes, 8));
  // For simulation, use a BRAMCore
  BlockRamByteEn#(addr, data, dataBytes) ram <- mkBlockRamBEOpts_SIMULATED(opts);
  return ram;
endmodule

`else

module mkBlockRamBEOpts#(BlockRamOpts opts)
         (BlockRamByteEn#(addr, data, dataBytes))
         provisos(Bits#(addr, addrWidth), Bits#(data, dataWidth),
                  Bounded#(addr), Mul#(dataBytes, 8, dataWidth),
                  Div#(dataWidth, dataBytes, 8));
  // For simulation, use a BRAMCore
  BlockRamByteEn#(addr, data, dataBytes) ram <- mkBlockRamBEOpts_ALTERA(opts);
  return ram;
endmodule

`endif



// ====================================
// True dual-port mixed-width block RAM
// ====================================

module mkBlockRamTrueMixed
         (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
                  Literal#(addrA), Literal#(addrB)

                  `ifdef Stratix10
                  , Add#(a__, TMul#(TDiv#(dataWidthB, 8), 8), TMul#(TDiv#(dataWidthA, 8), 8)),
                  Div#(TMul#(TDiv#(dataWidthA, 8), 8), TDiv#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8),
                  Div#(TMul#(TDiv#(dataWidthA, 8), 8), 8, TDiv#(dataWidthA, 8)),
                  Mul#(TDiv#(dataWidthB, 8), TExp#(aExtra), TDiv#(dataWidthA, 8)),
                  Add#(TDiv#(a__, 8), TDiv#(TMul#(TDiv#(dataWidthB, 8), 8), 8), TDiv#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                  Mul#(TExp#(aExtra), TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8), TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                  Mul#(TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8), 8, TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                  Div#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8), 8),
                  Add#(TDiv#(a__, 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8), 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8)),

                  Add#(b__, dataWidthA, TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                  Add#(b__, dataWidthB, TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8)),
                  Add#(TDiv#(c__, 8), TDiv#(dataWidthB, 8), TDiv#(dataWidthA, 8)),
                  Add#(c__, dataWidthB, dataWidthA)
                  `endif // Stratix10

                 );
  let ram <- mkBlockRamTrueMixedOpts(defaultBlockRamOpts); return ram;
endmodule

// BSV implementation using BlockRam.c routines.
typedef Bit#(64) BlockRamHandle;
import "BDPI" function ActionValue#(BlockRamHandle) createBlockRam(
  Bit#(32) sizeInBits);
import "BDPI" function Action blockRamWrite(
  BlockRamHandle handle, Bit#(m) addr, Bit#(n) data,
    Bit#(32) dataWidth, Bit#(32) addrWidth);
import "BDPI" function ActionValue#(Bit#(n)) blockRamRead(
  BlockRamHandle handle, Bit#(m) addr,
    Bit#(32) dataWidth, Bit#(32) addrWidth);

module mkBlockRamTrueMixedOpts_SIMULATE#(BlockRamOpts opts)
         (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA));
  // For simulation, use C interface
  Bit#(32) sizeInBits = fromInteger(2**valueOf(addrWidthA) *
                                       valueOf(dataWidthA));
  Bit#(32) addrWidthAInt = fromInteger(valueOf(addrWidthA));
  Bit#(32) addrWidthBInt = fromInteger(valueOf(addrWidthB));
  Bit#(32) dataWidthAInt = fromInteger(valueOf(dataWidthA));
  Bit#(32) dataWidthBInt = fromInteger(valueOf(dataWidthB));
  staticAssert(! isValid(opts.initFile), "Initialistion not supported");

  // State
  Reg#(BlockRamHandle) ramReg <- mkReg(0);
  Reg#(dataA) dataAReg1 <- mkConfigRegU;
  Reg#(dataA) dataAReg2 <- mkConfigRegU;
  Reg#(dataB) dataBReg1 <- mkConfigRegU;
  Reg#(dataB) dataBReg2 <- mkConfigRegU;

  // Wires
  Wire#(BlockRamHandle) ram <- mkBypassWire;

  // Rules
  rule create;
    BlockRamHandle h = ramReg;
    if (h == 0) begin
      h <- createBlockRam(sizeInBits);
      ramReg <= h;
    end
    ram <= h;
  endrule

  rule update;
    dataAReg2 <= dataAReg1;
    dataBReg2 <= dataBReg1;
  endrule

  // Port A
  method Action putA(Bool wr, addrA address, dataA x);
    if (wr)
      blockRamWrite(ram, pack(address), pack(x),
                      dataWidthAInt, addrWidthAInt);
    else begin
      let out <- blockRamRead(ram, pack(address),
                                dataWidthAInt, addrWidthAInt);
      dataAReg1 <= unpack(out);
    end
  endmethod

  method dataA dataOutA = opts.registerDataOut ? dataAReg2 : dataAReg1;

  // Port B
  method Action putB(Bool wr, addrB address, dataB x);
    if (wr)
      blockRamWrite(ram, pack(address), pack(x),
                      dataWidthBInt, addrWidthBInt);
    else begin
      let out <- blockRamRead(ram, pack(address),
                                dataWidthBInt, addrWidthBInt);
      dataBReg1 <= unpack(out);
    end
  endmethod

  method dataB dataOutB = opts.registerDataOut ? dataBReg2 : dataBReg1;

endmodule

`ifndef SIMULATE
// define the IP wrappers. s10 has additional conditions relative to sV
import "BVI" AlteraBlockRamTrueMixed =
  module mkBlockRamMaybeTrueMixedOpts_ALTERA#(BlockRamOpts opts)
         (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB)
                  `ifdef Stratix10
                  , Add#(0, dataWidthA, dataWidthB) // s10 requirement; equal sized dual ports!
                  `endif // s10
                  );

    parameter ADDR_WIDTH_A = valueOf(addrWidthA);
    parameter ADDR_WIDTH_B = valueOf(addrWidthB);
    parameter DATA_WIDTH_A = valueOf(dataWidthA);
    parameter DATA_WIDTH_B = valueOf(dataWidthB);
    parameter NUM_ELEMS_A  = valueOf(TExp#(addrWidthA));
    parameter NUM_ELEMS_B  = valueOf(TExp#(addrWidthB));
    parameter RD_DURING_WR =
      opts.readDuringWrite == OldData ? "OLD_DATA" : "DONT_CARE";
    parameter DO_REG_A       =
      opts.registerDataOut ? "CLOCK0" : "UNREGISTERED";
    parameter DO_REG_B       =
      opts.registerDataOut ? "CLOCK0" : "UNREGISTERED";
    parameter INIT_FILE      =
      case (opts.initFile) matches
        tagged Invalid: return "UNUSED";
        tagged Valid .x: return (x + ".mif");
      endcase;
    parameter DEV_FAMILY = `DeviceFamily;
    parameter STYLE = opts.style;

    // Port A
    method putA(WE_A, ADDR_A, DI_A) enable (EN_A) clocked_by(clk);
    method DO_A dataOutA;

    // Port B
    method putB(WE_B, ADDR_B, DI_B) enable (EN_B) clocked_by(clk);
    method DO_B dataOutB;

    default_clock clk(CLK, (*unused*) clk_gate);
    default_reset no_reset;

    schedule (dataOutA, dataOutB) CF (dataOutA, dataOutB, putA, putB);
    schedule (putA)               CF (putB);
    schedule (putA)               C  (putA);
    schedule (putB)               C  (putB);
  endmodule

`else

module mkBlockRamMaybeTrueMixedOpts_ALTERA#(BlockRamOpts opts) (BlockRam#(addr, data));
  staticAssert(False, "Altera BVI module used in sim enviroment.");
endmodule

`endif

// If s10, we need to add width adaptors to get a true dual port
module mkBlockRamTrueMixedOpts_S10#(BlockRamOpts opts)
  (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))

  provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
           Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
           Bounded#(addrA), Bounded#(addrB),
           Add#(addrWidthA, aExtra, addrWidthB),
           Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
           Literal#(addrA), Literal#(addrB),

           // compiler generated provisos I don't understand yet...
           Add#(a__, TMul#(TDiv#(dataWidthB, 8), 8), TMul#(TDiv#(dataWidthA, 8), 8)),
           Div#(TMul#(TDiv#(dataWidthA, 8), 8), TDiv#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8),
           Div#(TMul#(TDiv#(dataWidthA, 8), 8), 8, TDiv#(dataWidthA, 8)),
           Mul#(TDiv#(dataWidthB, 8), TExp#(aExtra), TDiv#(dataWidthA, 8)),
           Add#(TDiv#(a__, 8), TDiv#(TMul#(TDiv#(dataWidthB, 8), 8), 8), TDiv#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
           Mul#(TExp#(aExtra), TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8), TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
           Mul#(TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8), 8, TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
           Div#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8), 8),
           Add#(TDiv#(a__, 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8), 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8)),

           Add#(b__, dataWidthA, TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
           Add#(b__, dataWidthB, TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8))
          );

  Integer dataWidthA = valueOf(SizeOf#(addrA));
  Integer dataWidthB = valueOf(SizeOf#(addrB));

  // to elimiate the byte-allignment requirements, we pad to the nearest mul of 8
  BlockRamTrueMixed#(addrA,
                     Bit#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                     addrB,
                     Bit#(TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8))
                    ) bram <- mkBlockRamTrueMixedOptsMult8_S10(opts);

  method Action putA(Bool wr, addrA a, dataA x);
    bram.putA(wr, a, extend(pack(x)));
  endmethod

  method dataA dataOutA;
    return unpack(truncate(bram.dataOutA));
  endmethod

  method Action putB(Bool wr, addrB a, dataB x);
    bram.putB(wr, a, extend(pack(x)));
  endmethod

  method dataB dataOutB;
    return unpack(truncate(bram.dataOutB));
  endmethod
endmodule

module mkBlockRamTrueMixedOptsMult8_S10#(BlockRamOpts opts)
        (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))

        provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                 Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                 Bounded#(addrA), Bounded#(addrB),
                 Add#(addrWidthA, aExtra, addrWidthB),
                 Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
                 Mul#(TDiv#(dataWidthA, 8), 8, dataWidthA), // tautology...?
                 Div#(dataWidthA, TDiv#(dataWidthA, 8), 8), // tautology - compiler infered??
                 Literal#(addrA), Literal#(addrB),
                 Add#(TDiv#(aExtraData, 8), TDiv#(dataWidthB, 8), TDiv#(dataWidthA, 8)), // for extend() in the B addr calc
                 Add#(aExtraData, dataWidthB, dataWidthA) // for extend in the b data calc
                );

  // The sx10 BRAMs do not support true dual port operation.
  // as a fallback, this module adapts a dual-clock equal port size byte enabled BRAM into
  // a dual port; this adds a sizable mux on port B to select and mask out the desired portion,
  // as well as more address calculation logic.
  // fully comb
  Integer addrWidthA = valueOf(SizeOf#(addrA));
  Integer addrWidthB = valueOf(SizeOf#(addrB));
  Integer dataWidthA = valueOf(SizeOf#(dataA));
  Integer dataWidthB = valueOf(SizeOf#(dataB));

  Bit#(addrWidthB) width_ratio = fromInteger(dataWidthA / dataWidthB);

  // use a dual rwport bram with equal sized ports (supported by sx10).
  // port B has byte enables for the narrower of the 2 stored types
  // in order to support testing, if building for simlation, swap in the BRAMCore impl
  `ifdef SIMULATE
  BlockRamTrueMixedByteEn#(addrA, dataA, // port A
                           addrA, dataA, // port B
                           TDiv#(dataWidthA, 8) // Port B byte enables width
                          ) bram <- mkBlockRamTrueMixedBEOpts_SIMULATE(opts);
  `else
  BlockRamTrueMixedByteEn#(addrA, dataA, // port A
                           addrA, dataA, // port B
                           TDiv#(dataWidthA, 8) // Port B byte enables width
                          ) bram <- mkBlockRamMaybeTrueMixedBEOpts_ALTERA(opts);
  `endif

  // A, the wide side, is trivial
  method Action putA(Bool wr, addrA a, dataA x);
    bram.putA(wr, a, x);
  endmethod

  method dataA dataOutA;
    return bram.dataOutA;
  endmethod

  method Action putB(Bool wr, addrB addr_into_b, dataB x);
    // // the port is sizeOf(A) wide; we can enable 8 bits at a time by setting b_enables
    // calculate the effective address;
    addrA addr_into_a = unpack((pack(addr_into_b) / width_ratio)[addrWidthA-1:0]);
    // Integer shift = 0;
    Bit#(TDiv#(dataWidthB, 8)) b_lane_base_mask = fromInteger(2**(dataWidthB/8)-1); // all ones vector the width of dataB in bytes
    Bit#(TDiv#(dataWidthA, 8)) b_enables = extend(b_lane_base_mask) << (pack(addr_into_b)%width_ratio);
    // Bit#(dataWidthA) x_packed = 0;
    dataA x_packed = unpack(extend( pack(x) << (pack(addr_into_b)%width_ratio)*8 ));
    // need to move the B data into the correct portion
    bram.putB(wr, addr_into_a, x_packed, b_enables);
  endmethod

  method dataB dataOutB;
      Bit#(dataWidthA) x_packed = pack(bram.dataOutB);
      // need to mask out the right section
      dataB x = unpack(x_packed[dataWidthB-1:0]);
      return x;
  endmethod
endmodule

`ifdef SIMULATE

module mkBlockRamTrueMixedOpts#(BlockRamOpts opts)
         (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA));

  BlockRamTrueMixed#(addrA, dataA, addrB, dataB) ram <- mkBlockRamTrueMixedOpts_SIMULATE(opts);
  return ram;
endmodule

`else
`ifdef Stratix10

module mkBlockRamTrueMixedOpts#(BlockRamOpts opts)
         (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
                  Literal#(addrA), Literal#(addrB),

                  // compiler generated provisos I don't understand yet...
                  Add#(a__, TMul#(TDiv#(dataWidthB, 8), 8), TMul#(TDiv#(dataWidthA, 8), 8)),
                  Div#(TMul#(TDiv#(dataWidthA, 8), 8), TDiv#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8),
                  Div#(TMul#(TDiv#(dataWidthA, 8), 8), 8, TDiv#(dataWidthA, 8)),
                  Mul#(TDiv#(dataWidthB, 8), TExp#(aExtra), TDiv#(dataWidthA, 8)),
                  Add#(TDiv#(a__, 8), TDiv#(TMul#(TDiv#(dataWidthB, 8), 8), 8), TDiv#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                  Mul#(TExp#(aExtra), TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8), TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                  Mul#(TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8), 8, TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                  Div#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8), 8),
                  Add#(TDiv#(a__, 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8), 8), TDiv#(TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8), 8)),

                  Add#(b__, dataWidthA, TAdd#(TMul#(TDiv#(dataWidthA, 8), 8), 8)),
                  Add#(b__, dataWidthB, TAdd#(TMul#(TDiv#(dataWidthB, 8), 8), 8))
                 );

  BlockRamTrueMixed#(addrA, dataA, addrB, dataB) ram <- mkBlockRamTrueMixedOpts_S10(opts);
  return ram;
endmodule

`endif // s10

`ifdef StratixV
// No adaptors needed; mixed width is natively supported

module mkBlockRamTrueMixedOpts#(BlockRamOpts opts)
  (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))

  provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
           Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
           Bounded#(addrA), Bounded#(addrB),
           Add#(addrWidthA, aExtra, addrWidthB),
           Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
           Literal#(addrA), Literal#(addrB)
          );

  BlockRamTrueMixed#(addrA, dataA, addrB, dataB) bram <- mkBlockRamMaybeTrueMixedOpts_ALTERA(opts);
  return bram;
endmodule

`endif // StratixV

`endif // not SIMULATE

// ======================================================
// True dual-port mixed-width block RAM with byte-enables
// ======================================================

module mkBlockRamTrueMixedBE
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
                  Mul#(dataBBytes, 8, dataWidthB),
                  Div#(dataWidthB, dataBBytes, 8),
                  Mul#(dataABytes, 8, dataWidthA),
                  Div#(dataWidthA, dataABytes, 8),
                  Mul#(TExp#(aExtra), dataBBytes, dataABytes)

                  `ifdef Stratix10
                  , Div#(dataWidthB, 8, dataBBytes),
                  Div#(dataWidthA, TDiv#(dataWidthA, 8), 8),
                  Add#(TDiv#(a__, 8), TDiv#(dataWidthB, 8), TDiv#(dataWidthA, 8)),
                  Add#(a__, dataWidthB, dataWidthA)
                  `endif // Stratix10
                );

  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamTrueMixedBEOpts(defaultBlockRamOpts);
  return ram;
endmodule

// BSV implementation using BRAMCore
module mkBlockRamTrueMixedBEOpts_SIMULATE#(BlockRamOpts opts)
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
                  Mul#(dataBBytes, 8, dataWidthB),
                  Div#(dataWidthB, dataBBytes, 8),
                  Mul#(dataABytes, 8, dataWidthA),
                  Div#(dataWidthA, dataABytes, 8),
                  Mul#(TExp#(aExtra), dataBBytes, dataABytes));
  // For simulation, use a BRAMCore
  BRAM_DUAL_PORT_BE#(addrA, dataA, dataABytes) ram <-
      mkBRAMCore2BELoad(valueOf(TExp#(addrWidthA)), False,
                          fromMaybe("Zero", opts.initFile) + ".hex", False);

  // State
  Reg#(dataA) dataAReg <- mkConfigRegU;
  Reg#(dataA) dataBReg <- mkConfigRegU;
  Reg#(Bit#(aExtra)) offsetB1 <- mkConfigRegU;
  Reg#(Bit#(aExtra)) offsetB2 <- mkConfigRegU;

  // Rules
  rule update;
    offsetB2 <= offsetB1;
    dataAReg <= ram.a.read;
    dataBReg <= ram.b.read;
  endrule

  // Port A
  method Action putA(Bool wr, addrA address, dataA x);
    ram.a.put(wr ? -1 : 0, address, x);
  endmethod

  method dataA dataOutA = opts.registerDataOut ? dataAReg : ram.a.read;

  // Port B
  method Action putB(Bool wr, addrB address, dataB val, Bit#(dataBBytes) be);
    Bit#(aExtra) offset = truncate(pack(address));
    offsetB1 <= offset;
    Bit#(addrWidthA) addr = truncateLSB(pack(address));
    Bit#(dataWidthA) vals = pack(replicate(val));
    Vector#(TExp#(aExtra), Bit#(dataBBytes)) paddedBE;
    for (Integer i = 0; i < valueOf(TExp#(aExtra)); i=i+1)
      paddedBE[i] = (offset == fromInteger(i)) ? be : unpack(0);
    ram.b.put(wr ? pack(paddedBE) : 0, unpack(addr), unpack(vals));
  endmethod

  method dataB dataOutB;
    Vector#(TExp#(aExtra), dataB) vec = unpack(pack(
      opts.registerDataOut ? dataBReg : ram.b.read));
    return vec[opts.registerDataOut ? offsetB2 : offsetB1];
  endmethod
endmodule

`ifndef SIMULATE
// Altera implementation
import "BVI" AlteraBlockRamTrueMixedBE =
  module mkBlockRamMaybeTrueMixedBEOpts_ALTERA#(BlockRamOpts opts) // mkBlockRamTrueMixedBEOpts_EQUALONLY
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Mul#(dataBBytes, 8, dataWidthB),
                  Div#(dataWidthB, dataBBytes, 8)
                  `ifdef Stratix10
                  , Add#(0, dataWidthA, dataWidthB) // s10 requirement; equal sized dual ports!
                  `endif // Stratix10
                 );

    parameter ADDR_WIDTH_A = valueOf(addrWidthA);
    parameter ADDR_WIDTH_B = valueOf(addrWidthB);
    parameter DATA_WIDTH_A = valueOf(dataWidthA);
    parameter DATA_WIDTH_B = valueOf(dataWidthB);
    parameter NUM_ELEMS_A  = valueOf(TExp#(addrWidthA));
    parameter NUM_ELEMS_B  = valueOf(TExp#(addrWidthB));
    parameter BE_WIDTH = valueOf(dataBBytes);
    parameter RD_DURING_WR =
      opts.readDuringWrite == OldData ? "OLD_DATA" : "DONT_CARE";
    parameter DO_REG_A       =
      opts.registerDataOut ? "CLOCK0" : "UNREGISTERED";
    parameter DO_REG_B       =
      opts.registerDataOut ? "CLOCK0" : "UNREGISTERED";
    parameter INIT_FILE      =
      case (opts.initFile) matches
        tagged Invalid: return "UNUSED";
        tagged Valid .x: return (x + ".mif");
      endcase;
    parameter DEV_FAMILY = `DeviceFamily;
    parameter STYLE = opts.style;

    // Port A
    method putA(WE_A, ADDR_A, DI_A) enable (EN_A) clocked_by(clk);
    method DO_A dataOutA;

    // Port B
    method putB(WE_B, ADDR_B, DI_B, BE_B) enable (EN_B) clocked_by(clk);
    method DO_B dataOutB;

    default_clock clk(CLK, (*unused*) clk_gate);
    default_reset no_reset;

    schedule (dataOutA, dataOutB) CF (dataOutA, dataOutB, putA, putB);
    schedule (putA)               CF (putB);
    schedule (putA)               C  (putA);
    schedule (putB)               C  (putB);
  endmodule

`else

module mkBlockRamMaybeTrueMixedBEOpts_ALTERA#(BlockRamOpts opts) (BlockRam#(addr, data));
  staticAssert(False, "Altera BVI module used in sim enviroment.");
endmodule

`endif


module mkBlockRamTrueMixedBEOpts_S10#(BlockRamOpts opts)
      (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
    provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
             Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
             Bounded#(addrA), Bounded#(addrB),
             Add#(addrWidthA, aExtra, addrWidthB),
             Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
             Mul#(dataBBytes, 8, dataWidthB),
             Div#(dataWidthB, dataBBytes, 8),
             Mul#(dataABytes, 8, dataWidthA),
             Div#(dataWidthA, dataABytes, 8),
             Mul#(TExp#(aExtra), dataBBytes, dataABytes),

             // lane enables...
             Div#(dataWidthB, 8, dataBBytes),
             Div#(dataWidthA, TDiv#(dataWidthA, 8), 8),
             Add#(TDiv#(aExtraData, 8), TDiv#(dataWidthB, 8), TDiv#(dataWidthA, 8)), // for extend() in the B addr calc
             Add#(aExtraData, dataWidthB, dataWidthA) // for extend in the b data calc
            );

   Integer addrWidthA = valueOf(SizeOf#(addrA));
   Integer addrWidthB = valueOf(SizeOf#(addrB));
   Integer dataWidthA = valueOf(SizeOf#(dataA));
   Integer dataWidthB = valueOf(SizeOf#(dataB));
   Bit#(addrWidthB) width_ratio = fromInteger(dataWidthA / dataWidthB);

   // allow using this as a unneeded wrapper in sim for testing.
  `ifdef SIMULATE
  let bram <- mkBlockRamTrueMixedBEOpts_SIMULATE(opts);
  `else
  let bram <- mkBlockRamMaybeTrueMixedBEOpts_ALTERA(opts);
  `endif
  // return ram;

  // A, the wide side, is trivial
  method Action putA(Bool wr, addrA a, dataA x);
    bram.putA(wr, a, x);
  endmethod

  method dataA dataOutA;
    return bram.dataOutA;
  endmethod

  method Action putB(Bool wr, addrB addr_into_b, dataB x, Bit#(dataBBytes) be);
    // // the port is sizeOf(A) wide; we can enable 8 bits at a time by setting b_enables
    // calculate the effective address;
    addrA addr_into_a = unpack((pack(addr_into_b) / width_ratio)[addrWidthA-1:0]);
    // Integer shift = 0;
    Bit#(TDiv#(dataWidthB, 8)) b_lane_base_mask = be; // the required byte enables
    Bit#(TDiv#(dataWidthA, 8)) b_enables = extend(b_lane_base_mask) << (pack(addr_into_b)%width_ratio);
    // Bit#(dataWidthA) x_packed = 0;
    dataA x_packed = unpack(extend( pack(x) << (pack(addr_into_b)%width_ratio)*8 ));
    // need to move the B data into the correct portion
    bram.putB(wr, addr_into_a, x_packed, b_enables);
  endmethod

  method dataB dataOutB;
      Bit#(dataWidthA) x_packed = pack(bram.dataOutB);
      // need to mask out the right section
      dataB x = unpack(x_packed[dataWidthB-1:0]);
      return x;
  endmethod
endmodule


`ifdef SIMULATE

module mkBlockRamTrueMixedBEOpts#(BlockRamOpts opts)
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
                  Mul#(dataBBytes, 8, dataWidthB),
                  Div#(dataWidthB, dataBBytes, 8),
                  Mul#(dataABytes, 8, dataWidthA),
                  Div#(dataWidthA, dataABytes, 8),
                  Mul#(TExp#(aExtra), dataBBytes, dataABytes));
  // For simulation, use a BRAMCore
  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamTrueMixedBEOpts_SIMULATE(opts);
  return ram;
endmodule


`else // not SIMULATE

`ifdef Stratix10

module mkBlockRamTrueMixedBEOpts#(BlockRamOpts opts)
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
                  Mul#(dataBBytes, 8, dataWidthB),
                  Div#(dataWidthB, dataBBytes, 8),
                  Mul#(dataABytes, 8, dataWidthA),
                  Div#(dataWidthA, dataABytes, 8),
                  Mul#(TExp#(aExtra), dataBBytes, dataABytes),
                  Div#(dataWidthB, 8, dataBBytes),
                  Div#(dataWidthA, TDiv#(dataWidthA, 8), 8),
                  Add#(TDiv#(a__, 8), TDiv#(dataWidthB, 8), TDiv#(dataWidthA, 8)),
                  Add#(a__, dataWidthB, dataWidthA)
                  );
  // For simulation, use a BRAMCore
  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamTrueMixedBEOpts_S10(opts);
  return ram;
endmodule

`endif // Stratix10

`ifdef StratixV

module mkBlockRamTrueMixedBEOpts#(BlockRamOpts opts)
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
                  Mul#(dataBBytes, 8, dataWidthB),
                  Div#(dataWidthB, dataBBytes, 8),
                  Mul#(dataABytes, 8, dataWidthA),
                  Div#(dataWidthA, dataABytes, 8),
                  Mul#(TExp#(aExtra), dataBBytes, dataABytes));
  // For simulation, use a BRAMCore
  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamMaybeTrueMixedBEOpts_ALTERA(opts);
  return ram;
endmodule


`endif // StratixV
`endif // not SIMULATE


endpackage
