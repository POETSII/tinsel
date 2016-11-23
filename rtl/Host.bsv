// Copyright (c) Matthew Naylor

package Host;

// This host component forms part of the ring of mailboxes (see
// Ring.bsv) and contains an interface to a host machine via Altera's
// JTAG UART component (see JtagUart.bsv).  It allows messages to be
// sent from a host machine to any thread on the ring, and viceversa.

// =============================================================================
// Imports
// =============================================================================

import Globals   :: *;
import Interface :: *;
import ConfigReg :: *;
import JtagUart  :: *;
import Mailbox   :: *;
import Util      :: *;
import Queue     :: *;
import BlockRam  :: *;

// =============================================================================
// Interface
// =============================================================================

interface Host;
  // This part connects to the ring of mailboxes
  interface MailboxNet net;
`ifndef SIMULATE
  // This part connects to Altera's JTAG UART component
  interface JtagUartAvalon jtagAvalon;
`endif
endinterface

// =============================================================================
// Types
// =============================================================================

// Host -> FPGA byte stream format:
//   * byte 0 contains N-1, the number of flits in the message minus 1
//   * bytes 1..4 contain the destination address of the message
//   * reamaining N*BytesPerFlit bytes contain the message payload
//
// FPGA -> Host byte stream format:
//   * byte 0 contains N-1, the number of flits in the message minus 1
//   * reamaining N*BytesPerFlit bytes contain the message payload

// State machine for the deserialiser
typedef enum {
  DES_GET_LEN,
  DES_GET_DEST,
  DES_GET_PAYLOAD
} DeserialiserState deriving (Bits, Eq);

// State machine for the byte emitter
typedef enum {
  EMIT_LEN,
  EMIT_FETCH_BYTE_1,
  EMIT_FETCH_BYTE_2,
  EMIT_FETCH_BYTE_3,
  EMIT_BYTE
} EmitterState deriving (Bits, Eq);

// Size of the serialise buffer
typedef 6 LogMsgsPerSerialiseBuffer;
typedef TAdd#(LogMsgsPerSerialiseBuffer, `LogMaxFlitsPerMsg)
  LogFlitsPerSerialiseBuffer;
typedef Bit#(LogFlitsPerSerialiseBuffer) SerialiseBufferFlitAddr;
typedef Bit#(TAdd#(LogFlitsPerSerialiseBuffer, `LogBytesPerFlit))
  SerialiseBufferByteAddr;

// =============================================================================
// Implementation
// =============================================================================

module mkHost (Host);
  // Input and output ports
  InPort#(Flit)     fromNet  <- mkInPort;
  InPort#(Bit#(8))  fromJtag <- mkInPort;
  OutPort#(Bit#(8)) toJtag   <- mkOutPort;

  // Queue of output flits
  SizedQueue#(`LogMaxFlitsPerMsg, Flit) toNet <-
    mkUGShiftQueue(QueueOptFmax);

  // Create JTAG UART instance
  JtagUart uart <- mkJtagUart;

  // Conect ports to UART
  connectUsing(mkUGShiftQueue1(QueueOptFmax), toJtag.out, uart.jtagIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), uart.jtagOut, fromJtag.in);

  // Deserialiser
  // ============
  //
  // Deserialise the byte stream into a flit stream

  // State machine dispatch
  Reg#(DeserialiserState) desState <- mkReg(DES_GET_LEN);

  // Holds number of flits in incoming message
  Reg#(Bit#(`LogMaxFlitsPerMsg)) desFlitCount <- mkRegU;

  // Number of bytes in destination address that have been received
  Reg#(Bit#(2)) desDestCount <- mkReg(0);

  // Destination shift-register for serial->parallel conversion
  Reg#(Bit#(32)) desDest <- mkRegU;

  // Track number of messages in the toNet buffer
  // (Used to prevent dequeing of toNet until a full message present)
  Count#(TAdd#(1, `LogMaxFlitsPerMsg)) desInFlight <-
    mkCount(`MaxFlitsPerMsg);

  // Payload shift-register for serial->parallel conversion
  Reg#(FlitPayload) desShiftReg <- mkRegU;
  
  // Track number of bytes present in shift-register
  Reg#(Bit#(`LogBytesPerFlit)) desBytesPerFlitCount <- mkReg(0);

  rule deserialise (fromJtag.canGet);
    case (desState)
      DES_GET_LEN:
        begin
          fromJtag.get;
          desState <= DES_GET_DEST;
          desFlitCount <= truncate(fromJtag.value);
        end
      DES_GET_DEST:
        begin
          fromJtag.get;
          desDest <= {fromJtag.value, truncateLSB(desDest)};
          desDestCount <= desDestCount+1;
          if (allHigh(desDestCount)) desState <= DES_GET_PAYLOAD;
        end
      DES_GET_PAYLOAD:
        if (desInFlight.notFull && toNet.notFull) begin
          fromJtag.get;
          // Prepare flit
          Flit flit;
          flit.dest = truncate(desDest);
          flit.payload = {fromJtag.value, truncateLSB(desShiftReg)};
          flit.notFinalFlit = True;
          // Update state
          desShiftReg <= flit.payload;
          desBytesPerFlitCount <= desBytesPerFlitCount+1;
          // Ready to send flit?
          if (allHigh(desBytesPerFlitCount)) begin
            desFlitCount <= desFlitCount-1;
            // Ready to send final flit?
            if (desFlitCount == 0) begin
              flit.notFinalFlit = False;
              desInFlight.inc;
              desState <= DES_GET_LEN;
            end
            toNet.enq(flit);
          end
        end
    endcase
  endrule

  // Serialiser
  // ==========
  //
  // Serialise the flit stream into a byte stream

  // Block RAM to store incoming flit payload data
  BlockRamOpts bufferOpts = defaultBlockRamOpts;
  BlockRamTrueMixed#(SerialiseBufferFlitAddr, FlitPayload,
                     SerialiseBufferByteAddr, Bit#(8))
    serialiseBuffer <- mkBlockRamTrueMixedOpts(bufferOpts);

  // Above block RAM is treated like a queue, with front and back pointers
  Reg#(SerialiseBufferByteAddr) payloadFront <- mkReg(0);
  Reg#(SerialiseBufferFlitAddr) payloadBack <- mkReg(0);

  // Queue of requests for the byte emitter
  SizedQueue#(LogMsgsPerSerialiseBuffer, MsgLen) emitQueue <-
    mkUGSizedQueue;

  // Count the number of flits in the incoming message
  Reg#(Bit#(`LogMaxFlitsPerMsg)) serFlitCount <- mkReg(0);

  rule serialise (fromNet.canGet && emitQueue.notFull);
    fromNet.get;
    Flit flit = fromNet.value;
    serialiseBuffer.putA(True, payloadBack, flit.payload);
    payloadBack <= payloadBack+1;
    if (!flit.notFinalFlit) begin
      emitQueue.enq(serFlitCount);
      serFlitCount <= 0;
    end else
      serFlitCount <= serFlitCount+1;
  endrule

  // Emitter
  // =======
  //
  // Emit the serial byte stream

  // State machine
  Reg#(EmitterState) emitState <- mkReg(EMIT_LEN);

  // Number of bytes in message being emitted
  Reg#(Bit#(TAdd#(`LogBytesPerMsg, 1))) emitBytes <- mkRegU;

  // Byte being emitted
  Reg#(Bit#(8)) emitByteReg <- mkRegU;

  rule emitByte;
    case (emitState)
      EMIT_LEN:
        if (toJtag.canPut && emitQueue.canPeek && emitQueue.canDeq) begin
          let len = emitQueue.dataOut;
          emitQueue.deq;
          toJtag.put(zeroExtend(len));
          Bit#(`LogBytesPerFlit) offset = 0;
          emitBytes <= {zeroExtend(len)+1, offset};
          emitState <= EMIT_FETCH_BYTE_1;
        end
      EMIT_FETCH_BYTE_1:
        if (emitBytes == 0)
          emitState <= EMIT_LEN;
        else begin
          serialiseBuffer.putB(False, payloadFront, ?);
          payloadFront <= payloadFront+1;
          emitBytes <= emitBytes-1;
          emitState <= EMIT_FETCH_BYTE_2;
        end
      EMIT_FETCH_BYTE_2:
        emitState <= EMIT_FETCH_BYTE_3;
      EMIT_FETCH_BYTE_3:
        begin
          emitByteReg <= serialiseBuffer.dataOutB;
          emitState <= EMIT_BYTE;
        end
      EMIT_BYTE:
        if (toJtag.canPut) begin
          toJtag.put(emitByteReg);
          emitState <= EMIT_FETCH_BYTE_1;
        end
    endcase
  endrule

  // Interfaces
  interface MailboxNet net;
    interface In flitIn = fromNet.in;
    interface BOut flitOut;
      method Action get;
        toNet.deq;
        if (!toNet.dataOut.notFinalFlit) desInFlight.dec;
      endmethod
      method Bool valid = toNet.canDeq && desInFlight.value != 0;
      method Flit value = toNet.dataOut;
    endinterface
  endinterface

`ifndef SIMULATE
  interface JtagUartAvalon jtagAvalon = uart.jtagAvalon;
`endif

endmodule

endpackage
