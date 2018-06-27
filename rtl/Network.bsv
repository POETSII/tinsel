// Copyright (c) Matthew Naylor

package Network;

// This package supports creation of mesh of MailboxNet interfaces.
// Recall that a MailboxNet consists simply of a flit-sized input and
// output port (see Mailbox.bsv).

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
import Socket       :: *;
import Util         :: *;

// =============================================================================
// Mesh Router
// =============================================================================

// A mesh router is a component that connects to each mailbox and
// routes incoming flits into the mailbox or along the mesh,
// depending on the flit address.  Similarly, it takes outgoing
// flits from the mailbox and routes them back into the mailbox
// (loopback) or onto the mesh, depending on the flit address.
//
//    Top flit in       ----------+  +---------> Top flit out
//                                |  |
//                                v  |
//                           +------------+
//    Left flit in      ---->|            |----> Right flit out
//                           |    Mesh    |
//    Left flit out     <----|   Router   |<---- Right flit in
//                           |            |
//    Flit from mailbox ---->|            |----> Flit to mailbox
//                           +------------+
//                                ^  |
//                                |  |
//    Bottom flit in    ----------+  +---------> Bottom flit out
//                                            
//
// Care is taken to ensure that messages are atomic, i.e.
// the flits of a message are not interleaved with other flits.

// Mesh router interface
interface MeshRouter;
  interface In#(Flit)  leftIn;
  interface Out#(Flit) leftOut;
  interface In#(Flit)  rightIn;
  interface Out#(Flit) rightOut;
  interface In#(Flit)  topIn;
  interface Out#(Flit) topOut;
  interface In#(Flit)  bottomIn;
  interface Out#(Flit) bottomOut;
  interface In#(Flit)  fromMailbox;
  interface Out#(Flit) toMailbox;
  (* always_ready, always_enabled *)
  method Action setBoardId(BoardId id);
endinterface

// In which direction should a message be routed?
typedef enum { Left, Right, Up, Down, Mailbox } Route deriving (Bits, Eq);

// Routes may be locked to prevent interleaving of flits of different messages
typedef enum {
  Unlocked,     // Route is unlocked
  FromLeft,     // Route source must be from left
  FromRight,    // Route source must be from right
  FromTop,      // Route source must be from top
  FromBottom,   // Route source must be from bottom
  FromMailbox   // Route source must be from mailbox
} RouteLock deriving (Bits, Eq);

// The routing function has the following type
typedef function Route route(NetAddr addr) RouteFunc;

// Helper module: route from one of several input ports to a destination port
module mkRouterMux#(
  RouteFunc route,
  Route dest,
  OutPort#(Flit) destPort,
  Vector#(n, RouteLock) fromLock,
  Vector#(n, InPort#(Flit)) inPort) ();

  // Number of input ports
  Integer numPorts = valueOf(n);

  // Lock on the to-destination route
  // (To support multiple flits per message)
  Reg#(RouteLock) toDestLock <- mkConfigReg(Unlocked);

  // Track whether or not the destination port is busy
  Bool busy = False;

  // Compute the guard for routing from each input port
  Vector#(n, Bool) routeToDest;
  for (Integer i = 0; i < numPorts; i=i+1) begin
    routeToDest[i] =
         !busy
      && inPort[i].canGet
      && route(inPort[i].value.dest) == dest
      && (toDestLock == Unlocked || toDestLock == fromLock[i]);
    // When we find a sutable input port, destination becomes busy
    busy = busy || routeToDest[i];
  end

  // Generate routing rules
  for (Integer i = 0; i < numPorts; i=i+1) begin
    // Route from input port to destination
    rule toDest (destPort.canPut && routeToDest[i]);
      inPort[i].get;
      destPort.put(inPort[i].value);
      toDestLock <= inPort[i].value.notFinalFlit ? fromLock[i] : Unlocked;
    endrule
  end
endmodule

// Mesh router
(* synthesize *)
module mkMeshRouter#(MailboxId m) (MeshRouter);

  // Board id
  Wire#(BoardId) b <- mkDWire(?);

  // Ports
  InPort#(Flit)  leftInPort      <- mkInPort;
  OutPort#(Flit) leftOutPort     <- mkOutPort;
  InPort#(Flit)  rightInPort     <- mkInPort;
  OutPort#(Flit) rightOutPort    <- mkOutPort;
  InPort#(Flit)  topInPort       <- mkInPort;
  OutPort#(Flit) topOutPort      <- mkOutPort;
  InPort#(Flit)  bottomInPort    <- mkInPort;
  OutPort#(Flit) bottomOutPort   <- mkOutPort;
  InPort#(Flit)  fromMailboxPort <- mkInPort;
  OutPort#(Flit) toMailboxPort   <- mkOutPort;

  // Routing function
  function Route route(NetAddr addr);
    if      (addr.board.x < b.x)         return Left;
    else if (addr.board.x > b.x)         return Right;
    else if (getMailboxId(addr).x < m.x) return Left;
    else if (getMailboxId(addr).x > m.x) return Right;
    else if (addr.board.y < b.y)         return Down;
    else if (addr.board.y > b.y)         return Up;
    else if (getMailboxId(addr).y < m.y) return Down;
    else if (getMailboxId(addr).y > m.y) return Up;
    else return Mailbox;
  endfunction

  // Route to the mailbox
  mkRouterMux(
    route,
    Mailbox,
    toMailboxPort,
    vector(FromLeft, FromRight, FromTop, FromBottom, FromMailbox),
    vector(leftInPort, rightInPort, topInPort, bottomInPort, fromMailboxPort)
  );

  // Route left
  mkRouterMux(
    route,
    Left,
    leftOutPort,
    vector(FromRight,   FromTop,   FromBottom,   FromMailbox),
    vector(rightInPort, topInPort, bottomInPort, fromMailboxPort)
  );

  // Route right
  mkRouterMux(
    route,
    Right,
    rightOutPort,
    vector(FromLeft,   FromTop,   FromBottom,   FromMailbox),
    vector(leftInPort, topInPort, bottomInPort, fromMailboxPort)
  );

  // Route up
  mkRouterMux(
    route,
    Up,
    topOutPort,
    vector(FromLeft,   FromRight,   FromBottom,   FromMailbox),
    vector(leftInPort, rightInPort, bottomInPort, fromMailboxPort)
  );

  // Route down
  mkRouterMux(
    route,
    Down,
    bottomOutPort,
    vector(FromLeft,   FromRight,   FromTop,   FromMailbox),
    vector(leftInPort, rightInPort, topInPort, fromMailboxPort)
  );

  method Action setBoardId(BoardId id);
    b <= id;
  endmethod

  // Interface
  interface In  leftIn      = leftInPort.in;
  interface Out leftOut     = leftOutPort.out;
  interface In  rightIn     = rightInPort.in;
  interface Out rightOut    = rightOutPort.out;
  interface In  topIn       = topInPort.in;
  interface Out topOut      = topOutPort.out;
  interface In  bottomIn    = bottomInPort.in;
  interface Out bottomOut   = bottomOutPort.out;
  interface In  fromMailbox = fromMailboxPort.in;
  interface Out toMailbox   = toMailboxPort.out;

endmodule

// =============================================================================
// Flit-sized reliable links
// =============================================================================

interface BoardLink;
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

module mkBoardLink#(SocketId id) (BoardLink);
  
  // 64-bit link
  `ifdef SIMULATE
  ReliableLink link <- mkReliableLink(id);
  `else
  ReliableLink link <- mkReliableLink;
  `endif

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
// Mailbox Mesh
// =============================================================================

// Interface to external (off-board) network
interface ExtNetwork;
`ifndef SIMULATE
  // Avalon interfaces to 10G MACs
  interface Vector#(`NumNorthSouthLinks, AvalonMac) north;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) south;
  interface Vector#(`NumEastWestLinks, AvalonMac) east;
  interface Vector#(`NumEastWestLinks, AvalonMac) west;
`endif
endinterface

module mkMailboxMesh#(
         BoardId boardId,
         Vector#(`MailboxMeshYLen,
           Vector#(`MailboxMeshXLen, MailboxNet)) mailboxes)
       (ExtNetwork);

  // Create off-board links
  Vector#(`NumNorthSouthLinks, BoardLink) northLink <-
    mapM(mkBoardLink, northSocket);
  Vector#(`NumNorthSouthLinks, BoardLink) southLink <-
    mapM(mkBoardLink, southSocket);
  Vector#(`NumEastWestLinks, BoardLink) eastLink <-
    mapM(mkBoardLink, eastSocket);
  Vector#(`NumEastWestLinks, BoardLink) westLink <-
    mapM(mkBoardLink, westSocket);

  // Create mailbox routers
  Vector#(`MailboxMeshYLen,
    Vector#(`MailboxMeshXLen, MeshRouter)) routers =
      Vector::replicate(newVector());

  for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen; x=x+1) begin
      MailboxId mailboxId =
        MailboxId { x: fromInteger(x), y: fromInteger(y) };
      routers[y][x] <- mkMeshRouter(mailboxId);
      rule setBoardId;
        routers[y][x].setBoardId(boardId);
      endrule
    end

  // Connect mailboxes
  for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen; x=x+1) begin
      connectDirect(mailboxes[y][x].flitOut, routers[y][x].fromMailbox);
      connectUsing(mkUGShiftQueue1(QueueOptFmax),
                     routers[y][x].toMailbox, mailboxes[y][x].flitIn);
    end

  // Connect routers horizontally
  for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen-1; x=x+1) begin
      // Left to right direction
      connectUsing(mkUGShiftQueue1(QueueOptFmax),
                     routers[y][x].rightOut, routers[y][x+1].leftIn);
      // Right to left direction
      connectUsing(mkUGShiftQueue1(QueueOptFmax),
                     routers[y][x+1].leftOut, routers[y][x].rightIn);
  end

  // Connect routers vertically
  for (Integer y = 0; y < `MailboxMeshYLen-1; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen; x=x+1) begin
      // Top to bottom direction
      connectUsing(mkUGShiftQueue1(QueueOptFmax),
                     routers[y][x].topOut, routers[y+1][x].bottomIn);
      // Bottom to top direction
      connectUsing(mkUGShiftQueue1(QueueOptFmax),
                     routers[y+1][x].bottomOut, routers[y][x].topIn);
  end

  // Connect north links
  // -------------------

  // Extract mesh top inputs and outputs
  List#(In#(Flit)) topInList = Nil;
  List#(Out#(Flit)) topOutList = Nil;
  for (Integer x = `MailboxMeshXLen-1; x >= 0; x=x-1) begin
    topOutList = Cons(routers[`MailboxMeshYLen-1][x].topOut, topOutList);
    topInList = Cons(routers[`MailboxMeshYLen-1][x].topIn, topInList);
  end

  // Connect the outgoing links
  function In#(Flit) getFlitIn(BoardLink link) = link.flitIn;
  reduceConnect(topOutList, List::map(getFlitIn, toList(northLink)));
  
  // Connect the incoming links
  function Out#(Flit) getFlitOut(BoardLink link) = link.flitOut;
  expandConnect(List::map(getFlitOut, toList(northLink)), topInList);

  // Connect south links
  // -------------------

  // Extract mesh bottom inputs and outputs
  List#(In#(Flit)) botInList = Nil;
  List#(Out#(Flit)) botOutList = Nil;
  for (Integer x = `MailboxMeshXLen-1; x >= 0; x=x-1) begin
    botOutList = Cons(routers[0][x].bottomOut, botOutList);
    botInList = Cons(routers[0][x].bottomIn, botInList);
  end

  // Connect the outgoing links
  reduceConnect(botOutList, List::map(getFlitIn, toList(southLink)));
  
  // Connect the incoming links
  expandConnect(List::map(getFlitOut, toList(southLink)), botInList);

  // Connect east links
  // ------------------

  // Extract mesh right inputs and outputs
  List#(In#(Flit)) rightInList = Nil;
  List#(Out#(Flit)) rightOutList = Nil;
  for (Integer y = `MailboxMeshYLen-1; y >= 0; y=y-1) begin
    rightOutList = Cons(routers[y][`MailboxMeshXLen-1].rightOut, rightOutList);
    rightInList = Cons(routers[y][`MailboxMeshXLen-1].rightIn, rightInList);
  end

  // Connect the outgoing links
  reduceConnect(rightOutList, List::map(getFlitIn, toList(eastLink)));
  
  // Connect the incoming links
  expandConnect(List::map(getFlitOut, toList(eastLink)), rightInList);

  // Connect west links
  // ------------------

   // Extract mesh right inputs and outputs
  List#(In#(Flit)) leftInList = Nil;
  List#(Out#(Flit)) leftOutList = Nil;
  for (Integer y = `MailboxMeshYLen-1; y >= 0; y=y-1) begin
    leftOutList = Cons(routers[y][0].leftOut, leftOutList);
    leftInList = Cons(routers[y][0].leftIn, leftInList);
  end

  // Connect the outgoing links
  reduceConnect(leftOutList, List::map(getFlitIn, toList(westLink)));
  
  // Connect the incoming links
  expandConnect(List::map(getFlitOut, toList(westLink)), leftInList);

`ifndef SIMULATE
  function getMac(link) = link.avalonMac;
  interface Vector#(`NumNorthSouthLinks, AvalonMac) north =
    Vector::map(getMac, northLink);
  interface Vector#(`NumNorthSouthLinks, AvalonMac) south =
    Vector::map(getMac, southLink);
  interface Vector#(`NumEastWestLinks, AvalonMac) east =
    Vector::map(getMac, eastLink);
  interface Vector#(`NumEastWestLinks, AvalonMac) west =
    Vector::map(getMac, westLink);
`endif

endmodule

endpackage
