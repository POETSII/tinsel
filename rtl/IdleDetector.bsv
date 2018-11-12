// This module supports the Tinsel API function
//
//   uint32_t tinselIdle(bool vote);
//
// which blocks until either
//
//   1. a message is available to receive, or
//
//   2. all threads are blocked on a call to tinselIdle() and there
//      are no undelivered messages in the system.
//
// The function returns zero in the former case and non-zero in the latter.
// A return value of 1 denotes a non-unanamous vote, i.e. not all callers
// voted true, and a return value > 1 denotes a unanamous vote,
// i.e. all callers voted true.  The voting mechanism allows a
// global condition to be established, e.g. every thread in the system
// agreeing to halt.
//
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
  // Vote flag
  Bool vote;
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

  // The 'voteIn' bit is sampled when the `activeIn` bit goes low
  (* always_ready, always_enabled *)
  method Action voteIn(Bool vote);

  // In-flight message count
  (* always_ready, always_enabled *)
  method Action countIn(MsgCount count);

  // This flag is pulsed high to indicate that termination has been
  // detected.  This should disable message sending on all threads.
  // At this stage, message counters can also be reset to 0.
  method Bool detectedStage1;

  // This flag indicates a unanamous vote, to be sampled when
  // detectedStage1 is pulsed.
  method Bool voteStage1;

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
  Reg#(Bool) voteStage1Reg <- mkDReg(False);
  Reg#(Bool) detectedStage2Reg <- mkDReg(False);

  // Access activeIn, voteIn, and countIn signals
  Wire#(Bool) activeWire <- mkBypassWire;
  Wire#(Bool) voteWire <- mkBypassWire;
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
    token.black = black;
    token.vote = voteWire;
    token.done = in.done;
    token.count = countWire;
    // Construct flit containing output token
    Flit outFlit;
    outFlit.isIdleToken = True;
    outFlit.dest =
      NetAddr {
        board: BoardId { y: `MeshYLen , x: 0 },
        core: 0,
        thread: 0
      };
    outFlit.notFinalFlit = False;
    outFlit.payload = zeroExtend(pack(token));
    if (in.done) begin
      if (state == 0) begin
        detectedStage1Reg <= True;
        voteStage1Reg <= in.vote;
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

    method Action voteIn(Bool vote);
      voteWire <= vote;
    endmethod

    method Action countIn(MsgCount count);
      countWire <= count;
    endmethod

    method Bool detectedStage1 = detectedStage1Reg;
    method Bool voteStage1 = voteStage1Reg;
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
  method Bool vote;
  (* always_ready, always_enabled *)
  method Action idleDetectedStage1(Bool pulse);
  (* always_ready, always_enabled *)
  method Action idleVoteStage1(Bool pulse);
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

  // OR the "vote" wires from each core
  Vector#(n, Bool) votes = newVector;
  for (Integer i = 0; i < valueOf(n); i=i+1)
    votes[i] = core[i].vote;
  Bool unanamous <- mkPipelinedReductionTree( \&& , False, toList(votes));

  // Register the result
  Reg#(Bool) active <- mkConfigReg(True);
  Reg#(Bool) vote <- mkConfigReg(True);
  
  rule updateActive;
    active <= anyActive;
    vote <= unanamous;
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
    detector.idle.voteIn(vote);
    detector.idle.ackStage1(stage1AckWire);

    for (Integer i = 0; i < valueOf(n); i=i+1) begin
      core[i].idleDetectedStage1(detector.idle.detectedStage1);
      core[i].idleVoteStage1(detector.idle.voteStage1);
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
  Reg#(MsgCount) localCount <- mkConfigReg(0);
  PulseWire incWire <- mkPulseWire;
  PulseWire decWire <- mkPulseWire;

  // Sum counts received from worker boards
  Reg#(MsgCount) totalCount <- mkConfigReg(0);

  // Track the votes
  Reg#(Bool) nextVote <- mkConfigReg(False);
  Reg#(Bool) currentVote <- mkConfigReg(True);

  // Master is white at beginning of new probe,
  // and goes black when a message is sent/received
  Reg#(Bool) localBlack <- mkConfigReg(False);
  PulseWire localBlackReset <- mkPulseWire;

  // Enabled
  Wire#(Bool) enableWire <- mkBypassWire;

  // Update local message count
  rule updateLocalCount;
    if (incWire && !decWire)
      localCount <= localCount + 1;
    else if (!incWire && decWire)
      localCount <= localCount - 1;
  endrule

  // Update local colour
  rule updateLocalColour;
    if (localBlackReset)
      localBlack <= False;
    else if (incWire || decWire)
      localBlack <= True;
  endrule
 
  // FSM state
  // 0: probe
  // 1: stage 1 done
  // 2: stage 2 done
  Reg#(Bit#(2)) state <- mkConfigReg(0);

  // Disable host messages when either of these go high
  Reg#(Bool) disableHostMsgsReg <- mkConfigReg(False);
  Wire#(Bool) disableHostMsgsWire <- mkDWire(False);

  // For iterating over the worker boards
  Reg#(Bit#(`MeshXBits)) boardX <- mkConfigReg(0);
  Reg#(Bit#(`MeshYBits)) boardY <- mkConfigReg(0);

  // Count responses from worker boards
  Reg#(Bit#(TAdd#(`MeshXBits, `MeshYBits))) respCount <- mkConfigReg(0);

  // Are any responses so far black?
  Reg#(Bool) anyBlack <- mkConfigReg(False);

  // Goes high when probe has been sent to all workers
  Reg#(Bool) probeSent <- mkConfigReg(False);

  // Probe for termination
  rule probe (enableWire);
    // Construct idle token
    IdleToken token;
    token.black = False;
    token.done = False;
    token.count = 0;
    token.vote = currentVote;
    // Construct flit
    Flit flit;
    flit.dest =
      NetAddr {
        board: BoardId { y: boardY, x: boardX },
        core: 0,
        thread: 0
      };
    flit.payload = ?;
    flit.notFinalFlit = False;
    flit.isIdleToken = True;
    // New value for probeSent
    Bool probeSentNew = probeSent;

    // Continue sending current probe
    if (! probeSent) begin
      if (flitOutPort.canPut) begin
        // Send probe
        if (state != 0) token.done = True;
        flit.payload = zeroExtend(pack(token));
        flitOutPort.put(flit);

        // Move to next board
        if (boardX == fromInteger(`MeshXLen-1)) begin
          boardX <= 0;
          if (boardY == fromInteger(`MeshYLen-1)) begin
            boardY <= 0;
            probeSentNew = True;
          end else
            boardY <= boardY+1;
        end else
          boardX <= boardX+1;
      end
    end

    // Receive probe responses
    if (flitInPort.canGet) begin
      flitInPort.get;
      token = unpack(truncate(flitInPort.value.payload));
      if (respCount == fromInteger(`MeshXLen * `MeshYLen - 1)) begin
        respCount <= 0;
        probeSentNew = False;
        if (state == 0) begin
          totalCount <= localCount;
          anyBlack <= False;
          localBlackReset.send;
          currentVote <= nextVote && token.vote;
          nextVote <= True;
      
          disableHostMsgsWire <= True;
          if (!token.black && !localBlack && !anyBlack &&
                (totalCount+token.count == 0)) begin
            disableHostMsgsReg <= True;
            state <= 1;
          end
        end else if (state == 1)
          state <= 2;
        else if (state == 2) begin
          state <= 0;
          disableHostMsgsReg <= False;
        end
      end else begin
        respCount <= respCount+1;
        if (state == 0) begin
          totalCount <= totalCount + token.count;
          nextVote <= nextVote && token.vote;
          anyBlack <= anyBlack || token.black;
        end
      end
    end

    // Update probeSent
    probeSent <= probeSentNew;
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
