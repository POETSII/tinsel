// Copyright (c) Matthew Naylor

package ArrayOfSet;

// =============================================================================
// Imports
// =============================================================================

import BlockRam     :: *;
import Util         :: *;
import DReg         :: *;
import ConfigReg    :: *;

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
  // Try to remove any item from the set at the given array index
  method Action tryGet(Bit#(logNumSets) index);
  // Can an item be successfully removed? (Is the set non-empty?)
  // (Valid on cycle after call to "tryGet")
  method Bool canGetEarly;
  // (Valid on 2nd cycle after call to "tryGet")
  method Bool canGet;
  // Remove item
  // (Must only be called on 2nd cycle after call to "tryGet")
  method Action get;
  // The item removed
  // (Valid on cycle after call to "get")
  method Bit#(logSetSize) itemOut;
endinterface

// NOTE: items are not removed from a set until the cycle after
// "get" is called, so take care not to remove the same item fromt a
// set multiple times.

// =============================================================================
// Implementation
// =============================================================================

module mkArrayOfSet (ArrayOfSet#(logNumSets, logSetSize))
  provisos (Log#(TExp#(logSetSize), logSetSize));

  // The array of sets is stored in a mixed-width block RAM with
  // a 1-bit write-port and a (2**logSetSize)-bit read-port.
  BlockRamOpts arrayMemOpts = defaultBlockRamOpts;
  arrayMemOpts.registerDataOut = False;
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
  Reg#(Bit#(TExp#(logSetSize))) dataReg <- mkConfigRegU;
  Reg#(Bit#(logSetSize)) getItemReg <- mkConfigRegU;
  Reg#(Bit#(logNumSets)) clearIndexReg <- mkConfigRegU;

  // Wires
  Wire#(Bool) canGetWire <- mkDWire(False);
  Wire#(Bool) canGetEarlyWire <- mkDWire(False);
  Wire#(Bool) doSetBit <- mkDWire(False);
  Wire#(Bit#(logNumSets)) setIndexWire <- mkDWire(?);
  Wire#(Bit#(logSetSize)) setItemWire  <- mkDWire(?);

  // Rules
  // =====

  rule latch;
    // Latch RAM output
    dataReg <= arrayMem.dataOutA;
  endrule

  rule readStage1;
    let index = readStage1Input;
    canGetEarlyWire <= arrayMem.dataOutA != 0;
    // Trigger next stage
    readStage2Input <= readStage1Input;
  endrule

  rule readStage2;
    let index = readStage2Input;
    let set = dataReg;
    let item = countZerosLSB(set);
    // If the set is non-empty, then the "get" can be called
    if (set != 0) canGetWire <= True;
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

  method Action tryGet(Bit#(logNumSets) index);
    arrayMem.putA(False, index, ?);
    readStage1Input <= index;
  endmethod

  method Action get;
    doClearBit <= True;
  endmethod

  method Bool canGet = canGetWire;
  method Bool canGetEarly = canGetEarlyWire;

  method Bit#(logSetSize) itemOut = getItemReg;

endmodule

endpackage
