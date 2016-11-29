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
// routes incoming flits into the mailbox or along the ring,
// depending on the flit address.  Similarly, it takes outgoing
// flits from the mailbox and routes them back into the mailbox or
// along the ring, depending on the flit address.
//
//                           +------------+
//    Router flit in    ---->|    Ring    |----> Router flit out
//                           |   Router   |
//    Flit from mailbox ---->|            |----> Flit to mailbox
//                           +------------+
//
// Care is taken to ensure that messages are atomic, i.e.
// the flits of a message are not interleaved with other flits.

interface RingRouter;
  interface In#(Flit)  ringIn;
  interface Out#(Flit) ringOut;
  interface In#(Flit)  fromMailbox;
  interface Out#(Flit) toMailbox;
endinterface

// Router id
// (The +1 is because the ring will contain an extra node to
// forwards flits to a from a host machine)
typedef Bit#(TAdd#(`LogMailboxesPerBoard, 1)) RouterId;

// Extract destination router id from flit
function RouterId destRouter(Flit flit);
  Bit#(TSub#(`LogMaxThreads, `LogThreadsPerMailbox)) topBits =
    truncateLSB(flit.dest);
  return truncate(topBits);
endfunction

// Routes are locked to prevent interleaving
typedef enum {
  Unlocked,     // Route is unlocked
  FromMailbox,  // Route source must mailbox
  FromRing      // Route source must be ring
} RouteLock deriving (Bits, Eq);

module mkRingRouter#(RouterId myId) (RingRouter);

  // Ports
  InPort#(Flit)  ringInPort      <- mkInPort;
  OutPort#(Flit) ringOutPort     <- mkOutPort;
  InPort#(Flit)  fromMailboxPort <- mkInPort;
  OutPort#(Flit) toMailboxPort   <- mkOutPort;

  // Lock on the to-mailbox route
  Reg#(RouteLock) toMailboxLock <- mkConfigReg(Unlocked);

  // Lock on the to-ring route
  Reg#(RouteLock) toRingLock <- mkConfigReg(Unlocked);

  // Is the flit from the ring for me?
  Bool ringInForMe = destRouter(ringInPort.value) == myId;

  // Is the flit from the mailbox for me?
  Bool mailboxInForMe = destRouter(fromMailboxPort.value) == myId;

  // Route from the ring to the mailbox?
  Bool routeRingToMailbox = ringInPort.canGet && ringInForMe &&
                              toMailboxLock != FromMailbox;

  // Route from the ring to the ring?
  Bool routeRingToRing = ringInPort.canGet && !ringInForMe &&
                           toRingLock != FromMailbox;

  // Route flit from ring to mailbox
  rule ringToMailbox (toMailboxPort.canPut && routeRingToMailbox);
    ringInPort.get;
    toMailboxPort.put(ringInPort.value);
    toMailboxLock <= ringInPort.value.notFinalFlit ? FromRing : Unlocked;
  endrule

  // Route flit from mailbox to mailbox
  rule mailboxToMailbox (toMailboxPort.canPut &&
                           !routeRingToMailbox &&
                             fromMailboxPort.canGet && mailboxInForMe &&
                               toMailboxLock != FromRing);
    fromMailboxPort.get;
    toMailboxPort.put(fromMailboxPort.value);
    toMailboxLock <= fromMailboxPort.value.notFinalFlit ?
                       FromMailbox : Unlocked;
  endrule

  // Route flit from ring to ring
  rule ringToRing (ringOutPort.canPut && routeRingToRing);
    ringInPort.get;
    ringOutPort.put(ringInPort.value);
    toRingLock <= ringInPort.value.notFinalFlit ? FromRing : Unlocked;
  endrule

  // Route flit from mailbox to ring
  rule mailboxToRing (ringOutPort.canPut && !routeRingToRing &&
                        fromMailboxPort.canGet && !mailboxInForMe &&
                          toRingLock != FromRing);
    fromMailboxPort.get;
    ringOutPort.put(fromMailboxPort.value);
    toRingLock <= fromMailboxPort.value.notFinalFlit ? FromMailbox : Unlocked;
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

module mkRing#(Vector#(n, MailboxNet) mailboxes) ();

  // Create routers
  Vector#(n, RingRouter) routers;
  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    routers[i] <- mkRingRouter(fromInteger(i));
  end

  // Connect each router to a mailbox and form ring of routers
  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    connectDirect(mailboxes[i].flitOut, routers[i].fromMailbox);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i].toMailbox, mailboxes[i].flitIn);
    Integer next = (i+1) % valueOf(n);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i].ringOut, routers[next].ringIn);
  end

endmodule

endpackage
