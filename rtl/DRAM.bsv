package DRAM;

`ifdef SIMULATE

// ============================================================================
// Simulation
// ============================================================================

// Imports
// -------

import Mem    :: *;
import FIFOF  :: *;
import Vector :: *;

// Interface to C functions
import "BDPI" function Action ramInit();
import "BDPI" function Action ramWrite(Bit#(32) addr, Bit#(32) data);
import "BDPI" function ActionValue#(Bit#(32)) ramRead(Bit#(32) addr);

// Types
// -----

// Used for merging load and store requests into a single stream
typedef enum { PickNone, PickLoad, PickStore } Pick deriving (Bits, Eq);

// Implementation
// --------------

module mkDRAM (Mem);
  // State
  Vector#(`DRAMLatency, Reg#(Bool)) valids <- replicateM(mkReg(False));
  Vector#(`DRAMLatency, Reg#(MemReq)) reqs <- replicateM(mkRegU);
  FIFOF#(MemStoreReq)  storeReqFifo  <- mkUGFIFOF;
  FIFOF#(MemLoadReq)   loadReqFifo   <- mkUGFIFOF;
  FIFOF#(MemReq)       reqFifo       <- mkUGFIFOF;
  FIFOF#(MemStoreResp) storeRespFifo <- mkUGFIFOF;
  FIFOF#(MemLoadResp)  loadRespFifo  <- mkUGFIFOF;
  Reg#(Bool) toggle <- mkReg(False);

  // Constants
  Integer endIndex = `DRAMLatency-1;
  Integer wordsPerLine = `LineSize/32;

  // Merge load and store requests into single queue
  rule mergeReqs (reqFifo.notFull);
    Pick pick = PickNone;
    if (storeReqFifo.notEmpty && loadReqFifo.notEmpty)
      pick = toggle ? PickStore : PickLoad;
    else if (storeReqFifo.notEmpty)
      pick = PickStore;
    else if (loadReqFifo.notEmpty)
      pick = PickLoad;
    if (pick == PickLoad) begin
      loadReqFifo.deq;
      reqFifo.enq(LoadReq(loadReqFifo.first));
    end else if (pick == PickStore) begin
      storeReqFifo.deq;
      reqFifo.enq(StoreReq(storeReqFifo.first));
    end
    toggle <= !toggle;
  endrule

  // Try to perform a request
  rule step;
    Bool shift = False;
    if (valids[0]) begin
      case (reqs[0]) matches
        tagged LoadReq .req: begin
          if (loadRespFifo.notFull) begin
            shift = True;
            Vector#(`WordsPerLine, Bit#(32)) elems;
            Bit#(32) addr = {req.addr, 0};
            for (Integer i = 0; i < `WordsPerLine; i=i+1) begin
              let val <- ramRead(addr+fromInteger(4*i));
              elems[i] = val;
            end
            MemLoadResp resp;
            resp.id = req.id;
            resp.data = pack(elems);
            loadRespFifo.enq(resp);
          end
        end
        tagged StoreReq .req: begin
          if (storeRespFifo.notFull) begin
            shift = True;
            Vector#(`WordsPerLine, Bit#(32)) elems = unpack(req.data);
            Bit#(32) addr = {req.addr, 0};
            for (Integer i = 0; i < `WordsPerLine; i=i+1)
              ramWrite(addr+fromInteger(4*i), elems[i]);
            MemStoreResp resp = ?;
            resp.id = req.id;
            storeRespFifo.enq(resp);
          end
        end
      endcase
    end
    // Insert a new request
    if (reqFifo.notEmpty && (shift || !valids[endIndex])) begin
      reqFifo.deq;
      reqs[endIndex] <= reqFifo.first;
      valids[endIndex] <= True;
    end else
      valids[endIndex] <= False;
    // Shift requests
    for (Integer i = 0; i < endIndex; i=i+1) begin
      shift = shift || !valids[i];
      if (shift) begin
        reqs[i] <= reqs[i+1];
        valids[i] <= valids[i+1];
      end
    end
  endrule

  // Methods
  method Bool canPutLoad = loadReqFifo.notFull;
  method Action putLoadReq(MemLoadReq req);
    loadReqFifo.enq(req);
  endmethod
  method Bool canPutStore = storeReqFifo.notFull;
  method Action putStoreReq(MemStoreReq req);
    storeReqFifo.enq(req);
  endmethod
  method Bool canGetLoad = loadRespFifo.notEmpty;
  method ActionValue#(MemLoadResp) getLoadResp;
    loadRespFifo.deq; return loadRespFifo.first;
  endmethod
  method Bool canGetStore = storeRespFifo.notEmpty;
  method ActionValue#(MemStoreResp) getStoreResp;
    storeRespFifo.deq; return storeRespFifo.first;
  endmethod
endmodule

`else

// ============================================================================
// Synthesis
// ============================================================================

`endif

endpackage
