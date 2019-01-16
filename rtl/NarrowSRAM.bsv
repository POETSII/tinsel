package NarrowSRAM;

import DCacheTypes :: *;
import Util        :: *;

// ============================================================================
// Types
// ============================================================================

// SRAM request id
typedef Bit#(`LogDCachesPerDRAM) SRAMReqId;

// SRAM load request
typedef struct {
  SRAMReqId id;
  Bit#(`SRAMAddrWidth) addr;
  Bit#(`SRAMBurstWidth) burst;
  InflightDCacheReqInfo info;
} SRAMLoadReq deriving (Bits);

// SRAM store request
typedef struct {
  SRAMReqId id;
  Bit#(`SRAMAddrWidth) addr;
  Bit#(`SRAMDataWidth) data;
  Bit#(`SRAMBurstWidth) burst;
} SRAMStoreReq deriving (Bits);

// SRAM load response
typedef struct {
  SRAMReqId id;
  Bit#(`SRAMDataWidth) data;
  InflightDCacheReqInfo info;
} SRAMResp deriving (Bits);

// ============================================================================
// Interface
// ============================================================================

interface SRAM;
  interface In#(SRAMLoadReq) loadIn;
  interface In#(SRAMStoreReq) storeIn;
  interface BOut#(SRAMResp) respOut;
  method Option#(SRAMReqId) storeDone;
  interface SRAMExtIfc external;
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
import Queue     :: *;
import Assert    :: *;

// Types
// -----

// In simulation, external interface is empty
typedef Empty SRAMExtIfc;

// RAM identifier
typedef Bit#(3) RAMId;

// Interface to C functions
import "BDPI" function ActionValue#(Bit#(32)) ramRead(
                RAMId ramId, Bit#(32) addr);
import "BDPI" function Action ramWrite(RAMId ramId,
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

module mkSRAM#(RAMId id) (SRAM);
  // Ports
  InPort#(SRAMLoadReq) loadReqPort <- mkInPort;
  InPort#(SRAMStoreReq) storeReqPort <- mkInPort;

  // State
  SizedQueue#(`SRAMLatency, SRAMLoadReq) loadReqs <-
    mkUGShiftQueueCore(QueueOptFmax);
  SizedQueue#(`SRAMLatency, SRAMStoreReq) storeReqs <-
    mkUGShiftQueueCore(QueueOptFmax);

  Reg#(Bit#(`SRAMBurstWidth)) loadBurstCount <- mkReg(0);

  // Response buffers
  FIFOF#(SRAMResp) resps <- mkUGSizedFIFOF(32);

  // Counter
  Count#(TAdd#(`SRAMLogMaxInFlight, 1)) inFlightCount <-
    mkCount(2 ** `SRAMLogMaxInFlight);

  // Constants
  Integer maxBurst = 2 ** (`SRAMBurstWidth-1);

  // This wire indicates when a store has been completed
  Wire#(Option#(SRAMReqId)) storeDoneWire <- mkDWire(option(False, ?));

  // Handle load requests
  rule handleLoads;
    // Try to perform a load request
    if (loadReqs.canDeq) begin
      SRAMLoadReq req = loadReqs.dataOut;
      if (resps.notFull) begin
        if (loadBurstCount+1 == req.burst) begin
          loadReqs.deq;
          loadBurstCount <= 0;
        end else
          loadBurstCount <= loadBurstCount+1;
        Vector#(`WordsPerSRAMBeat, Bit#(32)) elems;
        Bit#(`LogBytesPerSRAMBeat) low = 0;
        Bit#(32) addr = {0, req.addr, low};
        for (Integer i = 0; i < `WordsPerSRAMBeat; i=i+1) begin
          let val <- ramRead(id, addr + zeroExtend(loadBurstCount) *
                                      `BytesPerSRAMBeat
                                  + fromInteger(4*i));
          elems[i] = val;
        end
        SRAMResp resp;
        resp.id = req.id;
        resp.data = pack(elems);
        resp.info = req.info;
        resp.info.beat = truncate(loadBurstCount);
        resps.enq(resp);
        inFlightCount.dec;
      end
    end
    // Insert a new request
    if (loadReqPort.canGet && loadReqs.notFull &&
          inFlightCount.available >= fromInteger(maxBurst)) begin
      loadReqPort.get;
      loadReqs.enq(loadReqPort.value);
      inFlightCount.incBy(zeroExtend(loadReqPort.value.burst));
    end
  endrule

  // Handle store requests
  rule handleStores;
    // Try to perform a request
    if (storeReqs.canDeq) begin
      SRAMStoreReq req = storeReqs.dataOut;
      myAssert(req.burst == 1, "SRAM only supports store burst of 1");
      storeReqs.deq;
      storeDoneWire <= option(True, req.id);

      Vector#(`WordsPerSRAMBeat, Bit#(32)) elems = unpack(req.data);
      Vector#(`WordsPerSRAMBeat, Bit#(4)) byteEns = replicate(-1);
      Bit#(`LogBytesPerSRAMBeat) low = 0;
      Bit#(32) addr = {0, req.addr, low};
      for (Integer i = 0; i < `WordsPerSRAMBeat; i=i+1)
        ramWrite(id, addr + fromInteger(4*i), elems[i],
                   byteEnToBitEn(byteEns[i]));
    end
    // Insert a new request
    if (storeReqPort.canGet && storeReqs.notFull) begin
      storeReqPort.get;
      storeReqs.enq(storeReqPort.value);
    end
  endrule

  // Interfaces
  interface loadIn = loadReqPort.in;
  interface storeIn = storeReqPort.in;

  interface BOut respOut;
    method Action get;
      resps.deq;
    endmethod
    method Bool valid = resps.notEmpty;
    method SRAMResp value = resps.first;
  endinterface

  method Option#(SRAMReqId) storeDone = storeDoneWire;

  interface SRAMExtIfc external;
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
import Assert    :: *;
import Util      :: *;

// Types
// -----

// Each SRAM on the DE5 is driven by two avalon masters
// (one read port and one write port)
(* always_ready, always_enabled *)
interface SRAMExtIfc;
  // Read port
  method Action r(
    Bit#(`SRAMDataWidth) readdata,
    Bool readdatavalid,
    Bool waitrequest
  );
  method Bit#(`SRAMAddrWidth) r_address;
  method Bool r_read;
  method Bit#(`SRAMBurstWidth) r_burstcount;

  // Write port
  method Action w(
    Bool waitrequest
  );
  method Bit#(`SRAMDataWidth) w_writedata;
  method Bit#(`SRAMAddrWidth) w_address;
  method Bool w_write;
  method Bit#(`SRAMBurstWidth) w_burstcount;
endinterface

// In-flight request
typedef struct {
  SRAMReqId id;
  Bit#(`SRAMBurstWidth) burst;
  InflightDCacheReqInfo info;
} SRAMInFlightReq deriving (Bits);

// SRAM Implementation
// -------------------

module mkSRAM#(t id) (SRAM);
  // Ports
  InPort#(SRAMLoadReq) loadReqPort <- mkInPort;
  InPort#(SRAMStoreReq) storeReqPort <- mkInPort;

  // Response buffer
  SizedQueue#(`SRAMLogMaxInFlight, Bit#(`SRAMDataWidth)) respBuffer <-
    mkUGSizedQueuePrefetch;

  // Queues
  SizedQueue#(`SRAMLogMaxInFlight, SRAMInFlightReq) inFlight <-
    mkUGSizedQueuePrefetch;

  // Registers
  Reg#(Bit#(`SRAMAddrWidth)) loadAddress <- mkRegU;
  Reg#(Bit#(`SRAMAddrWidth)) storeAddress <- mkRegU;
  Reg#(Bit#(`SRAMDataWidth)) writeData <- mkRegU;
  Reg#(Bool) doRead <- mkReg(False);
  Reg#(Bool) doWrite <- mkReg(False);
  Reg#(Bit#(`SRAMBurstWidth)) loadBurstReg <- mkReg(0);
  Reg#(Bit#(`SRAMBurstWidth)) storeBurstReg <- mkReg(0);
  Reg#(Bit#(`SRAMBurstWidth)) loadBurstCount <- mkReg(1);

  // Counter
  Count#(TAdd#(`SRAMLogMaxInFlight, 1)) inFlightCount <-
    mkCount(2 ** `SRAMLogMaxInFlight);

  // Constants
  Integer maxBurst = 2 ** (`SRAMBurstWidth-1);

  // Wires
  Wire#(Bool) loadWaitRequest <- mkBypassWire;
  Wire#(Bool) storeWaitRequest <- mkBypassWire;
  PulseWire putLoad <- mkPulseWire;
  PulseWire putStore <- mkPulseWire;

  // This wire indicates when a store has been submitted
  Wire#(Option#(SRAMReqId)) storeSubmittedWire <- mkDWire(option(False, ?));

  // Shift register to delay storeSubmittedWire by predefined number of cycles
  Vector#(`SRAMStoreLatency, Reg#(Option#(SRAMReqId)))
    storeDoneQueue <- replicateM(mkReg(option(False, ?)));

  // Shift the storeDoneQueue on every cycle
  rule updateStoreDone;
    for (Integer i = 0; i < `SRAMStoreLatency-1; i=i+1)
      storeDoneQueue[i] <= storeDoneQueue[i+1];
    storeDoneQueue[`SRAMStoreLatency-1] <= storeSubmittedWire;
  endrule

  // Rules
  rule putRequest;
    // Read port
    if (putLoad) begin
      doRead <= True;
    end else if (!loadWaitRequest) begin
      doRead <= False;
    end
    // Write port
    if (putStore) begin
      doWrite <= True;
    end else if (!storeWaitRequest) begin
      doWrite <= False;
    end
  endrule

  rule consumeLoadRequest;
    if (loadReqPort.canGet && !loadWaitRequest &&
          inFlightCount.available >= fromInteger(maxBurst)) begin
      SRAMLoadReq req = loadReqPort.value;
      loadReqPort.get;
      loadAddress <= req.addr;
      loadBurstReg <= req.burst;
      putLoad.send;
      inFlightCount.incBy(zeroExtend(req.burst));
      SRAMInFlightReq inflight;
      inflight.id = req.id;
      inflight.burst = req.burst;
      inflight.info = req.info;
      inFlight.enq(inflight);
    end
  endrule

  rule consumeStoreRequest;
    if (storeReqPort.canGet && !storeWaitRequest) begin
      SRAMStoreReq req = storeReqPort.value;
      myAssert(req.burst == 1, "SRAM only supports store burst of 1");
      storeReqPort.get;
      storeSubmittedWire <= option(True, req.id);
      storeAddress <= req.addr;
      writeData <= req.data;
      storeBurstReg <= req.burst;
      putStore.send;
    end
  endrule

  // Internal interfaces
  interface loadIn = loadReqPort.in;
  interface storeIn = storeReqPort.in;

  interface BOut respOut;
    method Action get;
      if (loadBurstCount == inFlight.dataOut.burst) begin
        inFlight.deq;
        loadBurstCount <= 1;
      end else
        loadBurstCount <= loadBurstCount+1;
      respBuffer.deq;
      inFlightCount.dec;
    endmethod
    method Bool valid =
      respBuffer.canDeq && inFlight.canDeq;
    method SRAMResp value;
      SRAMResp resp;
      resp.id = inFlight.dataOut.id;
      resp.info = inFlight.dataOut.info;
      resp.data = respBuffer.dataOut;
      return resp;
    endmethod
  endinterface

  method Option#(SRAMReqId) storeDone = storeDoneQueue[0];

  // External interface (two Avalon masters)
  interface SRAMExtIfc external;
    // Read port
    method Action r(readdata, readdatavalid, waitrequest);
      if (readdatavalid) respBuffer.enq(readdata);
      loadWaitRequest <= waitrequest;
    endmethod
    method r_address = loadAddress;
    method r_read = doRead;
    method r_burstcount = zeroExtend(loadBurstReg);

    // Write port
    method Action w(waitrequest);
      storeWaitRequest <= waitrequest;
    endmethod
    method w_writedata = writeData;
    method w_address = storeAddress;
    method w_write = doWrite;
    method w_burstcount = zeroExtend(storeBurstReg);
  endinterface
endmodule

`endif

endpackage
