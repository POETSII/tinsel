// Copyright (c) Matthew Naylor

package Network;

// This package supports creation of a bidirectional bus of MailboxNet
// interfaces. Recall that a MailboxNet consistents simply of a
// flit-sized input and output port.

// =============================================================================
// Imports
// =============================================================================

import Globals      :: *;
import Mailbox      :: *;
import Interface    :: *;
import Vector       :: *;
import Queue        :: *;
import ConfigReg    :: *;
import ReliableLink :: *;
import Mac          :: *;

// =============================================================================
// Bus Router
// =============================================================================

// A bus router is a component that connects to each mailbox and
// routes incoming flits into the mailbox or along the bus,
// depending on the flit address.  Similarly, it takes outgoing
// flits from the mailbox and routes them back into the mailbox
// (loopback) or onto the bus, depending on the flit address.
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
//
// The function used to determined whether a flit should be consumed,
// sent left, or sent right is taken as a parameter.

typedef enum { Left, Right, Me } Route deriving (Bits, Eq);
typedef function Route route(NetAddr addr) RouteFunc;

interface BusRouter;
  interface In#(Flit)  leftIn;
  interface Out#(Flit) leftOut;
  interface In#(Flit)  rightIn;
  interface Out#(Flit) rightOut;
  interface In#(Flit)  fromMailbox;
  interface Out#(Flit) toMailbox;
endinterface

// Routes are locked to prevent interleaving of flits from different messages
typedef enum {
  Unlocked,     // Route is unlocked
  FromMailbox,  // Route source must be from mailbox
  FromLeft,     // Route source must be from left
  FromRight     // Route source must be from right
} RouteLock deriving (Bits, Eq);

// Support for the loopback route is optional and may be disabled.
module mkBusRouter#(Bool enableLoopback, RouteFunc route) (BusRouter);

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
  Bool leftInForMe = route(leftInPort.value.dest) == Me;

  // Is the flit from the right for me?
  Bool rightInForMe = route(rightInPort.value.dest) == Me;

  // Is the flit from the mailbox for me?
  Bool mailboxInForMe = enableLoopback &&
                          route(fromMailboxPort.value.dest) == Me;

  // Is the flit from the mailbox for the right?
  Bool mailboxInForRight = route(fromMailboxPort.value.dest) == Right;

  // Is the flit from the mailbox for the left?
  Bool mailboxInForLeft = route(fromMailboxPort.value.dest) == Left;

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
// Routing functions
// =============================================================================

// The bidriectional bus of mailbox network interfaces looks like this:
//
//     +--+ ... --+--+--+--+--+--+--+
//     |  |       |  |  |  |  |  |  |
//     |  |       |  |  B  E  W  N  S
//     |  |       |  |
//    \_______________/
//        Mailboxes
//
// Each "+" denotes a BusRouter component.  The "E", "W", "N", and "S"
// represent the off-board links in the east, west, north and south
// directions respectively. And "B" represents the board manager
// component.  The mailboxes are those connected to the tinsel cores.

// Mailbox id
typedef Bit#(`LogMailboxesPerBoard) MailboxId;

// Routing rule for mailbox m on board b
function Route routeInternal(BoardId b, MailboxId m, NetAddr addr);
  if (addr.board == b && addr.space == 0) begin
    if (truncateLSB(addr.core) == m)
      return Me;
    else if (truncateLSB(addr.core) < m)
      return Left;
    else
      return Right;
  end else
    return Right;
endfunction

// Routing rule for the off-board east link
// If flit is for this board, route left
// If it's for the positive X direction, consume it
// Otherwise, route right
function Route routeEast(BoardId b, NetAddr addr) =
  addr.board == b ? Left :
    addr.board.x > b.x ? Me : Right;

// Routing rule for the off-board west link
// If flit is for this board, route left
// If it's for the positive X direction, route left
// If it's for the negative X direction, consume it
// Otherwise, route right
function Route routeWest(BoardId b, NetAddr addr) =
  (addr.board == b || addr.board.x > b.x) ? Left :
    addr.board.x < b.x ? Me : Right;

// Routing rule for the off-board north link
// If it's for the positive Y direction, consume it
// If it's for the negative Y direction, route right
// Otherwise, route left
function Route routeNorth(BoardId b, NetAddr addr) =
  addr.board.y > b.y ? Me :
    addr.board.y < b.y ? Right : Left;

// Routing rule for the off-board south link
// If it's for the negative Y direction, consume it
// Otherwise, route left
function Route routeSouth(BoardId b, NetAddr addr) =
  addr.board.y < b.y ? Me : Left;

// Routing rule for board manager component
// If it's not for this board, route right
// Otherwise, if it's a management packet, consume it
// Otherwise, route left
function Route routeMan(BoardId b, NetAddr addr) =
  addr.board != b ? Right :
    addr.space == 1 ? Me : Left;

// =============================================================================
// Flit-sized reliable links
// =============================================================================

interface ReliableFlitLink;
`ifndef SIMULATE
  // Avalon interface to 10G MAC
  interface AvalonMac avalonMac;
`endif
  // Internal interface
  interface In#(Flit) flitIn;
  interface Out#(Flit) flitOut;
  // Performance monitor
  method Bit#(32) numTimeouts;
endinterface

module mkReliableFlitLink (ReliableFlitLink);
  // 64-bit link
  ReliableLink link <- mkReliableLink;

  // Serialiser
  Serialiser#(PaddedFlit, Bit#(64)) ser <- mkSerialiser;

  // Deserialiser
  Deserialiser#(Bit#(64), PaddedFlit) des <- mkDeserialiser;

  // Connections
  connectUsing(mkUGQueue, ser.serialOut, link.streamIn);
  connectDirect(link.streamOut, des.serialIn);

  let unpaddedFlitIn  <- onIn(padFlit, ser.parallelIn);
  let unpaddedFlitOut <- onOut(unpadFlit, des.parallelOut);

`ifndef SIMULATE
  interface AvalonMac avalonMac = link.avalonMac;
`endif

  interface In flitIn = unpaddedFlitIn;
  interface Out flitOut = unpaddedFlitOut;
  method Bit#(32) numTimeouts = link.numTimeouts;
endmodule

// =============================================================================
// Bidirectional bus of mailboxes
// =============================================================================

// Interface to external (off-board) network
interface ExtNetwork;
  // Avalon interfaces to 10G MACs
  interface AvalonMac north;
  interface AvalonMac south;
  interface AvalonMac east;
  interface AvalonMac west;
endinterface

module mkBus#(BoardId boardId, Vector#(n, MailboxNet) mailboxes) (ExtNetwork)
  provisos (Add#(n, 4, m));

  // Create off-board links
  ReliableFlitLink northLink <- mkReliableFlitLink;
  ReliableFlitLink southLink <- mkReliableFlitLink;
  ReliableFlitLink eastLink  <- mkReliableFlitLink;
  ReliableFlitLink westLink  <- mkReliableFlitLink;

  // Create mailbox routers
  Vector#(m, BusRouter) routers;
  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    Bool enableLoopback = True;
    routers[i] <- mkBusRouter(enableLoopback,
                    routeInternal(boardId, fromInteger(i)));
  end

  // Create E, W, N, and S routers
  Integer linkBase = valueOf(n);
  routers[linkBase]   <- mkBusRouter(False, routeEast(boardId));
  routers[linkBase+1] <- mkBusRouter(False, routeWest(boardId));
  routers[linkBase+2] <- mkBusRouter(False, routeNorth(boardId));
  routers[linkBase+3] <- mkBusRouter(False, routeSouth(boardId));

  // Create bus terminators
  BOut#(Flit) leftNullOut  <- mkNullBOut;
  BOut#(Flit) rightNullOut <- mkNullBOut;

  // Connect mailboxes
  for (Integer i = 0; i < valueOf(n); i=i+1) begin
    connectDirect(mailboxes[i].flitOut, routers[i].fromMailbox);
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i].toMailbox, mailboxes[i].flitIn);
  end

  // Connect east link
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 routers[linkBase].toMailbox, eastLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 eastLink.flitOut, routers[linkBase].fromMailbox);

  // Connect west link
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 routers[linkBase+1].toMailbox, westLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 westLink.flitOut, routers[linkBase+1].fromMailbox);

  // Connect north link
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 routers[linkBase+2].toMailbox, northLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 northLink.flitOut, routers[linkBase+2].fromMailbox);

  // Connect south link
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 routers[linkBase+3].toMailbox, southLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 southLink.flitOut, routers[linkBase+3].fromMailbox);

  // Connect routers 
  for (Integer i = 0; i < valueOf(m)-1; i=i+1) begin
    // Left to right direction
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i].rightOut, routers[i+1].leftIn);
    // Right to left direction
    connectUsing(mkUGShiftQueue1(QueueOptFmax),
                   routers[i+1].leftOut, routers[i].rightIn);
  end

  // Terminate bus
  connectDirect(leftNullOut, routers[0].leftIn);
  connectDirect(rightNullOut, routers[valueOf(m)-1].rightIn);

  interface AvalonMac north = northLink.avalonMac;
  interface AvalonMac south = southLink.avalonMac;
  interface AvalonMac east  = eastLink.avalonMac;
  interface AvalonMac west  = westLink.avalonMac;

endmodule

endpackage
