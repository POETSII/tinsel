package DRAM;

// ============================================================================
// Interface
// ============================================================================

interface DRAM;
  interface MemDualResp internal;
  interface DRAMExtIfc external;
endinterface

`ifdef SIMULATE

// ============================================================================
// Simulation
// ============================================================================

// Imports
// -------

import Mem    :: *;
import FIFOF  :: *;
import Vector :: *;
import Assert :: *;

// Interface to C functions
import "BDPI" function Action ramInit();
import "BDPI" function Action ramWrite(Bit#(32) addr, Bit#(32) data);
import "BDPI" function ActionValue#(Bit#(32)) ramRead(Bit#(32) addr);

// Types
// -----

// In simulation, external interface is empty
typedef Empty DRAMExtIfc;

// Implementation
// --------------

module mkDRAM (DRAM);
  // State
  Vector#(`DRAMLatency, Reg#(Bool)) valids <- replicateM(mkReg(False));
  Vector#(`DRAMLatency, Reg#(MemReq)) reqs <- replicateM(mkRegU);
  FIFOF#(MemReq)       reqFifo       <- mkUGFIFOF;
  FIFOF#(MemStoreResp) storeRespFifo <- mkUGFIFOF;
  FIFOF#(MemLoadResp)  loadRespFifo  <- mkUGFIFOF;
  Reg#(Bool) toggle <- mkReg(False);
  Reg#(Bit#(32)) outstanding <- mkReg(0);
  Reg#(Bit#(`BurstWidth)) beat <- mkReg(0);

  // Wires
  Wire#(Bit#(32)) incOutstanding <- mkDWire(0);
  PulseWire decOutstanding1 <- mkPulseWire;
  PulseWire decOutstanding2 <- mkPulseWire;

  // Constants
  Integer endIndex = `DRAMLatency-1;
  Integer maxOutstanding = `DRAMLogMaxInFlight;

  // Try to perform a request
  rule step;
    Bool shift = False;
    if (valids[0]) begin
      MemReq req = reqs[0];
      if (! req.isStore) begin
        if (loadRespFifo.notFull) begin
          if (beat+1 == req.burst) begin
            shift = True;
            beat <= 0;
          end else
            beat <= beat+1;
          Vector#(`WordsPerBeat, Bit#(32)) elems;
          Bit#(32) addr = {req.addr, 0};
          for (Integer i = 0; i < `WordsPerBeat; i=i+1) begin
            let val <- ramRead(addr + zeroExtend(beat)*`BytesPerBeat +
                         fromInteger(4*i));
            elems[i] = val;
          end
          MemLoadResp resp;
          resp.id = req.id;
          resp.data = pack(elems);
          loadRespFifo.enq(resp);
        end
      end else begin
        dynamicAssert(req.burst == 1, "DRAM: burst writes not yet supported");
        if (storeRespFifo.notFull) begin
          shift = True;
          Vector#(`WordsPerBeat, Bit#(32)) elems = unpack(req.data);
          Bit#(32) addr = {req.addr, 0};
          for (Integer i = 0; i < `WordsPerBeat; i=i+1)
            ramWrite(addr+fromInteger(4*i), elems[i]);
          MemStoreResp resp;
          resp.id = req.id;
          storeRespFifo.enq(resp);
        end
      end
    end
    // Insert a new request
    Bool insert = False;
    if (reqFifo.notEmpty && (shift || !valids[endIndex])) begin
      reqFifo.deq;
      reqs[endIndex] <= reqFifo.first;
      insert = True;
    end
    // Shift requests
    for (Integer i = 0; i < endIndex; i=i+1) begin
      shift = shift || !valids[i];
      if (shift) begin
        reqs[i] <= reqs[i+1];
        valids[i] <= valids[i+1];
      end
    end
    if (insert) valids[endIndex] <= True;
    else if (shift) valids[endIndex] <= False;
  endrule

  // Track number of outstanding requests
  rule countOutstanding;
    let count = outstanding + incOutstanding;
    if (decOutstanding1) count = count-1;
    if (decOutstanding2) count = count-1;
    outstanding <= count;
  endrule

  // Interfaces
  interface MemDualResp internal;
    interface Req req;
      method Bool canPut = reqFifo.notFull &&
                    outstanding < fromInteger(maxOutstanding);
      method Action put(MemReq req);
        incOutstanding <= zeroExtend(req.burst);
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
  endinterface
  
  interface DRAMExtIfc external;
  endinterface
endmodule

`else

// ============================================================================
// Synthesis
// ============================================================================

// Imports
// -------

import Mem    :: *;
import Vector :: *;
import Queue  :: *;

// Types
// -----

// On FPGA, external interface is an Avalon master
(* always_ready, always_enabled *)
interface DRAMExtIfc;
  method Action m0(
    Bit#(`BusWidth) readdata,
    Bool readdatavalid,
    Bool waitrequest,
    Bool writeresponsevalid,
    Bit#(2) response
  );
  method Bit#(`BusWidth) m0_writedata;
  method Bit#(`DRAMAddrWidth) m0_address;
  method Bool m0_read;
  method Bool m0_write;
  method Bit#(`BurstWidth) m0_burstcount;
endinterface

// In-flight request
typedef struct {
  DCacheId id;
  Bool isStore;
} DRAMInFlightReq deriving (Bits);

// Implementation
// --------------

module mkDRAM (DRAM);
  // Queues
`ifdef DRAMPortHalfThroughput
  SizedQueue#(`DRAMLogMaxInFlight, DRAMInFlightReq) inFlight <-
    mkUGSizedQueue;
  SizedQueue#(`DRAMLogMaxInFlight, Bit#(`BusWidth)) respBuffer <-
    mkUGSizedQueue;
`else
  SizedQueue#(`DRAMLogMaxInFlight, DRAMInFlightReq) inFlight <-
    mkUGSizedQueuePrefetch;
  SizedQueue#(`DRAMLogMaxInFlight, Bit#(`BusWidth)) respBuffer <-
    mkUGSizedQueuePrefetch;
`endif

  // Registers
  Reg#(MemAddr) address <- mkRegU;
  Reg#(Bit#(`BusWidth)) writeData <- mkRegU;
  Reg#(Bool) doRead <- mkReg(False);
  Reg#(Bool) doWrite <- mkReg(False);

  // Wires
  Wire#(Bool) waitRequest <- mkBypassWire;
  PulseWire putLoad <- mkPulseWire;
  PulseWire putStore <- mkPulseWire;
  PulseWire consumeLoadResp <- mkPulseWire;
  PulseWire consumeStoreResp <- mkPulseWire;

  // Rules
  rule consumeResponse (consumeLoadResp || consumeStoreResp);
    inFlight.deq;
    respBuffer.deq;
  endrule

  rule putRequest;
    if (putLoad) begin
      doRead <= True;
      doWrite <= False;
    end else if (putStore) begin
      doRead <= False;
      doWrite <= True;
    end else if (!waitRequest) begin
      doRead <= False;
      doWrite <= False;
    end
  endrule

  // Internal interface
  interface MemDualResp internal;
    interface Req req;
      method Bool canPut = !waitRequest && inFlight.notFull;
      method Action put(MemReq req);
        address   <= req.addr;
        writeData <= req.data;
        if (req.isStore) putStore.send; else putLoad.send;
        DRAMInFlightReq inflightReq;
        inflightReq.id = req.id;
        inflightReq.isStore = req.isStore;
        inFlight.enq(inflightReq);
      endmethod
    endinterface

    interface Resp loadResp;
      method Bool canGet =
        inFlight.canPeek && inFlight.canDeq && !inFlight.dataOut.isStore &&
          respBuffer.canPeek && respBuffer.canDeq;
      method ActionValue#(MemLoadResp) get;
        consumeLoadResp.send;
        MemLoadResp resp;
        resp.id = inFlight.dataOut.id;
        resp.data = respBuffer.dataOut;
        return resp;
      endmethod
    endinterface

    interface Resp storeResp;
      method Bool canGet =
        inFlight.canPeek && inFlight.canDeq && inFlight.dataOut.isStore &&
          respBuffer.canPeek && respBuffer.canDeq;
      method ActionValue#(MemStoreResp) get;
        consumeStoreResp.send;
        MemStoreResp resp;
        resp.id = inFlight.dataOut.id;
        return resp;
      endmethod
    endinterface
  endinterface

  // External (Avalon master) interface
  interface DRAMExtIfc external;
    method Action m0(readdata,readdatavalid,waitrequest,
                       writeresponsevalid, response);
      if (readdatavalid || writeresponsevalid) respBuffer.enq(readdata);
      waitRequest <= waitrequest;
    endmethod
    method m0_writedata  = writeData;
    method m0_address;
      Bit#(32) byteAddress = {address, 0};
      return truncateLSB(byteAddress);
    endmethod
    method m0_read       = doRead;
    method m0_write      = doWrite;
    method m0_burstcount = 1;
  endinterface
endmodule

`endif

endpackage
