// Copyright (c) Matthew Naylor

package Core;

// ============================================================================
// Imports
// ============================================================================

import Vector    :: *;
import FIFO      :: *;
import BlockRam  :: *;
import Queue     :: *;
import Assert    :: *;
import Util      :: *;
import DReg      :: *;
import DCache    :: *;
import ConfigReg :: *;
import Interface :: *;
import Mailbox   :: *;
import Globals   :: *;
import HostLink  :: *;
import Mult      :: *;

// ============================================================================
// Control/status register (CSRs) supported
// ============================================================================

// Name       | CSR    | R/W | Function
// ---------- | ------ | --- | --------
// InstrAddr  | 0x800  | W   | Set address for instruction write
// Instr      | 0x801  | W   | Write to instruction memory
// Alloc      | 0x802  | W   | Alloc space for new message in scratchpad
// CanSend    | 0x803  | R   | 1 if can send, 0 otherwise
// HartId     | 0xf14  | R   | Globally unique hardware thread id
// CanRecv    | 0x805  | R   | 1 if can receive, 0 otherwise
// SendLen    | 0x806  | W   | Set message length for send
// SendPtr    | 0x807  | W   | Set message pointer for send
// Send       | 0x808  | W   | Send message to supplied destination
// Recv       | 0x809  | R   | Return pointer to message received
// WaitUntil  | 0x80a  | W   | Sleep until can-send or can-recv
// FromHost   | 0x80b  | R   | Read word from host-link
// ToHost     | 0x80c  | W   | Write word to host-link
// NewThread  | 0x80d  | W   | Create new thread with the given id
// Emit       | 0x80f  | W   | Emit char to console (simulation only)

// ============================================================================
// Types
// ============================================================================

// An index to instruction memory
typedef Bit#(`LogInstrsPerCore) InstrIndex;

// A byte-address in instruction memory
typedef Bit#(TAdd#(`LogInstrsPerCore, 2)) InstrAddr;

// For each thread, we keep the following info
typedef struct {
  // Program counter
  InstrAddr pc;
  // Message length for send operation
  MsgLen msgLen;
  // Message pointer for send operation
  MailboxThreadMsgAddr msgPtr;
  // Write address for instruction memory
  InstrIndex instrWriteIndex;
  // Thread identifier (must be final field of struct)
  ThreadId id;
} ThreadState deriving (Bits);

// Register file index
// (Register file constains 32 registers per thread)
typedef Bit#(TAdd#(`LogThreadsPerCore, 5)) RegFileIndex;

// RV32I instruction type (one-hot encoding)
typedef struct {
  Bool isRType;  Bool isIType;
  Bool isSType;  Bool isSBType;
  Bool isUType;  Bool isUJType;
} InstrType deriving (Bits);

// Decoded CSR
typedef struct {
  Bool isInstrAddr;   Bool isInstr;
  Bool isAlloc;       Bool isCanSend;
  Bool isHartId;      Bool isCanRecv;
  Bool isSendLen;     Bool isSendPtr;
  Bool isSend;        Bool isRecv;
  Bool isWaitUntil;   Bool isFromHost;
  Bool isToHost;      Bool isNewThread;
  `ifdef SIMULATE
  Bool isEmit;
  `endif
} CSR deriving (Bits);

// Decoded operation
typedef struct {
  Bool isAdd;            Bool isSub;
  Bool isSetIfLessThan;  Bool isShiftLeft;
  Bool isShiftRight;     Bool isAnd;
  Bool isOr;             Bool isXor;
  Bool isOpUI;           Bool isJump;
  Bool isJumpReg;
  Bool isBranchEq;       Bool isBranchNotEq;
  Bool isBranchLessThan; Bool isBranchGreaterOrEqualTo;
  Bool isLoad;           Bool isStore;
  Bool isCSR;            CSR csr;
  Bool isAddOrSub;       Bool isBitwise;
  Bool isFence;          Bool isMult;
  Bool isMultH;          Bool isMultASigned;
  Bool isMultBSigned;
} Op deriving (Bits);

// Instruction result
typedef struct {
  Bit#(33) add;       Bit#(32) csr;
  Bit#(32) shiftLeft; Bit#(32) shiftRight;
  Bit#(32) bitwise;   Bit#(32) opui;
  Bit#(66) mult;
} InstrResult deriving (Bits);

// Width of load or store access
typedef struct {
  // Byte, half-word, and full-word accesses
  Bool b; Bool h; Bool w;
} AccessWidth deriving (Bits);

// The type for data passed between each pipeline stage
typedef struct {
  ThreadState thread;      // Current thread state
  Bit#(32) instr;          // RV32I-encoded instruction
  Bit#(32) valA;           // Value of 1st register operand
  Bit#(32) valB;           // Value of 2nd register operand
  Bit#(32) imm;            // Immediate operand
  Bit#(32) aluB;           // Second operand to ALU
  Bool writeRegFile;       // Enable writeback to register file
  Bit#(32) writeVal;       // Value to write to register file
  InstrType instrType;     // RV32I instruction type
  AccessWidth accessWidth; // Byte, half-word, or word access?
  Bit#(32) loadVal;        // Result of load instruction
  Bit#(32) memAddr;        // Memory address for load or store
  Bool isScratchpadAccess; // Does memory address map to scratchpad?
  Op op;                   // Decoded operation
  InstrResult instrResult; // Instruction result
  InstrAddr jumpBase;      // Base of jump relative (PC or register)
  InstrAddr targetPC;      // Next PC if branch taken
  InstrAddr nextPC;        // Next PC if branch not taken
  Bool canSend;            // Mailbox can send
  Bool canRecv;            // Mailbox can receive
  Bool retry;              // Instruction needs retried later
} PipelineToken deriving (Bits);

// For each suspended thread, we have the following info
typedef struct {
  ThreadState thread;      // Thread state
  Bool isLoad;             // Is it waiting for a load?
  Bool isStore;            // Or a store?
  Bit#(5) destReg;         // Destination register for the load result
  Bit#(2) loadSelector;    // Bottom two bits of load address
  AccessWidth accessWidth; // Access width of load (byte, half, word)
  Bool isUnsignedLoad;     // Sign-extension behaviour for load
} SuspendedThreadState deriving (Bits);

// For each suspended thread in the writeback queue, we have:
typedef struct {
  Bool write;
  ThreadState thread;
  Bit#(5) destReg;
  Bit#(32) writeVal;
} Writeback deriving (Bits);

// Token for thread-resumption pipeline
typedef struct {
  ThreadId id;
  Bit#(32) data;
} ResumeToken deriving (Bits);

// ============================================================================
// Decoder
// ============================================================================

// RV32I instruction fields
function Bit#(7) opcode(Bit#(32) instr)  = instr[6:0];
function Bit#(5) rd(Bit#(32) instr)      = instr[11:7];
function Bit#(3) funct3(Bit#(32) instr)  = instr[14:12];
function Bit#(5) rs1(Bit#(32) instr)     = instr[19:15];
function Bit#(5) rs2(Bit#(32) instr)     = instr[24:20];
function Bit#(7) funct7(Bit#(32) instr)  = instr[31:25];

// Compute immediate for each type of RV32I instruction
function Bit#(32) immI(Bit#(32) i) =
  signExtend({i[31],i[30:20]});
function Bit#(32) immS(Bit#(32) i) =
  signExtend({i[31],i[30:25],i[11:8],i[7]});
function Bit#(32) immB(Bit#(32) i) =
  signExtend({i[31],i[7],i[30:25],i[11:8],1'b0});
function Bit#(32) immU(Bit#(32) i) =
  {i[31],i[30:20],i[19:12],12'b0};
function Bit#(32) immJ(Bit#(32) i) =
  signExtend({i[31],i[19:12],i[20],i[30:25],i[24:21],1'b0});

// Determine instruction type from instruction
function InstrType decodeInstrType(Bit#(32) instr);
  Bit#(5) op = instr[6:2];
  InstrType t;
  t.isRType  = op == 'b01100;      /* Arithmetic */
  t.isIType  = op == 'b00100       /* Arithmetic-immediate */
            || op == 'b00000       /* Loads */
            || op == 'b11001       /* JALR */
            || op == 'b00011       /* Fences */
            || op == 'b11100;      /* System */
  t.isSType  = op == 'b01000;      /* Stores */
  t.isSBType = op == 'b11000;      /* Branches */
  t.isUType  = op == 'b01101       /* LUI */
            || op == 'b00101;      /* AUIPC */
  t.isUJType = op == 'b11011;      /* JAL */
  return t;
endfunction

// Determine immediate operand from instruction and type
function Bit#(32) decodeImm(Bit#(32) instr, InstrType t);
  return when(t.isIType , immI(instr))
       | when(t.isSType , immS(instr))
       | when(t.isSBType, immB(instr))
       | when(t.isUType , immU(instr))
       | when(t.isUJType, immJ(instr));
endfunction

// Is it a CSR instruction?
function Bool isCSROp(Bit#(32) instr) = instr[6:2] == 'b11100;

// Decode operation
function Op decodeOp(Bit#(32) instr);
  Op ret = ?;
  Bit#(5) op = instr[6:2];
  Bit#(3) minorOp = funct3(instr);
  // Arithmetic operations
  Bool isArithReg = instr[25] == 0 && op == 'b01100; // Second arg a register
  Bool isArithImm = op == 'b00100; // Second arg an immediate
  Bool isArith = isArithReg || isArithImm;
  ret.isAdd = minorOp == 'b000 && (isArithImm || isArithReg && instr[30] == 0);
  ret.isSub = minorOp == 'b000 && isArithReg && instr[30] == 1;
  ret.isAddOrSub = ret.isAdd || ret.isSub;
  ret.isSetIfLessThan = (minorOp == 'b010 || minorOp == 'b011) && isArith;
  ret.isShiftLeft = minorOp == 'b001 && isArith;
  ret.isShiftRight = minorOp == 'b101 && isArith;
  ret.isAnd = minorOp == 'b111 && isArith;
  ret.isOr = minorOp == 'b110 && isArith;
  ret.isXor = minorOp == 'b100 && isArith;
  ret.isBitwise = ret.isAnd || ret.isOr || ret.isXor;
  // Multiplication
  Bool isMultOrMultH = instr[25] == 1 && op == 'b01100;
  ret.isMult = isMultOrMultH && minorOp[1:0] == 0;
  ret.isMultH = isMultOrMultH && minorOp[1:0] != 0;
  ret.isMultASigned = minorOp[1:0] != 3;
  ret.isMultBSigned = minorOp[1:0] == 1;
  // Load or add-to upper immediate
  ret.isOpUI = op == 'b01101 || op == 'b00101;
  // Jump operations
  ret.isJump = op == 'b11011 || op == 'b11001;
  ret.isJumpReg = op == 'b11001;
  // Branch operations
  Bool isBranch = op == 'b11000;
  ret.isBranchEq = isBranch && minorOp == 'b000;
  ret.isBranchNotEq = isBranch && minorOp == 'b001;
  ret.isBranchLessThan =
    isBranch && (minorOp == 'b100 || minorOp == 'b110);
  ret.isBranchGreaterOrEqualTo =
    isBranch && (minorOp == 'b101 || minorOp == 'b111);
  // Load & store operations
  ret.isLoad = op == 'b00000;
  ret.isStore = op == 'b01000;
  // Fence operation
  ret.isFence = op == 'b00011;
  // CSR read/write operation
  ret.isCSR = isCSROp(instr);
  Bit#(4) csrIndex = instr[23:20];
  // Hardware thread id CSR
  ret.csr.isHartId = ret.isCSR && csrIndex == 'h4;
  // Instruction memory CSRs
  ret.csr.isInstrAddr = ret.isCSR && csrIndex == 'h0;
  ret.csr.isInstr = ret.isCSR && csrIndex == 'h1;
  // Mailbox CSR
  ret.csr.isAlloc     = ret.isCSR && csrIndex == 'h2;
  ret.csr.isCanSend   = ret.isCSR && csrIndex == 'h3;
  ret.csr.isCanRecv   = ret.isCSR && csrIndex == 'h5;
  ret.csr.isSendLen   = ret.isCSR && csrIndex == 'h6;
  ret.csr.isSendPtr   = ret.isCSR && csrIndex == 'h7;
  ret.csr.isSend      = ret.isCSR && csrIndex == 'h8;
  ret.csr.isRecv      = ret.isCSR && csrIndex == 'h9;
  ret.csr.isWaitUntil = ret.isCSR && csrIndex == 'ha;
  ret.csr.isFromHost  = ret.isCSR && csrIndex == 'hb;
  ret.csr.isToHost    = ret.isCSR && csrIndex == 'hc;
  ret.csr.isNewThread = ret.isCSR && csrIndex == 'hd;
  `ifdef SIMULATE
  ret.csr.isEmit      = ret.isCSR && csrIndex == 'hf;
  `endif
  return ret;
endfunction

// Is second ALU operand an immediate?
function Bool isALUImm(Bit#(32) instr) = !unpack(instr[5]);

// Is comparison signed or unsigned
function Bool isUnsignedCmp(Bit#(32) instr) =
     funct3(instr) == 3'b011   // SLTU
  || funct3(instr) == 3'b110   // BLTU
  || funct3(instr) == 3'b111;  // BGEU

// Is shift an arithmetic (sign-preserving) shift?
function Bool isArithShift(Bit#(32) instr) =
  funct7(instr)[5] == 1;

// Add PC to upper immediate?
function Bool addPCtoUI(Bit#(32) instr) =
  instr[5] == 0;

// Compute width of load or store access
function AccessWidth decodeAccessWidth(Bit#(32) instr);
  AccessWidth access;
  Bit#(2) w = funct3(instr)[1:0];
  access.w = w == 2;
  access.h = w == 1;
  access.b = w == 0;
  return access;
endfunction

// Is load signed or unsigned?
function Bool isUnsignedLoad(Bit#(32) instr) = unpack(funct3(instr)[2]);

// Does operation write to register file?
function Bool isRegFileWrite(Op op) =
     op.isAdd            || op.isSub
  || op.isSetIfLessThan  || op.isShiftLeft
  || op.isShiftRight     || op.isAnd
  || op.isOr             || op.isXor
  || op.isOpUI           || op.isJump
  || op.isCSR            || op.isMult
  || op.isMultH;

// ==============
// Loads & Stores
// ==============

// Compute byte-enable given access width
// and bottom two bits of address
function Bit#(4) genByteEnable(AccessWidth access, Bit#(2) a);
  return when(access.w, 4'b1111)
       | when(access.h, {a[1],a[1],~a[1],~a[1]})
       | when(access.b, {pack(a==3),pack(a==2),pack(a==1),pack(a==0)});
endfunction

// Align a write using access width
function Bit#(32) writeAlign(AccessWidth access, Bit#(32) x);
  return when(access.w, x)
       | when(access.h, {x[15:0], x[15:0]})
       | when(access.b, {x[7:0], x[7:0], x[7:0], x[7:0]});
endfunction

// Compute loaded word using access width,
// bottom two bits of load address,
// and a flag indicating whether load is unsigned or not
function Bit#(32) loadMux(Bit#(32) x, AccessWidth access,
                          Bit#(2) a, Bool isUnsigned);
  Bit#(8)  b = case (a) matches
                 0: x[7:0];
                 1: x[15:8];
                 2: x[23:16];
                 3: x[31:24];
               endcase;
  Bit#(16) h = a[1] == 0 ? x[15:0] : x[31:16];
  return when(access.w, x)
       | when(access.h, {isUnsigned ? 0 : signExtend(h[15]), h})
       | when(access.b, {isUnsigned ? 0 : signExtend(b[7]), b});
endfunction

// ============================================================================
// Interface
// ============================================================================

interface Core;
  interface DCacheClient  dcacheClient;
  interface MailboxClient mailboxClient;
  interface HostLinkCore  hostLinkCore;
  (* always_ready, always_enabled *)
  method Action setBoardId(BoardId id);
endinterface

// ============================================================================
// Pipeline 
// ============================================================================

// Diagram
// =======
//                                         +-----------+
//                         +==========+  +-| Run Queue |<--------+
//                         | Schedule |<-+ +-----------+         |
//                         |          |<-+ +--------------+      |
//                         +==========+  +-| Resume Queue |<-+   |
//                             ||          +--------------+  |   |
//                             \/                            |   |
//     +-----------+       +=======+                         |   |
//     | Instr Mem |<----->| Fetch |                         |   | 
//     +-----------+       +=======+                         |   |
//                             ||                            |   |
//                             \/                            |   |
//     +-----------+       +========+                        |   |
//  +->| Reg File  |<----->| Decode |                        |   |
//  |  +-----------+       +========+                        |   |
//  |                          ||                            |   |
//  |                          \/                            |   |
//  |                      +============+                    |   |
//  |                      | Execute    |                    |   |
//  |                      | or Suspend |---+                |   |
//  |                      +============+   |                |   |
//  |                          ||           |                |   |
//  |                          \/           |                |   |
//  |                      +============+   |                |   |
//  +----------------------| Write Back |--------------------+   |
//                         |            |------------------------+
//                         +============+   |        
//                             /\           |        
//                             ||           |        
//                         +========+       |  +---------------+
//                         | Resume |       +->| Suspend state |
//                         |        |<---------| per thread    |
//                         +========+          +---------------+

// Properties
// ==========
//
// Hazard-free: at most one instruction per thread in pipeline at any
// time.
//
// Non-blocking: if instruction accesses busy resource (e.g. memory)
// then it is retried later by requeueing into the run queue.
//
// Five high-level stages, but several are sub-pipelined.
//
// Loads and stores are suspended in the Execute stage.  The Resume
// stage waits for memory responses and queues up writeback &
// resumption requests for the Write Back stage.

(* synthesize *)
module mkCore#(CoreId myId) (Core);

  staticAssert(`LogThreadsPerCore >= 4, "Number of threads must be >= 16");

  // Number of threads
  Integer numThreads = 2 ** `LogThreadsPerCore;

  // Board id
  Wire#(BoardId) boardId <- mkDWire(?);

  // Global state
  // ------------

  // Ports
  OutPort#(DCacheReq)    dcacheReq    <- mkOutPort;
  InPort#(DCacheResp)    dcacheResp   <- mkInPort;
  OutPort#(HostLinkFlit) toHostPort   <- mkOutPort;
  InPort#(HostLinkFlit)  fromHostPort <- mkInPort;

  // Queue of runnable threads
  QueueInit runQueueInit;
  runQueueInit.size = 1;
  runQueueInit.file = Invalid;
  SizedQueue#(`LogThreadsPerCore, ThreadState) runQueue <-
    mkUGSizedQueueInit(runQueueInit);

  // Queue of suspended threads pending resumption
  SizedQueue#(`LogThreadsPerCore, ThreadState) resumeQueue <- mkUGSizedQueue;

  // Queue of writeback requests from threads pending resumption
  Queue#(Writeback) writebackQueue <- mkUGShiftQueue(QueueOptFmax);

  // Information about suspended threads
  BlockRam#(ThreadId, SuspendedThreadState) suspended <- mkBlockRam;

  // Instruction memory
  BlockRamOpts instrMemOpts = defaultBlockRamOpts;
  instrMemOpts.initFile = Valid("InstrMem");
  instrMemOpts.registerDataOut = False;
  BlockRam#(InstrIndex, Bit#(32)) instrMem <- mkBlockRamOpts(instrMemOpts);

  // Register file (duplicated to allow two reads per cycle)
  BlockRamOpts regFileOpts = defaultBlockRamOpts;
  BlockRam#(RegFileIndex, Bit#(32)) regFileA <- mkBlockRamOpts(regFileOpts);
  BlockRam#(RegFileIndex, Bit#(32)) regFileB <- mkBlockRamOpts(regFileOpts);

  // Mailbox
  MailboxClientUnit mailbox <- mkMailboxClientUnit(myId);

  // Connection to mailbox client wakeup queue
  InPort#(ThreadEventPair) wakeupPort <- mkInPort;
  connectUsing(mkUGShiftQueue1(QueueOptFmax),
                 mailbox.wakeup, wakeupPort.in);

  // Multiplier
  Mult#(33) mult <- mkSignedMult;

  // Pipeline stages
  Reg#(Bool)          fetch1Fire         <- mkDReg(False);
  Reg#(PipelineToken) fetch2Input        <- mkVReg;
  Reg#(PipelineToken) decode1Input       <- mkVReg;
  Reg#(PipelineToken) execute1Input      <- mkVReg;
  Reg#(PipelineToken) execute2Input      <- mkVReg;
  Reg#(PipelineToken) execute3Input      <- mkVReg;
  Reg#(Bool)          writebackFire      <- mkDReg(False);
  Reg#(PipelineToken) writebackInput     <- mkRegU;
 
  // Resume queue arbiter
  // --------------------

  // There is a conflict on the resume queue: both the execute stage
  // (hanlding the NewThread CSR) and the writeback stage (handling
  // thread resumptions) are trying to make a thread runnable.  This
  // arbiter resolves the conflict, giving priority to the writeback
  // stage.

  // This wire is from the writeback stage
  PulseWire resumeWire <- mkPulseWire;

  // These wires are from the execute stage
  PulseWire newThreadEnqWire <- mkPulseWire;
  Wire#(ThreadId) newThreadIdWire <- mkDWire(?);

  rule resumeQueueEnq (resumeWire || newThreadEnqWire);
    ThreadState newThread = ?;
    newThread.pc = 0;
    newThread.id = newThreadIdWire;
    newThread.msgLen = 0;
    resumeQueue.enq(resumeWire ? writebackQueue.dataOut.thread : newThread);
  endrule

  // Schedule stage
  // --------------

  // True if previous thread was taken from runQueue;
  // False if taken from resumeQueue
  Reg#(Bool) prevFromRunQueue <- mkReg(False);

  rule schedule1 (runQueue.canDeq || resumeQueue.canDeq);
    // Take next thread from runQueue or resumeQueue using fair merge
    if (resumeQueue.canDeq && (prevFromRunQueue || !runQueue.canDeq)) begin
      resumeQueue.deq;
      prevFromRunQueue <= False;
      fetch1Fire <= True;
    end else if (runQueue.canDeq) begin
      runQueue.deq;
      prevFromRunQueue <= True;
      fetch1Fire <= True;
    end
  endrule

  // Fetch stage
  // -----------

  rule fetch1 (fetch1Fire);
    // Obtain scheduled thread
    ThreadState next = prevFromRunQueue ? runQueue.dataOut
                                        : resumeQueue.dataOut;
    // Create a pipeline token to hold new instruction
    PipelineToken token = ?;
    token.thread = next;
    // Use thread's PC to fetch instruction
    instrMem.read(truncateLSB(next.pc));
    // Trigger second fetch sub-stage
    fetch2Input  <= token;
  endrule

  rule fetch2;
    PipelineToken token = fetch2Input;
    // Register instruction memory outputs
    token.instr = instrMem.dataOut;
    // Fetch operands from register files
    regFileA.read({token.thread.id, rs1(token.instr)});
    regFileB.read({token.thread.id, rs2(token.instr)});
    // Prepare mailbox operation
    if (isCSROp(token.instr))
      mailbox.prepare(token.thread.id);
    // Trigger next stage
    decode1Input <= token;
  endrule

  // Decode stage
  // ------------

  rule decode1;
    PipelineToken token = decode1Input;
    // Compute instruction's operation and type
    token.op = decodeOp(token.instr);
    token.instrType = decodeInstrType(token.instr);
    // Compute access width of load or store
    token.accessWidth = decodeAccessWidth(token.instr);
    // Compute instruction's immediate
    token.imm = decodeImm(token.instr, token.instrType);
    // CSR-immediate instructions not yet supported
    if (token.op.isCSR)
      myAssert(token.instr[14] == 0, "CSR-immediate instrs not supported");
    // Trigger second decode sub-stage
    execute1Input <= token;
  endrule

  // Execute stage
  // -------------

  rule execute1;
    PipelineToken token = execute1Input;
    // Save register values
    token.valA = regFileA.dataOut;
    token.valB = regFileB.dataOut;
    // Compute ALU's second operand
    token.aluB = isALUImm(token.instr) ? token.imm : token.valB;
    // Determine memory address for load or store
    token.memAddr = token.valA + token.imm;
    // Base of jump (could be PC or register)
    token.jumpBase = token.op.isJumpReg ?
                       truncate(token.valA) : token.thread.pc;
    // Mailbox send
    if (mailbox.canSend && token.op.csr.isSend)
      mailbox.send(token.thread.id, token.thread.msgLen,
                     unpack(truncate(token.valA)), token.thread.msgPtr);
    // Mailbox receive
    if (mailbox.canRecv && token.op.csr.isRecv)
      mailbox.recv;
    // Mailbox set message length
    if (token.op.csr.isSendLen)
      token.thread.msgLen = truncate(token.valA);
    // Mailbox set message pointer
    if (token.op.csr.isSendPtr)
      token.thread.msgPtr = byteAddrToMsgIndex(token.valA);
    // Mailbox scratchpad access
    token.isScratchpadAccess = token.memAddr[31:`LogScratchpadBytes] == 0;
    // Mailbox can-send / can-recv
    token.canSend = mailbox.canSend;
    token.canRecv = mailbox.canRecv;
    // Address for write-port of instrMem
    if (token.op.csr.isInstrAddr)
      token.thread.instrWriteIndex = truncate(token.valA);
    // Emit char to console (simulation only)
    `ifdef SIMULATE
    if (token.op.csr.isEmit) begin
      $display("Thread %d: 0x%x @ %d", {myId, token.thread.id},
                  token.valA, $time);
    end
    `endif
    // Multiplication
    Bit#(33) mulA = {token.op.isMultASigned ? token.valA[31] : 0, token.valA};
    Bit#(33) mulB = {token.op.isMultBSigned ? token.valB[31] : 0, token.valB};
    token.instrResult.mult = mult.mult(mulA, mulB);
    // Triger next stage
    execute2Input <= token;
  endrule

  rule execute2;
    PipelineToken token = execute2Input;
    InstrResult res = token.instrResult;
    // 33-bit addition/subtraction (result used for comparisons too)
    Bool ucmp = isUnsignedCmp(token.instr);
    let addA = {ucmp ? 1'b0 : token.valA[31], token.valA};
    let addB = {ucmp ? 1'b0 : token.aluB[31], token.aluB};
    res.add = (addA + (token.op.isAdd ? addB : ~addB)) +
                (token.op.isAdd ? 0 : 1);
    // Shift left
    res.shiftLeft = token.valA << token.aluB[4:0];
    // Shift right (both logical and arithmetic cases)
    Bit#(1) shiftExt = isArithShift(token.instr) ? token.valA[31] : 1'b0;
    Int#(33) shiftRes = unpack({shiftExt, token.valA}) >> token.aluB[4:0];
    res.shiftRight = truncate(pack(shiftRes));
    // Bitwise operations
    res.bitwise = when (token.op.isAnd, token.valA & token.aluB)
                | when (token.op.isOr,  token.valA | token.aluB)
                | when (token.op.isXor, token.valA ^ token.aluB);
    // Load upper immediate (+ PC)
    res.opui = token.imm + (addPCtoUI(token.instr) ?
                              zeroExtend(token.thread.pc) : 0);
    // Write to instruction memory
    if (token.op.csr.isInstr)
      instrMem.write(token.thread.instrWriteIndex, token.valA);
    // Load or store: send request to data cache or scratchpad
    Bool retry = False;
    Bool suspend = False;
    if (token.op.isLoad || token.op.isStore || token.op.isFence) begin
      // Determine data to write and assoicated byte-enables
      Bit#(32) writeData = writeAlign(token.accessWidth, token.valB);
      Bit#(4)  byteEn    = genByteEnable(token.accessWidth, token.memAddr[1:0]);
      if (token.isScratchpadAccess && !token.op.isFence) begin
        if (mailbox.scratchpadReq.canPut) begin
          // Prepare scratchpad request
          ScratchpadReq req;
          req.id = {truncate(myId), token.thread.id};
          req.isStore  = token.op.isStore;
          req.wordAddr = truncate(token.memAddr[31:2]);
          req.data     = writeData;
          req.byteEn   = byteEn;
          // Issue scratchpad request
          mailbox.scratchpadReq.put(req);
          suspend = True;
        end else
          retry = True;
      end else begin
        if (dcacheReq.canPut) begin
          // Prepare data cache request
          DCacheReq req;
          req.id = {truncate(myId), token.thread.id};
          req.cmd.isLoad = token.op.isLoad;
          req.cmd.isStore = token.op.isStore;
          req.cmd.isFlush = token.op.isFence;
          req.cmd.isFlushResp = False;
          req.addr = token.op.isFence ? 0 : token.memAddr;
          req.data = token.op.isFence ? 0 : writeData;
          req.byteEn = byteEn;
          // Issue data cache request
          dcacheReq.put(req);
          suspend = True;
        end else
          retry = True;
      end
    end
    // Allocate space for an incoming message in mailbox scratchpad
    if (token.op.csr.isAlloc) begin
      if (mailbox.allocateReq.canPut) begin
        // Prepare mailbox allocation request
        AllocReq req;
        req.id = {truncate(myId), token.thread.id};
        req.msgIndex = byteAddrToMsgIndex(token.valA);
        // Issue request
        mailbox.allocateReq.put(req);
      end else
        retry = True;
    end
    // WaitUntil CSR
    if (token.op.csr.isWaitUntil) begin
      mailbox.sleep(token.thread.id, truncate(token.valA));
      suspend = True;
    end
    // ToHost CSR
    if (token.op.csr.isToHost) begin
      if (toHostPort.canPut) begin
        HostLinkFlit flit;
        flit.coreId = myId;
        flit.isBroadcast = False;
        flit.cmd = cmdStdOut;
        flit.arg = token.valA;
        toHostPort.put(flit);
      end else
        retry = True;
    end
    // FromHost CSR
    if (token.op.csr.isFromHost) begin
      if (fromHostPort.canGet && fromHostPort.value.cmd == cmdStdIn)
        fromHostPort.get;
      else
        retry = True;
    end
    // NewThread CSR
    if (token.op.csr.isNewThread) begin
      newThreadIdWire <= truncate(token.valA);
      newThreadEnqWire.send;
      // Only succeed if the resumeQueue has not been claimed
      // by the writeback stage
      if (resumeWire) retry = True;
    end
    // Record state of suspended thread
    if (suspend) begin
      SuspendedThreadState susp;
      susp.thread = token.thread;
      susp.thread.pc = token.thread.pc + 4;
      susp.isLoad = token.op.isLoad;
      susp.isStore = token.op.isStore;
      susp.destReg = rd(token.instr);
      susp.loadSelector = token.memAddr[1:0];
      susp.accessWidth = token.accessWidth;
      susp.isUnsignedLoad = isUnsignedLoad(token.instr);
      suspended.write(token.thread.id, susp);
    end 
    // Compute next PC
    token.nextPC = token.thread.pc + (retry ? 0 : 4);
    // Compute jump/branch target
    token.targetPC = token.jumpBase + truncate(token.imm);
    token.targetPC[0] = token.op.isJumpReg ? 0 : token.targetPC[0];
    // CSR read
    res.csr =
        when(token.op.csr.isCanSend,  zeroExtend(pack(token.canSend)))
      | when(token.op.csr.isCanRecv,  zeroExtend(pack(token.canRecv)))
      | when(token.op.csr.isHartId,
               zeroExtend({pack(boardId), myId, token.thread.id}))
      | when(token.op.csr.isRecv,     mailbox.recvAddr)
      | when(token.op.csr.isFromHost, fromHostPort.value.arg);
    // Trigger next stage
    token.retry = retry;
    token.instrResult = res;
    if (! suspend) execute3Input <= token;
  endrule

  rule execute3;
    PipelineToken token = execute3Input;
    // Compute results of comparison
    InstrResult res = token.instrResult;
    Bool eq = res.add == 0;
    Bool lt = res.add[32] == 1;
    // Setup write to destination register
    Op op = token.op;
    token.writeVal =
        when(op.isAddOrSub,       res.add[31:0])
      | when(op.isSetIfLessThan,  lt ? 1 : 0)
      | when(op.isShiftLeft,      res.shiftLeft)
      | when(op.isShiftRight,     res.shiftRight)
      | when(op.isBitwise,        res.bitwise)
      | when(op.isOpUI,           res.opui)
      | when(op.isJump,           zeroExtend(token.nextPC))
      | when(op.isCSR,            res.csr)
      | when(op.isMult,           res.mult[31:0])
      | when(op.isMultH,          res.mult[63:32]);
    // Setup new PC
    Bool takeBranch =
         op.isJump
      || (op.isBranchEq               && eq)
      || (op.isBranchNotEq            && !eq)
      || (op.isBranchLessThan         && lt)
      || (op.isBranchGreaterOrEqualTo && !lt);
    token.thread.pc = takeBranch ? token.targetPC : token.nextPC;
    // Write to register file?
    token.writeRegFile =
      isRegFileWrite(token.op) && rd(token.instr) != 0 && !token.retry;
    // Trigger next stage
    writebackFire <= True;
    writebackInput <= token;
  endrule

  // Writeback stage
  // ---------------

  rule writeback;
    // Should we write to the register file?
    Bool writeToRegFile = False;
    // If so, what value and which destination?
    Bit#(32) writeVal = ?;
    RegFileIndex dest = ?;
    // Process an instruction from execute stage
    if (writebackFire) begin
      PipelineToken token = writebackInput;
      if (token.writeRegFile) begin
        writeToRegFile = True;
        writeVal = writebackInput.writeVal;
        dest = {token.thread.id, rd(token.instr)};
      end
      // Put thread back in the run queue
      runQueue.enq(token.thread);
    end 
    // Try to service a request from the writeback queue
    if (writebackQueue.canPeek && writebackQueue.canDeq) begin
      Writeback wb = writebackQueue.dataOut;
      // Can the thread be resumed?
      Bool resume = False;
      // If register file's write-port is not in use then 
      // a pending load result can be written back
      if (!writeToRegFile && wb.write) begin
        // Write to register file
        writeToRegFile = True;
        writeVal = wb.writeVal;
        dest = {wb.thread.id, wb.destReg};
        writebackQueue.deq;
        resume = True;
      end else if (!wb.write) begin
        writebackQueue.deq;
        resume = True;
      end
      // Put thread in the resume queue
      if (resume) resumeWire.send;
    end
    // Register file write
    if (writeToRegFile) begin
      regFileA.write(dest, writeVal);
      regFileB.write(dest, writeVal);
    end
  endrule

  // Thread resumption stage
  // -----------------------

  // Pipeline sub-stages for thread resumption
  Reg#(Bool)          resumeThread1Fire  <- mkDReg(False);
  Reg#(ResumeToken)   resumeThread1Input <- mkConfigRegU;
  Reg#(ResumeToken)   resumeThread2Input <- mkVReg;
  Reg#(ResumeToken)   resumeThread3Input <- mkVReg;
 
  rule resumeThread1 (resumeThread1Fire
                       || dcacheResp.canGet
                       || mailbox.scratchpadResp.canGet
                       || wakeupPort.canGet);
    ResumeToken token = resumeThread1Input;
    if (!resumeThread1Fire) begin
      if (dcacheResp.canGet) begin
        dcacheResp.get;
        token.id   = truncate(dcacheResp.value.id);
        token.data = dcacheResp.value.data;
      end else if (mailbox.scratchpadResp.canGet) begin
        mailbox.scratchpadResp.get;
        token.id   = truncate(mailbox.scratchpadResp.value.id);
        token.data = mailbox.scratchpadResp.value.data;
      end else if (wakeupPort.canGet) begin
        wakeupPort.get;
        token.id = truncate(wakeupPort.value.id);
      end
    end
    // Fetch info about suspended thread
    suspended.read(token.id);
    // Trigger next sub-stage
    resumeThread2Input <= token;
  endrule

  rule resumeThread2;
    // Trigger next sub-stage
    resumeThread3Input <= resumeThread2Input;
  endrule

  rule resumeThread3;
    let susp = suspended.dataOut;
    let token = resumeThread3Input;
    // Prepare request for writeback stage
    Writeback wb;
    wb.write = susp.isLoad && susp.destReg != 0;
    wb.thread = susp.thread;
    wb.thread.id = token.id;
    wb.destReg = susp.destReg;
    wb.writeVal = loadMux(token.data, susp.accessWidth,
                    susp.loadSelector, susp.isUnsignedLoad);
    if (writebackQueue.notFull) begin
      writebackQueue.enq(wb);
    end else begin
      // Retry if queue full
      resumeThread1Fire <= True;
      resumeThread1Input <= token;
    end
  endrule

  // Interface
  // ---------

  interface DCacheClient dcacheClient;
    interface Out dcacheReqOut = dcacheReq.out;
    interface In  dcacheRespIn = dcacheResp.in;
  endinterface

  interface MailboxClient mailboxClient = mailbox.client;

  interface HostLinkCore hostLinkCore;
    interface In fromHost = fromHostPort.in;
    interface Out toHost  = toHostPort.out;
  endinterface

  method Action setBoardId(BoardId id);
    boardId <= id;
  endmethod

endmodule

endpackage
