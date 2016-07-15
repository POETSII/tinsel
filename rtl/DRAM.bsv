package DRAM;

`ifdef SIMULATE

// ============================================================================
// Simulation
// ============================================================================

// Imports
// -------

import Mem    :: *;
import FIFOF  :: *;
import Vector :: *;

// Interface to C functions
import "BDPI" function Action ramInit();
import "BDPI" function Action ramWrite(Bit#(32) addr, Bit#(32) data);
import "BDPI" function ActionValue#(Bit#(32)) ramRead(Bit#(32) addr);

// Implementation
// --------------

module mkDRAM (MemDualResp);
  // State
  Vector#(`DRAMLatency, Reg#(Bool)) valids <- replicateM(mkReg(False));
  Vector#(`DRAMLatency, Reg#(MemReq)) reqs <- replicateM(mkRegU);
  FIFOF#(MemReq)       reqFifo       <- mkUGFIFOF;
  FIFOF#(MemStoreResp) storeRespFifo <- mkUGFIFOF;
  FIFOF#(MemLoadResp)  loadRespFifo  <- mkUGFIFOF;
  Reg#(Bool) toggle <- mkReg(False);
  Reg#(Bit#(32)) outstanding <- mkReg(0);

  // Wires
  PulseWire incOutstanding <- mkPulseWire;
  PulseWire decOutstanding1 <- mkPulseWire;
  PulseWire decOutstanding2 <- mkPulseWire;

  // Constants
  Integer endIndex = `DRAMLatency-1;
  Integer wordsPerLine = `LineSize/32;
  Integer maxOutstanding = `DRAMPipelineLen;

  // Try to perform a request
  rule step;
    Bool shift = False;
    if (valids[0]) begin
      MemReq req = reqs[0];
      if (! req.isStore) begin
        if (loadRespFifo.notFull) begin
          shift = True;
          Vector#(`WordsPerLine, Bit#(32)) elems;
          Bit#(32) addr = {req.addr, 0};
          for (Integer i = 0; i < `WordsPerLine; i=i+1) begin
            let val <- ramRead(addr+fromInteger(4*i));
            elems[i] = val;
          end
          MemLoadResp resp;
          resp.id = req.id;
          resp.data = pack(elems);
          loadRespFifo.enq(resp);
        end
      end else begin
        if (storeRespFifo.notFull) begin
          shift = True;
          Vector#(`WordsPerLine, Bit#(32)) elems = unpack(req.data);
          Bit#(32) addr = {req.addr, 0};
          for (Integer i = 0; i < `WordsPerLine; i=i+1)
            ramWrite(addr+fromInteger(4*i), elems[i]);
          MemStoreResp resp = ?;
          resp.id = req.id;
          storeRespFifo.enq(resp);
        end
      end
    end
    // Insert a new request
    if (reqFifo.notEmpty && (shift || !valids[endIndex])) begin
      reqFifo.deq;
      reqs[endIndex] <= reqFifo.first;
      valids[endIndex] <= True;
    end else
      valids[endIndex] <= False;
    // Shift requests
    for (Integer i = 0; i < endIndex; i=i+1) begin
      shift = shift || !valids[i];
      if (shift) begin
        reqs[i] <= reqs[i+1];
        valids[i] <= valids[i+1];
      end
    end
  endrule

  // Track number of outstanding requests
  rule countOutstanding;
    let count = outstanding;
    if (incOutstanding) count = count+1;
    if (decOutstanding1) count = count-1;
    if (decOutstanding2) count = count-1;
    outstanding <= count;
  endrule

  // Interfaces
  interface Req req;
    method Bool canPut = reqFifo.notFull &&
                  outstanding < fromInteger(maxOutstanding);
    method Action put(MemReq req);
      incOutstanding.send;
      reqFifo.enq(req);
    endmethod
  endinterface

  interface Resp loadResp;
    method Bool canGet = loadRespFifo.notEmpty;
    method ActionValue#(MemLoadResp) get;
      decOutstanding1.send;
      loadRespFifo.deq; return loadRespFifo.first;
    endmethod
  endinterface

  interface Resp storeResp;
    method Bool canGet = storeRespFifo.notEmpty;
    method ActionValue#(MemStoreResp) get;
      decOutstanding2.send;
      storeRespFifo.deq; return storeRespFifo.first;
    endmethod
  endinterface
endmodule

`else

// ============================================================================
// Synthesis
// ============================================================================

`endif

endpackage
