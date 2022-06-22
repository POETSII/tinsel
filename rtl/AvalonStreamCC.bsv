import Mac :: *;
import MIMO :: *;
import Clocks :: *;
import BRAMFIFO :: * ;
import FIFOLevel :: * ;
import FIFOF :: * ;
import Vector :: * ;

typedef struct {
  Bit#(512) data;
  Bool valid;
  Bool startofpacket;
  Bool endofpacket;
  Bit#(1) error;
  Bit#(6) empty;
} AvalonSource deriving (Bits);

typedef struct {
  Bit#(512) data;
  Bool valid;
  Bool startofpacket;
  Bool endofpacket;
  Bit#(6) error;
  Bit#(6) empty;
} AvalonSinkData deriving (Bits);


interface AvalonCCIfc;
  interface AvalonMac external;
endinterface

interface NStageFIFOF#(type element_type, numeric type stages);
  method Action enq( element_type x1 );
  method Action deq();
  method element_type first();
  method Action clear();

  method Bool notFull;
  method Bool notEmpty;
endinterface

module mkPipelinesUGFIFOF(NStageFIFOF#(element_type, stages)) provisos (Bits#(element_type, width_any), Add#(1, z__, stages));

  Vector#(stages, FIFOF#(element_type)) pipeline_fifos = newVector();
  for (Integer s=0; s<valueOf(stages); s=s+1) begin
    pipeline_fifos[s] <- mkUGLFIFOF();
  end

  for (Integer s=0; s<valueOf(stages)-1; s=s+1) begin
    rule runpipeline;
      if (pipeline_fifos[s].notEmpty && pipeline_fifos[s+1].notFull) begin
        pipeline_fifos[s+1].enq(pipeline_fifos[s].first);
        pipeline_fifos[s].deq;
      end
    endrule
  end

  method Action enq(element_type x1) = pipeline_fifos[0].enq(x1);
  method Bool notFull = pipeline_fifos[0].notFull;
  method Bool notEmpty = pipeline_fifos[valueOf(stages)-1].notEmpty;
  method element_type first = pipeline_fifos[valueOf(stages)-1].first;
  method Action deq = pipeline_fifos[valueOf(stages)-1].deq;

  method Action clear();
    for (Integer s=0; s<valueOf(stages); s=s+1) begin
      pipeline_fifos[0].clear();
    end
  endmethod

endmodule


module mkAvalonStreamConverter#(AvalonMac avalonIn)
                             (Clock clockInternal, Clock clockExternalTx, Clock clockExternalRx,
                             Reset resetInternal, Reset resetExternalTx, Reset resetExternalRx,
                              AvalonCCIfc ifc);

    SyncFIFOCountIfc#(AvalonSource, 16) internalToExternalFIFO <- mkSyncFIFOCount(clockInternal, resetInternal, clockExternalTx);
    // FIFOF#(AvalonSource) internalToExternal_readoutFIFO <- mkGSizedFIFOF(False, True, 2, clocked_by clockExternalTx, reset_by resetExternalTx); // unguarded dequeue
    NStageFIFOF#(AvalonSource, 4) internalToExternal_readoutFIFO <- mkPipelinesUGFIFOF(clocked_by clockExternalTx, reset_by resetExternalTx); // unguarded dequeue

    SyncFIFOCountIfc#(AvalonSinkData, 16) externalToInternalFIFO <- mkSyncFIFOCount(clockExternalRx, resetExternalRx, clockInternal);
    // FIFOF#(AvalonSinkData) externalToInternal_readinFIFO <- mkGSizedFIFOF(True, False, 2, clocked_by clockExternalRx, reset_by resetExternalRx); // unguarded enqueue
    NStageFIFOF#(AvalonSinkData, 4) externalToInternal_readinFIFO <- mkPipelinesUGFIFOF(clocked_by clockExternalRx, reset_by resetExternalRx); // unguarded dequeue

    // (* no_implicit_conditions, fire_when_enabled *)
    rule enq_out;
      AvalonSource flit = AvalonSource { data:avalonIn.source_data,
                            valid:avalonIn.source_valid,
                            startofpacket:avalonIn.source_startofpacket,
                            endofpacket:avalonIn.source_endofpacket,
                            error:avalonIn.source_error,
                            empty:avalonIn.source_empty };
      if (flit.valid) internalToExternalFIFO.enq(flit);
    endrule

    rule move_out_to_deqfifo (internalToExternal_readoutFIFO.notFull);
      internalToExternal_readoutFIFO.enq(internalToExternalFIFO.first);
      internalToExternalFIFO.deq;
    endrule

    rule drive_input_rdy;
      avalonIn.source(internalToExternalFIFO.sNotFull);
    endrule

    // (* no_implicit_conditions, fire_when_enabled *)
    rule drive_input_sink;
      AvalonSinkData flit = externalToInternalFIFO.first;
      avalonIn.sink( flit.data, flit.valid, flit.startofpacket, flit.endofpacket, flit.error, flit.empty );
      if (avalonIn.sink_ready) begin
        externalToInternalFIFO.deq;
      end
    endrule

    rule move_in_to_enqfifo (externalToInternal_readinFIFO.notEmpty);
      externalToInternalFIFO.enq(externalToInternal_readinFIFO.first);
      externalToInternal_readinFIFO.deq;
    endrule

    interface AvalonMac external;
      // Avalon streaming source interface
      method Bit#(512) source_data = internalToExternal_readoutFIFO.first.data;
      method Bool source_valid = internalToExternal_readoutFIFO.first.valid && internalToExternal_readoutFIFO.notEmpty;
      method Bool source_startofpacket = internalToExternal_readoutFIFO.first.startofpacket;
      method Bool source_endofpacket = internalToExternal_readoutFIFO.first.endofpacket;
      method Bit#(1) source_error = internalToExternal_readoutFIFO.first.error;
      method Bit#(6) source_empty = internalToExternal_readoutFIFO.first.empty;
      method Action source(Bool source_ready);
        if (source_ready && internalToExternal_readoutFIFO.notEmpty) internalToExternal_readoutFIFO.deq;
      endmethod

      // Avalon streaming sink interface
      method Bool sink_ready = externalToInternal_readinFIFO.notFull;
      method Action sink(Bit#(512) sink_data, Bool sink_valid,
                           Bool sink_startofpacket, Bool sink_endofpacket,
                             Bit#(6) sink_error, Bit#(6) sink_empty);
         AvalonSinkData flit = AvalonSinkData { data:sink_data,
                               valid:sink_valid,
                               startofpacket:sink_startofpacket,
                               endofpacket:sink_endofpacket,
                               error:sink_error,
                               empty:sink_empty };
        if (sink_valid && externalToInternalFIFO.sNotFull) externalToInternal_readinFIFO.enq(flit);
      endmethod
    endinterface

endmodule
