// SPDX-License-Identifier: BSD-2-Clause
package ReliableLink;

// =============================================================================
// Overview
// =============================================================================
//
// This module supports a window-based reliable link layer on top of a
// 10G Ethernet MAC.  We assume that bit errors are detected by the
// MAC and that packets containing such bit errors have been dropped
// (see Mac.bsv).
//
// The basic structure is as follows:
//
//
//              +-----Reliable Link ----+
//  (Reliable)  |                       |  (Unreliable)
//              |  +-----------------+  |
//   User TX ----->| Transmit Buffer |  |--> MAC TX
//              |  +-----------------+  |
//   User RX <--|                       |<-- MAC RX
//              |                       |
//              +-----------------------+

// =============================================================================
// Imports
// =============================================================================

import Interface    :: *;
import BlockRam     :: *;
import DReg         :: *;
import Queue        :: *;
import Mac          :: *;
import ConfigReg    :: *;
import Util         :: *;
import Socket       :: *;
import Vector       :: *;

// =============================================================================
// Transmit Buffer
// =============================================================================
//
// The transmit buffer is a queue with two front pointers:
//
//  1. A "next pointer", marking the next item to be sent.
//     This pointer is incremented when the "take" method is called,
//     but the item is not discarded from the queue at this point.
//     The item pointed-at is available on the output of the transmit
//     buffer along with a sequence number (which in implementaiton
//     terms is simply the integer value of the "next pointer").
//
//  2. An "ack pointer", marking the first item that has yet to be
//     acknowledged by the receiver. The "ack" method takes a sequence
//     number as an argument, denoting the next item expected by the
//     receiver.  When called, the "ack" method updates the "ack
//     pointer" to the next item expected by the receiver.
//
// A timer is used to trigger the resend mechanism.  The timer is
// constantly reset to 0 while the buffer is empty.  When the buffer
// is non-empty, the timer is incremented.  If an ack is received the
// timer is reset to 0.  If the timer reaches a pre-defined timeout
// value then the "next pointer" is reset to the "ack pointer" and the
// timer is reset to 0.

// Interface
// ---------

typedef Bit#(`LogTransmitBufferSize) TransmitBufferPtr;

interface TransmitBuffer;
  // Put an item into the queue
  method Action enq(Bit#(64) item);
  // Guard on the above method
  method Bool canEnq;

  // Next item to send
  method Bit#(64) dataOut;
  // And what is its sequence number?
  method TransmitBufferPtr seqNum;
  // Use this method to move on to next item
  method Action take;

  // Acknowledge receipt of items
  method Action ack(TransmitBufferPtr nextExpected);

  // Number of unsent items in the buffer
  method TransmitBufferPtr unsent;

  // The timeout action can only occur when this method is called
  method Action enableTimeout;

  // Performance monitor
  method Bit#(32) numTimeouts;
endinterface

// Implementation
// --------------

module mkTransmitBuffer (TransmitBuffer);

  // Pointer to the next packet to send
  Reg#(TransmitBufferPtr) nextPtr <- mkConfigReg(0);

  // Pointer to the first packet yet to be acknowledged
  Reg#(TransmitBufferPtr) ackPtr <- mkConfigReg(0);

  // Pointer to back of queue
  Reg#(TransmitBufferPtr) backPtr <- mkConfigReg(0);

  // Contents of the buffer
  BlockRamOpts contentsOpts = defaultBlockRamOpts;
  contentsOpts.registerDataOut = False;
  BlockRam#(TransmitBufferPtr, Bit#(64)) contents <-
    mkBlockRamOpts(contentsOpts);

  // Track the nummber of unsent items in the buffer
  Reg#(TransmitBufferPtr) unsentReg <- mkConfigReg(0);

  // This register goes high when a timeout occurs
  Reg#(Bool) timeoutFlag <- mkConfigReg(False);

  // The timeout action can only occur when this signal is high
  PulseWire timeoutEn <- mkPulseWire;

  // The timer
  Reg#(Bit#(16)) timer <- mkConfigReg(0);

  // Signal to reset the timer
  PulseWire resetTimer <- mkPulseWireOR;

  // Signals that an element has is taken from the buffer
  PulseWire doTake <- mkPulseWire;

  // Performance monitor
  Reg#(Bit#(32)) numTimeoutsReg <- mkConfigReg(0);

  rule readNext;
    TransmitBufferPtr ptr = nextPtr;
    // Reset next pointer on a timeout
    if (timeoutFlag && timeoutEn) begin
      ptr = ackPtr;
      numTimeoutsReg <= numTimeoutsReg+1;
      resetTimer.send;
    end else if (doTake) ptr = ptr + 1;
    // Dereference nextPtr
    contents.read(ptr);
    unsentReg <= backPtr - ptr;
    // Update nextPtr
    nextPtr <= ptr;
  endrule

  rule updateTimer;
    if (ackPtr == backPtr || resetTimer) begin
      timer <= 0;
      timeoutFlag <= False;
    end else if (timer == `LinkTimeout)
      timeoutFlag <= True;
    else
      timer <= timer+1;
  endrule

  method Bool canEnq;
    TransmitBufferPtr ptr = backPtr+1;
    return (ptr != ackPtr && ptr != nextPtr);
  endmethod
  method Action enq(Bit#(64) item);
    contents.write(backPtr, item);
    backPtr <= backPtr+1;
  endmethod

  method Bit#(64) dataOut = contents.dataOut;
  method TransmitBufferPtr seqNum = nextPtr;

  method Action take;
    doTake.send;
  endmethod

  method Action ack(TransmitBufferPtr nextExpected);
    ackPtr <= nextExpected;
    if (ackPtr != nextExpected) resetTimer.send;
  endmethod

  method TransmitBufferPtr unsent = unsentReg;

  method Action enableTimeout = timeoutEn.send;

  method Bit#(32) numTimeouts = numTimeoutsReg;

endmodule

// =============================================================================
// Reliable Link
// =============================================================================

// Each transmitted packet (over the unreliable link) contains a
// header and zero or more items.  The header contains:
//
// 1. The pair (n, s) denoting that the packet contains n items
//    beginning with sequence number s.
//
// 2. The value s denoting the sequence number of next item expected
//    on the receive channel.  This serves as an acknowledgement that
//    all items up to, but not including, this item have been
//    received.

typedef struct {
  Bit#(7) numItems;           // Number of items being sent
  TransmitBufferPtr seqNum;   // Seq num of first item being sent
  TransmitBufferPtr ack;      // Seq num of next item to receive
} PacketHeader deriving (Bits);

// Interface
// ---------

interface ReliableLink;
`ifndef SIMULATE
  // Avalon interface to 10G MAC
  interface AvalonMac avalonMac;
`endif
  // Internal interface
  interface In#(Bit#(64)) streamIn;
  interface BOut#(Bit#(64)) streamOut;
  // Performance monitor
  method Bit#(32) numTimeouts;
endinterface

// Implementation
// --------------

module mkReliableLinkCore#(Mac mac) (ReliableLink);

  // Ports
  OutPort#(MacBeat) toMACPort   <- mkOutPort;
  InPort#(MacBeat)  fromMACPort <- mkInPort;
  InPort#(Bit#(64)) inPort      <- mkInPort;

  // Connections
  connectUsing(mkUGQueue, toMACPort.out, mac.fromUser);
  connectUsing(mkUGQueue, mac.toUser, fromMACPort.in);

  // Transmit buffer
  TransmitBuffer transmitBuffer <- mkTransmitBuffer;

  // Receive buffer
  SizedQueue#(`LogReliableLinkRecvBufferSize, Bit#(64))
    receiveBuffer <- mkUGSizedQueuePrefetch;

  // Transmitter
  // -----------

  // 3-state machine
  // State 0: send header
  // State 1: send header
  // State 2: send body

  // for 100G, this is a 2-state machine:
  // send header+body
  // send body if needed.
  Reg#(Bit#(1)) txState <- mkConfigReg(0);

  // Number of items to send
  Reg#(Bit#(7)) numItemsToSend <- mkConfigReg(0);

  // Sequence number of next item to receive
  Reg#(TransmitBufferPtr) nextItemToRecv <- mkConfigReg(0);

  // Count the number of idle cycles since an ACK was last sent
  Reg#(Bit#(8)) idlesSinceACKSent <- mkConfigReg(0);

  Reg#(Bit#(4)) ratelim <- mkReg(0);

  (* no_implicit_conditions, fire_when_enabled *)
  rule count;
    ratelim <= ratelim+1;
  endrule

  rule transmit0 (txState == 0 && toMACPort.canPut && ratelim == 0);

    // Bound number of items in packet
    // (Must be less than the size of the MAC receive buffer)
    myAssert(`TransmitBound < 2**`LogMacRecvBufferSize,
               "TransmitBound is too large");
    Bit#(7) couldSend = transmitBuffer.unsent > `TransmitBound ?
      `TransmitBound : truncate(transmitBuffer.unsent);
    Bit#(7) toSend = 0;
    if (couldSend > 0)
      toSend = 1;
    else
      toSend = 0;

    // if (toSend != 0) $display("[transmit0] ", transmitBuffer.dataOut);

    numItemsToSend <= toSend;
    // fix single flit per 100G frame; huge overhead, but saves building a buffer

    // Construct header
    PacketHeader h;
    h.numItems = toSend;
    h.seqNum   = zeroExtend(transmitBuffer.seqNum);
    h.ack      = nextItemToRecv;
    // Construct beat
    Bit#(64) h1 = zeroExtend(pack(h));
    // Send the 2nd beat of the header.  This beat contains the
    // ethernet type/length field, which we set to 0x600.
    Bit#(64) h2 = 64'h0600_0600_0600_0600;


    Bool eop = True; //toSend <= 6;

    Vector#(8, Bit#(64)) data_words;
    data_words[0] = h1;
    data_words[1] = h2;

    if (toSend != 0) data_words[2] = transmitBuffer.dataOut;
    else data_words[2] = 0;

    data_words[3] = 0;
    data_words[4] = 0;
    data_words[5] = 0;
    data_words[6] = 0;
    data_words[7] = 0;

    Bit#(512) data = pack(data_words);

    MacBeat beat;
    beat.start = False;
    beat.stop  = False;
    beat.data  = data;
    // Send beat (only send empty ACK once every 40 idle cycles)
    if (toSend != 0) begin
      beat.start = True;
      beat.stop  = eop;
      toMACPort.put(beat);
      idlesSinceACKSent <= 0;
      transmitBuffer.take;
      // Next state
    end else if (idlesSinceACKSent == 40) begin
      beat.start = True;
      beat.stop  = True;
      toMACPort.put(beat);
      idlesSinceACKSent <= 0;
    end else begin
      idlesSinceACKSent <= idlesSinceACKSent + 1;
      beat.data = 0;
    end

    txState <= eop ? 0 : 1;
    if (eop) transmitBuffer.enableTimeout;
    myAssert(eop, "Cannot handle multi-stage packets yet!");
  endrule


  rule transmit1 (txState == 1 && toMACPort.canPut);
    // $display("[transmit1]");
    myAssert(False, "Cannot handle multi-stage packets yet!");
    // Construct beat
    MacBeat beat;
    beat.start = False;
    beat.stop  = numItemsToSend == 1;
    beat.data  = zeroExtend(transmitBuffer.dataOut);
    // Send beat
    toMACPort.put(beat);
    // Update state
    txState <= beat.stop ? 0 : 1;
    numItemsToSend <= numItemsToSend-1;
    transmitBuffer.take;
    if (beat.stop) transmitBuffer.enableTimeout;
  endrule

  // Receiver
  // --------

  // Count of number of elements in the receive buffer
  Count#(TAdd#(`LogReliableLinkRecvBufferSize, 1)) receiveCount <-
    mkCount(2 ** `LogReliableLinkRecvBufferSize);

  // 2-state machine
  // State 0: receive header+body
  // State 1: receive body
  Reg#(Bit#(2)) rxState <- mkConfigReg(0);

  // Number of items to receive
  Reg#(Bit#(7)) numItemsToRecv <- mkConfigReg(0);

  rule receive0 (rxState == 0 && fromMACPort.canGet);
    // Receive beat
    MacBeat beat = fromMACPort.value;
    Vector#(8, Bit#(64)) data_words = unpack(beat.data);
    // Extract header
    PacketHeader h = unpack(truncate(data_words[0]));
    // Inform transmit buffer of acknowledgement
    transmitBuffer.ack(h.ack);
    // if (h.numItems != 0) $display("[receive0]");
    myAssert(h.numItems <= 1, "Too many items - we cannot enq more than 1 flit per cycle yet - receive0 numItems");

    // Is there space in the receive buffer?
    Bool space = receiveCount.available > `TransmitBound;
    // Are the received items in the expected sequence
    if (h.numItems != 0 && h.seqNum == nextItemToRecv && space) begin
      // $display("[receiveBuffer.enq] ", data_words[2]);
      numItemsToRecv <= h.numItems;
      nextItemToRecv <= nextItemToRecv + zeroExtend(h.numItems);
      receiveCount.incBy(zeroExtend(h.numItems));

      myAssert(receiveBuffer.notFull, "Receive buffer overflow!");
      fromMACPort.get;
      receiveBuffer.enq(data_words[2]);
    end else begin
      // Drop packet
      fromMACPort.get;
      numItemsToRecv <= 0;
    end
    // Next state
    myAssert(beat.stop, "Too many beats - we cannot enq more than 1 flit per cycle yet receive0 beat.stop");
    rxState <= beat.stop ? 0 : 1;
    // rxState <= 0;
  endrule

  // rule receive1 (rxState == 1 && fromMACPort.canGet);
  //   // Ignore second beat of header.
  //   fromMACPort.get;
  //   MacBeat beat = fromMACPort.value;
  //   rxState <= beat.stop ? 0 : 2;
  // endrule

  rule receive1 (rxState == 1 && fromMACPort.canGet);
    // We never send a non-min-sized packet, but that does not stop us getting them.
    // so in sim, assert, but in hardware, wait until eop and return to normal polling.
    fromMACPort.get;
    MacBeat beat = fromMACPort.value;
    rxState <= beat.stop ? 0 : 1;
    // $display("[receive1]");
    myAssert(False, "Too many beats - we cannot enq more than 1 flit per cycle yet - receive1");
    // Receive beat
    // MacBeat beat = fromMACPort.value;
    // // Are there any items to receive?
    // if (numItemsToRecv != 0) begin
    //   myAssert(receiveBuffer.notFull, "Receive buffer overflow!");
    //   // Receive items
    //   fromMACPort.get;
    //   receiveBuffer.enq(beat.data);
    //   numItemsToRecv <= numItemsToRecv - 1;
    //   rxState <= beat.stop ? 0 : 2;
    // end else begin
    //   // Ignore data
    //   fromMACPort.get;
    //   rxState <= beat.stop ? 0 : 2;
    // end
  endrule

  // Fill transmit buffer
  rule fillTransmitBuffer (inPort.canGet && transmitBuffer.canEnq);
    // $display("[fillTransmitBuffer]");
    inPort.get;
    transmitBuffer.enq(inPort.value);
  endrule

  // 10G MAC interfaces
  `ifndef SIMULATE
  interface avalonMac = mac.avalonMac;
  `endif

  interface In streamIn = inPort.in;

  interface BOut streamOut;
    method Action get;
      receiveBuffer.deq;
      receiveCount.dec;
    endmethod
    method Bool valid = receiveBuffer.canDeq && receiveBuffer.canPeek;
    method Bit#(64) value = receiveBuffer.dataOut;
  endinterface

  // Performance counter
  method Bit#(32) numTimeouts = transmitBuffer.numTimeouts;

endmodule

`ifndef SIMULATE

// FPGA version
module mkReliableLink (ReliableLink);
  Mac mac <- mkMac;
  ReliableLink link <- mkReliableLinkCore(mac);
  return link;
endmodule

`else

// Simulation version
module mkReliableLink#(SocketId id) (ReliableLink);
  Mac mac <- mkMac(id);
  ReliableLink link <- mkReliableLinkCore(mac);
  return link;
endmodule

// Simulation loopback verison
module mkReliableLinkLoopback (ReliableLink);
  Mac mac <- mkMacLoopback;
  ReliableLink link <- mkReliableLinkCore(mac);
  return link;
endmodule

`endif

endpackage
