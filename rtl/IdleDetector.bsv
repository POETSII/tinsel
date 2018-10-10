// This module supports the Tinsel API function
//
//   bool tinselIdle();
//
// which blocks until either
//
//   1. a message is available to receive, or
//
//   2. all threads are blocked on a call to tinselIdle() and there
//      are no undelivered messages in the system.
//
// The function returns false in the former case and true in the latter.
// The implementation below is based on Safra's termination detection
// algorithm (EWD998).

import Mailbox   :: *;
import Globals   :: *;
import Interface :: *;
import Queue     :: *;
import Vector    :: *;
import ConfigReg :: *;
import Util      :: *;
import DReg      :: *;

// The total number of messages sent by all threads on an FPGA minus
// the total number of messages received by all threads on an FPGA.
typedef Int#(62) MsgCount;

// The "token" in Safra's algorithm
typedef struct {
  // Colour: black or white
  Bool black;
  // Termination flag
  Bool done;
  // In-flight message count
  MsgCount count;
} IdleToken deriving (Bits);

// We added the 1-bit termination flag to Safra's token.  The idea is
// that once termination is detected, the token is sent twice round
// all the FPGAs with the termination bit set. In the first round,
// each FPGA observes that termination has been detected and disables
// message sending -- at this point, all calls to tinselIdle() return
// true before the token is forwarded. In the second round, sending is
// enabled again. The two rounds are required to avoid a machine that
// has returned from tinselIdle() from sending a message to a machine
// that has not yet returned from tinselIdle().

// The signals we watch in order to detect termination, as well as the
// signals we trigger when it is detected.
interface IdleSignals;
  // The 'activeIn' bit goes low when all threads on
  // the FPGA are in a call to tinselIdle()
  (* always_ready, always_enabled *)
  method Action activeIn(Bool active);

  // In-flight message count
  (* always_ready, always_enabled *)
  method Action countIn(MsgCount count);

  // This flag is pulsed high to indicate that termination has been
  // detected.  This should disable message sending on all threads.
  // At this stage, message counters can also be reset to 0.
  method Bool detectedStage1;

  // This flag is pulsed high to enable sending again on all threads.
  method Bool detectedStage2;

  // Acknowledgement from cores that stage 1 has completed
  (* always_ready, always_enabled *)
  method Action ackStage1(Bool pulse);
endinterface

// The idle detector itself is designed to connect onto mailbox (0,0).
interface IdleDetector;
  interface IdleSignals idle;
  // Network side
  interface In#(Flit) netFlitIn;
  interface Out#(Flit) netFlitOut;
  // Mailbox side
  interface In#(Flit) mboxFlitIn;
  interface Out#(Flit) mboxFlitOut;
endinterface

// An idle probe is initiated by the bridge FPGA board sending a token
// to the origin worker FPGA board.  Each woker then passes the
// token one step closer to the bridge.  Eventually the token ends up
// back at the bridge.  We use the following function to determine the
// coordinates of an FPGA board that is "one step" closer to the
// bridge.
function BoardId getNextBoard(BoardId me) =
  BoardId {
    x: me.x == `MeshXLen-1 ? 0      : me.x+1
  , y: me.x == `MeshXLen-1 ? me.y+1 : me.y
  };

module mkIdleDetector#(BoardId me) (IdleDetector);

  // Ports  
  InPort#(Flit) netInPort <- mkInPort;
  OutPort#(Flit) netOutPort <- mkOutPort;
  InPort#(Flit) mboxInPort <- mkInPort;
  OutPort#(Flit) mboxOutPort <- mkOutPort;

  // An idle-detection token waits here
  Queue1#(Flit) tokenInQueue <- mkUGShiftQueue1(QueueOptFmax);
  Queue1#(Flit) tokenOutQueue <- mkUGShiftQueue1(QueueOptFmax);

  // Goes high when multi-flit message being processed
  Reg#(Bool) multiFlit <- mkConfigReg(False);

  // Forward flits
  rule forward;
    // From mailbox side to network side
    if (netOutPort.canPut) begin
      if (tokenOutQueue.canDeq && !multiFlit) begin
        netOutPort.put(tokenOutQueue.dataOut);
        tokenOutQueue.deq;
      end else if (mboxInPort.canGet) begin
        Flit flit = mboxInPort.value;
        netOutPort.put(flit);
        mboxInPort.get;
        multiFlit <= flit.notFinalFlit;
      end
    end

    // From network side to mailbox side
    if (netInPort.canGet) begin
      Flit flit = netInPort.value;
      if (flit.isIdleToken) begin
        myAssert(tokenInQueue.notFull, "Multiple idle tokens in flight");
        tokenInQueue.enq(flit);
        netInPort.get;
      end else if (mboxOutPort.canPut) begin
        mboxOutPort.put(flit);
        netInPort.get;
      end
    end
  endrule

  // Idle detection states
  // 0: waiting to forward the idle token
  // 1: waiting for stage 1 ack 
  // 2: waiting for second done token
  Reg#(Bit#(2)) state <- mkConfigReg(0);

  // FPGA colour (white or black);
  Reg#(Bool) black <- mkConfigReg(False);

  // Pulse registers
  Reg#(Bool) detectedStage1Reg <- mkDReg(False);
  Reg#(Bool) detectedStage2Reg <- mkDReg(False);

  // Access activeIn and countIn signals
  Wire#(Bool) activeWire <- mkBypassWire;
  Wire#(MsgCount) countWire <- mkBypassWire;

  // Ack from cores that stage 1 is complete
  Wire#(Bool) ackStage1Wire <- mkBypassWire;

  // This wire is pulsed when token is forwarded
  Wire#(Bool) tokenForwarded <- mkDWire(False);

  // Update colour
  rule updateColour;
    if (tokenForwarded)
      black <= False;
    else if (activeWire)
      black <= True;
  endrule

  // Send idle token
  rule sendToken (tokenInQueue.canDeq);
    myAssert(tokenOutQueue.notFull, "Multiple idle tokens in flight");
    // Extract input token from flit
    IdleToken in = unpack(truncate(tokenInQueue.dataOut.payload));
    IdleToken token;
    token.black = black || in.black;
    token.done = in.done;
    token.count = in.count + countWire;
    // Construct flit containing output token
    Flit outFlit;
    outFlit.isIdleToken = True;
    outFlit.dest =
      NetAddr {
        board: getNextBoard(me),
        core: 0,
        thread: 0
      };
    outFlit.notFinalFlit = False;
    outFlit.payload = zeroExtend(pack(token));
    if (in.done) begin
      if (state == 0) begin
        detectedStage1Reg <= True;
        state <= 1;
      end else if (state == 1) begin
        if (ackStage1Wire) begin
          tokenInQueue.deq;
          tokenOutQueue.enq(outFlit);
          state <= 2;
        end
      end else if (state == 2) begin
        detectedStage2Reg <= True;
        state <= 0;
        tokenInQueue.deq;
        tokenOutQueue.enq(outFlit);
      end
    end else begin
      myAssert(state == 0, "Unexpected idle done token");
      if (!activeWire) begin
        tokenForwarded <= True;
        tokenInQueue.deq;
        tokenOutQueue.enq(outFlit);
      end
    end
  endrule
 
  interface IdleSignals idle;
    method Action activeIn(Bool active);
      activeWire <= active;
    endmethod

    method Action countIn(MsgCount count);
      countWire <= count;
    endmethod

    method Bool detectedStage1 = detectedStage1Reg;
    method Bool detectedStage2 = detectedStage2Reg;

    method Action ackStage1(Bool pulse);
      ackStage1Wire <= pulse;
    endmethod
  endinterface

  interface In netFlitIn = netInPort.in;
  interface Out netFlitOut = netOutPort.out;

  interface In mboxFlitIn = mboxInPort.in;
  interface Out mboxFlitOut = mboxOutPort.out;

endmodule

// Pipelined reduction tree
module mkPipelinedReductionTree#(
         function a reduce(a x, a y),
         a init,
         List#(a) xs)
       (a) provisos(Bits#(a, _));
  Integer len = List::length(xs);
  if (len == 0)
    return error("mkSumList applied to empty list");
  else if (len == 1)
    return xs[0];
  else begin
    List#(a) ys = xs;
    List#(a) reduced = Nil;
    for (Integer i = 0; i < len; i=i+2) begin
      Reg#(a) r <- mkConfigReg(init);
      rule assignOut;
        r <= reduce(ys[0], ys[1]);
      endrule
      ys = List::drop(2, ys);
      reduced = Cons(readReg(r), reduced);
    end
    a res <- mkPipelinedReductionTree(reduce, init, reduced);
    return res;
  end
endmodule

interface IdleDetectorClient;
  method Bit#(1) incSent;
  method Bit#(1) incReceived;
  method Bool active;
  (* always_ready, always_enabled *)
  method Action idleDetectedStage1(Bool pulse);
  (* always_ready, always_enabled *)
  method Action idleDetectedStage2(Bool pulse);
  method Bool idleStage1Ack;
endinterface

// Connect cores to idle detector
module connectCoresToIdleDetector#(
         Vector#(n, IdleDetectorClient) core, IdleDetector detector) ()
           provisos (Log#(n, log_n), Add#(log_n, 1, m), Add#(_a, m, 62));

  // Sum "incSent" wires from each core
  Vector#(n, Bit#(m)) incSents = newVector;
  for (Integer i = 0; i < valueOf(n); i=i+1)
    incSents[i] = zeroExtend(core[i].incSent);
  Bit#(m) incSent <- mkPipelinedReductionTree( \+ , 0, toList(incSents));

  // Sum "incRecv" wires from each core
  Vector#(n, Bit#(m)) incRecvs = newVector;
  for (Integer i = 0; i < valueOf(n); i=i+1)
    incRecvs[i] = zeroExtend(core[i].incReceived);
  Bit#(m) incRecv <- mkPipelinedReductionTree( \+ , 0, toList(incRecvs));

  // Maintain the total count
  Reg#(MsgCount) count <- mkConfigReg(0);

  rule updateCount;
    count <= count + unpack(zeroExtend(incSent))
                   - unpack(zeroExtend(incRecv));
  endrule

  // OR the "active" wires from each core
  Vector#(n, Bool) actives = newVector;
  for (Integer i = 0; i < valueOf(n); i=i+1)
    actives[i] = core[i].active;
  Bool anyActive <- mkPipelinedReductionTree( \|| , True, toList(actives));

  // Register the result
  Reg#(Bool) active <- mkConfigReg(True);
  
  rule updateActive;
    active <= anyActive;
  endrule

  // Counter number of stage 1 acks
  Reg#(Bit#(m)) numAcks <- mkConfigReg(0);

  // Sum stage 1 ack wires from each core
  Vector#(n, Bit#(m)) incAcks = newVector;
  for (Integer i = 0; i < valueOf(n); i=i+1)
    incAcks[i] = zeroExtend(pack(core[i].idleStage1Ack));
  Bit#(m) incAck <- mkPipelinedReductionTree( \+ , 0, toList(incAcks));

  // Stage 1 output ack
  Wire#(Bool) stage1AckWire <- mkDWire(False);

  rule updateAcks;
    Bit#(m) total = numAcks + incAck;
    if (total == fromInteger(valueOf(n))) begin
      numAcks <= 0;
      stage1AckWire <= True;
    end else begin
      numAcks <= total;
    end
  endrule

  // Direct connections
  rule connect;
    // Feed signals to the idle detector
    detector.idle.countIn(count);
    detector.idle.activeIn(active);
    detector.idle.ackStage1(stage1AckWire);

    for (Integer i = 0; i < valueOf(n); i=i+1) begin
      core[i].idleDetectedStage1(detector.idle.detectedStage1);
      core[i].idleDetectedStage2(detector.idle.detectedStage2);
    end
  endrule

endmodule

// Idle-detect master
// ==================
//
// (Runs on the bridge board)

interface IdleDetectMaster;
  interface In#(Flit) flitIn;
  interface Out#(Flit) flitOut;

  // Disable host messages when this is high
  method Bool disableHostMsgs;

  // Increment in-flight message count
  method Action incCount;

  // Decrement in-flight message count
  method Action decCount;

  (* always_ready, always_enabled *)
  method Action enabled(Bool en);
endinterface

module mkIdleDetectMaster (IdleDetectMaster);
 
  // Ports  
  InPort#(Flit) flitInPort <- mkInPort;
  OutPort#(Flit) flitOutPort <- mkOutPort;

  // Number of messages sent minus number of message received
  Reg#(MsgCount) count <- mkConfigReg(0);
  PulseWire incWire <- mkPulseWire;
  PulseWire decWire <- mkPulseWire;

  // Enabled
  Wire#(Bool) enableWire <- mkBypassWire;

  // Update message count
  rule updateCount;
    if (incWire && !decWire)
      count <= count + 1;
    else if (!incWire && decWire)
      count <= count - 1;
  endrule
  
  // FSM state
  // 0: send probe
  // 1: receive probe
  // 2: send stage1 done
  // 3: receive stage1 done
  // 4: send stage2 done
  // 5: receive stage2 done
  Reg#(Bit#(3)) state <- mkConfigReg(0);

  // Disable host messages when either of these go high
  Reg#(Bool) disableHostMsgsReg <- mkConfigReg(False);
  Wire#(Bool) disableHostMsgsWire <- mkDWire(False);

  // Probe for termination
  rule probe (enableWire);
    // Construct idle token
    IdleToken token;
    token.black = False;
    token.done = False;
    token.count = 0;
    // Construct flit
    Flit flit;
    flit.dest =
      NetAddr {
        board: BoardId { y: 0, x: 0 },
        core: 0,
        thread: 0
      };
    flit.payload = ?;
    flit.notFinalFlit = False;
    flit.isIdleToken = True;
    // State machine
    case (state)
      // Send probe
      0: if (flitOutPort.canPut) begin
           flit.payload = zeroExtend(pack(token));
           flitOutPort.put(flit);
           state <= 1;
         end
      // Receive probe
      1: if (flitInPort.canGet) begin
           disableHostMsgsWire <= True;
           flitInPort.get;
           token = unpack(truncate(flitInPort.value.payload));
           if (!token.black && (count+token.count == 0)) begin
             disableHostMsgsReg <= True;
             state <= 2;
           end else
             state <= 0;
         end
      // Send stage 1 done
      2: if (flitOutPort.canPut) begin
           token.done = True;
           flit.payload = zeroExtend(pack(token));
           flitOutPort.put(flit);
           state <= 3;
         end
      // Receive stage 1 done
      3: if (flitInPort.canGet) begin
           flitInPort.get;
           state <= 4;
         end
      // Send stage 2 done
      4: if (flitOutPort.canPut) begin
           token.done = True;
           flit.payload = zeroExtend(pack(token));
           flitOutPort.put(flit);
           state <= 5;
         end
      // Receive stage 2 done
      5: if (flitInPort.canGet) begin
           flitInPort.get;
           state <= 0;
           disableHostMsgsReg <= False;
         end
    endcase
  endrule

  interface In flitIn = flitInPort.in;
  interface Out flitOut = flitOutPort.out;
  method Bool disableHostMsgs =
    disableHostMsgsWire || disableHostMsgsReg;
  method Action incCount = incWire.send;
  method Action decCount = decWire.send;
  method Action enabled(Bool en);
    enableWire <= en;
  endmethod

endmodule
