// Copyright (c) Matthew Naylor

package MailboxNetwork;

// This package supports creation of a bidirectional bus of mailboxes.

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
// Bus Router
// =============================================================================

// A bus router is a component that connects to each mailbox and
// routes incoming flits into the mailbox or along the bus,
// depending on the flit address.  Similarly, it takes outgoing
// flits from the mailbox and routes them back into the mailbox or
// onto the bus, depending on the flit address.
//
//                           +------------+
//    Left flit in      ---->|            |----> Right flit out
//                           |    Bus     |
//    Left flit out     <----|   Router   |<---- Right flit in
//                           |            |
//    Flit from mailbox ---->|            |----> Flit to mailbox
//                           +------------+
//
// Care is taken to ensure that messages are atomic, i.e.
// the flits of a message are not interleaved with other flits.

interface BusRouter;
  interface In#(Flit)  leftIn;
  interface Out#(Flit) leftOut;
  interface In#(Flit)  rightIn;
  interface Out#(Flit) rightOut;
  interface In#(Flit)  fromMailbox;
  interface Out#(Flit) toMailbox;
endinterface

// Router id
typedef Bit#(`LogMailboxesPerBoard) RouterId;

// Extract destination router id from flit
function RouterId destRouter(Flit flit);
  Bit#(TSub#(`LogMaxThreads, `LogThreadsPerMailbox)) topBits =
    truncateLSB(flit.dest);
  return truncate(topBits);
endfunction

// Routes are locked to prevent interleaving of flits from different messages
typedef enum {
  Unlocked,     // Route is unlocked
  FromMailbox,  // Route source must be from mailbox
  FromLeft,     // Route source must be from left
  FromRight     // Route source must be from right
} RouteLock deriving (Bits, Eq);

module mkBusRouter#(RouterId myId) (BusRouter);

  // Ports
  InPort#(Flit)  leftInPort      <- mkInPort;
  OutPort#(Flit) leftOutPort     <- mkOutPort;
  InPort#(Flit)  rightInPort     <- mkInPort;
  OutPort#(Flit) rightOutPort    <- mkOutPort;
  InPort#(Flit)  fromMailboxPort <- mkInPort;
  OutPort#(Flit) toMailboxPort   <- mkOutPort;

  // Lock on the to-mailbox route
  Reg#(RouteLock) toMailboxLock <- mkConfigReg(Unlocked);

  // Lock on the to-right route
  Reg#(RouteLock) toRightLock <- mkConfigReg(Unlocked);

  // Lock on the to-left route
  Reg#(RouteLock) toLeftLock <- mkConfigReg(Unlocked);

  // Is the flit from the left for me?
  Bool leftInForMe = destRouter(leftInPort.value) == myId;

  // Is the flit from the right for me?
  Bool rightInForMe = destRouter(rightInPort.value) == myId;

  // Is the flit from the mailbox for me?
  Bool mailboxInForMe = destRouter(fromMailboxPort.value) == myId;

  // Is the flit from the mailbox for the right?
  Bool mailboxInForRight = destRouter(fromMailboxPort.value) > myId;

  // Is the flit from the mailbox for the left?
  Bool mailboxInForLeft = destRouter(fromMailboxPort.value) < myId;

  // Route from the left to the mailbox?
  Bool routeLeftToMailbox = leftInPort.canGet && leftInForMe &&
                              (toMailboxLock == Unlocked ||
                                 toMailboxLock == FromLeft);

  // Route from the right to the mailbox?
  Bool routeRightToMailbox = !routeLeftToMailbox &&
                               rightInPort.canGet && rightInForMe &&
                                 (toMailboxLock == Unlocked ||
                                    toMailboxLock == FromRight);

  // Route from the left to the right?
  Bool routeLeftToRight = leftInPort.canGet && !leftInForMe &&
                            (toRightLock == Unlocked ||
                               toRightLock == FromLeft);

  // Route from the right to the left?
  Bool routeRightToLeft = rightInPort.canGet && !rightInForMe &&
                            (toLeftLock == Unlocked ||
                               toLeftLock == FromRight);

  // Route from left to mailbox
  rule leftToMailbox (toMailboxPort.canPut && routeLeftToMailbox);
    leftInPort.get;
    toMailboxPort.put(leftInPort.value);
    toMailboxLock <= leftInPort.value.notFinalFlit ? FromLeft : Unlocked;
  endrule

  // Route from right to mailbox
  rule rightToMailbox (toMailboxPort.canPut && routeRightToMailbox);
    rightInPort.get;
    toMailboxPort.put(rightInPort.value);
    toMailboxLock <= rightInPort.value.notFinalFlit ? FromRight : Unlocked;
  endrule

  // Route from mailbox to mailbox
  rule mailboxToMailbox (toMailboxPort.canPut &&
                           !routeLeftToMailbox && !routeRightToMailbox &&
                             fromMailboxPort.canGet && mailboxInForMe &&
                               (toMailboxLock == Unlocked ||
                                  toMailboxLock == FromMailbox));
    fromMailboxPort.get;
    toMailboxPort.put(fromMailboxPort.value);
    toMailboxLock <= fromMailboxPort.value.notFinalFlit ?
                       FromMailbox : Unlocked;
  endrule

  // Route from left to right
  rule leftToRight (rightOutPort.canPut && routeLeftToRight);
    leftInPort.get;
    rightOutPort.put(leftInPort.value);
    toRightLock <= leftInPort.value.notFinalFlit ? FromLeft : Unlocked;
  endrule

  // Route from right to left
  rule rightToLeft (leftOutPort.canPut && routeRightToLeft);
    rightInPort.get;
    leftOutPort.put(rightInPort.value);
    toLeftLock <= rightInPort.value.notFinalFlit ? FromRight : Unlocked;
  endrule

  // Route from mailbox to right
  rule mailboxToRight (rightOutPort.canPut && !routeLeftToRight &&
                        fromMailboxPort.canGet && mailboxInForRight &&
                          (toRightLock == Unlocked ||
                             toRightLock == FromMailbox));
    fromMailboxPort.get;
    rightOutPort.put(fromMailboxPort.value);
    toRightLock <= fromMailboxPort.value.notFinalFlit ? FromMailbox : Unlocked;
  endrule

  // Route from mailbox to left
  rule mailboxToLeft (leftOutPort.canPut && !routeRightToLeft &&
                        fromMailboxPort.canGet && mailboxInForLeft &&
                          (toLeftLock == Unlocked ||
                             toLeftLock == FromMailbox));
    fromMailboxPort.get;
    leftOutPort.put(fromMailboxPort.value);
    toLeftLock <= fromMailboxPort.value.notFinalFlit ? FromMailbox : Unlocked;
  endrule

  // Interface
  interface In  leftIn      = leftInPort.in;
  interface Out leftOut     = leftOutPort.out;
  interface In  rightIn     = rightInPort.in;
  interface Out rightOut    = rightOutPort.out;
  interface In  fromMailbox = fromMailboxPort.in;
  interface Out toMailbox   = toMailboxPort.out;

endmodule

// =============================================================================
// Bidirectional bus of mailboxes
// =============================================================================

module mkBus#(Vector#(n, MailboxNet) mailboxes) ();

  // Create routers
  Vector#(n, BusRouter) routers;
  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    routers[i] <- mkBusRouter(fromInteger(i));
  end

  // Create bus terminators
  BOut#(Flit) leftNullOut  <- mkNullBOut;
  BOut#(Flit) rightNullOut <- mkNullBOut;

  // Connect each router to a mailbox
  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    connectDirect(mailboxes[i].flitOut, routers[i].fromMailbox);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i].toMailbox, mailboxes[i].flitIn);
  end

  // Connect routers 
  for (Integer i = 0; i < valueOf(n)-1; i=i+1) begin
    // Left to right direction
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i].rightOut, routers[i+1].leftIn);
    // Right to left direction
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i+1].leftOut, routers[i].rightIn);
  end

  // Terminate bus
  connectDirect(leftNullOut, routers[0].leftIn);
  connectDirect(rightNullOut, routers[vakueOf(n)-1].rightIn);

endmodule

endpackage
