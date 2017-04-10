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
    registerDataOut: True,
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
    parameter DEV_FAMILY = `DeviceFamily;

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
    parameter DEV_FAMILY = `DeviceFamily;

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

// ====================================
// True dual-port mixed-width block RAM
// ====================================

module mkBlockRamTrueMixed
         (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Add#(addrWidthA, aExtra, addrWidthB),
                  Mul#(TExp#(aExtra), dataWidthB, dataWidthA));
  let ram <- mkBlockRamTrueMixedOpts(defaultBlockRamOpts); return ram;
endmodule

`ifdef SIMULATE

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

module mkBlockRamTrueMixedOpts#(BlockRamOpts opts)
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

`else

// Altera implementation

import "BVI" AlteraBlockRamTrueMixed =
  module mkBlockRamTrueMixedOpts#(BlockRamOpts opts)
         (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB));

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

`endif

// ======================================================
// True dual-port mixed-width block RAM with byte-enables
// ======================================================

module mkBlockRamTrueMixedBE
      (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
    provisos(Bits#(addrA, awidthA), Bits#(dataA, dwidthA),
             Bits#(addrB, awidthB), Bits#(dataB, dwidthB),
             Bounded#(addrA), Bounded#(addrB),
             Add#(awidthA, aExtra, awidthB),
             Mul#(TExp#(aExtra), dwidthB, dwidthA),
             Mul#(dataBBytes, 8, dwidthB),
             Div#(dwidthB, dataBBytes, 8),
             Mul#(dataABytes, 8, dwidthA),
             Div#(dwidthA, dataABytes, 8),
             Mul#(TExp#(aExtra), dataBBytes, dataABytes));
  let ram <- mkBlockRamTrueMixedBEOpts(defaultBlockRamOpts); return ram;
endmodule

`ifdef SIMULATE

// BSV implementation using BRAMCore

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

`else

// Altera implementation

import "BVI" AlteraBlockRamTrueMixedBE =
  module mkBlockRamTrueMixedBEOpts#(BlockRamOpts opts)
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
                  Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
                  Bounded#(addrA), Bounded#(addrB),
                  Mul#(dataBBytes, 8, dataWidthB),
                  Div#(dataWidthB, dataBBytes, 8));

    parameter ADDR_WIDTH_A = valueOf(addrWidthA);
    parameter ADDR_WIDTH_B = valueOf(addrWidthB);
    parameter DATA_WIDTH_A = valueOf(dataWidthA);
    parameter DATA_WIDTH_B = valueOf(dataWidthB);
    parameter NUM_ELEMS_A  = valueOf(TExp#(addrWidthA));
    parameter NUM_ELEMS_B  = valueOf(TExp#(addrWidthB));
    parameter BE_WIDTH     = valueOf(dataBBytes);
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

`endif

endpackage
