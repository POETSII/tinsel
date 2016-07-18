// Copyright (c) Matthew Naylor

package Tinsel;

// ============================================================================
// Imports
// ============================================================================

import Vector   :: *;
import FIFO     :: *;
import BlockRam :: *;
import Queue    :: *;
import Util     :: *;
import DReg     :: *;
import Assert   :: *;

// ============================================================================
// Types
// ============================================================================

// An index to instruction memory
typedef Bit#(`LogInstrsPerCore) InstrIndex;

// A byte-address in instruction memory
typedef Bit#(TAdd#(`LogInstrsPerCore, 2)) InstrAddr;

// An index to data memory
typedef Bit#(`LogDataWordsPerCore) DataIndex;

// A byte-address in data memory
typedef Bit#(TAdd#(`LogDataWordsPerCore, 2)) DataAddr;

// Threads
typedef Bit#(`LogThreadsPerCore) ThreadId;
typedef struct {
  InstrAddr pc;
  ThreadId  id;
} Thread deriving (Bits);

// Register file index
// (Register file constains 32 registers per thread)
typedef Bit#(TAdd#(`LogThreadsPerCore, 5)) RegFileIndex;

// RV32I instruction type (one-hot encoding)
typedef struct {
  Bool isRType;  Bool isIType;
  Bool isSType;  Bool isSBType;
  Bool isUType;  Bool isUJType;
} InstrType deriving (Bits);

// Decoded operation
typedef struct {
  Bool isAdd;            Bool isSub;
  Bool isSetIfLessThan;  Bool isShiftLeft;
  Bool isShiftRight;     Bool isAnd;
  Bool isOr;             Bool isXor;
  Bool isOpUI;           Bool isJump;
  Bool isBranchEq;       Bool isBranchNotEq;
  Bool isBranchLessThan; Bool isBranchGreaterOrEqualTo;
  Bool isLoad;           Bool isStore;
  Bool isCSR;            Bool isBitwise;
  Bool isAddOrSub;
} Op deriving (Bits);

// Instruction result
typedef struct {
  Bit#(33) add;
  Bit#(32) shiftLeft; Bit#(32) shiftRight;
  Bit#(32) bitwise;   Bit#(32) opui;
  Bit#(32) load;      Bit#(32) csr;
} InstrResult deriving (Bits);

// Width of load or store access
typedef struct {
  // Byte, half-word, and full-word accesses
  Bool b; Bool h; Bool w;
} AccessWidth deriving (Bits);

// The type for data passed between each pipeline stage
typedef struct {
  Thread thread;           // Current thread
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
  Bit#(2) loadSelector;    // Byte offset of load address
  Op op;                   // Decoded operation
  InstrResult instrResult; // Instruction result
  InstrAddr targetPC;      // Next PC if branch taken
  InstrAddr nextPC;        // Next PC if branch not taken
} PipelineToken deriving (Bits);

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

// Decode operation
function Op decodeOp(Bit#(32) instr);
  Op ret;
  Bit#(5) op = instr[6:2];
  Bit#(3) minorOp = funct3(instr);
  // Arithmetic operations
  Bool isArithReg = op == 'b01100; // Second arg a register
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
  // Load or add-to upper immediate
  ret.isOpUI = op == 'b01101 || op == 'b00101;
  // Jump operations
  ret.isJump = op == 'b11011 || op == 'b11001;
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
  // CSR set/clear operations
  ret.isCSR = op == 'b11100 && minorOp[2:1] == 'b01;
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
  || op.isLoad           || op.isCSR;

// ==============
// Loads & Stores
// ==============

// Compute byte-enable given access width
// and bottom two bits of address
function Bit#(4) genByteEnable(AccessWidth access, Bit#(2) a);
  return when(access.w, 4'b1111)
       | when(access.h, {~a[1],~a[1],a[1],a[1]})
       | when(access.b, {pack(a==0),pack(a==1),pack(a==2),pack(a==3)});
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

// Currently I/O is limited to a single 32-bit output accessible to
// RISC-V programs via CSR instructions.

interface Tinsel;
  (* always_ready *)
  method Bit#(32) out;
endinterface

// ============================================================================
// Pipeline 
// ============================================================================

// Diagram
// =======
//                         +----------+    +-----------+
//                         | Schedule |<---| Run Queue |<---+
//                         +----------+    +-----------+    |
//                             ||                           |
//                             \/                           |
//     +-----------+       +-------+                        |
//     | Instr Mem |<----->| Fetch |                        | 
//     +-----------+       +-------+                        |
//                             ||                           |
//                             \/                           |
//     +-----------+       +--------+                       |
//  +->| Reg File  |<----->| Decode |                       |
//  |  +-----------+       +--------+                       |
//  |                          ||                           |
//  |                          \/                           |
//  |                      +---------+                      |
//  |                      | Execute |                      |
//  |                      +---------+                      |
//  |                          ||                           |
//  |                          \/                           |
//  |                      +------------+                   |
//  +----------------------| Write Back |-------------------+
//                         +------------+

// Properties
// ==========
//
// Hazard-free: at most one instruction per thread in pipeline at any
// time.
//
// Non-blocking: self-contained core with no I/O or resource sharing
// with other cores.
//
// Five high-level stages, but several are sub-pipelined.

module tinselCore (Tinsel);

  staticAssert(`LogThreadsPerCore >= 4, "Number of threads must be >= 16");

  // Number of threads
  Integer numThreads = 2 ** `LogThreadsPerCore;

  // Global state
  // ------------

  // Queue of runnable threads
  QueueInit runQueueInit;
  runQueueInit.size = numThreads;
  runQueueInit.file = Valid("RunQueue");
  SizedQueue#(`LogThreadsPerCore, Thread) runQueue <-
    mkSizedQueueInit(runQueueInit);

  // Instruction memory
  BlockRamOpts instrMemOpts = defaultBlockRamOpts;
  instrMemOpts.initFile = Valid("InstrMem");
  BlockRam#(InstrIndex, Bit#(32)) instrMem <- mkBlockRamOpts(instrMemOpts);

  // Register file (duplicated to allow two reads per cycle)
  BlockRamOpts regFileOpts = defaultBlockRamOpts;
  BlockRam#(RegFileIndex, Bit#(32)) regFileA <- mkBlockRamOpts(regFileOpts);
  BlockRam#(RegFileIndex, Bit#(32)) regFileB <- mkBlockRamOpts(regFileOpts);

  // Data memory
  BlockRamOpts dataMemOpts = defaultBlockRamOpts;
  dataMemOpts.initFile = Valid("DataMem");
  BlockRamBE#(DataIndex, Bit#(32)) dataMem <- mkBlockRamBEOpts(dataMemOpts);

  // Pipeline stages
  Reg#(Bool)          fetch1Fire     <- mkDReg(False);
  Reg#(PipelineToken) fetch2Input    <- mkVReg;
  Reg#(PipelineToken) decode1Input   <- mkVReg;
  Reg#(PipelineToken) decode2Input   <- mkVReg;
  Reg#(PipelineToken) execute1Input  <- mkVReg;
  Reg#(PipelineToken) execute2Input  <- mkVReg;
  Reg#(PipelineToken) execute3Input  <- mkVReg;
  Reg#(PipelineToken) writebackInput <- mkVReg;
  
  // This is (currently) the only output from the core
  Reg#(Bit#(32)) emitReg <- mkReg(0);

  // Schedule stage
  // --------------

  rule schedule1;
    // Request next thread from run queue
    runQueue.deq;
    // Trigger next stage
    fetch1Fire <= True;
  endrule

  // Fetch stage
  // -----------

  rule fetch1 (fetch1Fire);
    // Obtain scheduled thread
    Thread next = runQueue.dataOut;
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
    // Trigger next stage
    decode1Input <= token;
  endrule

  // Decode stage
  // ------------

  rule decode1;
    PipelineToken token = decode1Input;
    // Remember instruction memory outputs
    token.instr = instrMem.dataOut;
    // Fetch operands from register files
    regFileA.read({token.thread.id, rs1(token.instr)});
    regFileB.read({token.thread.id, rs2(token.instr)});
    // Compute instruction's operation and type
    token.op = decodeOp(token.instr);
    token.instrType = decodeInstrType(token.instr);
    // Compute access width of load or store
    token.accessWidth = decodeAccessWidth(token.instr);
    // Trigger second decode sub-stage
    decode2Input <= token;
  endrule

  rule decode2;
    PipelineToken token = decode2Input;
    // Compute instruction's immediate
    token.imm = decodeImm(token.instr, token.instrType);
    // Trigger next stage
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
    // Initiate load from data memory
    DataAddr loadAddr = truncate(token.valA + immI(token.instr));
    if (token.op.isLoad)
      dataMem.read(truncateLSB(loadAddr));
    token.loadSelector = loadAddr[1:0];
    // Perform store to data memory
    DataAddr storeAddr = truncate(token.valA + immS(token.instr));
    Bit#(4) byteEn = genByteEnable(token.accessWidth, storeAddr[1:0]);
    Bit#(32) writeData = writeAlign(token.accessWidth, token.valB);
    if (token.op.isStore)
      dataMem.write(truncateLSB(storeAddr), writeData, byteEn);
    // Triger next stage
    execute2Input <= token;
  endrule

  rule execute2;
    PipelineToken token = execute2Input;
    InstrResult res = ?;
    // 33-bit addition/subtraction (result used for comparisons too)
    Bool ucmp = isUnsignedCmp(token.instr);
    let addA = {ucmp ? 1'b0 : token.valA[31], token.valA};
    let addB = {ucmp ? 1'b0 : token.valB[31], token.aluB};
    res.add = (addA + (token.op.isAdd ? addB : ~addB)) +
                (token.op.isAdd ? 0 : 1);
    // Shift left
    res.shiftLeft = token.valA << token.aluB[4:0];
    // Shift right (both logical and arithmetic cases)
    Bit#(1) shiftExt = isArithShift(token.instr) ? token.valA[31] : 1'b0;
    res.shiftRight = truncate({shiftExt, token.valA} >> token.aluB[4:0]);
    // Bitwise operations
    res.bitwise = when (token.op.isAnd, token.valA & token.aluB)
                | when (token.op.isOr,  token.valA | token.aluB)
                | when (token.op.isXor, token.valA ^ token.aluB);
    // Load upper immediate (+ PC)
    res.opui = token.imm + (addPCtoUI(token.instr) ?
                              zeroExtend(token.thread.pc) : 0);
    // Compute next PC
    token.nextPC = token.thread.pc + 4;
    // Compute jump/branch target
    token.targetPC = truncate(zeroExtend(token.thread.pc) + token.imm);
    // CSR read
    res.csr = zeroExtend(token.thread.id);
    // CSR set/clear bits
    Bool csrClear = unpack(token.instr[12]);
    if (token.op.isCSR)
      emitReg <= csrClear ? (emitReg & ~token.valA)
                          : (emitReg | token.valA);
    // Trigger next stage
    token.instrResult = res;
    execute3Input <= token;
  endrule

  rule execute3;
    PipelineToken token = execute3Input;
    // Compute results of comparison
    InstrResult res = token.instrResult;
    Bool eq = res.add == 0;
    Bool lt = res.add[32] == 1;
    // Determine result of load
    token.instrResult.load =
      loadMux(dataMem.dataOut, token.accessWidth,
                token.loadSelector, isUnsignedLoad(token.instr));
    // Setup write to destination register
    Op op = token.op;
    token.writeVal =
        when(op.isAddOrSub,      res.add[31:0])
      | when(op.isSetIfLessThan, lt ? 1 : 0)
      | when(op.isShiftLeft,     res.shiftLeft)
      | when(op.isShiftRight,    res.shiftRight)
      | when(op.isBitwise,       res.bitwise)
      | when(op.isOpUI,          res.opui)
      | when(op.isJump,          zeroExtend(token.nextPC))
      | when(op.isCSR,           res.csr);
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
      isRegFileWrite(token.op) && rd(token.instr) != 0;
    // Trigger next stage
    writebackInput <= token;
  endrule

  // Writeback stage
  // ---------------

  rule writeback;
    PipelineToken token = writebackInput;
    // Register file write
    Bit#(32) writeVal = token.op.isLoad ?
                          token.instrResult.load : token.writeVal;
    if (token.writeRegFile) begin
      regFileA.write({token.thread.id, rd(token.instr)}, writeVal);
      regFileB.write({token.thread.id, rd(token.instr)}, writeVal);
    end
    // Put thread back in the run queue
    runQueue.enq(token.thread);
  endrule

  method out = emitReg;
endmodule

// ============================================================================
// Top-level core for simulation
// ============================================================================

module tinselCoreSim ();
  Tinsel tinsel <- tinselCore;
  rule display;
    $display($time, ": ", tinsel.out);
  endrule
endmodule

endpackage
