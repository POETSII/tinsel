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
import IdleDetector :: *;

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
    else if (addr.board.y < b.y)         return Down;
    else if (addr.board.y > b.y)         return Up;
    else if (getMailboxId(addr).x < m.x) return Left;
    else if (getMailboxId(addr).x > m.x) return Right;
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
// Inter-board router (NoC bypass)
// =============================================================================
//
// A BoardRotuer is a wrapper around a board link that allows NoC
// bypass, i.e. the forwarding of a flit directly to another board
// link without having to pass through the NoC.

interface BoardRouter;
`ifndef SIMULATE
  // Avalon interface to 10G MAC
  interface AvalonMac avalonMac;
`endif
  // Flits to be sent to another board
  interface In#(Flit) flitIn;
  // Flits received from another board
  interface Out#(Flit) flitOut;  // To NoC
  interface Out#(Flit) leftOut;  // To another board link
  interface Out#(Flit) rightOut; // To another board link
  interface Out#(Flit) upOut;    // To another board link
  // Board id
  (* always_ready, always_enabled *)
  method Action setBoardId(BoardId id);
  // Performance monitor
  method Bit#(32) numTimeouts;
endinterface

module mkNorthSouthBoardRouter#(SocketId sockId) (BoardRouter);

  // Board id
  Wire#(BoardId) b <- mkDWire(?);

  // Create inter-board link
  BoardLink link <- mkBoardLink(sockId);

  // Ports
  InPort#(Flit)  fromLink     <- mkInPort;
  OutPort#(Flit) flitOutPort  <- mkOutPort;
  OutPort#(Flit) leftOutPort  <- mkOutPort;
  OutPort#(Flit) rightOutPort <- mkOutPort;
  OutPort#(Flit) upOutPort    <- mkOutPort;

  connectUsing(mkUGShiftQueue1(QueueOptFmax), link.flitOut, fromLink.in);

  rule route (fromLink.canGet);
    Flit flitIn = fromLink.value;
    if (flitIn.dest.board.x < b.x) begin
      if (leftOutPort.canPut) begin
        fromLink.get;
        leftOutPort.put(flitIn);
      end
    end else if (flitIn.dest.board.x > b.x) begin
      if (rightOutPort.canPut) begin
        fromLink.get;
        rightOutPort.put(flitIn);
      end
    end else if (flitIn.dest.board.y != b.y) begin
      if (upOutPort.canPut) begin
        fromLink.get;
        upOutPort.put(flitIn);
      end
    end else if (flitOutPort.canPut) begin
      fromLink.get;
      flitOutPort.put(flitIn);
    end
  endrule

  interface In  flitIn   = link.flitIn;
  interface Out flitOut  = flitOutPort.out;
  interface Out leftOut  = leftOutPort.out;
  interface Out rightOut = rightOutPort.out;
  interface Out upOut    = upOutPort.out;

  method Action setBoardId(BoardId id);
    b <= id;
  endmethod

  // Performance monitor
  method Bit#(32) numTimeouts = link.numTimeouts;
endmodule

module mkEastWestBoardRouter#(SocketId sockId) (BoardRouter);

  // Board id
  Wire#(BoardId) b <- mkDWire(?);

  // Create inter-board link
  BoardLink link <- mkBoardLink(sockId);

  // Ports
  InPort#(Flit)  fromLink     <- mkInPort;
  OutPort#(Flit) flitOutPort  <- mkOutPort;
  OutPort#(Flit) leftOutPort  <- mkOutPort;
  OutPort#(Flit) rightOutPort <- mkOutPort;
  OutPort#(Flit) upOutPort    <- mkOutPort;

  connectUsing(mkUGShiftQueue1(QueueOptFmax), link.flitOut, fromLink.in);

  rule route (fromLink.canGet);
    Flit flitIn = fromLink.value;
    if (flitIn.dest.board.x != b.x) begin
      if (upOutPort.canPut) begin
        fromLink.get;
        upOutPort.put(flitIn);
      end
    end else if (flitIn.dest.board.y < b.y) begin
      if (rightOutPort.canPut) begin
        fromLink.get;
        rightOutPort.put(flitIn);
      end
    end else if (flitIn.dest.board.y > b.y) begin
      if (leftOutPort.canPut) begin
        fromLink.get;
        leftOutPort.put(flitIn);
      end
    end else if (flitOutPort.canPut) begin
      fromLink.get;
      flitOutPort.put(flitIn);
    end
  endrule

  interface In  flitIn   = link.flitIn;
  interface Out flitOut  = flitOutPort.out;
  interface Out leftOut  = leftOutPort.out;
  interface Out rightOut = rightOutPort.out;
  interface Out upOut    = upOutPort.out;

  method Action setBoardId(BoardId id);
    b <= id;
  endmethod

  // Performance monitor
  method Bit#(32) numTimeouts = link.numTimeouts;
endmodule

// =============================================================================
// Mailbox Mesh
// =============================================================================

// Interface to external (off-board) network
interface ExtNetwork;
`ifndef SIMULATE
  // Avalon interfaces to 10G MACs
  interface Vector#(1, AvalonMac) north;
  interface Vector#(1, AvalonMac) south;
  interface Vector#(1, AvalonMac) east;
  interface Vector#(1, AvalonMac) west;
`endif
endinterface

module mkMailboxMesh#(
         BoardId boardId,
         Vector#(`MailboxMeshYLen,
           Vector#(`MailboxMeshXLen, MailboxNet)) mailboxes,
         IdleDetector idle)
       (ExtNetwork);

  // Create off-board links
  Vector#(1, BoardRouter) northLink <-
    mapM(mkNorthSouthBoardRouter, northSocket);
  Vector#(1, BoardRouter) southLink <-
    mapM(mkNorthSouthBoardRouter, southSocket);
  Vector#(1, BoardRouter) eastLink <-
    mapM(mkEastWestBoardRouter, eastSocket);
  Vector#(1, BoardRouter) westLink <-
    mapM(mkEastWestBoardRouter, westSocket);

  rule setBoardLinkId;
    southLink[0].setBoardId(boardId);
    northLink[0].setBoardId(boardId);
    eastLink[0].setBoardId(boardId);
    westLink[0].setBoardId(boardId);
  endrule

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
      // Mailbox (0,0) is special (connects to idle detector)
      if (x == 0 && y == 0) begin
        // Connect mailbox to router via idle-detector
        connectDirect(mailboxes[y][x].flitOut, idle.mboxFlitIn);
        connectUsing(mkUGShiftQueue1(QueueOptFmax),
                       idle.netFlitOut, routers[y][x].fromMailbox);
 
        // Connect router to mailbox via idle-detector
        connectUsing(mkUGShiftQueue1(QueueOptFmax),
                       routers[y][x].toMailbox, idle.netFlitIn);
        connectUsing(mkUGShiftQueue1(QueueOptFmax),
                       idle.mboxFlitOut, mailboxes[y][x].flitIn);
      end else begin
        connectDirect(mailboxes[y][x].flitOut, routers[y][x].fromMailbox);
        connectUsing(mkUGShiftQueue1(QueueOptFmax),
                       routers[y][x].toMailbox, mailboxes[y][x].flitIn);
      end
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

  // Add outputs from other board routers
  topOutList = Cons(southLink[0].upOut,
                 Cons(eastLink[0].leftOut,
                   Cons(westLink[0].leftOut, topOutList)));

  // Connect the outgoing links
  function In#(Flit) getFlitIn(BoardRouter link) = link.flitIn;
  reduceConnect(mkFlitMerger,
    topOutList, List::map(getFlitIn, toList(northLink)));

  // Connect the incoming links
  function Out#(Flit) getFlitOut(BoardRouter link) = link.flitOut;
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

  // Add outputs from other board routers
  botOutList = Cons(northLink[0].upOut,
                 Cons(eastLink[0].rightOut,
                   Cons(westLink[0].rightOut, botOutList)));

  // Connect the outgoing links
  reduceConnect(mkFlitMerger, botOutList,
    List::map(getFlitIn, toList(southLink)));
  
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

  // Add outputs from other board routers
  rightOutList = Cons(westLink[0].upOut,
                   Cons(northLink[0].rightOut,
                     Cons(southLink[0].rightOut, rightOutList)));

  // Connect the outgoing links
  reduceConnect(mkFlitMerger,
    rightOutList, List::map(getFlitIn, toList(eastLink)));
  
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

  // Add outputs from other board routers
  leftOutList = Cons(eastLink[0].upOut,
                  Cons(northLink[0].leftOut,
                    Cons(southLink[0].leftOut, leftOutList)));

  // Connect the outgoing links
  reduceConnect(mkFlitMerger,
    leftOutList, List::map(getFlitIn, toList(westLink)));
  
  // Connect the incoming links
  expandConnect(List::map(getFlitOut, toList(westLink)), leftInList);

`ifndef SIMULATE
  function AvalonMac getMac(BoardLink link) = link.avalonMac;
  interface north = Vector::map(getMac, northLink);
  interface south = Vector::map(getMac, southLink);
  interface east = Vector::map(getMac, eastLink);
  interface west = Vector::map(getMac, westLink);
`endif

endmodule

// =============================================================================
// Flit merger
// =============================================================================

// Fair merge two flit ports
module mkFlitMerger#(Out#(Flit) left, Out#(Flit) right) (Out#(Flit));

  // Ports
  InPort#(Flit) leftIn <- mkInPort;
  InPort#(Flit) rightIn <- mkInPort;
  OutPort#(Flit) outPort <- mkOutPort;

  connectUsing(mkUGShiftQueue1(QueueOptFmax), left, leftIn.in);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), right, rightIn.in);

  // State
  Reg#(Bool) prevChoiceWasLeft <- mkReg(False);
  Reg#(RouteLock) lock <- mkReg(Unlocked);

  // Rules
  rule merge (outPort.canPut);
    Bool chooseRight = 
      lock == FromRight ||
        (lock == Unlocked &&
           rightIn.canGet &&
             (!leftIn.canGet || prevChoiceWasLeft));
    // Consume input
    if (chooseRight) begin
      if (rightIn.canGet) begin
        rightIn.get;
        outPort.put(rightIn.value);
        lock <= rightIn.value.notFinalFlit ? FromRight : Unlocked;
        prevChoiceWasLeft <= False;
      end
    end else if (leftIn.canGet) begin
      leftIn.get;
      outPort.put(leftIn.value);
      lock <= leftIn.value.notFinalFlit ? FromLeft : Unlocked;
      prevChoiceWasLeft <= True;
    end
  endrule

  // Interface
  return outPort.out;

endmodule

endpackage
