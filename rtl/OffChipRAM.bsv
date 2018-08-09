// Create a single off-chip RAM interface allowing access to a DRAM
// and two SRAMs.
//
//        +------+  +--------+  +--------+
//        | DRAM |  | SRAM A |  | SRAM B |
//        +------+  +--------+  +--------+
//           |          |           |
//           +----------+-----------+
//                      |

import DRAM       :: *;
import NarrowSRAM :: *;
import WideSRAM   :: *;
import Interface  :: *;
import Queue      :: *;
import Util       :: *;
import Vector     :: *;

interface OffChipRAM;
  interface In#(DRAMReq) reqIn;
  interface BOut#(DRAMResp) respOut;
  interface DRAMExtIfc extDRAM;
  interface Vector#(2, SRAMExtIfc) extSRAM;
endinterface

module mkOffChipRAM#(RAMId base) (OffChipRAM);

  // Create DRAM instance
  DRAM dram <- mkDRAM(base);

  // Create SRAM instances
  WideSRAM sramA <- mkWideSRAM(base+1);
  WideSRAM sramB <- mkWideSRAM(base+2);

  // Incoming request port
  InPort#(DRAMReq) reqInPort <- mkInPort;

  // Outgoing response queue
  Queue#(DRAMResp) respQueue <- mkUGQueue;

  // Connections to SRAM
  OutPort#(DRAMReq) toSRAMA <- mkOutPort;
  OutPort#(DRAMReq) toSRAMB <- mkOutPort;
  OutPort#(DRAMReq) toDRAM  <- mkOutPort;
  connectUsing(mkUGQueue, toSRAMA.out, sramA.reqIn);
  connectUsing(mkUGQueue, toSRAMB.out, sramB.reqIn);
  connectUsing(mkUGQueue, toDRAM.out, dram.reqIn);

  InPort#(DRAMResp) fromSRAMA <- mkInPort;
  InPort#(DRAMResp) fromSRAMB <- mkInPort;
  InPort#(DRAMResp) fromDRAM  <- mkInPort;
  connectDirect(sramA.respOut, fromSRAMA.in);
  connectDirect(sramB.respOut, fromSRAMB.in);
  connectDirect(dram.respOut, fromDRAM.in);

  // Forward requests
  rule forwardRequests (reqInPort.canGet);
    DRAMReq reqIn = reqInPort.value;
    // Get the upper bits of the address
    Bit#(TSub#(`LogBeatsPerMem, `LogBeatsPerSRAM)) upperBits =
      truncateLSB(reqIn.addr);
    // Determine where to send request to
    if (upperBits == 1) begin
      // Send to SRAM A
      if (toSRAMA.canPut) begin
        toSRAMA.put(reqInPort.value);
        reqInPort.get;
      end
    end else if (upperBits == 2) begin
      // Send to SRAM B
      if (toSRAMB.canPut) begin
        toSRAMB.put(reqInPort.value);
        reqInPort.get;
      end
    end else begin
      // Send to DRAM
      if (toDRAM.canPut) begin
        toDRAM.put(reqInPort.value);
        reqInPort.get;
      end
    end
  endrule

  // Forward responses
  rule forwardResps (respQueue.notFull);
    if (fromSRAMA.canGet) begin
      respQueue.enq(fromSRAMA.value);
      fromSRAMA.get;
    end else if (fromSRAMB.canGet) begin
      respQueue.enq(fromSRAMB.value);
      fromSRAMB.get;
    end else if (fromDRAM.canGet) begin
      respQueue.enq(fromDRAM.value);
      fromDRAM.get;
    end
  endrule

  // Request interface
  interface reqIn = reqInPort.in;

  // Response interface
  interface BOut respOut;
    method Action get = respQueue.deq;
    method Bool valid = respQueue.canDeq;
    method DRAMResp value = respQueue.dataOut;
  endinterface

  // External interfaces
  interface extDRAM = dram.external;
  interface extSRAM = 
    vector(sramA.external, sramB.external);

endmodule
