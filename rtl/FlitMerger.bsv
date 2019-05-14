// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package FlitMerger;

import Util      :: *;
import Globals   :: *;
import Interface :: *;
import Queue     :: *;

typedef enum {NotLocked, LockLeft, LockRight} Lock deriving (Bits, Eq);

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
  Reg#(Lock) lock <- mkReg(NotLocked);

  // Rules
  rule merge (outPort.canPut);
    Bool chooseRight = 
      lock == LockRight ||
        (lock == NotLocked &&
           rightIn.canGet &&
             (!leftIn.canGet || prevChoiceWasLeft));
    // Consume input
    if (chooseRight) begin
      if (rightIn.canGet) begin
        rightIn.get;
        outPort.put(rightIn.value);
        lock <= rightIn.value.notFinalFlit ? LockRight : NotLocked;
        prevChoiceWasLeft <= False;
      end
    end else if (leftIn.canGet) begin
      leftIn.get;
      outPort.put(leftIn.value);
      lock <= leftIn.value.notFinalFlit ? LockLeft : NotLocked;
      prevChoiceWasLeft <= True;
    end
  endrule

  // Interface
  return outPort.out;

endmodule

endpackage
