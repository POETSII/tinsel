package OffChipSRAM;

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
} SRAMResp deriving (Bits);

// ============================================================================
// Interface
// ============================================================================

interface SRAM;
  interface In#(SRAMLoadReq) loadIn;
  interface In#(SRAMStoreReq) storeIn;
  interface BOut#(SRAMResp) respOut;
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

// Types
// -----

// On FPGA, external interface is two Avalon masters
(* always_ready, always_enabled *)
interface SRAMExtIfc;
  // Read port
  method Action r(
    Bit#(`BeatWidth) readdata,
    Bool readdatavalid,
    Bool waitrequest
  );
  method Bit#(`LogBeatsPerSRAM) r_address;
  method Bool r_read;
  method Bit#(`BeatBurstWidth) r_burstcount;
  //method Bit#(`BytesPerBeat) r_byteenable;

  // Write port
  method Action w(
    Bool waitrequest
  );
  method Bit#(`BeatWidth) w_writedata;
  method Bit#(`LogBeatsPerSRAM) w_address;
  method Bool w_write;
  method Bit#(`BeatBurstWidth) w_burstcount;
endinterface

// In-flight request
typedef struct {
  SRAMReqId id;
} SRAMInFlightReq deriving (Bits);

// Implementation
// --------------

module mkSRAM#(RAMId id) (SRAM);
  // Ports
  InPort#(SRAMLoadReq) loadReqPort <- mkInPort;
  InPort#(SRAMStoreReq) storeReqPort <- mkInPort;

  // Queues
  SizedQueue#(`SRAMLogMaxInFlight, SRAMInFlightReq) inFlight <-
    mkUGSizedQueuePrefetch;
  SizedQueue#(`SRAMLogMaxInFlight, Bit#(`BeatWidth)) respBuffer <-
    mkUGSizedQueuePrefetch;

  // Registers
  Reg#(Bit#(`LogBeatsPerSRAM)) loadAddress <- mkRegU;
  Reg#(Bit#(`LogBeatsPerSRAM)) storeAddress <- mkRegU;
  Reg#(Bit#(`BeatWidth)) writeData <- mkRegU;
  Reg#(Bit#(`BytesPerBeat)) byteEn <- mkRegU;
  Reg#(Bool) doRead <- mkReg(False);
  Reg#(Bool) doWrite <- mkReg(False);
  Reg#(Bit#(`BeatBurstWidth)) readBurstReg <- mkReg(0);
  Reg#(Bit#(`BeatBurstWidth)) writeBurstReg <- mkReg(0);

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

  // TODO: inFlight.notFull is insufficient condition for burst loads
  // in the consumeLoadRequest rule
  staticAssert(`BeatBurstWidth == 1, "Bursts on FPGA not yet supported");

  rule consumeLoadRequest;
    if (loadReqPort.canGet && !loadWaitRequest && inFlight.notFull) begin
      SRAMReq req = loadReqPort.value;
      loadReqPort.get;
      loadAddress <= req.addr;
      loadBurstReg <= req.burst;
      //byteEn <= req.byteEn;
      putLoad.send;
      SRAMInFlightReq inflightReq;
      inflightReq.id = req.id;
      inFlight.enq(inflightReq);
    end
  endrule

  rule consumeStoreRequest;
    if (storeReqPort.canGet && !storeWaitRequest) begin
      SRAMReq req = storeReqPort.value;
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
      inFlight.deq;
      respBuffer.deq;
    endmethod
    method Bool valid = inFlight.canPeek && inFlight.canDeq &&
                           respBuffer.canPeek && respBuffer.canDeq;
    method SRAMResp value;
      SRAMResp resp;
      resp.id = inFlight.dataOut.id;
      resp.data = respBuffer.dataOut;
      return resp;
    endmethod
  endinterface

  // External interface (two Avalon masters)
  interface SRAMExtIfc external;
    // Read port
    method Action r(readdata, readdatavalid, waitrequest);
      if (readdatavalid) respBuffer.enq(readdata);
      loadWaitRequest <= waitrequest;
    endmethod
    method r_address = loadAddress;
    method r_read = doRead;
    method r_burstcount = loadBurstReg;
    //method r_byteenable;

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

`endif

endpackage
