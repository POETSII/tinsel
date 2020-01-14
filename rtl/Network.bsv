// SPDX-License-Identifier: BSD-2-Clause
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
import FlitMerger   :: *;

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
  function Route route(NetAddr a);
         if (a.addr.board != b)   return Down;
    else if (a.addr.host.valid)   return Down;
    else if (a.addr.mbox.y < m.y) return Down;
    else if (a.addr.mbox.y > m.y) return Up;
    else if (a.addr.mbox.x < m.x) return Left;
    else if (a.addr.mbox.x > m.x) return Right;
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
// Board router
// =============================================================================

// Similar to a mesh router, but: (1) different routing function,
// which routes between boards rather than mailboxes; (2) no loopback
// in the sense that packets coming from mailbox mesh never get routed back
// onto mailbox mesh.  This is a first step towards supporting
// programmable board routers.
module mkBoardRouter(MeshRouter);

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
  function Route route(NetAddr a);
         if (a.addr.host.valid)    return a.addr.host.value == 0 ? Left : Right;
    else if (a.addr.board.x < b.x) return Left;
    else if (a.addr.board.x > b.x) return Right;
    else if (a.addr.board.y < b.y) return Down;
    else if (a.addr.board.y > b.y) return Up;
    else return Mailbox;
  endfunction

  // Route to the mailbox
  mkRouterMux(
    route,
    Mailbox,
    toMailboxPort,
    vector(FromLeft, FromRight, FromTop, FromBottom),
    vector(leftInPort, rightInPort, topInPort, bottomInPort)
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

// Inter FPGA link with explicit enable line. (Can be useful to
// disable links for sandboxing in multi-box environments.)
module mkBoardLink#(Bool en, SocketId id) (BoardLink);
  
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
  connectUsing(mkUGQueue, enableOut(en, ser.serialOut), link.streamIn);
  connectDirect(enableBOut(en, link.streamOut), des.serialIn);

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
         Vector#(4, Bool) linkEnable,
         Vector#(`MailboxMeshYLen,
           Vector#(`MailboxMeshXLen, MailboxNet)) mailboxes,
         IdleDetector idle)
       (ExtNetwork);

  // Create off-board links
  Vector#(`NumNorthSouthLinks, BoardLink) northLink <-
    mapM(mkBoardLink(linkEnable[0]), northSocket);
  Vector#(`NumNorthSouthLinks, BoardLink) southLink <-
    mapM(mkBoardLink(linkEnable[1]), southSocket);
  Vector#(`NumEastWestLinks, BoardLink) eastLink <-
    mapM(mkBoardLink(linkEnable[2]), eastSocket);
  Vector#(`NumEastWestLinks, BoardLink) westLink <-
    mapM(mkBoardLink(linkEnable[3]), westSocket);

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

  // Board router
  // ------------

  // For routing messages between boards
  MeshRouter boardRouter <- mkBoardRouter;

  // Set board id for board router
  rule setBoardRouterId;
    boardRouter.setBoardId(boardId);
  endrule

  // Connect board router to north link
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    boardRouter.topOut, northLink[0].flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    northLink[0].flitOut, boardRouter.topIn);

  // Connect board router to south link
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    boardRouter.bottomOut, southLink[0].flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    southLink[0].flitOut, boardRouter.bottomIn);

  // Connect board router to east link
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    boardRouter.rightOut, eastLink[0].flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    eastLink[0].flitOut, boardRouter.rightIn);

  // Connect board router to west link
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    boardRouter.leftOut, westLink[0].flitIn);
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
    westLink[0].flitOut, boardRouter.leftIn);

  // Connect mailbox mesh south rim to board router
  function List#(t) single(t elem) = List::cons(elem, Nil);
  List#(Out#(Flit)) botOutList = Nil;
  for (Integer x = `MailboxMeshXLen-1; x >= 0; x=x-1)
    botOutList = Cons(routers[0][x].bottomOut, botOutList);
  function In#(Flit) getFlitIn(BoardLink link) = link.flitIn;
  reduceConnect(mkFlitMerger, botOutList, single(boardRouter.fromMailbox));

  // Connect board router to mailbox mesh south rim
  function In#(Flit) getBottomIn(MeshRouter r) = r.bottomIn;
  Vector#(`MailboxMeshXLen, In#(Flit)) southRimInPorts =
    map(getBottomIn, routers[0]);
  function Bit#(`MailboxMeshXBits) flitGetX(Flit flit) =
    flit.dest.addr.mbox.x;
  let southRimDistributor <- mkResponseDistributor(flitGetX,
    mkUGShiftQueue1(QueueOptFmax), southRimInPorts);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), boardRouter.toMailbox,
    southRimDistributor);

  // Detect inter-board activity
  // ---------------------------

  // Latch to improve timing
  Reg#(Bool) activityReg <- mkReg(False);

  // Determine when a flit arrives on a link,
  // provided that flit is not a stage 1 idle token
  function Bool active(BoardLink link);
    Flit flit =  link.flitOut.value;
    IdleToken in = unpack(truncate(flit.payload));
    return (link.flitOut.valid && (flit.isIdleToken ? !in.stage1 : True));
  endfunction

  // For barrier release phase
  rule informIdleDetector;
    Bool activity = False;
    for (Integer i = 0; i < `NumNorthSouthLinks; i=i+1)
     activity = activity || active(southLink[i]);
    for (Integer i = 0; i < `NumNorthSouthLinks; i=i+1)
      activity = activity || active(northLink[i]);
    for (Integer i = 0; i < `NumEastWestLinks; i=i+1)
      activity = activity || active(westLink[i]);
    for (Integer i = 0; i < `NumEastWestLinks; i=i+1)
      activity = activity || active(eastLink[i]);
    activityReg <= activity;
    idle.idle.interBoardActivity(activityReg);
  endrule

`ifndef SIMULATE
  function AvalonMac getMac(BoardLink link) = link.avalonMac;
  interface north = Vector::map(getMac, northLink);
  interface south = Vector::map(getMac, southLink);
  interface east = Vector::map(getMac, eastLink);
  interface west = Vector::map(getMac, westLink);
`endif

endmodule

endpackage
