package OffChipRAM;

// The OffChipRAM module is an (optional) module that attempts to
// combine all off-chip DRAM and SRAM resources on the DE5-NET in a
// way that is benificial for POETS applications.  The two DRAMs and
// the single SRAM are all assumed to have the same data width, and
// are arranged as follows:
//
//                        SRAM
//                          |
//                      +-------+
//             DRAM 0   | Merge |     DRAM 1
//               |      +-------+       |
//               |         |  |         |
//               +---------+  +---------+
//               |                      |
//               |                      |
//        DRAM Client 0          DRAM Client 1
//
// On the DE5-NET, each DRAM is 2GB in capacity and the SRAM is only
// 32MB.  We make the assumption that, for POETS applications, each
// thread's memory paritition is most likely to be accessed near the
// beginning.  So a simple way to reduce pressure on the DRAMs is to
// spread (say, in a 1024 thread configuration) the first 64KB of each
// thread's partition accross *both* DRAM and SRAM by iterleaving
// adddresses at the cache-line granularity.  The attraction of the SRAM 
// in this role is that it can sustain full throughput regardless
// of the access pattern, whereas DRAM performance suffers when
// all threads are accessing their own partitions at the same time.
//
// This module has been designed very much with the DE5-NET in mind.
// In particular, we assume exactly two DRAMs and one SRAM, all with
// the same data width.

import Interface :: *;
import Queue     :: *;
import Vector    :: *;
import DRAM      :: *;
import QSRAM     :: *;

// Does the given address map to SRAM?
function Bool mapsToSRAM(Bit#(1) dramId, Bit#(`LogBeatsPerDRAM) addr);
  // Separate address into MSB and rest
  Bit#(1) msb = truncateLSB(addr);
  Bit#(TSub#(`LogBeatsPerDRAM, 1)) rest = truncate(addr);
  // The bottom bits address beats within a line
  Bit#(`LogBeatsPerLine) bottom = truncate(rest);
  Bit#(TSub#(TSub#(`LogBeatsPerDRAM, 1), `LogBeatsPerLine)) middle =
    truncateLSB(rest);
  // Separate upper half of address space into partition index and offset
  Bit#(`LogThreadsPerDRAM) partIndex = truncateLSB(middle);
  Bit#(`LogLinesPerDRAMPartition) partOffset = truncate(middle);
  Bool oddLine = unpack(truncate(middle));
  // The portion of the partition offset outside the reach of SRAM
  Bit#(TSub#(TSub#(`LogLinesPerDRAMPartition, `LogLinesPerSRAMPartition), 1))
    aboveSRAM = truncateLSB(partOffset);
  // Maps to SRAM if it's an odd line and within reach
  return msb == 1 && oddLine && aboveSRAM == 0;
endfunction

// If so, where in SRAM does it map to?
function Bit#(`LogBeatsPerSRAM)
    toSRAMAddr(Bit#(1) dramId, Bit#(`LogBeatsPerDRAM) addr);
  // Drop MSB
  Bit#(TSub#(`LogBeatsPerDRAM, 1)) rest = truncate(addr);
  // Chop the bottom bits which address beats within a line
  Bit#(TSub#(TSub#(`LogBeatsPerDRAM, 1), `LogBeatsPerLine)) middle =
    truncateLSB(rest);
  // Separate upper half of address space into partition index and offset
  Bit#(`LogThreadsPerDRAM) partIndex = truncateLSB(middle);
  Bit#(TAdd#(`LogLinesPerSRAMPartition, 1)) partOffsetPlus = truncate(middle);
  Bit#(`LogLinesPerSRAMPartition) partOffset = truncateLSB(partOffsetPlus);
  // Return SRAM address
  return { dramId, partIndex, partOffset };
endfunction

interface OffChipRAM;
  interface DRAM dram0;
  interface DRAM dram1;
  interface SRAMExtIfc sramExt;
endinterface

module mkOffChipRAM (OffChipRAM);
  // Instantiate SRAM
  SRAM sram <- mkSRAM(0);

  // Instantiate DRAMs
  DRAM dramA <- mkDRAM(1);
  DRAM dramB <- mkDRAM(2);

  // Request ports
  InPort#(DRAMReq) reqInA <- mkInPort;
  InPort#(DRAMReq) reqInB <- mkInPort;

  // DRAM request ports
  OutPort#(DRAMReq) dramReqsA <- mkOutPort;
  OutPort#(DRAMReq) dramReqsB <- mkOutPort;

  // Connect DRAM request ports
  connectUsing(mkUGQueue, dramReqsA.out, dramA.reqIn);
  connectUsing(mkUGQueue, dramReqsB.out, dramB.reqIn);

  // SRAM request ports
  OutPort#(SRAMLoadReq) sramLoadReqsA <- mkOutPort;
  OutPort#(SRAMLoadReq) sramLoadReqsB <- mkOutPort;
  OutPort#(SRAMStoreReq) sramStoreReqsA <- mkOutPort;
  OutPort#(SRAMStoreReq) sramStoreReqsB <- mkOutPort;

  // Pass request to the appropriate RAM
  function Action pass(Bit#(1)                dramId,
                       InPort#(DRAMReq)       reqIn,
                       OutPort#(DRAMReq)      dramReqOut,
                       OutPort#(SRAMStoreReq) sramStoreReqOut,
                       OutPort#(SRAMLoadReq)  sramLoadReqOut) = action
    if (reqIn.canGet) begin
      DRAMReq req = reqIn.value;
      if (mapsToSRAM(dramId, req.addr)) begin
        if (req.isStore) begin
          SRAMStoreReq sramReq;
          sramReq.id    = {dramId, req.id};
          sramReq.addr  = toSRAMAddr(dramId, req.addr);
          sramReq.data  = req.data;
          sramReq.burst = req.burst;
          if (sramStoreReqOut.canPut) begin
            reqIn.get;
            sramStoreReqOut.put(sramReq);
          end
        end else begin
          SRAMLoadReq sramReq;
          sramReq.id = {dramId, req.id};
          sramReq.addr = toSRAMAddr(dramId, req.addr);
          sramReq.burst = req.burst;
          sramReq.info = unpack(truncate(req.data));
          if (sramLoadReqOut.canPut) begin
            reqIn.get;
            sramLoadReqOut.put(sramReq);
          end
        end
      end else begin
        if (dramReqOut.canPut) begin
          reqIn.get;
          dramReqOut.put(req);
        end
      end
    end
  endaction;

  // Call the pass-request function for each client
  rule passRequest;
    // Handle DRAM client A 
    pass(0, reqInA, dramReqsA, sramStoreReqsA, sramLoadReqsA);
    // Handle DRAM client B
    pass(1, reqInB, dramReqsB, sramStoreReqsB, sramLoadReqsB);
  endrule

  // Merge the SRAM load requests
  let mergedLoads <- mkMergeTwo(Fair, mkUGQueue,
                       sramLoadReqsA.out, sramLoadReqsB.out);

  // Merge the SRAM store requests
  let mergedStores <- mkMergeTwo(Fair, mkUGQueue,
                        sramStoreReqsA.out, sramStoreReqsB.out);

  // Connect mergers to SRAM inputs
  connectUsing(mkUGQueue, mergedLoads, sram.loadIn);
  connectUsing(mkUGQueue, mergedStores, sram.storeIn);

  // Response buffers
  Queue#(DRAMResp) respBufferA <- mkUGQueue;
  Queue#(DRAMResp) respBufferB <- mkUGQueue;

  // Port for recieving SRAM responses
  InPort#(SRAMResp) sramRespPort <- mkInPort;

  // Connect SRAM response port
  connectDirect(sram.respOut, sramRespPort.in);

  // Ports for receiving DRAM responses
  InPort#(DRAMResp) dramRespPortA <- mkInPort;
  InPort#(DRAMResp) dramRespPortB <- mkInPort;

  // Connect DRAM response ports
  connectDirect(dramA.respOut, dramRespPortA.in);
  connectDirect(dramB.respOut, dramRespPortB.in);

  // Fill the response buffers
  // (Favouring SRAM responses)
  rule fillResps;
    SRAMResp resp = sramRespPort.value;
    Bit#(1) dramId = truncateLSB(resp.id);
    DRAMResp dramResp;
    dramResp.id = truncate(resp.id);
    dramResp.data = resp.data;
    dramResp.info = resp.info;
    // Get SRAM response?
    Bool getSRAM = False;
    // Try to enqueue respBufferA
    if (respBufferA.notFull) begin
      if (sramRespPort.canGet && dramId == 0) begin
        getSRAM = True;
        respBufferA.enq(dramResp);
      end else if (dramRespPortA.canGet) begin
        dramRespPortA.get;
        respBufferA.enq(dramRespPortA.value);
      end
    end
    // Try to enqueue respBufferB
    if (respBufferB.notFull) begin
      if (sramRespPort.canGet && dramId == 1) begin
        getSRAM = True;
        respBufferB.enq(dramResp);
      end else if (dramRespPortB.canGet) begin
        dramRespPortB.get;
        respBufferB.enq(dramRespPortB.value);
      end
    end
    // Dequeue SRAM
    if (getSRAM) sramRespPort.get;
  endrule

  interface DRAM dram0;
    interface reqIn = reqInA.in;
    interface BOut respOut;
      method Action get = respBufferA.deq;
      method Bool valid = respBufferA.canDeq && respBufferA.canPeek;
      method DRAMResp value = respBufferA.dataOut;
    endinterface
    interface external = dramA.external;
  endinterface

  interface DRAM dram1;
    interface reqIn = reqInB.in;
    interface BOut respOut;
      method Action get = respBufferB.deq;
      method Bool valid = respBufferB.canDeq && respBufferB.canPeek;
      method DRAMResp value = respBufferB.dataOut;
    endinterface
    interface external = dramB.external;
  endinterface

  interface SRAMExtIfc sramExt = sram.external;
endmodule

endpackage
