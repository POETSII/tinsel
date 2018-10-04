// Copyright (c) Matthew Naylor

package Core;

// ============================================================================
// Imports
// ============================================================================

import Vector      :: *;
import FIFO        :: *;
import BlockRam    :: *;
import Queue       :: *;
import Assert      :: *;
import Util        :: *;
import DReg        :: *;
import DCache      :: *;
import ConfigReg   :: *;
import Interface   :: *;
import Mailbox     :: *;
import Globals     :: *;
import DebugLink   :: *;
import FPU         :: *;
import FPUOps      :: *;
import InstrMem    :: *;
import DCacheTypes :: *;

// ============================================================================
// Control/status registers (CSRs) supported
// ============================================================================

// Name        | CSR    | R/W | Function
// ----------- | ------ | --- | --------
// InstrAddr   | 0x800  | W   | Set address for instruction write
// Instr       | 0x801  | W   | Write to instruction memory
// Alloc       | 0x802  | W   | Alloc space for new message in scratchpad
// CanSend     | 0x803  | R   | 1 if can send, 0 otherwise
// HartId      | 0xf14  | R   | Globally unique hardware thread id
// CanRecv     | 0x805  | R   | 1 if can receive, 0 otherwise
// SendLen     | 0x806  | W   | Set message length for send
// SendPtr     | 0x807  | W   | Set message pointer for send
// Send        | 0x808  | W   | Send message to supplied destination
// Recv        | 0x809  | R   | Return pointer to message received
// WaitUntil   | 0x80a  | W   | Sleep until can-send or can-recv
// FromUart    | 0x80b  | R   | Try to read byte from DebugLink UART
// ToUart      | 0x80c  | RW  | Try to write byte to DebugLink UART
// NewThread   | 0x80d  | W   | Create new thread with the given id
// KillThread  | 0x80e  | W   | Kill the currently running thread
// Emit        | 0x80f  | W   | Emit char to console (simulation only)
// FFlag       | 0x001  | RW  | Floating-point accrued exception flags
// FRM         | 0x002  | RW  | Floating-point dynamic rounding mode
// FCSR        | 0x003  | RW  | Concatenation of FRM and FFlag
// Cycle       | 0xc00  | R   | 32-bit cycle counter
// Flush       | 0xc01  | R   | Cache line flush

// Currently, only the CSRRW instruction is supported for accessing CSRs.

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
  // Floating-point accrued exception flags
  Bit#(5) fpFlags;
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
// (Register file contains 32 integer registers 
// and 32 floating-point registers per thread)
typedef Bit#(TAdd#(`LogThreadsPerCore, 6)) RegFileIndex;

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
  Bool isWaitUntil;   Bool isFromUart;
  Bool isToUart;      Bool isNewThread;
  Bool isKillThread;  Bool isFFlag;
  Bool isFRM;         Bool isFCSR;
  Bool isCycle;       Bool isFlush;
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
  Bool isFPUOp;          Bool isFPAdd;
  Bool isFPMult;         Bool isFPMove;
  Bool isFPDiv;          Bool isFPCmp;
  Bool isFPConv;         Bool isFPMAdd;
  Bool isFPSign;
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

// Word access
AccessWidth wordAccess = AccessWidth { b: False, h: False, w: True };

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
  Bit#(1) destRegFile;     // Write result to int (0) or float (1) reg file?
} PipelineToken deriving (Bits);

// For each suspended thread, we have the following info
typedef struct {
  ThreadState thread;      // Thread state
  Bool isLoad;             // Is it waiting for a load?
  Bool isStore;            // Or a store?
  Bool isFPUOp;            // Or an FPU operation?
  Bool isWaitUntil;        // Or a wait-until operation?
  Bit#(6) destReg;         // Destination register for the result
  Bit#(2) loadSelector;    // Bottom two bits of load address
  AccessWidth accessWidth; // Access width of load (byte, half, word)
  Bool isUnsignedLoad;     // Sign-extension behaviour for load
} SuspendedThreadState deriving (Bits);

// For each suspended thread in the writeback queue, we have:
typedef struct {
  Bool write;
  ThreadState thread;
  Bit#(6) destReg;
  Bit#(32) writeVal;
} Writeback deriving (Bits);

// Token for thread-resumption pipeline
typedef struct {
  ThreadId id;
  Bit#(32) data;
  Bit#(5) fpFlags;
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
function Bit#(5) rs3(Bit#(32) instr)     = instr[31:27];
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
  t.isRType  = op == 'b01100       /* Arithmetic */
            || op[4:3] == 2'b10;   /* Floating-point */
  t.isIType  = op == 'b00100       /* Arithmetic-immediate */
            || op[4:1] == 'b0000   /* Loads */
            || op == 'b11001       /* JALR */
            || op == 'b00011       /* Fences */
            || op == 'b11100;      /* System */
  t.isSType  = op[4:1] == 'b0100;  /* Stores */
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
  ret.isLoad = instr[6:3] == 4'b0000;
  ret.isStore = instr[6:3] == 4'b0100;
  // Fence operation
  ret.isFence = op == 'b00011;
  // CSR read/write operation
  ret.isCSR = isCSROp(instr);
  Bit#(6) csrIndex = {instr[31:30], instr[23:20]};
  // Hardware thread id CSR
  ret.csr.isHartId = ret.isCSR && csrIndex == 'h34;
  // Instruction memory CSRs
  ret.csr.isInstrAddr = ret.isCSR && csrIndex == 'h20;
  ret.csr.isInstr = ret.isCSR && csrIndex == 'h21;
  // Mailbox CSR
  ret.csr.isAlloc        = ret.isCSR && csrIndex == 'h22;
  ret.csr.isCanSend      = ret.isCSR && csrIndex == 'h23;
  ret.csr.isCanRecv      = ret.isCSR && csrIndex == 'h25;
  ret.csr.isSendLen      = ret.isCSR && csrIndex == 'h26;
  ret.csr.isSendPtr      = ret.isCSR && csrIndex == 'h27;
  ret.csr.isSend         = ret.isCSR && csrIndex == 'h28;
  ret.csr.isRecv         = ret.isCSR && csrIndex == 'h29;
  ret.csr.isWaitUntil    = ret.isCSR && csrIndex == 'h2a;
  ret.csr.isFromUart     = ret.isCSR && csrIndex == 'h2b;
  ret.csr.isToUart       = ret.isCSR && csrIndex == 'h2c;
  ret.csr.isNewThread    = ret.isCSR && csrIndex == 'h2d;
  ret.csr.isKillThread   = ret.isCSR && csrIndex == 'h2e;
  `ifdef SIMULATE
  ret.csr.isEmit         = ret.isCSR && csrIndex == 'h2f;
  `endif
  // Floating-point CSR
  ret.csr.isFFlag        = ret.isCSR && csrIndex == 'h01;
  ret.csr.isFRM          = ret.isCSR && csrIndex == 'h02;
  ret.csr.isFCSR         = ret.isCSR && csrIndex == 'h03;
  // Cycle count CSR 
  ret.csr.isCycle        = ret.isCSR && csrIndex == 'h30;
  // Cache line flush CSR
  ret.csr.isFlush        = ret.isCSR && csrIndex == 'h31;
  // Floating-point operations
  ret.isFPMAdd = instr[6:4] == 3'b100;
  ret.isFPAdd  = instr[6:4] == 3'b101 && instr[31:28] == 4'b0000;
  ret.isFPMult = instr[6:4] == 3'b101 && instr[31:27] == 5'b00010;
  ret.isFPDiv  = instr[6:4] == 3'b101 && instr[31:27] == 5'b00011;
  ret.isFPMove = instr[6:4] == 3'b101 && instr[31:29] == 3'b111
                                      && instr[12]    == 0;
  ret.isFPCmp  = instr[6:4] == 3'b101 && instr[31:27] == 5'b10100;
  ret.isFPConv = instr[6:4] == 3'b101 && instr[31:29] == 3'b110;
  ret.isFPSign = instr[6:4] == 3'b101 && instr[31:27] == 5'b00100;
  ret.isFPUOp  = (instr[6:5] == 2'b10 && !ret.isFPMove && !ret.isFPSign) ||
                   ret.isMult || ret.isMultH;
  return ret;
endfunction

// Read the first operand from the integer reg file (0) or the
// floating-point reg file (1)?
function Bit#(1) rs1RegFile(Bit#(32) instr);
  // Is it a floating-point (FP) operation?
  Bool fp = instr[6:5] == 2'b10;
  // Assuming it's an FP operation, is it a fused operation with 3 operands?
  Bool fused = instr[4] == 0;
  // Assuming it's an FP operation, does it take its first operand
  // from the integer reg file?
  Bool intOperand = instr[31:30] == 2'b11 && instr[28] == 1;
  return fp && (fused || !intOperand) ? 1 : 0;
endfunction

// Read the second operand from the integer reg file (0) or the
// floating-point reg file (1)?
function Bit#(1) rs2RegFile(Bit#(32) instr);
  // Is it a floating-point (FP) operation?
  Bool fpOp = instr[6:5] == 2'b10;
  // Is it a floating-point store instruction?
  Bool fpStore = instr[6:2] == 5'b01001;
  return (fpOp || fpStore) ? 1 : 0;
endfunction

// Write result to integer reg file (0) or floating-point reg file (1)?
function Bit#(1) rdRegFile(Bit#(32) instr);
  // Is it a floating-point (FP) operation?
  Bool fpOp = instr[6:5] == 2'b10;
  // Is it a floating-point load instruction?
  Bool fpLoad = instr[6:2] == 5'b00001;
  // Assuming it's an FP operation, is it a fused operation with 3 operands?
  Bool fused = instr[4] == 0;
  // Assuming it's an FP operation, does it write its result to
  // the integer reg file?
  Bool intResult = instr[31:28] == 4'b1100 ||
                   instr[31:28] == 4'b1110 ||
                   instr[31:28] == 4'b1010;
  return (fpLoad || (fpOp && (fused || !intResult))) ? 1 : 0;
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
  || op.isCSR            || op.isFPMove
  || op.isFPSign;

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
  interface DCacheClient    dcacheClient;
  interface MailboxClient   mailboxClient;
  interface DebugLinkClient debugLinkClient;
  interface FPUClient       fpuClient;
  interface InstrMemClient  instrMemClient;

  // Each core can see its board id
  (* always_ready, always_enabled *)
  method Action setBoardId(BoardId id);

  // For idle-detection (see IdleDetector.bsv)
  method Bit#(1) incSent;
  method Bit#(1) incReceived;
  method Bool active;
  (* always_ready, always_enabled *)
  method Action idleDetectedStage1(Bool pulse);
  (* always_ready, always_enabled *)
  method Action idleDetectedStage2(Bool pulse);
  method Bool idleStage1Ack
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
  OutPort#(DCacheReq)     dcacheReq         <- mkOutPort;
  InPort#(DCacheResp)     dcacheResp        <- mkInPort;
  OutPort#(DebugLinkFlit) toDebugLinkPort   <- mkOutPort;
  InPort#(DebugLinkFlit)  fromDebugLinkPort <- mkInPort;
  OutPort#(FPUReq)        toFPUPort         <- mkOutPort;
  InPort#(FPUResp)        fromFPUPort       <- mkInPort;

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
  // Declared outside core since it may be shared
  // Wire-based interfaced used due to synthesis boundary
  Wire#(Bool)       instrMemWrite     <- mkDWire(False);
  Wire#(InstrIndex) instrMemReadAddr  <- mkDWire(0);
  Wire#(InstrIndex) instrMemWriteAddr <- mkDWire(0);
  Wire#(Bit#(32))   instrMemWriteData <- mkDWire(?);
  Wire#(Bit#(32))   instrMemReadData  <- mkDWire(?);

  // The schedule stage doesn't fire when thi
  // The first pipeline stage ('schedule') doesn't fire when this is high
  Wire#(Bool) stall <- mkDWire(False);

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

  // Pipeline stages
  Reg#(Bool)          fetch1Fire         <- mkDReg(False);
  Reg#(PipelineToken) fetch2Input        <- mkVReg;
  Reg#(PipelineToken) decode1Input       <- mkVReg;
  Reg#(PipelineToken) execute1Input      <- mkVReg;
  Reg#(PipelineToken) execute2Input      <- mkVReg;
  Reg#(PipelineToken) execute3Input      <- mkVReg;
  Reg#(Bool)          writebackFire      <- mkDReg(False);
  Reg#(PipelineToken) writebackInput     <- mkRegU;
 
  // Cycle counter
  // -------------

  // Cycle counter
  Reg#(Bit#(32)) cycleCounter <- mkConfigReg(0);

  // Update cycle counter
  rule updateCycleCounter;
    cycleCounter <= cycleCounter + 1;
  endrule

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

  // For idle detection (see IdleDetect.bsv)
  Reg#(Bit#(1)) incSentReg <- mkDReg(0);
  Reg#(Bit#(1)) incRecvReg <- mkDReg(0);

  // Schedule stage
  // --------------

  // True if previous thread was taken from runQueue;
  // False if taken from resumeQueue
  Reg#(Bool) prevFromRunQueue <- mkReg(False);

  rule schedule1 (!stall && (runQueue.canDeq || resumeQueue.canDeq));
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
    instrMemReadAddr <= truncateLSB(next.pc);
    // Trigger second fetch sub-stage
    fetch2Input  <= token;
  endrule

  rule fetch2;
    PipelineToken token = fetch2Input;
    // Register instruction memory outputs
    token.instr = instrMemReadData;
    // Does result go to integer or floating-point register file?
    token.destRegFile = rdRegFile(token.instr);
    // Fetch operands from integer or floating-point register file?
    Bit#(1) rfA = rs1RegFile(token.instr);
    Bit#(1) rfB = rs2RegFile(token.instr);
    // Determine registers to read
    Bit#(5) regA = rs1(token.instr);
    Bit#(5) regB = rs2(token.instr);
    // Read register file
    regFileA.read({token.thread.id, rfA, regA});
    regFileB.read({token.thread.id, rfB, regB});
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
    // Currently, only CSRRW is supported for accessing CSRs
    if (token.op.isCSR)
      myAssert(token.instr[14:12] == 3'b001,
                 "CSR instruction: only CSRRW supported");
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
    if (mailbox.canSend && token.op.csr.isSend) begin
      mailbox.send(token.thread.id, token.thread.msgLen,
                     unpack(truncate(token.valA)), token.thread.msgPtr);
      incSentReg <= True;
    end
    // Mailbox receive
    if (mailbox.canRecv && token.op.csr.isRecv) begin
      mailbox.recv;
      incRecvReg <= True;
    end
    // Mailbox set message length
    if (token.op.csr.isSendLen)
      token.thread.msgLen = truncate(token.valA);
    // Mailbox set message pointer
    if (token.op.csr.isSendPtr)
      token.thread.msgPtr = byteAddrToMsgIndex(token.valA);
    // Mailbox scratchpad access
    token.isScratchpadAccess = token.memAddr[31:`LogOffChipRAMBaseAddr] == 0;
    // Mailbox can-send / can-recv
    token.canSend = mailbox.canSend;
    token.canRecv = mailbox.canRecv;
    // Address for write-port of instrMem
    if (token.op.csr.isInstrAddr) begin
      token.thread.instrWriteIndex = truncate(token.valA);
      // Stall pipeline because instruction write will happen on next cycle
      stall <= True;
    end
    // Emit char to console (simulation only)
    `ifdef SIMULATE
    if (token.op.csr.isEmit) begin
      $display("Thread %d: 0x%x @ %d", {myId, token.thread.id},
                  token.valA, $time);
    end
    `endif
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
    if (token.op.csr.isInstr) begin
      instrMemWrite <= True;
      instrMemWriteAddr <= token.thread.instrWriteIndex;
      instrMemWriteData <= token.valA;
    end
    // Load or store: send request to data cache or scratchpad
    Bool retry = False;
    Bool suspend = False;
    if (token.op.isLoad || token.op.isStore || token.op.csr.isFlush) begin
      // Determine data to write and assoicated byte-enables
      Bit#(32) writeData = writeAlign(token.accessWidth, token.valB);
      Bit#(4)  byteEn    = genByteEnable(token.accessWidth, token.memAddr[1:0]);
      if (token.isScratchpadAccess && !token.op.csr.isFlush) begin
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
          // Line number and way to flush
          Bit#(`DCacheLogNumWays) way = truncate(token.valA);
          Bit#(`LogBytesPerLine) bottom = 0;
          Bit#(32) line = {truncate(token.valA[31:`DCacheLogNumWays]), bottom};
          // Prepare data cache request
          DCacheReq req;
          req.id = {truncate(myId), token.thread.id};
          req.cmd.isLoad = token.op.isLoad;
          req.cmd.isStore = token.op.isStore;
          req.cmd.isFlush = token.op.csr.isFlush;
          req.cmd.isFlushResp = False;
          req.addr = token.op.csr.isFlush ? zeroExtend(line) : token.memAddr;
          req.data = token.op.csr.isFlush ? zeroExtend(way) : writeData;
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
      if (mailbox.canSleep) begin
        mailbox.sleep(token.thread.id, truncate(token.valA));
        suspend = True;
      end else
        retry = True;
    end
    // ToUart CSR
    if (token.op.csr.isToUart) begin
      if (toDebugLinkPort.canPut) begin
        DebugLinkFlit flit;
        flit.coreId = myId;
        flit.isBroadcast = False;
        flit.threadId = token.thread.id;
        flit.cmd = cmdStdOut;
        flit.payload = truncate(token.valA);
        toDebugLinkPort.put(flit);
      end
    end
    // FromUart CSR
    Bool stdinValid = fromDebugLinkPort.canGet &&
                        fromDebugLinkPort.value.cmd == cmdStdIn &&
                          fromDebugLinkPort.value.threadId == token.thread.id;
    if (token.op.csr.isFromUart && stdinValid) fromDebugLinkPort.get;
    // NewThread CSR
    if (token.op.csr.isNewThread) begin
      newThreadIdWire <= truncate(token.valA);
      newThreadEnqWire.send;
      // Only succeed if the resumeQueue has not been claimed
      // by the writeback stage
      if (resumeWire) retry = True;
    end
    // KillThread CSR
    Bool killThread = False;
    if (token.op.csr.isKillThread) killThread = True;
    // FPU operation (including integer multiplication)
    if (token.op.isFPUOp) begin
      FPUReq req;
      req.id = {truncate(myId), token.thread.id};
      req.opcode = ?;
      if (token.op.isMult  || token.op.isMultH) req.opcode = IntMult;
      if (token.op.isFPAdd)   req.opcode = FPAddSub;
      if (token.op.isFPMult)  req.opcode = FPMult;
      if (token.op.isFPDiv)   req.opcode = FPDiv;
      if (token.op.isFPCmp)   req.opcode = FPCompare;
      if (token.op.isFPConv)  req.opcode =
                                token.instr[28] == 0 ? FPToInt : FPFromInt;
      req.in.lowerOrUpper = token.op.isMult ? 0 : 1;
      req.in.addOrSub = token.op.isFPAdd ? token.instr[27] : token.instr[2];
      req.in.cmpEQ = token.instr[13];
      req.in.cmpLT = token.instr[12];
      req.in.arg1 = {token.op.isMultASigned ? token.valA[31] : 0, token.valA};
      req.in.arg2 = {token.op.isMultBSigned ? token.valB[31] : 0, token.valB};
      if (toFPUPort.canPut) begin
        toFPUPort.put(req);
        suspend = True;
      end else
        retry = True;
    end
    // Record state of suspended thread
    if (suspend) begin
      SuspendedThreadState susp;
      susp.thread = token.thread;
      susp.thread.pc = token.thread.pc + 4;
      susp.isLoad = token.op.isLoad;
      susp.isStore = token.op.isStore;
      susp.isFPUOp = token.op.isFPUOp;
      susp.isWaitUntil = token.op.csr.isWaitUntil;
      susp.destReg = {token.destRegFile, rd(token.instr)};
      susp.loadSelector = token.memAddr[1:0];
      susp.accessWidth = susp.isFPUOp ? wordAccess : token.accessWidth;
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
      | when(token.op.csr.isFromUart,
               zeroExtend({pack(stdinValid), fromDebugLinkPort.value.payload}))
      | when(token.op.csr.isToUart,
               zeroExtend(pack(toDebugLinkPort.canPut)))
      | when(token.op.csr.isFFlag || token.op.csr.isFCSR, 
               zeroExtend(token.thread.fpFlags))
      | when(token.op.csr.isFRM, 0)
      | when(token.op.csr.isCycle, cycleCounter);
    // Floating-point CSR write
    if (token.op.csr.isFFlag || token.op.csr.isFCSR) begin
      token.thread.fpFlags = truncate(token.valA);
    end
    // Trigger next stage
    token.retry = retry;
    token.instrResult = res;
    if (!suspend && !killThread) execute3Input <= token;
  endrule

  rule execute3;
    PipelineToken token = execute3Input;
    // Compute results of comparison
    InstrResult res = token.instrResult;
    Bool eq = res.add == 0;
    Bool lt = res.add[32] == 1;
    // Floating point sign injection
    Bit#(1) fpSignBit = token.instr[13:12] == 0 ? token.valB[31] :
                          (token.instr[13:12] == 1 ? ~token.valB[31] :
                             (token.valA[31] ^ token.valB[31]));
    Bit#(32) fpSignResult = { fpSignBit, token.valA[30:0] };
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
      | when(op.isFPMove,         token.valA)
      | when(op.isFPSign,         fpSignResult);
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
      isRegFileWrite(token.op) && !token.retry &&
        {token.destRegFile, rd(token.instr)} != 0;
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
        dest = {token.thread.id, token.destRegFile, rd(token.instr)};
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
                       || wakeupPort.canGet
                       || fromFPUPort.canGet);
    ResumeToken token = resumeThread1Input;
    token.fpFlags = 0;
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
        token.data = zeroExtend(wakeupPort.value.wakeEvent);
      end else if (fromFPUPort.canGet) begin
        fromFPUPort.get;
        FPUOpOutput out = fromFPUPort.value.out;
        token.id = truncate(fromFPUPort.value.id);
        token.data = out.val;
        token.fpFlags = { out.invalid, out.divByZero, out.overflow,
                            out.underflow, out.inexact };
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
    wb.write = (susp.isLoad || susp.isFPUOp || susp.isWaitUntil) &&
                  susp.destReg != 0;
    wb.thread = susp.thread;
    wb.thread.id = token.id;
    wb.thread.fpFlags = wb.thread.fpFlags | token.fpFlags;
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

  interface DebugLinkClient debugLinkClient;
    interface In fromDebugLink = fromDebugLinkPort.in;
    interface Out toDebugLink  = toDebugLinkPort.out;
  endinterface

  interface FPUClient fpuClient;
    interface Out fpuReqOut = toFPUPort.out;
    interface In  fpuRespIn = fromFPUPort.in;
  endinterface

  interface InstrMemClient instrMemClient;
    method write     = instrMemWrite;
    method addr      = instrMemReadAddr | instrMemWriteAddr;
    method writeData = instrMemWriteData;
    method Action resp(Bit#(32) readData);
      instrMemReadData <= readData;
    endmethod
  endinterface

  method Action setBoardId(BoardId id);
    boardId <= id;
  endmethod

  method Bit#(1) incSent = incSentReg;
  method Bit#(1) incReceived = incRecvReg;
  method Bool active = mailbox.active;
  method Action idleDetectedStage1(Bool pulse);
    mailbox.idleDetectedStage1(pulse);
  endmethod
  method Action idleDetectedStage2(Bool pulse);
    mailbox.idleDetectedStage2(pulse);
  endmethod
  method Bool idleStage1Ack = mailbox.idleStage1Ack;

endmodule

endpackage
