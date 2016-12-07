package DRAM;

// ============================================================================
// Types
// ============================================================================

// DRAM request id
typedef DCacheId DRAMReqId;

// DRAM address
typedef TSub#(30, `LogDRAMWidthInWords) DRAMAddrNumBits;
typedef Bit#(DRAMAddrNumBits) DRAMAddr;

// DRAM request
typedef struct {
  Bool isStore;
  DRAMReqId id;
  DRAMAddr addr;
  Bit#(`DRAMWidth) data;
  Bit#(`DRAMBurstWidth) burst;
  Bit#(`DRAMWidthInBytes) byteEn;
} DRAMReq deriving (Bits);

// DRAM load response
typedef struct {
  DRAMReqId id;
  Bit#(`DRAMWidth) data;
} DRAMLoadResp deriving (Bits);

// DRAM store response
typedef struct {
  DRAMReqId id;
} DRAMStoreResp deriving (Bits);

// ============================================================================
// Interface
// ============================================================================

interface DRAM;
  interface In#(DRAMReq) reqIn;
  interface BOut#(DRAMLoadResp) loadResp;
  interface BOut#(DRAMStoreResp) storeResp;
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

// Types
// -----

// In simulation, external interface is empty
typedef Empty DRAMExtIfc;

// DRAM identifier
typedef Bit#(3) DRAMId;

// Interface to C functions
import "BDPI" function ActionValue#(Bit#(32)) ramRead(
                DRAMId ramId, Bit#(32) addr);
import "BDPI" function Action ramWrite(DRAMId ramId,
                Bit#(32) addr, Bit#(32) data, Bit#(32) bitEn);

// Functions
// ---------

// Convert 4-bit byte-enable to 32-bit bit-enable
function Bit#(32) byteEnToBitEn(Bit#(4) x);
  function Bit#(8) ext(Bit#(1) b) = signExtend(b);
  return { ext(x[3]), ext(x[2]), ext(x[1]), ext(x[0]) };
endfunction

// Implementation
// --------------

module mkDRAM#(DRAMId id) (DRAM);
  // Ports
  InPort#(DRAMReq) reqPort <- mkInPort;

  // State
  SizedQueue#(`DRAMLatency, DRAMReq) reqs <- mkUGShiftQueueCore(QueueOptFmax);
  Reg#(Bit#(`DRAMBurstWidth)) burstCount <- mkReg(0);
  Reg#(Bit#(32)) outstanding <- mkReg(0);

  // Wires
  Wire#(Bit#(32)) incOutstanding <- mkDWire(0);
  PulseWire decOutstanding1 <- mkPulseWire;
  PulseWire decOutstanding2 <- mkPulseWire;

  // Response buffers
  FIFOF#(DRAMLoadResp)  loadResps  <- mkUGSizedFIFOF(16);
  FIFOF#(DRAMStoreResp) storeResps <- mkUGSizedFIFOF(16);

  // Constants
  Integer maxOutstanding = 2 ** `DRAMLogMaxInFlight;

  rule step;
    // Try to perform a request
    if (reqs.canDeq) begin
      DRAMReq req = reqs.dataOut;
      if (! req.isStore) begin
        if (loadResps.notFull) begin
          if (burstCount+1 == req.burst) begin
            reqs.deq;
            burstCount <= 0;
          end else
            burstCount <= burstCount+1;
          Vector#(`DRAMWidthInWords, Bit#(32)) elems;
          Bit#(32) addr = {req.addr, 0};
          for (Integer i = 0; i < `DRAMWidthInWords; i=i+1) begin
            let val <- ramRead(id, addr + zeroExtend(burstCount) *
                                        `DRAMWidthInBytes
                                    + fromInteger(4*i));
            elems[i] = val;
          end
          DRAMLoadResp resp;
          resp.id = req.id;
          resp.data = pack(elems);
          loadResps.enq(resp);
          decOutstanding1.send;
        end
      end else begin
        myAssert(req.burst == 1, "DRAM: burst writes not yet supported");
        if (storeResps.notFull) begin
          reqs.deq;
          Vector#(`DRAMWidthInWords, Bit#(32)) elems = unpack(req.data);
          Vector#(`DRAMWidthInWords, Bit#(4)) byteEns = unpack(req.byteEn);
          Bit#(32) addr = {req.addr, 0};
          for (Integer i = 0; i < `DRAMWidthInWords; i=i+1)
            ramWrite(id, addr+fromInteger(4*i), elems[i],
                       byteEnToBitEn(byteEns[i]));
          DRAMStoreResp resp;
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
    method DRAMLoadResp value = loadResps.first;
  endinterface

  interface BOut storeResp;
    method Action get;
      storeResps.deq;
    endmethod
    method Bool valid = storeResps.notEmpty;
    method DRAMStoreResp value = storeResps.first;
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
    Bit#(`DRAMWidth) readdata,
    Bool readdatavalid,
    Bool waitrequest,
    Bool writeresponsevalid,
    Bit#(2) response
  );
  method Bit#(`DRAMWidth) m_writedata;
  method Bit#(`DRAMAddrWidth) m_address;
  method Bool m_read;
  method Bool m_write;
  method Bit#(`DRAMBurstWidth) m_burstcount;
  method Bit#(`DRAMWidthInBytes) m_byteenable;
endinterface

// In-flight request
typedef struct {
  DRAMReqId id;
  Bool isStore;
} DRAMInFlightReq deriving (Bits);

// Implementation
// --------------

module mkDRAM#(t id) (DRAM);
  // Ports
  InPort#(DRAMReq) reqPort <- mkInPort;

  // Queues
  SizedQueue#(`DRAMLogMaxInFlight, DRAMInFlightReq) inFlight <-
    mkUGSizedQueuePrefetch;
  SizedQueue#(`DRAMLogMaxInFlight, Bit#(`DRAMWidth)) respBuffer <-
    mkUGSizedQueuePrefetch;

  // Registers
  Reg#(DRAMAddr) address <- mkRegU;
  Reg#(Bit#(`DRAMWidth)) writeData <- mkRegU;
  Reg#(Bit#(`DRAMWidthInBytes)) byteEn <- mkRegU;
  Reg#(Bool) doRead <- mkReg(False);
  Reg#(Bool) doWrite <- mkReg(False);
  Reg#(Bit#(`DRAMBurstWidth)) burstReg <- mkReg(0);

  // Wires
  Wire#(Bool) waitRequest <- mkBypassWire;
  PulseWire putLoad <- mkPulseWire;
  Wire#(Bit#(`DRAMBurstWidth)) burstWire <- mkDWire(0);
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
      DRAMReq req = reqPort.value;
      reqPort.get;
      address   <= req.addr;
      writeData <= req.data;
      byteEn    <= req.byteEn;
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
    method DRAMLoadResp value;
      DRAMLoadResp resp;
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
    method DRAMStoreResp value;
      DRAMStoreResp resp;
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
    method m_byteenable = byteEn;
  endinterface
endmodule

`endif

// ============================================================================
// Connections to DRAM
// ============================================================================

`ifdef DRAMUseDualPortFrontend

// Convert to DRAM request
function DRAMReq toDRAMReq(MemReq req);
  function Bit#(`BytesPerBeat) ext(Bit#(1) b) = signExtend(b);
  return DRAMReq {
    id:      req.id,
    isStore: req.isStore,
    addr:    truncateLSB(req.addr),
    data:    {req.data, req.data},
    burst:   req.isStore ? 1 : truncateLSB(req.burst),
    byteEn:  { ext(~req.addr[0]), ext(req.addr[0]) }
  };
endfunction

// Convert from DRAM store response
function MemStoreResp fromDRAMStoreResp(DRAMStoreResp resp) =
  MemStoreResp { id: resp.id };

// Connect vector of data caches to DRAM via dual-port frontend
module connectDCachesToDRAM#(
         Vector#(`DCachesPerDRAM, DCache) caches, DRAM dram) ();

  // Connect requests
  function getReqOut(cache) = cache.reqOut;
  let reqs <- mkMergeTree(Fair,
                mkUGShiftQueue1(QueueOptFmax),
                map(getReqOut, caches));
  let dramReqs <- onOut(toDRAMReq, reqs);
  connectUsing(mkUGQueue, dramReqs, dram.reqIn);

  // Connect store responses
  function DCacheId getStoreRespKey(MemStoreResp resp) = resp.id;
  function getStoreRespIn(cache) = cache.storeRespIn;
  let dramStoreResps <- mkResponseDistributor(
                         getStoreRespKey,
                         mkUGShiftQueue1(QueueOptFmax),
                         map(getStoreRespIn, caches));
  let storeResp <- onBOut(fromDRAMStoreResp, dram.storeResp);
  connectDirect(storeResp, dramStoreResps);

  // Connect load responses
  // ======================

  // Create port to access responses from DRAM
  InPort#(DRAMLoadResp) respPort <- mkInPort;
  connectDirect(dram.loadResp, respPort.in);
  Bit#(1) respWay = truncateLSB(respPort.value.id);

  // Partition caches into halves
  Vector#(2, Vector#(TDiv#(`DCachesPerDRAM, 2), DCache)) half = newVector;
  half[0] = take(caches);
  half[1] = drop(caches);

  // Create response distributor for each half
  Vector#(2, In#(MemLoadResp)) loadResps;
  function Bit#(TSub#(`LogDCachesPerDRAM, 1))
    getLoadRespKey(MemLoadResp resp) = truncate(resp.id);
  function getLoadRespIn(cache) = cache.loadRespIn;
  for (Integer i = 0; i < 2; i=i+1) begin
    loadResps[i] <- mkResponseDistributor(
                      getLoadRespKey,
                      mkUGShiftQueue2(QueueOptFmax),
                      map(getLoadRespIn, half[i]));
  end
 
  // State machine used to serialise DRAM response
  Vector#(2, Reg#(DRAMLoadResp)) buffer <- replicateM(mkRegU);
  Vector#(2, Reg#(Bit#(2)))      state  <- replicateM(mkReg(0));

  // Serialise DRAM response
  for (Integer i = 0; i < 2; i=i+1) begin

    // Consume response from DRAM
    rule consume (respPort.canGet &&
                    respWay == fromInteger(i) &&
                      state[i] == 0);
      respPort.get;
      buffer[i] <= respPort.value;
      state[i] <= 1;
    endrule


    // Produce response to distributor
    rule produceTry (state[i] != 0);
      MemLoadResp resp;
      resp.id = buffer[i].id;
      resp.data = truncateLSB(buffer[i].data);
      loadResps[i].tryPut(resp);
    endrule

    rule produce (state[i] != 0);
      if (loadResps[i].didPut) begin
        if (state[i] == 1) begin
          state[i] <= 2;
          Bit#(`BeatWidth) rest = truncate(buffer[i].data);
          buffer[i] <= DRAMLoadResp { id: buffer[i].id, data: {rest, ?} };
        end else
          state[i] <= 0;
      end
    endrule

  end

endmodule

`else /* Don't use dual-port frontend */

// Convert to DRAM request
function DRAMReq toDRAMReq(MemReq req) =
  DRAMReq {
    id:      req.id,
    isStore: req.isStore,
    addr:    req.addr,
    data:    req.data,
    burst:   req.burst,
    byteEn:  -1
  };

// Convert from DRAM load response
function MemLoadResp fromDRAMLoadResp(DRAMLoadResp resp) =
  MemLoadResp { id: resp.id, data: resp.data };

// Convert from DRAM store response
function MemStoreResp fromDRAMStoreResp(DRAMStoreResp resp) =
  MemStoreResp { id: resp.id };

// Connect vector of data caches to DRAM
module connectDCachesToDRAM#(
         Vector#(`DCachesPerDRAM, DCache) caches, DRAM dram) ();

  // Connect requests
  function getReqOut(cache) = cache.reqOut;
  let reqs <- mkMergeTreeB(Fair,
                mkUGShiftQueue1(QueueOptFmax),
                map(getReqOut, caches));
  let dramReqs <- onOut(toDRAMReq, reqs);
  connectUsing(mkUGQueue, dramReqs, dram.reqIn);

  // Connect load responses
  function DCacheId getLoadRespKey(MemLoadResp resp) = resp.id;
  function getLoadRespIn(cache) = cache.loadRespIn;
  let dramLoadResps <- mkResponseDistributor(
                        getLoadRespKey,
                        mkUGShiftQueue1(QueueOptFmax),
                        map(getLoadRespIn, caches));
  let loadResp <- onBOut(fromDRAMLoadResp, dram.loadResp);
  connectDirect(loadResp, dramLoadResps);

  // Connect store responses
  function DCacheId getStoreRespKey(MemStoreResp resp) = resp.id;
  function getStoreRespIn(cache) = cache.storeRespIn;
  let dramStoreResps <- mkResponseDistributor(
                         getStoreRespKey,
                         mkUGShiftQueue1(QueueOptFmax),
                         map(getStoreRespIn, caches));
  let storeResp <- onBOut(fromDRAMStoreResp, dram.storeResp);
  connectDirect(storeResp, dramStoreResps);

endmodule

`endif

endpackage
