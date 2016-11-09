// Copyright (c) Matthew Naylor

package Ring;

// This package provides an arbitrary sized ring of mailboxes.

// =============================================================================
// Imports
// =============================================================================

import Globals   :: *;
import Mailbox   :: *;
import Interface :: *;
import Vector    :: *;
import Queue     :: *;
import ConfigReg :: *;

// =============================================================================
// Ring Router
// =============================================================================

// A ring router is a component that connects to each mailbox and
// routes incoming packets into the mailbox or along the ring,
// depending on the packet address.  Similarly, it takes outgoing
// packets from the mailbox and routes them back into the mailbox or
// along the ring, depending on the packet address.
//
//                           +------------+
//    Router packet in  ---->|    Ring    |----> Router packet out
//                           |   Router   |
//  Packet from mailbox ---->|            |----> Packet to mailbox
//                           +------------+
//
// Care is taken to ensure that message bursts are atomic, i.e.
// messages within a burst are not interleaved with other messages.

interface RingRouter;
  interface In#(Packet)  ringIn;
  interface Out#(Packet) ringOut;
  interface In#(Packet)  fromMailbox;
  interface Out#(Packet) toMailbox;
endinterface

// Router id
typedef Bit#(`LogRingSize) RouterId;

// Extract destination router id from packet
function RouterId destRouter(Packet pkt);
  Bit#(TSub#(`LogMaxThreads, `LogThreadsPerMailbox)) topBits =
    truncateLSB(pkt.dest);
  return truncate(topBits);
endfunction

module mkRingRouter#(RouterId myId) (RingRouter);

  // Ports
  InPort#(Packet)  ringInPort      <- mkInPort;
  OutPort#(Packet) ringOutPort     <- mkOutPort;
  InPort#(Packet)  fromMailboxPort <- mkInPort;
  OutPort#(Packet) toMailboxPort   <- mkOutPort;

  // Is the to-mailbox port is locked by mailbox-to-mailbox route?
  // (i.e. is there a burst in progress on that route?)
  Reg#(Bool) toMailboxLock <- mkConfigReg(False);

  // Is the to-ring port is locked by the mailbox-to-ring route?
  // (i.e. is there a burst in progress on that route?)
  Reg#(Bool) toRingLock <- mkConfigReg(False);

  // Is the packet from the ring for me?
  Bool ringInForMe = destRouter(ringInPort.value) == myId;

  // Is the packet from the mailbox for me?
  Bool mailboxInForMe = destRouter(fromMailboxPort.value) == myId;

  // Can we route from the ring to the mailbox?
  Bool routeRingToMailbox = ringInPort.canGet && ringInForMe && !toMailboxLock;

  // Can we route from the ring to the ring?
  Bool routeRingToRing = ringInPort.canGet && !ringInForMe && !toRingLock;

  // Route packet from ring to mailbox
  rule ringToMailbox (toMailboxPort.canPut && routeRingToMailbox);
    ringInPort.get;
    toMailboxPort.put(ringInPort.value);
  endrule

  // Route packet from mailbox to mailbox
  rule mailboxToMailbox (toMailboxPort.canPut &&
                           !routeRingToMailbox &&
                              fromMailboxPort.canGet && mailboxInForMe);
    fromMailboxPort.get;
    toMailboxPort.put(fromMailboxPort.value);
    toMailboxLock <= fromMailboxPort.value.notEndOfBurst;
  endrule

  // Route packet from ring to ring
  rule ringToRing (ringOutPort.canPut && routeRingToRing);
    ringInPort.get;
    ringOutPort.put(ringInPort.value);
  endrule

  // Route packet from mailbox to ring
  rule mailboxToRing (ringOutPort.canPut &&
                        !routeRingToRing &&
                           fromMailboxPort.canGet && !mailboxInForMe);
    fromMailboxPort.get;
    ringOutPort.put(fromMailboxPort.value);
    toRingLock <= fromMailboxPort.value.notEndOfBurst;
  endrule

  // Interface
  interface In  ringIn      = ringInPort.in;
  interface Out ringOut     = ringOutPort.out;
  interface In  fromMailbox = fromMailboxPort.in;
  interface Out toMailbox   = toMailboxPort.out;

endmodule

// =============================================================================
// Ring of Mailboxes
// =============================================================================

module mkRing#(Vector#(`RingSize, Mailbox) mailboxes) ();

  // Create routers
  Vector#(`RingSize, RingRouter) routers;
  for (Integer i = 0; i < `RingSize; i=i+1)
    routers[i] <- mkRingRouter(fromInteger(i));

  // Connect each router to a mailbox and form ring of routers
  for (Integer i = 0; i < `RingSize; i=i+1) begin
    connectDirect(mailboxes[i].packetOut, routers[i].fromMailbox);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i].toMailbox, mailboxes[i].packetIn);
    Integer next = (i+1) % `RingSize;
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i].ringOut, routers[next].ringIn);
  end

endmodule

endpackage
