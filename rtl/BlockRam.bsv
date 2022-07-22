// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor, Jon Woodruff

/*
 * Copyright (c) 2022 Simon W. Moore
 * All rights reserved.
 *
 * @BERI_LICENSE_HEADER_START@
 *
 * Licensed to BERI Open Systems C.I.C. (BERI) under one or more contributor
 * license agreements.  See the NOTICE file distributed with this work for
 * additional information regarding copyright ownership.  BERI licenses this
 * file to you under the BERI Hardware-Software License, Version 1.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at:
 *
 *   http://www.beri-open-systems.org/legal/license-1-0.txt
 *
 * Unless required by applicable law or agreed to in writing, Work distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations under the License.
 *
 * @BERI_LICENSE_HEADER_END@
 *
 * ----------------------------------------------------------------------------
 * Builds on BlockRAM by Matt Naylor, et al.
 * This "v" version uses pure Verilog to describe basic single and
 * true dual-port RAMs that can be inferred as BRAMs like M20K on Stratix 10.
 */


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

// True dual-port mixed-width block RAM
interface BlockRamTrueMixedPadded#
            (type addrA, type dataA,
             type addrB, type dataB,
             numeric type paddedWidthA, numeric type paddedWidthB);
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

// True dual-port mixed-width block RAM with byte-enables
// (Port B has the byte enables and must be smaller than port A)
interface BlockRamTrueMixedByteEnPadded#
            (type addrA, type dataA,
             type addrB, type dataB,
             numeric type dataBBytes,
             numeric type paddedWidthA, numeric type paddedWidthB
             );
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



module mkBlockRamBEOpts#(BlockRamOpts opts)
         (BlockRamByteEn#(addr, data, dataBytes))
         provisos(Bits#(addr, addrWidth), Bits#(data, dataWidth),
                  Bounded#(addr), Mul#(dataBytes, 8, dataWidth),
                  Div#(dataWidth, dataBytes, 8));
  // For simulation, use a BRAMCore
  `ifdef SIMULATE
  BlockRamByteEn#(addr, data, dataBytes) ram <- mkBlockRamBEOpts_SIMULATED(opts);
  `else
  BlockRamByteEn#(addr, data, dataBytes) ram <- mkBlockRamBEOpts_ALTERA(opts);
  `endif

  return ram;
endmodule



// ====================================
// True dual-port mixed-width block RAM
// ====================================

module mkBlockRamTrueMixed
         (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))
         provisos(Bits#(addrA, awidthA), Bits#(dataA, dwidthA),
                  Bits#(addrB, awidthB), Bits#(dataB, dwidthB),
                  Bounded#(addrA),       Bounded#(addrB),
                  Add#(awidthA, aExtra, awidthB),
                  Log#(TExp#(aExtra), aExtra),
                  Mul#(TExp#(aExtra), dwidthB, dwidthA),
                  Mul#(dataBBytes, 8, dwidthB),
                  Div#(dwidthB, dataBBytes, 8),
                  Mul#(dataABytes, 8, dwidthA),
                  Div#(dwidthA, dataABytes, 8),
                  Mul#(TExp#(aExtra), dataBBytes, dataABytes),
                  Log#(dataABytes, logdataABytes),
                  Log#(dataBBytes, logdataBBytes),
                  Add#(aExtra, logdataBBytes, logdataABytes),
                  Log#(TDiv#(dwidthB, 8), logdataBBytes), Div#(dwidthB, TDiv#(dwidthB, 8), 8)
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
    if (wr) begin
      blockRamWrite(ram, pack(address), pack(x),
                      dataWidthAInt, addrWidthAInt);
      dataAReg1 <= x;
    end else begin
      let out <- blockRamRead(ram, pack(address),
                                dataWidthAInt, addrWidthAInt);
      dataAReg1 <= unpack(out);
    end
  endmethod

  method dataA dataOutA = opts.registerDataOut ? dataAReg2 : dataAReg1;

  // Port B
  method Action putB(Bool wr, addrB address, dataB x);
    if (wr) begin
      blockRamWrite(ram, pack(address), pack(x),
                      dataWidthBInt, addrWidthBInt);
      dataBReg1 <= x;
    end else begin
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

`endif // simulate
//

module mkBlockRamTrueMixedBE_BsvCtrlLogic#(BlockRamOpts opts)
      (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
    provisos(Bits#(addrA, awidthA), Bits#(dataA, dwidthA),
             Bits#(addrB, awidthB), Bits#(dataB, dwidthB),
             Bounded#(addrA),       Bounded#(addrB),
             Add#(awidthA, aExtra, awidthB),
             Log#(expaExtra, aExtra),
             Mul#(expaExtra, dwidthB, dwidthA),
             Mul#(dataBBytes, 8, dwidthB),
             Div#(dwidthB, dataBBytes, 8),
             Mul#(dataABytes, 8, dwidthA),
             Div#(dwidthA, dataABytes, 8),
             Mul#(expaExtra, dataBBytes, dataABytes),
             Log#(dataABytes, logdataABytes),
             Log#(dataBBytes, logdataBBytes),
             Add#(aExtra, logdataBBytes, logdataABytes));

  // staticAssert(opts.registerDataOut == False, "bsv ctrl logic does not support internal reg");
  staticAssert(opts.initFile == Invalid, "bsv ctrl logic does not support init file");

  // Instatitate byte-wide RAMs to fit the data width of port A since it is the widest port
  // Vector#(dataABytes, BlockRamTrueDualPort#(Bit#(awidthA), Bit#(8))) rams <- replicateM(mkDualPortBlockRAM);
  `ifdef SIMULATE
  staticAssert(opts.initFile == Invalid, "bsv ctrl logic does not support init file");
  Vector#(dataABytes, BlockRamTrueMixed#(Bit#(awidthA), Bit#(8), Bit#(awidthA), Bit#(8))) rams <- replicateM(mkBlockRamTrueMixedOpts_SIMULATE(opts));
  `else
  staticAssert(opts.initFile == Invalid, "bsv ctrl logic does not support init file");
  Vector#(dataABytes, BlockRamTrueMixed#(Bit#(awidthA), Bit#(8), Bit#(awidthA), Bit#(8))) rams <- replicateM(mkBlockRamMaybeTrueMixedOpts_ALTERA(opts));
  `endif


  // addrB needed during read to select the right word
  Reg#(addrB)           save_addrB <- mkReg(unpack(0));
  Reg#(addrB)           save_addrB_delay <- mkReg(unpack(0));

  rule delay_addrB_reg;
    save_addrB_delay <= save_addrB;
  endrule

  Wire#(Maybe#(addrA)) check_addrA <- mkDWire(Invalid);
  Wire#(Maybe#(addrB)) check_addrB <- mkDWire(Invalid);

  // For simulation only. Should be optimised away on FPGA.
  rule assert_no_write_collision(isValid(check_addrA) && isValid(check_addrB));
    Bit#(awidthA) addrA = pack(fromMaybe(?, check_addrA));
    Bit#(awidthA) addrB = truncate(pack(fromMaybe(?, check_addrB))>>valueOf(aExtra));
    dynamicAssert(addrA != addrB, "ERROR in mkBlockRamTrueMixedBE: address collision on two writes");
  endrule

  method Action putA(we, a, d);
    Bit#(dwidthA) data = pack(d);
    // $display("mixed dpram putA: we=%1d  re=%1d", we, re);
    for(Integer n=0; n<valueOf(dataABytes); n=n+1)
        rams[n].putA(we, pack(a), data[n*8+7:n*8]);
    if(we) check_addrA <= tagged Valid a;
  endmethod

  method Action putB(we, a, d, be);
    // $display("mixed dpram putB: we=%1d  re=%1d", we, re);
    for(Integer n=0; n<valueOf(dataBBytes); n=n+1)
      begin
        Bit#(aExtra) bank_select = truncate(pack(a));
        Bit#(logdataBBytes) byte_select = fromInteger(n);
        Bit#(logdataABytes) bram_select = {bank_select, byte_select};
        Bit#(awidthA) addr = truncate(unpack(pack(a) >> valueOf(aExtra)));
        Bit#(dwidthB) data = pack(d);
        rams[bram_select].putB(we && (be[n]==1), addr, data[n*8+7:n*8]);
        $display("Write to ram[%d]",bram_select);
      end
    save_addrB <= a;
    if(we) check_addrB <= tagged Valid a;
  endmethod

  method dataA dataOutA;
    Vector#(dataABytes,Bit#(8)) b;
    for(Integer n=0; n<valueOf(dataABytes); n=n+1)
      begin
        b[n] = rams[n].dataOutA;
       end
    return unpack(pack(b));
  endmethod

  method dataB dataOutB;
    Vector#(dataBBytes,Bit#(8)) b;
    addrB addr = opts.registerDataOut ? save_addrB_delay : save_addrB;
    for(Integer n=0; n<valueOf(dataBBytes); n=n+1)
      begin
        Bit#(aExtra) bank_select = truncate(pack(addr));
        Bit#(logdataBBytes) byte_select = fromInteger(n);
        Bit#(logdataABytes) bram_select = {bank_select, byte_select};
        b[n] = rams[bram_select].dataOutB;
      end
    return unpack(pack(b));
  endmethod

endmodule



`ifdef Stratix10

module mkBlockRamTrueMixedOpts#(BlockRamOpts opts)
  (BlockRamTrueMixed#(addrA, dataA, addrB, dataB))

  provisos(Bits#(addrA, awidthA), Bits#(dataA, dwidthA),
           Bits#(addrB, awidthB), Bits#(dataB, dwidthB),
           Bounded#(addrA),       Bounded#(addrB),
           Add#(awidthA, aExtra, awidthB),
           Log#(TExp#(aExtra), aExtra),
           Mul#(TExp#(aExtra), dwidthB, dwidthA),
           Mul#(dataBBytes, 8, dwidthB),
           Div#(dwidthB, dataBBytes, 8),
           Mul#(dataABytes, 8, dwidthA),
           Div#(dwidthA, dataABytes, 8),
           Mul#(TExp#(aExtra), dataBBytes, dataABytes),
           Log#(dataABytes, logdataABytes),
           Log#(dataBBytes, logdataBBytes),
           Add#(aExtra, logdataBBytes, logdataABytes),
           Log#(TDiv#(dwidthB, 8), logdataBBytes), Div#(dwidthB, TDiv#(dwidthB, 8), 8)
           );

  BlockRamTrueMixedBE#(addrA, dataA, addrB, dataB) bram <- mkBlockRamTrueMixedBEOpts(opts);

  method Action putA(Bool wr, addrA a, dataA x);
    bram.putA(wr, a, x);
  endmethod

  method dataA dataOutA;
    return bram.dataOutA;
  endmethod

  method Action putB(Bool wr, addrB a, dataB x);
    bram.putB(wr, a, x, wr ? ~0 : 0);
  endmethod

  method dataB dataOutB;
    return bram.dataOutB;
  endmethod

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

  `ifdef SIMULATE
  BlockRamTrueMixed#(addrA, dataA, addrB, dataB) bram <- mkBlockRamMaybeTrueMixedOpts_ALTERA(opts);
  `else // not SIMULATE
  BlockRamTrueMixed#(addrA, dataA, addrB, dataB) bram <- mkBlockRamMaybeTrueMixedOpts_SIMULATE(opts);
  `endif // not SIMULATE

  return bram;
endmodule

`endif // StratixV

// ======================================================
// True dual-port mixed-width block RAM with byte-enables
// ======================================================

module mkBlockRamTrueMixedBE
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         // provisos(Bits#(addrA, awidthA), Bits#(dataA, dwidthA),
         //          Bits#(addrB, awidthB), Bits#(dataB, dwidthB),
         //          Bounded#(addrA),       Bounded#(addrB),
         //          Add#(awidthA, aExtra, awidthB),
         //          Log#(expaExtra, aExtra),
         //          Mul#(expaExtra, dwidthB, dwidthA),
         //          Mul#(dataBBytes, 8, dwidthB),
         //          Div#(dwidthB, dataBBytes, 8),
         //          Mul#(dataABytes, 8, dwidthA),
         //          Div#(dwidthA, dataABytes, 8),
         //          Mul#(expaExtra, dataBBytes, dataABytes),
         //          Log#(dataABytes, logdataABytes),
         //          Log#(dataBBytes, logdataBBytes),
         //          Add#(aExtra, logdataBBytes, logdataABytes)
         //        );
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

  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamTrueMixedBEOpts_SIMULATE(defaultBlockRamOpts);
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
    // $display("BRAM BE SIM pA writing ", x, " to addr ", address);
    ram.a.put(wr ? -1 : 0, address, x);
  endmethod

  method dataA dataOutA = opts.registerDataOut ? dataAReg : ram.a.read;

  // Port B
  method Action putB(Bool wr, addrB address, dataB val, Bit#(dataBBytes) be);
    // $display("BRAM BE SIM pB writing ", val, " to addr ", address);
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


// In order to work around the proviso match restrictions, this module calculates the required padding to build a
// arbitary width bram packed into a byte-alligned store

module mkBlockRamTrueMixedBEOpts_S10#(BlockRamOpts opts)
  (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
  provisos(Bits#(addrA, awidthA), Bits#(dataA, dwidthA),
           Bits#(addrB, awidthB), Bits#(dataB, dwidthB),
           Bounded#(addrA),       Bounded#(addrB),
           Add#(awidthA, aExtra, awidthB),
           Log#(expaExtra, aExtra),
           Mul#(expaExtra, dwidthB, dwidthA),
           Mul#(dataBBytes, 8, dwidthB),
           Div#(dwidthB, dataBBytes, 8),
           Mul#(dataABytes, 8, dwidthA),
           Div#(dwidthA, dataABytes, 8),
           Mul#(expaExtra, dataBBytes, dataABytes),
           Log#(dataABytes, logdataABytes),
           Log#(dataBBytes, logdataBBytes),
           Add#(aExtra, logdataBBytes, logdataABytes)

           );
  // For simulation, use a BRAMCore
  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamTrueMixedBE_BsvCtrlLogic(opts); // swapping to S10 crashes bsc.
  return ram;

endmodule


`ifdef Stratix10
//
module mkBlockRamTrueMixedBEOpts#(BlockRamOpts opts)
         (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
         provisos(Bits#(addrA, awidthA), Bits#(dataA, dwidthA),
                  Bits#(addrB, awidthB), Bits#(dataB, dwidthB),
                  Bounded#(addrA),       Bounded#(addrB),
                  Add#(awidthA, aExtra, awidthB),
                  Log#(expaExtra, aExtra),
                  Mul#(expaExtra, dwidthB, dwidthA),
                  Mul#(dataBBytes, 8, dwidthB),
                  Div#(dwidthB, dataBBytes, 8),
                  Mul#(dataABytes, 8, dwidthA),
                  Div#(dwidthA, dataABytes, 8),
                  Mul#(expaExtra, dataBBytes, dataABytes),
                  Log#(dataABytes, logdataABytes),
                  Log#(dataBBytes, logdataBBytes),
                  Add#(aExtra, logdataBBytes, logdataABytes)

                  );
  // For simulation, use a BRAMCore
  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamTrueMixedBE_BsvCtrlLogic(opts); // swapping to S10 crashes bsc.
  return ram;
endmodule

// ======================================================
// True dual-port mixed-width block RAM with byte-enables
// ======================================================

// Don't rely on support for mixed-width true dual-port BRAMs, which
// are no longer available on the Stratix 10.
`ifdef SIMULATE

module mkBlockRamTrueBEOpts_SIMULATE#(BlockRamOpts opts)
      (BlockRamTrueMixedByteEn#(addrA, dataA, addrA, dataA, dataABytes))
    provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
             Bounded#(addrA),
             Mul#(dataABytes, 8, dataWidthA),
             Div#(dataWidthA, dataABytes, 8),
             Literal#(dataA));

  // For simulation, use a BRAMCore
  BRAM_DUAL_PORT_BE#(addrA, dataA, dataABytes) ram <-
      mkBRAMCore2BELoad(valueOf(TExp#(addrWidthA)), False,
                          fromMaybe("Zero", opts.initFile) + ".hex", False);

  // State
  Reg#(dataA) dataAReg <- mkConfigReg(0);
  Reg#(dataA) dataBReg <- mkConfigReg(0);

  // Rules
  rule update;
    dataAReg <= ram.a.read;
    dataBReg <= ram.b.read;
  endrule

  // Port A
  method Action putA(Bool wr, addrA address, dataA x);
    ram.a.put(wr ? -1 : 0, address, x);
  endmethod

  method dataA dataOutA = opts.registerDataOut ? dataAReg : ram.a.read;

  // Port B
  method Action putB(Bool wr, addrA addr, dataA val, Bit#(dataABytes) be);
    ram.b.put(wr ? be : 0, addr, val);
  endmethod

  method dataA dataOutB = opts.registerDataOut ? dataBReg : ram.b.read;

endmodule

`endif


//
// module mkBlockRamPortableTrueMixedBEOpts_old#(BlockRamOpts opts)
//          (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
//          provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
//                   Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
//                   Bounded#(addrA), Bounded#(addrB),
//                   Literal#(dataA),
//                   Add#(addrWidthA, aExtra, addrWidthB),
//                   Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
//                   Mul#(dataBBytes, 8, dataWidthB),
//                   Div#(dataWidthB, dataBBytes, 8),
//                   Mul#(dataABytes, 8, dataWidthA),
//                   Div#(dataWidthA, dataABytes, 8),
//                   Mul#(TExp#(aExtra), dataBBytes, dataABytes));
//
//   BlockRamOpts internalOpts = opts;
//   internalOpts.registerDataOut = False;
//
//   `ifdef SIMULATE
//   BlockRamTrueMixedByteEn#(addrA, dataA, addrA, dataA, dataABytes) ram <-
//     mkBlockRamTrueBEOpts(internalOpts);
//   `else
//   BlockRamTrueMixedByteEn#(addrA, dataA, addrA, dataA, dataABytes) ram <-
//     mkBlockRamMaybeTrueMixedBEOpts_ALTERA(internalOpts);
//   `endif
//
//   // State
//   Reg#(dataA) dataAReg <- mkConfigRegU;
//   Reg#(dataA) dataBReg <- mkConfigRegU;
//   Reg#(Bit#(aExtra)) offsetB1 <- mkConfigRegU;
//   Reg#(Bit#(aExtra)) offsetB2 <- mkConfigRegU;
//
//   // Rules
//   rule update;
//     offsetB2 <= offsetB1;
//     dataAReg <= ram.dataOutA;
//     dataBReg <= ram.dataOutB;
//   endrule
//
//   // Port A
//   method Action putA(Bool wr, addrA address, dataA x);
//     ram.putA(wr, address, x);
//   endmethod
//
//   method dataA dataOutA = opts.registerDataOut ? dataAReg : ram.dataOutA;
//
//   // Port B
//   method Action putB(Bool wr, addrB address, dataB val, Bit#(dataBBytes) be);
//     Bit#(aExtra) offset = truncate(pack(address));
//     offsetB1 <= offset;
//     Bit#(addrWidthA) addr = truncateLSB(pack(address));
//     Bit#(dataWidthA) vals = pack(replicate(val));
//     Vector#(TExp#(aExtra), Bit#(dataBBytes)) paddedBE;
//     for (Integer i = 0; i < valueOf(TExp#(aExtra)); i=i+1)
//       paddedBE[i] = (offset == fromInteger(i)) ? be : unpack(0);
//     ram.putB(wr, unpack(addr), unpack(vals), pack(paddedBE));
//   endmethod
//
//   method dataB dataOutB;
//     Vector#(TExp#(aExtra), dataB) vec = unpack(pack(
//       opts.registerDataOut ? dataBReg : ram.dataOutB));
//     return vec[opts.registerDataOut ? offsetB2 : offsetB1];
//   endmethod
//
// endmodule
//
// module mkBlockRamPortableTrueMixedBEOpts#(BlockRamOpts opts)
//          (BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes))
//          provisos(Bits#(addrA, addrWidthA), Bits#(dataA, dataWidthA),
//                   Bits#(addrB, addrWidthB), Bits#(dataB, dataWidthB),
//                   Bounded#(addrA), Bounded#(addrB),
//                   Add#(addrWidthA, aExtra, addrWidthB),
//                   Mul#(TExp#(aExtra), dataWidthB, dataWidthA),
//                   Mul#(dataBBytes, 8, dataWidthB),
//                   Div#(dataWidthB, dataBBytes, 8),
//                   Mul#(dataABytes, 8, dataWidthA),
//                   Div#(dataWidthA, dataABytes, 8),
//                   Mul#(TExp#(aExtra), dataBBytes, dataABytes),
//                   Literal#(dataA), Literal#(dataB),
//                   Add#(aExtra, TLog#(dataBBytes), TLog#(dataABytes))
//                   );
//
//   BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram1 <-
//     mkBlockRamPortableTrueMixedBEOpts_old(opts);
//   // BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram2 <-
//   //   mkBlockRamPortableTrueMixedBEOpts_new(opts);
//   //
//   // return opts.registerDataOut ? ram1 : ram2;
//   return ram1;
// endmodule


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

  `ifdef SIMULATE
  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamTrueMixedBE_BsvCtrlLogic(opts);
  `else // not SIMULATE
  BlockRamTrueMixedByteEn#(addrA, dataA, addrB, dataB, dataBBytes) ram <- mkBlockRamTrueMixedBE_BsvCtrlLogic(opts);
  `endif // not SIMULATE

  return ram;
endmodule

`endif // StratixV

// ====================================
// True dual-port same-width block RAM
// ====================================

module mkBlockRamTrueOpts#(BlockRamOpts opts) (BlockRamTrue#(addr, data))
      provisos(
      Bits#(data, dataWidth),
      Bits#(addr, addrWidth),
      Bounded#(addr)
      );

  // equal width TDP module. This does not require the data to be a mul of 8 bits wide.
  // The MixedTDP emulator operates bytewise, so use the underlying BRAM module.

  `ifdef SIMULATE
  BlockRamTrueMixed#(addr, data, addr, data) bram <- mkBlockRamTrueMixedOpts_SIMULATE(opts);
  `else
  BlockRamTrueMixed#(addr, data, addr, data) bram <- mkBlockRamTrueMixedOpts_ALTERA(opts);
  `endif
  return bram;

endmodule


endpackage
