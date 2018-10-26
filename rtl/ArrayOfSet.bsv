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
  // (Valid on the cycle after call to "tryGet")
  method Bool canGet;
  // Remove item
  // (Only call when canGet is true)
  method Action get;
  // The item removed
  // (Valid on the cycle after call to "get")
  method Bit#(logSetSize) itemOut;
endinterface

// =============================================================================
// Implementation
// =============================================================================

module mkArrayOfSet (ArrayOfSet#(logNumSets, logSetSize))
  provisos (Log#(TExp#(logSetSize), logSetSize));

  // The array of sets is stored in a mixed-width block RAM with
  // a 1-bit write-port and a (2**logSetSize)-bit read-port.
  BlockRamOpts arrayMemOpts = defaultBlockRamOpts;
  arrayMemOpts.readDuringWrite = OldData;
  arrayMemOpts.registerDataOut = False;
  BlockRamTrueMixed#(
    // Read port
    Bit#(logNumSets), Bit#(TExp#(logSetSize)),
    // Write port
    Bit#(TAdd#(logNumSets, logSetSize)), Bit#(1)) arrayMem <-
      mkBlockRamTrueMixedOpts(arrayMemOpts);

  // Read pipeline state
  Reg#(Bool) readStage1Fire <- mkDReg(False);
  Reg#(Bit#(logNumSets)) readStage1Input <- mkConfigRegU;
  Reg#(Bit#(logSetSize)) getItemReg <- mkConfigRegU;

  // Wires
  Wire#(Bool) canGetWire <- mkDWire(False);
  Wire#(Bool) doGetWire <- mkDWire(False);
  Wire#(Bool) doSetBit <- mkDWire(False);
  Wire#(Bit#(logNumSets)) setIndexWire <- mkDWire(?);
  Wire#(Bit#(logSetSize)) setItemWire  <- mkDWire(?);
  Wire#(Bit#(logSetSize)) getItemWire <- mkDWire(?);

  // Rules
  // =====

  rule readStage1 (readStage1Fire);
    let index = readStage1Input;
    // Output of arrayMem available
    let set = arrayMem.dataOutA;
    let item = countZerosLSB(set);
    // If the set is non-empty, then an item will be removed
    if (set != 0) canGetWire <= True;
    // Inputs to update stage
    let itemIndex = pack(item)[valueOf(logSetSize)-1:0];
    getItemWire <= itemIndex;
    getItemReg <= itemIndex;
  endrule

  rule updateStage;
    if (doGetWire)
      arrayMem.putB(True, {readStage1Input, getItemWire}, 0);
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

  method Bool canPut = !doGetWire;

  method Action tryGet(Bit#(logNumSets) index);
    // Are we currently writing the the index in question?
    Bool hazard = doGetWire && readStage1Input == index;
    // Only attempt tryGet if there's no hazard
    if (!hazard) begin
      arrayMem.putA(False, index, ?);
      readStage1Input <= index;
      readStage1Fire <= True;
    end
  endmethod

  method Bool canGet = canGetWire;

  method Action get;
    doGetWire <= True;
  endmethod

  method Bit#(logSetSize) itemOut = getItemReg;

endmodule

endpackage
