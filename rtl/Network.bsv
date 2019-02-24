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
module mkMeshRouter#(RouteFunc route) (MeshRouter);

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

// Mailbox router
(* synthesize *)
module mkMailboxRouter#(MailboxId m) (MeshRouter);
  // Board id
  Wire#(BoardId) b <- mkDWire(?);

  // Routing function
  function Route route(NetAddr addr);
    if      (addr.board != b)            return Up;
    else if (getMailboxId(addr).x < m.x) return Left;
    else if (getMailboxId(addr).x > m.x) return Right;
    else if (getMailboxId(addr).y < m.y) return Down;
    else if (getMailboxId(addr).y > m.y) return Up;
    else return Mailbox;
  endfunction

  MeshRouter r <- mkMeshRouter(route);

  method Action setBoardId(BoardId id);
    b <= id;
  endmethod

  // Interface
  interface In  leftIn      = r.leftIn;
  interface Out leftOut     = r.leftOut;
  interface In  rightIn     = r.rightIn;
  interface Out rightOut    = r.rightOut;
  interface In  topIn       = r.topIn;
  interface Out topOut      = r.topOut;
  interface In  bottomIn    = r.bottomIn;
  interface Out bottomOut   = r.bottomOut;
  interface In  fromMailbox = r.fromMailbox;
  interface Out toMailbox   = r.toMailbox;
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
// Inter-board mesh
// =============================================================================

// The inter-board mesh attaches to the top of the mailbox mesh:
//
// North --- South --- East --- West
//   |         |        |        |
//   +---------+--------+--------+
//   |        Mailbox mesh       |
//   +---------------------------+

interface InterBoardMesh;
  interface Vector#(4, In#(Flit)) flitInVec;
  interface Vector#(4, Out#(Flit)) flitOutVec;
  interface AvalonMac northMac;
  interface AvalonMac southMac;
  interface AvalonMac eastMac;
  interface AvalonMac westMac;
  (* always_ready, always_enabled *)
  method Action setBoardId(BoardId id);
endinterface

(* synthesize *)
module mkInterBoardMesh (InterBoardMesh);

  // Board id
  Wire#(BoardId) b <- mkDWire(?);

  // North link
  // ----------

  function Route routeNorth(NetAddr addr);
    if      (addr.board == b)            return Down;
    else if (addr.board.x != b.x)        return Right;
    else if (addr.board.y < b.y)         return Right;
    else return Up;
  endfunction

  BoardLink northLink <- mkBoardLink(northSocket);
  MeshRouter north <- mkMeshRouter(routeNorth);

  // South link
  // ----------

  function Route routeSouth(NetAddr addr);
    if      (addr.board == b)            return Down;
    else if (addr.board.x != b.x)        return Right;
    else if (addr.board.y > b.y)         return Left;
    else return Up;
  endfunction

  BoardLink southLink <- mkBoardLink(southSocket);
  MeshRouter south <- mkMeshRouter(routeSouth);

  // East link
  // ---------

  function Route routeEast(NetAddr addr);
    if      (addr.board == b)            return Down;
    else if (addr.board.x > b.x)         return Up;
    else if (addr.board.x < b.x)         return Right;
    else return Left;
  endfunction

  BoardLink eastLink <- mkBoardLink(eastSocket);
  MeshRouter east <- mkMeshRouter(routeEast);

  // West link
  // ---------

  function Route routeWest(NetAddr addr);
    if      (addr.board == b)            return Down;
    else if (addr.board.x < b.x)         return Up;
    else return Left;
  endfunction

  BoardLink westLink <- mkBoardLink(westSocket);
  MeshRouter west <- mkMeshRouter(routeWest);

  // Connect routers to board links
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    northLink.flitOut, north.topIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    north.topOut, northLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    southLink.flitOut, south.topIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    south.topOut, southLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    eastLink.flitOut, east.topIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    east.topOut, eastLink.flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    westLink.flitOut, west.topIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    west.topOut, westLink.flitIn);

  // Connect routers together
  connectUsing(mkUGShiftQueue1(QueueOptFmax), north.rightOut, south.leftIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), south.leftOut, north.rightIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), south.rightOut, east.leftIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), east.leftOut, south.rightIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), east.rightOut, west.leftIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), west.leftOut, east.rightIn);

  // Handle unused connections
  BOut#(Flit) nullOut <- mkNullBOut;
  connectDirect(nullOut, north.fromMailbox);
  connectDirect(nullOut, south.fromMailbox);
  connectDirect(nullOut, east.fromMailbox);
  connectDirect(nullOut, west.fromMailbox);
  connectDirect(nullOut, north.leftIn);
  connectDirect(nullOut, west.rightIn);

  Vector#(4, In#(Flit)) vecIn =
    vector(north.bottomIn, south.bottomIn,
           east.bottomIn, west.bottomIn);
  Vector#(4, Out#(Flit)) vecOut =
    vector(north.bottomOut, south.bottomOut,
           east.bottomOut, west.bottomOut);

  method Action setBoardId(BoardId id);
    b <= id;
  endmethod

  interface flitInVec = vecIn;
  interface flitOutVec = vecOut;
  interface northMac = northLink.avalonMac;
  interface southMac = southLink.avalonMac;
  interface eastMac = eastLink.avalonMac;
  interface westMac = westLink.avalonMac;
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

  // Create inter-board mesh
  InterBoardMesh interBoardMesh <- mkInterBoardMesh;
  rule interSetBoardId;
    interBoardMesh.setBoardId(boardId);
  endrule

  // Create mailbox routers
  Vector#(`MailboxMeshYLen,
    Vector#(`MailboxMeshXLen, MeshRouter)) routers =
      Vector::replicate(newVector());

  for (Integer y = 0; y < `MailboxMeshYLen; y=y+1)
    for (Integer x = 0; x < `MailboxMeshXLen; x=x+1) begin
      MailboxId mailboxId =
        MailboxId { x: fromInteger(x), y: fromInteger(y) };
      routers[y][x] <- mkMailboxRouter(mailboxId);
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

  // Connect inter-board mesh to mailbox mesh
  // ----------------------------------------

  // Get top row of mesh
  List#(In#(Flit)) topInList = Nil;
  List#(Out#(Flit)) topOutList = Nil;
  for (Integer x = `MailboxMeshXLen-1; x >= 0; x=x-1) begin
    topOutList = Cons(routers[`MailboxMeshYLen-1][x].topOut, topOutList);
    topInList = Cons(routers[`MailboxMeshYLen-1][x].topIn, topInList);
  end

  // Get inter-board mesh links
  List#(Out#(Flit)) interOutList = toList(interBoardMesh.flitOutVec);
  List#(In#(Flit)) interInList = toList(interBoardMesh.flitInVec);

  // Connect
  reduceConnect(mkFlitMerger, interOutList, topInList);
  reduceConnect(mkFlitMerger, topOutList, interInList);

  // Handle unused connections
  // -------------------------

  BOut#(Flit) nullOut <- mkNullBOut;
  for (Integer x = `MailboxMeshXLen-1; x >= 0; x=x-1)
    connectDirect(nullOut, routers[0][x].bottomIn);
  for (Integer y = `MailboxMeshYLen-1; y >= 0; y=y-1)
    connectDirect(nullOut, routers[y][`MailboxMeshXLen-1].rightIn);
  for (Integer y = `MailboxMeshYLen-1; y >= 0; y=y-1)
    connectDirect(nullOut, routers[y][0].leftIn);

`ifndef SIMULATE
  interface north = vector(interBoardMesh.northMac);
  interface south = vector(interBoardMesh.southMac);
  interface east = vector(interBoardMesh.eastMac);
  interface west = vector(interBoardMesh.westMac);
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
