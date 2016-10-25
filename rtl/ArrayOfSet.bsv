// Copyright (c) Matthew Naylor

package ArrayOfSet;

// =============================================================================
// Imports
// =============================================================================

import BlockRam  :: *;
import Util      :: *;
import DReg      :: *;
import ConfigReg :: *;

// =============================================================================
// Interface
// =============================================================================

// The ArrayOfSet data structure is parameterised by the number of
// sets in the array and the max size of each set.

interface ArrayOfSet#(type logNumSets, type logSetSize);
  // Put an item into the set at the given array index
  method Action put(Bit#(logNumSets) index, Bit#(logSetSize) item);
  // Guard for above method
  method Bool canPut;
  // Remove any item from the set at the given array index
  method Action get(Bit#(logNumSets) index);
  // Was an item sucessfully removed? (That is, was the set non-empty?)
  // (Valid on 2nd cycle after call to "get")
  method Bool success;
  // The item removed
  // (Valid on 3rd cycle after call to "get")
  method Bit#(logSetSize) itemOut;
endinterface

// TIMING CONSTRAINT: when the "get" method is called, an item is not
// removed from the set until the 3rd cycle after the call.  This
// means that two "get" calls to the same set should always be
// seperated by three cycles, otherwise it will be possible to remove
// the same item several times (without having reinserted it).

// =============================================================================
// Implementation
// =============================================================================

module mkArrayOfSet (ArrayOfSet#(logNumSets, logSetSize))
  provisos (Log#(TExp#(logSetSize), logSetSize));

  // The array of sets is stored in a mixed-width block RAM with
  // a 1-bit write-port and a (2**logSetSize)-bit read-port.
  BlockRamOpts arrayMemOpts = defaultBlockRamOpts;
  arrayMemOpts.readDuringWrite = OldData;
  BlockRamTrueMixed#(
    // Read port
    Bit#(logNumSets), Bit#(TExp#(logSetSize)),
    // Write port
    Bit#(TAdd#(logNumSets, logSetSize)), Bit#(1)) arrayMem <-
      mkBlockRamTrueMixedOpts(arrayMemOpts);

  // Read pipeline state
  Reg#(Bit#(logNumSets)) readStage1Input <- mkVReg;
  Reg#(Bit#(logNumSets)) readStage2Input <- mkVReg;

  // Update stage state
  Reg#(Bool) doClearBit  <- mkDReg(False);
  Reg#(Bit#(logSetSize)) getItemReg <- mkConfigRegU;
  Reg#(Bit#(logNumSets)) clearIndexReg <- mkConfigRegU;

  // Wires
  Wire#(Bool) getSuccessWire <- mkDWire(False);
  Wire#(Bool) doSetBit <- mkDWire(False);
  Wire#(Bit#(logNumSets)) setIndexWire <- mkDWire(?);
  Wire#(Bit#(logSetSize)) setItemWire  <- mkDWire(?);

  // Rules
  // =====

  rule readStage1;
    // Trigger next stage
    readStage2Input <= readStage1Input;
  endrule

  rule readStage2;
    let index = readStage2Input;
    // Output of arrayMem available
    let set = arrayMem.dataOutA;
    let item = countZerosMSB(set);
    // If the set is non-empty, then the "get" is successful
    if (set != 0) begin
      getSuccessWire <= True;
      // Trigger update stage
      doClearBit <= True;
    end
    // Inputs to update stage
    getItemReg <= pack(item)[valueOf(logSetSize)-1:0];
    clearIndexReg <= index;
  endrule

  rule updateStage;
    if (doClearBit)
      arrayMem.putB(True, {clearIndexReg, getItemReg}, 0);
    else if (doSetBit) begin
      arrayMem.putB(True, {setIndexWire, setItemWire}, 1);
    end
  endrule

  // Methods
  // =======

  method Action put(Bit#(logNumSets) index, Bit#(logSetSize) item);
    doSetBit     <= True;
    setIndexWire <= index;
    setItemWire  <= item;
  endmethod

  method Bool canPut = !doClearBit;

  method Action get(Bit#(logNumSets) index);
    arrayMem.putA(False, index, ?);
    readStage1Input <= index;
  endmethod

  method Bool success = getSuccessWire;

  method Bit#(logSetSize) itemOut = getItemReg;

endmodule

endpackage
