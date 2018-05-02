package DRAM;

// ============================================================================
// Types
// ============================================================================

// DRAM request id
typedef DCacheId DRAMReqId;

// DRAM request
typedef struct {
  Bool isStore;
  DRAMReqId id;
  Bit#(`LogBeatsPerDRAM) addr;
  Bit#(`BeatWidth) data;
  Bit#(`BeatBurstWidth) burst;
  //Bit#(`BytesPerBeat) byteEn;
} DRAMReq deriving (Bits);

// DRAM load response
typedef struct {
  DRAMReqId id;
  Bit#(`BeatWidth) data;
} DRAMResp deriving (Bits);

// ============================================================================
// Interface
// ============================================================================

interface DRAM;
  interface In#(DRAMReq) reqIn;
  interface BOut#(DRAMResp) respOut;
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
  Reg#(Bit#(`BeatBurstWidth)) burstCount <- mkReg(0);
  Reg#(Bit#(32)) outstanding <- mkReg(0);

  // Wires
  Wire#(Bit#(32)) incOutstanding <- mkDWire(0);
  PulseWire decOutstanding <- mkPulseWire;

  // Response buffers
  FIFOF#(DRAMResp) resps <- mkUGSizedFIFOF(32);

  // Constants
  Integer maxOutstanding = 2 ** `DRAMLogMaxInFlight;

  rule step;
    // Try to perform a request
    if (reqs.canDeq) begin
      DRAMReq req = reqs.dataOut;
      if (!req.isStore) begin
        if (resps.notFull) begin
          if (burstCount+1 == req.burst) begin
            reqs.deq;
            burstCount <= 0;
          end else
            burstCount <= burstCount+1;
          Vector#(`WordsPerBeat, Bit#(32)) elems;
          Bit#(`LogBytesPerBeat) low = 0;
          Bit#(32) addr = {0, req.addr, low};
          for (Integer i = 0; i < `WordsPerBeat; i=i+1) begin
            let val <- ramRead(id, addr + zeroExtend(burstCount) *
                                        `BytesPerBeat
                                    + fromInteger(4*i));
            elems[i] = val;
          end
          DRAMResp resp;
          resp.id = req.id;
          resp.data = pack(elems);
          resps.enq(resp);
          decOutstanding.send;
        end
      end else begin
        myAssert(req.burst == 1, "DRAM: burst writes not yet supported");
        Vector#(`WordsPerBeat, Bit#(32)) elems = unpack(req.data);
        Vector#(`WordsPerBeat, Bit#(4)) byteEns =
          //unpack(req.byteEn);
          replicate(-1);
        Bit#(`LogBytesPerBeat) low = 0;
        Bit#(32) addr = {0, req.addr, low};
        for (Integer i = 0; i < `WordsPerBeat; i=i+1)
          ramWrite(id, addr+fromInteger(4*i), elems[i],
                     byteEnToBitEn(byteEns[i]));
        reqs.deq;
        decOutstanding.send;
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
    if (decOutstanding) count = count-1;
    outstanding <= count;
  endrule

  // Interfaces
  interface In reqIn = reqPort.in;

  interface BOut respOut;
    method Action get;
      resps.deq;
    endmethod
    method Bool valid = resps.notEmpty;
    method DRAMResp value = resps.first;
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
import Assert    :: *;
import Util      :: *;

// Types
// -----

// On FPGA, external interface is an Avalon master
(* always_ready, always_enabled *)
interface DRAMExtIfc;
  method Action m(
    Bit#(`BeatWidth) readdata,
    Bool readdatavalid,
    Bool waitrequest
  );
  method Bit#(`BeatWidth) m_writedata;
  method Bit#(`LogBeatsPerDRAM) m_address;
  method Bool m_read;
  method Bool m_write;
  method Bit#(`BeatBurstWidth) m_burstcount;
  //method Bit#(`BytesPerBeat) m_byteenable;
endinterface

// In-flight request
typedef struct {
  DRAMReqId id;
  Bit#(`BeatBurstWidth) burst;
} DRAMInFlightReq deriving (Bits);

// Implementation
// --------------

module mkDRAM#(t id) (DRAM);
  // Ports
  InPort#(DRAMReq) reqPort <- mkInPort;

  // Queues
  SizedQueue#(`DRAMLogMaxInFlight, DRAMInFlightReq) inFlight <-
    mkUGSizedQueuePrefetch;
  SizedQueue#(`DRAMLogMaxInFlight, Bit#(`BeatWidth)) respBuffer <-
    mkUGSizedQueuePrefetch;

  // Counter
  Count#(TAdd#(`DRAMLogMaxInFlight, 1)) inFlightCount <-
    mkCount(2 ** `DRAMLogMaxInFlight);

  // Max burst
  Integer maxBurst = 2 ** (`BeatBurstWidth-1);

  // Registers
  Reg#(Bit#(`LogBeatsPerDRAM)) address <- mkRegU;
  Reg#(Bit#(`BeatWidth)) writeData <- mkRegU;
  Reg#(Bit#(`BytesPerBeat)) byteEn <- mkRegU;
  Reg#(Bool) doRead <- mkReg(False);
  Reg#(Bool) doWrite <- mkReg(False);
  Reg#(Bit#(`BeatBurstWidth)) burstReg <- mkReg(0);
  Reg#(Bit#(`BeatBurstWidth)) burstCount <- mkReg(1);

  // Wires
  Wire#(Bool) waitRequest <- mkBypassWire;
  PulseWire putLoad <- mkPulseWire;
  PulseWire putStore <- mkPulseWire;

  // Rules
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

  rule consumeRequest;
    if (reqPort.canGet && !waitRequest) begin
      DRAMReq req = reqPort.value;
      if (inFlightCount.available >= fromInteger(maxBurst)) begin
        reqPort.get;
        address   <= req.addr;
        writeData <= req.data;
        burstReg  <= req.burst;
        //byteEn    <= req.byteEn;
        if (req.isStore) putStore.send; else putLoad.send;
        if (!req.isStore) begin
          DRAMInFlightReq inflightReq;
          inflightReq.id = req.id;
          inflightReq.burst = req.burst;
          inFlight.enq(inflightReq);
          inFlightCount.incBy(zeroExtend(req.burst));
        end
      end
    end
  endrule

  // Internal interfaces
  interface In reqIn = reqPort.in;

  interface BOut respOut;
    method Action get;
      if (burstCount == inFlight.dataOut.burst) begin
        inFlight.deq;
        burstCount <= 1;
      end else
        burstCount <= burstCount+1;
      respBuffer.deq;
      inFlightCount.dec;
    endmethod
    method Bool valid = inFlight.canPeek && inFlight.canDeq &&
                           respBuffer.canPeek && respBuffer.canDeq;
    method DRAMResp value;
      DRAMResp resp;
      resp.id = inFlight.dataOut.id;
      resp.data = respBuffer.dataOut;
      return resp;
    endmethod
  endinterface

  // External (Avalon master) interface
  interface DRAMExtIfc external;
    method Action m(readdata,readdatavalid,waitrequest);
      if (readdatavalid) respBuffer.enq(readdata);
      waitRequest <= waitrequest;
    endmethod
    method m_writedata  = writeData;
    method m_address    = address;
    method m_read       = doRead;
    method m_write      = doWrite;
    method m_burstcount = burstReg;
    //method m_byteenable = byteEn;
  endinterface
endmodule

`endif

endpackage
