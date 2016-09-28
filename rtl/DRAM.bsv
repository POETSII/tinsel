package DRAM;

// ============================================================================
// Interface
// ============================================================================

interface DRAM;
  interface In#(MemReq) reqIn;
  interface Out#(MemLoadResp) loadResp;
  interface Out#(MemStoreResp) storeResp;
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
import Assert    :: *;
import Interface :: *;

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
  InPort#(MemReq)        reqPort       <- mkInPort;
  OutPort#(MemStoreResp) storeRespPort <- mkOutPort;
  OutPort#(MemLoadResp)  loadRespPort  <- mkOutPort;

  // State
  Vector#(`DRAMLatency, Reg#(Bool)) valids <- replicateM(mkReg(False));
  Vector#(`DRAMLatency, Reg#(MemReq)) reqs <- replicateM(mkRegU);
  Reg#(Bool) toggle <- mkReg(False);
  Reg#(Bit#(`BurstWidth)) beat <- mkReg(0);
  Reg#(Bit#(32)) outstanding <- mkReg(0);

  // Wires
  Wire#(Bit#(32)) incOutstanding <- mkDWire(0);
  PulseWire decOutstanding1 <- mkPulseWire;
  PulseWire decOutstanding2 <- mkPulseWire;

  // Constants
  Integer endIndex = `DRAMLatency-1;
  Integer maxOutstanding = 2 ** `DRAMLogMaxInFlight;

  // Try to perform a request
  rule step;
    Bool shift = False;
    if (valids[0]) begin
      MemReq req = reqs[0];
      if (! req.isStore) begin
        if (loadRespPort.canPut) begin
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
          loadRespPort.put(resp);
          decOutstanding1.send;
        end
      end else begin
        dynamicAssert(req.burst == 1, "DRAM: burst writes not yet supported");
        if (storeRespPort.canPut) begin
          shift = True;
          Vector#(`WordsPerBeat, Bit#(32)) elems = unpack(req.data);
          Bit#(32) addr = {req.addr, 0};
          for (Integer i = 0; i < `WordsPerBeat; i=i+1)
            ramWrite(addr+fromInteger(4*i), elems[i]);
          MemStoreResp resp;
          resp.id = req.id;
          storeRespPort.put(resp);
          decOutstanding2.send;
        end
      end
    end
    // Insert a new request
    Bool insert = False;
    if (reqPort.canGet && (shift || !valids[endIndex])
                       && outstanding < fromInteger(maxOutstanding)) begin
      reqPort.get;
      reqs[endIndex] <= reqPort.value;
      insert = True;
      incOutstanding <= zeroExtend(reqPort.value.burst);
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
  interface In  reqIn           = reqPort.in;
  interface Out loadResp        = loadRespPort.out;
  interface Out storeResp       = storeRespPort.out;
  interface DRAMExtIfc external;
  endinterface
endmodule

`else

// ============================================================================
// Synthesis
// ============================================================================

// Imports
// -------

import Mem       :: *;
import Vector    :: *;
import Queue     :: *;
import Interface :: *;

// Types
// -----

// On FPGA, external interface is an Avalon master
(* always_ready, always_enabled *)
interface DRAMExtIfc;
  method Action m(
    Bit#(`BusWidth) readdata,
    Bool readdatavalid,
    Bool waitrequest,
    Bool writeresponsevalid,
    Bit#(2) response
  );
  method Bit#(`BusWidth) m_writedata;
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
  InPort#(MemReq)        reqPort       <- mkInPort;
  OutPort#(MemStoreResp) storeRespPort <- mkOutPort;
  OutPort#(MemLoadResp)  loadRespPort  <- mkOutPort;

  // Queues
  SizedQueue#(`DRAMLogMaxInFlight, DRAMInFlightReq) inFlight <-
    mkUGSizedQueuePrefetch;
  SizedQueue#(`DRAMLogMaxInFlight, Bit#(`BusWidth)) respBuffer <-
    mkUGSizedQueuePrefetch;

  // Registers
  Reg#(MemAddr) address <- mkRegU;
  Reg#(Bit#(`BusWidth)) writeData <- mkRegU;
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

  rule putLoadResp;
    if (loadRespPort.canPut && inFlight.canPeek &&
          inFlight.canDeq && !inFlight.dataOut.isStore &&
            respBuffer.canPeek && respBuffer.canDeq) begin
      consumeLoadResp.send;
      MemLoadResp resp;
      resp.id = inFlight.dataOut.id;
      resp.data = respBuffer.dataOut;
      loadRespPort.put(resp);
    end
  endrule

  rule putStoreResp;
    if (storeRespPort.canPut && inFlight.canPeek &&
          inFlight.canDeq && inFlight.dataOut.isStore &&
            respBuffer.canPeek && respBuffer.canDeq) begin
      consumeStoreResp.send;
      MemStoreResp resp;
      resp.id = inFlight.dataOut.id;
      storeRespPort.put(resp);
    end
  endrule

  // Internal interface
  interface In  reqIn           = reqPort.in;
  interface Out loadResp        = loadRespPort.out;
  interface Out storeResp       = storeRespPort.out;

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

endpackage
