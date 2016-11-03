package DRAM;

// ============================================================================
// Interface
// ============================================================================

interface DRAM;
  interface In#(MemReq) reqIn;
  interface BOut#(MemLoadResp) loadResp;
  interface BOut#(MemStoreResp) storeResp;
  interface DRAMExtIfc external;
endinterface

`ifdef SIMULATE

// ============================================================================
// Simulation
// ============================================================================

// Imports
// -------

import Globals   :: *;
import FIFOF     :: *;
import Vector    :: *;
import Util      :: *;
import Interface :: *;
import DCache    :: *;
import Queue     :: *;

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
  // Ports
  InPort#(MemReq) reqPort <- mkInPort;

  // State
  SizedQueue#(`DRAMLatency, MemReq) reqs <- mkUGShiftQueueCore(QueueOptFmax);
  Reg#(Bit#(`BurstWidth)) beat <- mkReg(0);
  Reg#(Bit#(32)) outstanding <- mkReg(0);

  // Wires
  Wire#(Bit#(32)) incOutstanding <- mkDWire(0);
  PulseWire decOutstanding1 <- mkPulseWire;
  PulseWire decOutstanding2 <- mkPulseWire;

  // Response buffers
  FIFOF#(MemLoadResp)  loadResps  <- mkUGSizedFIFOF(16);
  FIFOF#(MemStoreResp) storeResps <- mkUGSizedFIFOF(16);

  // Constants
  Integer maxOutstanding = 2 ** `DRAMLogMaxInFlight;

  rule step;
    // Try to perform a request
    if (reqs.canDeq) begin
      MemReq req = reqs.dataOut;
      if (! req.isStore) begin
        if (loadResps.notFull) begin
          if (beat+1 == req.burst) begin
            reqs.deq;
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
          loadResps.enq(resp);
          decOutstanding1.send;
        end
      end else begin
        myAssert(req.burst == 1, "DRAM: burst writes not yet supported");
        if (storeResps.notFull) begin
          reqs.deq;
          Vector#(`WordsPerBeat, Bit#(32)) elems = unpack(req.data);
          Bit#(32) addr = {req.addr, 0};
          for (Integer i = 0; i < `WordsPerBeat; i=i+1)
            ramWrite(addr+fromInteger(4*i), elems[i]);
          MemStoreResp resp;
          resp.id = req.id;
          storeResps.enq(resp);
          decOutstanding2.send;
        end
      end
    end
    // Insert a new request
    if (reqPort.canGet && reqs.notFull &&
          outstanding < fromInteger(maxOutstanding)) begin
      reqPort.get;
      reqs.enq(reqPort.value);
      incOutstanding <= zeroExtend(reqPort.value.burst);
    end
  endrule

  // Track number of outstanding requests
  rule countOutstanding;
    let count = outstanding + incOutstanding;
    if (decOutstanding1) count = count-1;
    if (decOutstanding2) count = count-1;
    outstanding <= count;
  endrule

  // Interfaces
  interface In reqIn = reqPort.in;

  interface BOut loadResp;
    method Action get;
      loadResps.deq;
    endmethod
    method Bool valid = loadResps.notEmpty;
    method MemLoadResp value = loadResps.first;
  endinterface

  interface BOut storeResp;
    method Action get;
      storeResps.deq;
    endmethod
    method Bool valid = storeResps.notEmpty;
    method MemStoreResp value = storeResps.first;
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

import Globals   :: *;
import Vector    :: *;
import Queue     :: *;
import Interface :: *;
import DCache    :: *;

// Types
// -----

// On FPGA, external interface is an Avalon master
(* always_ready, always_enabled *)
interface DRAMExtIfc;
  method Action m(
    Bit#(`BeatWidth) readdata,
    Bool readdatavalid,
    Bool waitrequest,
    Bool writeresponsevalid,
    Bit#(2) response
  );
  method Bit#(`BeatWidth) m_writedata;
  method Bit#(`DRAMAddrWidth) m_address;
  method Bool m_read;
  method Bool m_write;
  method Bit#(`BurstWidth) m_burstcount;
endinterface

// In-flight request
typedef struct {
  DCacheId id;
  Bool isStore;
} DRAMInFlightReq deriving (Bits);

// Implementation
// --------------

module mkDRAM (DRAM);
  // Ports
  InPort#(MemReq) reqPort <- mkInPort;

  // Queues
  SizedQueue#(`DRAMLogMaxInFlight, DRAMInFlightReq) inFlight <-
    mkUGSizedQueuePrefetch;
  SizedQueue#(`DRAMLogMaxInFlight, Bit#(`BeatWidth)) respBuffer <-
    mkUGSizedQueuePrefetch;

  // Registers
  Reg#(MemAddr) address <- mkRegU;
  Reg#(Bit#(`BeatWidth)) writeData <- mkRegU;
  Reg#(Bool) doRead <- mkReg(False);
  Reg#(Bool) doWrite <- mkReg(False);
  Reg#(Bit#(`BurstWidth)) burstReg <- mkReg(0);

  // Wires
  Wire#(Bool) waitRequest <- mkBypassWire;
  PulseWire putLoad <- mkPulseWire;
  Wire#(Bit#(`BurstWidth)) burstWire <- mkDWire(0);
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
      burstReg <= burstWire;
    end else if (putStore) begin
      doRead <= False;
      doWrite <= True;
      burstReg <= burstWire;
    end else if (!waitRequest) begin
      doRead <= False;
      doWrite <= False;
      burstReg <= 0;
    end
  endrule

  rule consumeRequest;
    if (reqPort.canGet && !waitRequest && inFlight.notFull) begin
      MemReq req = reqPort.value;
      reqPort.get;
      address   <= req.addr;
      writeData <= req.data;
      if (req.isStore) putStore.send; else putLoad.send;
      burstWire <= req.burst;
      DRAMInFlightReq inflightReq;
      inflightReq.id = req.id;
      inflightReq.isStore = req.isStore;
      inFlight.enq(inflightReq);
    end
  endrule

  // Internal interfaces
  interface In reqIn = reqPort.in;

  interface BOut loadResp;
    method Action get;
      consumeLoadResp.send;
    endmethod
    method Bool valid = inFlight.canPeek && inFlight.canDeq &&
                          respBuffer.canPeek && respBuffer.canDeq &&
                            !inFlight.dataOut.isStore;
    method MemLoadResp value;
      MemLoadResp resp;
      resp.id = inFlight.dataOut.id;
      resp.data = respBuffer.dataOut;
      return resp;
    endmethod
  endinterface

  interface BOut storeResp;
    method Action get;
      consumeStoreResp.send;
    endmethod
    method Bool valid = inFlight.canPeek && inFlight.canDeq &&
                          respBuffer.canPeek && respBuffer.canDeq &&
                            inFlight.dataOut.isStore;
    method MemStoreResp value;
      MemStoreResp resp;
      resp.id = inFlight.dataOut.id;
      return resp;
    endmethod
  endinterface

  // External (Avalon master) interface
  interface DRAMExtIfc external;
    method Action m(readdata,readdatavalid,waitrequest,
                       writeresponsevalid, response);
      if (readdatavalid || writeresponsevalid) respBuffer.enq(readdata);
      waitRequest <= waitrequest;
    endmethod
    method m_writedata  = writeData;
    method m_address;
      Bit#(32) byteAddress = {address, 0};
      return truncateLSB(byteAddress);
    endmethod
    method m_read       = doRead;
    method m_write      = doWrite;
    method m_burstcount = burstReg;
  endinterface
endmodule

`endif

// ============================================================================
// Connect data caches to DRAM
// ============================================================================

// Connect vector of data caches to DRAM
module connectDCachesToDRAM#(
         Vector#(`DCachesPerDRAM, DCache) caches, DRAM dram) ();

  // Connect requests
  function getReqOut(cache) = cache.reqOut;
  let dramReqs <- mkMergeTree(Fair,
                    mkUGShiftQueue1(QueueOptFmax),
                    map(getReqOut, caches));
  connectUsing(mkUGQueue, dramReqs, dram.reqIn);

  // Connect load responses
  function DCacheId getLoadRespKey(MemLoadResp resp) = resp.id;
  function getLoadRespIn(cache) = cache.loadRespIn;
  let dramLoadResps <- mkResponseDistributor(
                        getLoadRespKey,
                        mkUGShiftQueue1(QueueOptFmax),
                        map(getLoadRespIn, caches));
  connectDirect(dram.loadResp, dramLoadResps);

  // Connect store responses
  function DCacheId getStoreRespKey(MemStoreResp resp) = resp.id;
  function getStoreRespIn(cache) = cache.storeRespIn;
  let dramStoreResps <- mkResponseDistributor(
                         getStoreRespKey,
                         mkUGShiftQueue1(QueueOptFmax),
                         map(getStoreRespIn, caches));
  connectDirect(dram.storeResp, dramStoreResps);

endmodule

endpackage
