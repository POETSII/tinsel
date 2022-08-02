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

package BlockRAMv;

import Vector  :: *;
import Assert  :: *;
import RegFile :: *;
import DReg    :: *;

// ==========
// Interfaces
// ==========

// Basic dual-port block RAM with a read port and a write port
interface BlockRam#(type addr, type data);
  method Action write(addr a, data d);  // write data (d) to address (a)
  method Action read(addr a);           // initiate read from address (a)
  method data dataOut;                  // read result returned
  method Bool dataOutValid;             // True when read result available
endinterface


// True dual-port block RAM: ports A and B can write or read independently
interface BlockRamTrueDualPort#(type addr, type data);
  // Port A
  method Action putA(Bool we, addr a, data d); // initiate read or write (not both)
  method data dataOutA;                        // read result returned
  method Bool dataOutValidA;                   // True when read result available
  // Port B
  method Action putB(Bool we, addr a, data d); // initiate read or write (not both)
  method data dataOutB;                        // read result returned
  method Bool dataOutValidB;                   // True when read result available
endinterface

//function getDataOutValidA(BlockRamTrueDualPort#(addr,data) ram) = ram.dataOutValidA;
//function getDataOutValidB(BlockRamTrueDualPort#(addr,data) ram) = ram.dataOutValidB;


// =====================
// Verilog Instatiations
// =====================

`ifndef SIMULATE
import "BVI" VerilogBlockRAM_OneCycle =
  module mkBlockRAM_Verilog(BlockRam#(addr, data))
         provisos(Bits#(addr, addrWidth),
                  Bits#(data, dataWidth));

    parameter ADDR_WIDTH     = valueOf(addrWidth);
    parameter DATA_WIDTH     = valueOf(dataWidth);

    method write(WR_ADDR, DI) enable (WE) clocked_by(clk);
    method read(RD_ADDR) enable (RE) clocked_by(clk);
    method DO dataOut;
    method DO_VALID dataOutValid;

    default_clock clk(CLK, (*unused*) clk_gate);
    default_reset no_reset;

    schedule (dataOut)      CF (dataOut, dataOutValid, read, write);
    schedule (dataOutValid) CF (dataOut, dataOutValid, read, write);
    schedule (read)         CF (write);
    schedule (write)        C  (write);
    schedule (read)         C  (read);
  endmodule
`endif


`ifndef SIMULATE
// Verilog true dual-port block RAM for Verilog simulation and synthesis
import "BVI" VerilogBlockRAM_TrueDualPort_OneCycle =
  module mkDualPortBlockRAM_Verilog(BlockRamTrueDualPort#(addr, data))
         provisos(Bits#(addr, addrWidth),
                  Bits#(data, dataWidth));

    parameter ADDR_WIDTH     = valueOf(addrWidth);
    parameter DATA_WIDTH     = valueOf(dataWidth);

    method putA(WE_A, ADDR_A, DI_A) enable (EN_A) clocked_by(clk);
    method DO_A dataOutA;
    method DO_VALID_A dataOutValidA;

    method putB(WE_B, ADDR_B, DI_B) enable (EN_B) clocked_by(clk);
    method DO_B dataOutB;
    method DO_VALID_B dataOutValidB;

    default_clock clk(CLK, (*unused*) clk_gate);
    default_reset no_reset;

    schedule (dataOutA)      CF (dataOutA, dataOutB, putA, putB);
    schedule (dataOutB)      CF (dataOutA, dataOutB, putA, putB);
    schedule (dataOutValidA) CF (dataOutValidA,dataOutValidB,dataOutA,dataOutB,putA,putB);
    schedule (dataOutValidB) CF (dataOutValidA,dataOutValidB,dataOutA,dataOutB,putA,putB);
    schedule (putA)          CF (putB);
    schedule (putB)          CF (putA);
    schedule (putA)          C  (putA);
    schedule (putB)          C  (putB);
  endmodule
`endif


// ===========================================
// Bluespec module memory primates for Bluesim
// ===========================================

`ifdef SIMULATE
module mkBlockRAM_Bluesim(BlockRam#(addr, data))
  provisos(Bits#(addr, addrWidth),
           Bits#(data, dataWidth),
           Bounded#(addr));

  RegFile#(addr, data)    ram <- mkRegFileFull;
  Reg#(data)       dataOutReg <- mkReg(unpack(0));
  Reg#(Bool)  dataOutValidReg <- mkDReg(False);

  method Action write(addr a, data d) = ram.upd(a, d);
  method Action read(addr a);
    dataOutReg <= ram.sub(a);
    dataOutValidReg <= True;
  endmethod
  method data dataOut = dataOutReg;
  method Bool dataOutValid = dataOutValidReg;
endmodule
`endif

`ifdef SIMULATE
// Matt Naylor's dual-port BRAM using RegFile, for simulation only
module mkDualPortBlockRAM_Bluesim(BlockRamTrueDualPort#(addr, data))
  provisos(Bits#(addr, addrWidth),
           Bits#(data, dataWidth),
           Bounded#(addr),
           Literal#(data));

  RegFile#(addr, data) regFileA <- mkRegFileFull;
  RegFile#(addr, data) regFileB <- mkRegFileFull;
  RegFile#(addr, Bit#(64)) regFileALastWriteTime <- mkRegFileFull;
  RegFile#(addr, Bit#(64)) regFileBLastWriteTime <- mkRegFileFull;
  Reg#(Bit#(64)) timer <- mkReg(64'haaaaaaaaaaaaaaab); // gross hack due to the above RegFiles being initialsied to this value
  Reg#(data) dataOutAReg <- mkReg(0);
  Reg#(data) dataOutBReg <- mkReg(0);
  Reg#(Bool) dataOutValidAReg <- mkDReg(False);
  Reg#(Bool) dataOutValidBReg <- mkDReg(False);

  rule updateTimer;
    timer <= timer + 1;
    dynamicAssert(timer < 64'hffffffff_ffffffff,
      "End of timer lifetime.  Panic!");
  endrule

  method Action putA(Bool we, addr a, data d);
    if (we)
      begin
        regFileA.upd(a, d);
        regFileALastWriteTime.upd(a, timer);
      end
    else
      dataOutValidAReg <= True;
    dataOutAReg <=
      regFileALastWriteTime.sub(a) >= regFileBLastWriteTime.sub(a) ?
        regFileA.sub(a) : regFileB.sub(a);
  endmethod
  method data dataOutA = dataOutAReg;
  method Bool dataOutValidA = dataOutValidAReg;

  method Action putB(Bool we, addr a, data d);
    if (we)
      begin
        regFileB.upd(a, d);
        regFileBLastWriteTime.upd(a, timer);
      end
    else
      dataOutValidBReg <= True;
    dataOutBReg <=
      regFileALastWriteTime.sub(a) >= regFileBLastWriteTime.sub(a) ?
        regFileA.sub(a) : regFileB.sub(a);
  endmethod

  method data dataOutB = dataOutBReg;
  method Bool dataOutValidB = dataOutValidBReg;

endmodule
`endif

// ================================================
// Select memory primative based on simulation mode
// ================================================

module mkBlockRAM_bsvMux(BlockRam#(addr, data))
  provisos(Bits#(addr, addrWidth),
           Bits#(data, dataWidth),
           Bounded#(addr));
`ifdef SIMULATE
  BlockRam#(addr,data) ram <- mkBlockRAM_Bluesim;
`else
  BlockRam#(addr,data) ram <- mkBlockRAM_Verilog;
`endif
  method write = ram.write;
  method read = ram.read;
  method dataOut = ram.dataOut;
  method dataOutValid = ram.dataOutValid;
endmodule


module mkDualPortBlockRAM(BlockRamTrueDualPort#(addr, data))
  provisos(Bits#(addr, addrWidth),
           Bits#(data, dataWidth),
           Bounded#(addr),
           Literal#(data));

`ifdef SIMULATE
  BlockRamTrueDualPort#(addr,data) ram <- mkDualPortBlockRAM_Bluesim;
`else
  BlockRamTrueDualPort#(addr,data) ram <- mkDualPortBlockRAM_Verilog;
`endif
return ram;
  // method putA = ram.putA;
  // method dataOutA = ram.dataOutA;
  // method dataOutValidA = ram.dataOutValidA;
  // method putB = ram.putB;
  // method dataOutB = ram.dataOutB;
  // method dataOutValidB = ram.dataOutValidB;
endmodule

// =====================================================
// Bluespec modules that use the Verilog BRAM primatives
// =====================================================

// True dual-port mixed-width block RAM with byte-enables
// (Port B has the byte enables and must be smaller than port A)

interface BlockRamTrueMixedByteEn_bsvMux#
            (type addrA, type dataA,
             type addrB, type dataB,
             numeric type dataBBytes);
  // Port A
  method Action putA(Bool wr, addrA a, dataA d);
  method dataA dataOutA;
  method Bool dataOutValidA; // added to original interface to indicate when data is available
  // Port B
  method Action putB(Bool wr, addrB a, dataB d, Bit#(dataBBytes) be);
  method dataB dataOutB;
  method Bool dataOutValidB; // added to original interface to indicate when data is available
endinterface


module mkBlockRamTrueMixedBE_bsvMux
      (BlockRamTrueMixedByteEn_bsvMux#(addrA, dataA, addrB, dataB, dataBBytes))
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
             Add#(aExtra, logdataBBytes, logdataABytes),
             Literal#(dataB), Literal#(addrB));

  // Instatitate byte-wide RAMs to fit the data width of port A since it is the widest port
  Vector#(dataABytes, BlockRamTrueDualPort#(Bit#(awidthA), Bit#(8))) rams <- replicateM(mkDualPortBlockRAM);

  // addrB needed during read to select the right word.
  // save_* are required to get NewData RdW read logic.
  Reg#(Bool) save_wrA <- mkReg(False);
  Reg#(dataA) save_dataA <- mkRegU();
  Wire#(Maybe#(addrA)) check_addrA <- mkDWire(Invalid);


  // we save both the current read addr and the list written addr for B.
  // this allows to reconstruct NewData RdW for partial writes, until a
  // new location is read.
  Wire#(Tuple4#(Bool, addrB, dataB, Bit#(dataBBytes))) putB_args <- mkDWire( tuple4(False, 0, 0, 0) );
  Reg#(Bool) save_wrB <- mkReg(False);
  Reg#(Vector#(dataBBytes, Bool)) save_enB <- mkReg(unpack(0));
  Reg#(dataB) save_dataB <- mkRegU();
  Reg#(addrB) save_addrB <- mkReg(unpack(0));
  Reg#(addrB) save_lastwr_addrB <- mkReg(unpack(0));
  Wire#(Maybe#(addrB)) check_addrB <- mkDWire(Invalid);
  Wire#(dataB) bout <- mkWire();

  // For simulation only. Should be optimised away on FPGA.
  rule assert_no_write_collision(isValid(check_addrA) && isValid(check_addrB));
    Bit#(awidthA) addrA = pack(fromMaybe(?, check_addrA));
    Bit#(awidthA) addrB = truncate(pack(fromMaybe(?, check_addrB))>>valueOf(aExtra));
    dynamicAssert(addrA != addrB, "ERROR in mkBlockRamTrueMixedBE: address collision on two writes");
  endrule

  // in order to have the compier check put* and dataout* are always_enabled,
  // we keep the bodies in dedicated rules.
  rule dinB_r;
    let wr = tpl_1(putB_args);
    let a  = tpl_2(putB_args);
    let d  = tpl_3(putB_args);
    let be = tpl_4(putB_args);
    save_addrB <= a;
    save_wrB <= wr;
    if (wr) begin
      save_dataB <= d;
      save_enB <= unpack(be);
      save_lastwr_addrB <= a;
    end
    for(Integer n=0; n<valueOf(dataBBytes); n=n+1)
      if(be[n]==1)
        begin
          Bit#(aExtra) bank_select = truncate(pack(a));
          Bit#(logdataBBytes) byte_select = fromInteger(n);
          Bit#(logdataABytes) bram_select = {bank_select, byte_select};
          Bit#(awidthA) addr = truncate(unpack(pack(a) >> valueOf(aExtra)));
          Bit#(dwidthB) data = pack(d);
          rams[bram_select].putB(wr, addr, data[n*8+7:n*8]);
        end
    if(wr) check_addrB <= tagged Valid a;
  endrule


  // putB needs to be always_enabled, so we stay in sync with the model.
  // split into a trival method and a rule without preconditions to ensure this.
  (* fire_when_enabled, no_implicit_conditions *)
  rule doutB_r;
    Vector#(dataBBytes,Bit#(8)) b;
    Bit#(dwidthB) data = pack(save_dataB);
    for(Integer n=0; n<valueOf(dataBBytes); n=n+1) begin
      Bit#(aExtra) bank_select = truncate(pack(save_addrB));
      Bit#(logdataBBytes) byte_select = fromInteger(n);
      Bit#(logdataABytes) bram_select = {bank_select, byte_select};
      if (pack(save_lastwr_addrB) == pack(save_addrB) && save_enB[n]) begin
        // $display($time, " [mkBlockRamTrueMixedBE_bsvMux::doutB_r] using bypasssed word at pos ", n, " v: %x", data);
        b[n] = data[n*8+7:n*8];
        // b[n] = 0;
      end else begin
        b[n] = rams[bram_select].dataOutA;
      end
    end
    // $display($time, " [mkBlockRamTrueMixedBE_bsvMux::doutB_r] final bytes to return %x", b);
    bout <= unpack(pack(b));
  endrule

  method Action putA(wr, a, d);
    save_wrA <= wr;
    save_dataA <= d;
    Bit#(dwidthA) data = pack(d);
    for(Integer n=0; n<valueOf(dataABytes); n=n+1)
        rams[n].putA(wr, pack(a), data[n*8+7:n*8]);
    if(wr) check_addrA <= tagged Valid a;
  endmethod

  method dataA dataOutA;
    if (save_wrA) begin
      return save_dataA;
    end else begin
      Vector#(dataABytes,Bit#(8)) b;
      for(Integer n=0; n<valueOf(dataABytes); n=n+1)
        begin
          b[n] = rams[n].dataOutA;
           let v = rams[n].dataOutValidA;
         end
      return unpack(pack(b));
    end
  endmethod

  method Action putB(wr, a, d, be);
    putB_args <= tuple4(wr, a, d, be);
  endmethod

  method dataB dataOutB;
    return bout;
  endmethod

  // just return valid from first RAM since the timing for all of them is the same and all read?
  method Bool dataOutValidA = rams[0].dataOutValidA;
  method Bool dataOutValidB = rams[0].dataOutValidB;

endmodule

endpackage: BlockRAMv
