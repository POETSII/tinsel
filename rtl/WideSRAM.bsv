// Wrap an SRAM (narrow data) in a WideSRAM (wide data) interface so
// that SRAMs can easily be mapped into the address space alongside
// DRAMs.  Also make sure that loads can't overtake stores (since
// SRAMs have separate load and store ports).  Using caches, stores
// can't overtake loads because a store is never issued to a line that
// is currently being loaded.

import DRAM       :: *;
import NarrowSRAM :: *;
import Interface  :: *;
import Queue      :: *;
import Util       :: *;
import Vector     :: *;

interface WideSRAM;
  interface In#(DRAMReq) reqIn;
  interface BOut#(DRAMResp) respOut;
  interface SRAMExtIfc external;
endinterface

module mkWideSRAM#(RAMId id) (WideSRAM);

  // Create SRAM instance
  SRAM sram <- mkSRAM(id);

  // Incoming request port
  InPort#(DRAMReq) reqInPort <- mkInPort;

  // Outgoing response queue
  Queue#(DRAMResp) respQueue <- mkUGQueue;

  // Connections to SRAM
  OutPort#(SRAMLoadReq) loadPort <- mkOutPort;
  OutPort#(SRAMStoreReq) storePort <- mkOutPort;
  InPort#(SRAMResp) respInPort <- mkInPort;
  connectUsing(mkUGQueue, loadPort.out, sram.loadIn);
  connectUsing(mkUGQueue, storePort.out, sram.storeIn);
  connectDirect(sram.respOut, respInPort.in);

  // Prevent loads from overtaking stores from same client
  // There are too many clients to keep a precise log, so
  // we keep an approximate one using the DCache id.
  Vector#(`DCachesPerDRAM, UpDown#(2)) busy <- replicateM(mkUpDown);

  // Store queue and progress counter
  Queue#(DRAMReq) storeQueue <- mkUGQueue;
  Reg#(Bit#(2)) storeCount <- mkReg(0);

  // Perform store request
  rule performStore (storeQueue.canDeq && storePort.canPut);
    DRAMReq reqIn = storeQueue.dataOut;
    Vector#(4, Bit#(64)) chunks = unpack(reqIn.data);
    SRAMStoreReq reqOut;
    reqOut.id = reqIn.id;
    reqOut.addr = { truncate(reqIn.addr), storeCount };
    reqOut.data = chunks[storeCount];
    reqOut.burst = 1;
    storePort.put(reqOut);
    storeCount <= storeCount + 1;
    if (storeCount == 3) storeQueue.deq;
  endrule

  // Forward requests to SRAM
  // But don't serve a load while there's an outstanding store from same client
  rule forwardRequests (reqInPort.canGet);
    DRAMReq reqIn = reqInPort.value;
    myAssert(reqIn.burst == 1, "WideSRAM only supports burst of 1");
    // Approximate client id
    DRAMReqId client = reqIn.id;
    if (reqIn.isStore) begin
      if (storeQueue.notFull && busy[client].canInc) begin
        busy[client].inc;
        storeQueue.enq(reqIn);
        reqInPort.get;
      end 
    end else begin
      if (loadPort.canPut) begin
        if (busy[client].value == 0) begin
          SRAMLoadReq reqOut;
          reqOut.id = reqIn.id;
          reqOut.addr = { truncate(reqIn.addr), 2'b00 };
          reqOut.burst = 4;
          reqOut.info = unpack(truncate(reqIn.data));
          loadPort.put(reqOut);
          reqInPort.get;
        end 
      end
    end
  endrule

  // Response buffer and count
  Vector#(4, Reg#(Bit#(64))) respBuffer <- replicateM(mkRegU);
  Reg#(Bit#(2)) respCount <- mkReg(0);

  // Consume load responses
  rule consumeLoadResps (respInPort.canGet);
    SRAMResp respIn = respInPort.value;
    respBuffer[respCount] <= respIn.data;
    if (respCount == 3) begin
      if (respQueue.notFull) begin
        DRAMResp respOut;
        Vector#(4, Bit#(64)) data;
        for (Integer i = 0; i < 4; i=i+1) data[i] = respBuffer[i];
        data[3] = respIn.data;
        respInPort.get;
        respOut.id = respIn.id;
        respOut.data = pack(data);
        respOut.info = respIn.info;
        respOut.finalBeat = True;
        respQueue.enq(respOut);
        respCount <= 0;
      end
    end else begin
      respInPort.get;
      respCount <= respCount + 1;
    end
  endrule

  // Count number of store-done responses
  Reg#(Bit#(2)) storeDoneCount <- mkReg(0);

  // Consume store-done responses
  rule consumeStoreDones;
    Option#(SRAMReqId) done = sram.storeDone();
    // Approximate client id
    DRAMReqId client = truncateLSB(done.value);
    // Increment count
    if (done.valid) storeDoneCount <= storeDoneCount + 1;
    // Clear busy bit when store completes
    if (done.valid && storeDoneCount == 3) busy[client].dec;
  endrule

  // Request interface
  interface reqIn = reqInPort.in;

  // Response interface
  interface BOut respOut;
    method Action get = respQueue.deq;
    method Bool valid = respQueue.canDeq;
    method DRAMResp value = respQueue.dataOut;
  endinterface

  // External interface
  interface SRAMExtIfc external = sram.external;

endmodule
