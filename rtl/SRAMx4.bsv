package SRAMx4;

import DCacheTypes :: *;
import Util        :: *;

// ============================================================================
// Types
// ============================================================================

// SRAM request id
typedef Bit#(`LogDCachesPerBoard) SRAMReqId;

// SRAM load request
typedef struct {
  SRAMReqId id;
  Bit#(`LogBeatsPerSRAM) addr;
  Bit#(`BeatBurstWidth) burst;
  InflightDCacheReqInfo info;
} SRAMLoadReq deriving (Bits);

// SRAM store request
typedef struct {
  SRAMReqId id;
  Bit#(`LogBeatsPerSRAM) addr;
  Bit#(`BeatWidth) data;
  Bit#(`BeatBurstWidth) burst;
  //Bit#(`BytesPerBeat) byteEn;
} SRAMStoreReq deriving (Bits);

// SRAM load response
typedef struct {
  SRAMReqId id;
  Bit#(`BeatWidth) data;
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

  Reg#(Bit#(`BeatBurstWidth)) loadBurstCount <- mkReg(0);
  Reg#(Bit#(`BeatBurstWidth)) storeBurstCount <- mkReg(0);

  // Response buffers
  FIFOF#(SRAMResp) resps <- mkUGSizedFIFOF(32);

  // Count outstanding loads
  Reg#(Bit#(32)) outstanding <- mkReg(0);
  Wire#(Bit#(32)) incOutstanding <- mkDWire(0);
  PulseWire decOutstanding <- mkPulseWire;

  // Constants
  Integer maxOutstanding = 2 ** `SRAMLogMaxInFlight;

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
        Vector#(`WordsPerBeat, Bit#(32)) elems;
        Bit#(`LogBytesPerBeat) low = 0;
        Bit#(32) addr = {0, req.addr, low};
        dynamicAssert(addr < `BytesPerSRAM, "Overflowed SRAM");
        for (Integer i = 0; i < `WordsPerBeat; i=i+1) begin
          let val <- ramRead(id, addr + zeroExtend(loadBurstCount) *
                                      `BytesPerBeat
                                  + fromInteger(4*i));
          elems[i] = val;
        end
        SRAMResp resp;
        resp.id = req.id;
        resp.data = pack(elems);
        resp.info = req.info;
        resp.info.beat = truncate(loadBurstCount);
        resps.enq(resp);
        decOutstanding.send;
      end
    end
    // Insert a new request
    if (loadReqPort.canGet && loadReqs.notFull &&
          outstanding < fromInteger(maxOutstanding)) begin
      loadReqPort.get;
      loadReqs.enq(loadReqPort.value);
      incOutstanding <= zeroExtend(loadReqPort.value.burst);
    end
  endrule

  // Handle store requests
  rule handleStores;
    // Try to perform a request
    if (storeReqs.canDeq) begin
      SRAMStoreReq req = storeReqs.dataOut;
      myAssert(req.burst == 1, "SRAM: burst writes not yet supported");
      Vector#(`WordsPerBeat, Bit#(32)) elems = unpack(req.data);
      Vector#(`WordsPerBeat, Bit#(4)) byteEns =
        //unpack(req.byteEn);
        replicate(-1);
      Bit#(`LogBytesPerBeat) low = 0;
      Bit#(32) addr = {0, req.addr, low};
      dynamicAssert(addr < `BytesPerSRAM, "Overflowed SRAM");
      for (Integer i = 0; i < `WordsPerBeat; i=i+1)
        ramWrite(id, addr+fromInteger(4*i), elems[i],
                   byteEnToBitEn(byteEns[i]));
      storeReqs.deq;
      storeDoneWire <= option(True, req.id);
    end
    // Insert a new request
    if (storeReqPort.canGet && storeReqs.notFull) begin
      storeReqPort.get;
      storeReqs.enq(storeReqPort.value);
    end
  endrule

  // Track number of outstanding loads
  rule countOutstanding;
    let count = outstanding + incOutstanding;
    if (decOutstanding) count = count-1;
    outstanding <= count;
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
interface SRAMUnitExtIfc;
  // Read port
  method Action r(
    Bit#(72) readdata,
    Bool readdatavalid,
    Bool waitrequest
  );
  method Bit#(20) r_address;
  method Bool r_read;
  method Bit#(`BeatBurstWidth) r_burstcount;

  // Write port
  method Action w(
    Bool waitrequest
  );
  method Bit#(72) w_writedata;
  method Bit#(20) w_address;
  method Bool w_write;
  method Bit#(`BeatBurstWidth) w_burstcount;
endinterface

// There are 4 SRAM units on the DE5
interface SRAMExtIfc;
  interface SRAMUnitExtIfc sramA;
  interface SRAMUnitExtIfc sramB;
  interface SRAMUnitExtIfc sramC;
  interface SRAMUnitExtIfc sramD;
endinterface

// In-flight request
typedef struct {
  SRAMReqId id;
  InflightDCacheReqInfo info;
} SRAMInFlightReq deriving (Bits);

// SRAM Unit Implementation
// ------------------------

typedef struct {
  Bit#(20) addr;
  Bit#(`BeatBurstWidth) burst;
} SRAMUnitLoadReq deriving (Bits);

typedef struct {
  Bit#(20) addr;
  Bit#(72) data;
  Bit#(`BeatBurstWidth) burst;
} SRAMUnitStoreReq deriving (Bits);

interface SRAMUnit;
  interface In#(SRAMUnitLoadReq) loadIn;
  interface In#(SRAMUnitStoreReq) storeIn;
  interface BOut#(Bit#(72)) respOut;
  interface SRAMUnitExtIfc ext;
endinterface

module mkSRAMUnit (SRAMUnit);
  // Ports
  InPort#(SRAMUnitLoadReq) loadReqPort <- mkInPort;
  InPort#(SRAMUnitStoreReq) storeReqPort <- mkInPort;

  // Response buffer
  SizedQueue#(`SRAMLogMaxInFlight, Bit#(72)) respBuffer <-
    mkUGSizedQueuePrefetch;

  // In-flight counter
  Count#(TAdd#(`SRAMLogMaxInFlight, 1)) inFlight <-
    mkCount(2 ** `SRAMLogMaxInFlight);

  // Registers
  Reg#(Bit#(20)) loadAddress <- mkRegU;
  Reg#(Bit#(20)) storeAddress <- mkRegU;
  Reg#(Bit#(72)) writeData <- mkRegU;
  Reg#(Bool) doRead <- mkReg(False);
  Reg#(Bool) doWrite <- mkReg(False);
  Reg#(Bit#(`BeatBurstWidth)) loadBurstReg <- mkReg(0);
  Reg#(Bit#(`BeatBurstWidth)) storeBurstReg <- mkReg(0);

  // Wires
  Wire#(Bool) loadWaitRequest <- mkBypassWire;
  Wire#(Bool) storeWaitRequest <- mkBypassWire;
  PulseWire putLoad <- mkPulseWire;
  PulseWire putStore <- mkPulseWire;

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

  // TODO
  staticAssert(`BeatBurstWidth == 1, "Bursts on FPGA not yet supported");

  rule consumeLoadRequest;
    if (loadReqPort.canGet && !loadWaitRequest && inFlight.notFull) begin
      SRAMUnitLoadReq req = loadReqPort.value;
      loadReqPort.get;
      loadAddress <= req.addr;
      loadBurstReg <= req.burst;
      putLoad.send;
      inFlight.inc;
    end
  endrule

  rule consumeStoreRequest;
    if (storeReqPort.canGet && !storeWaitRequest) begin
      SRAMUnitStoreReq req = storeReqPort.value;
      storeReqPort.get;
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
      inFlight.dec;
      respBuffer.deq;
    endmethod
    method Bool valid = respBuffer.canPeek && respBuffer.canDeq;
    method Bit#(72) value = respBuffer.dataOut;
  endinterface

  method Option#(SRAMReqId) storeDone = storeDoneWire;

  // External interface (two Avalon masters)
  interface SRAMUnitExtIfc ext;
    // Read port
    method Action r(readdata, readdatavalid, waitrequest);
      if (readdatavalid) respBuffer.enq(readdata);
      loadWaitRequest <= waitrequest;
    endmethod
    method r_address = loadAddress;
    method r_read = doRead;
    method r_burstcount = loadBurstReg;

    // Write port
    method Action w(waitrequest);
      storeWaitRequest <= waitrequest;
    endmethod
    method w_writedata = writeData;
    method w_address = storeAddress;
    method w_write = doWrite;
    method w_burstcount = storeBurstReg;
  endinterface
endmodule

// SRAM Implementation
// -------------------

module mkSRAM#(t id) (SRAM);
  // Ports
  InPort#(SRAMLoadReq) loadReqPort <- mkInPort;
  InPort#(SRAMStoreReq) storeReqPort <- mkInPort;

  // Queues
  SizedQueue#(`SRAMLogMaxInFlight, SRAMInFlightReq) inFlight <-
    mkUGSizedQueuePrefetch;

  // Create 4x SRAM units
  Vector#(4, SRAMUnit) sramUnits <- replicateM(mkSRAMUnit);

  // Ports for connections to SRAM units
  Vector#(4, OutPort#(SRAMUnitLoadReq)) loadOutPort <- replicateM(mkOutPort);
  Vector#(4, OutPort#(SRAMUnitStoreReq)) storeOutPort <- replicateM(mkOutPort);
  Vector#(4, InPort#(Bit#(72))) respInPort <- replicateM(mkInPort);

  // Create connections
  for (Integer i = 0; i < 4; i = i+1) begin
    connectUsing(mkUGQueue, loadOutPort[i].out, sramUnits[i].loadIn);
    connectUsing(mkUGQueue, storeOutPort[i].out, sramUnits[i].storeIn);
    connectDirect(sramUnits[i].respOut, respInPort[i].in);
  end

  function canPutFunc(out) = out.canPut;
  Bool canPutLoad = all(canPutFunc, loadOutPort);
  Bool canPutStore = all(canPutFunc, storeOutPort);

  function canGetFunc(in) = in.canGet;
  Bool canGetResp = all(canGetFunc, respInPort);

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

  // TODO:
  staticAssert(`BeatBurstWidth == 1, "Bursts on FPGA not yet supported");

  rule consumeLoadRequest;
    if (loadReqPort.canGet && canPutLoad && inFlight.notFull) begin
      SRAMUnitLoadReq req;
      req.addr = loadReqPort.value.addr;
      req.burst = loadReqPort.value.burst;
      for (Integer i = 0; i < 4; i = i+1)
        loadOutPort[i].put(req);
      loadReqPort.get;
      SRAMInFlightReq inflightReq;
      inflightReq.id = loadReqPort.value.id;
      inflightReq.info = loadReqPort.value.info;
      inflightReq.info.beat = 0; // TODO
      inFlight.enq(inflightReq);
    end
  endrule

  rule consumeStoreRequest;
    if (storeReqPort.canGet && canPutStore) begin
      SRAMUnitStoreReq req;
      req.addr  = storeReqPort.value.addr;
      req.burst = storeReqPort.value.burst;
      Vector#(4, Bit#(64)) datas = unpack(storeReqPort.value.data);
      for (Integer i = 0; i < 4; i = i+1) begin
        req.data = {?, datas[i]};
        storeOutPort[i].put(req);
      end
      storeReqPort.get;
      storeSubmittedWire <= option(True, req.id);
    end
  endrule

  // Internal interfaces
  interface loadIn = loadReqPort.in;
  interface storeIn = storeReqPort.in;

  interface BOut respOut;
    method Action get;
      inFlight.deq;
      for (Integer i = 0; i < 4; i = i+1)
        respInPort[i].get;
    endmethod
    method Bool valid = canGetResp && inFlight.canPeek && inFlight.canDeq;
    method SRAMResp value;
      SRAMResp resp;
      resp.id = inFlight.dataOut.id;
      resp.info = inFlight.dataOut.info;
      Vector#(4, Bit#(64)) data;
      for (Integer i = 0; i < 4; i = i+1)
        data[i] = truncate(respInPort[i].value);
      resp.data = pack(data);
      return resp;
    endmethod
  endinterface

  method Option#(SRAMReqId) storeDone = storeDoneQueue[0];

  // External interface
  interface SRAMExtIfc external;
    interface sramA = sramUnits[0].ext;
    interface sramB = sramUnits[1].ext;
    interface sramC = sramUnits[2].ext;
    interface sramD = sramUnits[3].ext;
  endinterface
endmodule

`endif

endpackage
