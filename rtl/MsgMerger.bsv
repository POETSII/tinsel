// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) Matthew Naylor

package MsgMerger;

import Util      :: *;
import Globals   :: *;
import Interface :: *;
import Queue     :: *;

// Fair merge two message ports
module mkMsgMerger#(Out#(Msg) left, Out#(Msg) right) (Out#(Msg));

  // Ports
  InPort#(Msg) leftIn <- mkInPort;
  InPort#(Msg) rightIn <- mkInPort;
  OutPort#(Msg) outPort <- mkOutPort;

  connectUsing(mkUGShiftQueue1(QueueOptFmax), left, leftIn.in);
  connectUsing(mkUGShiftQueue1(QueueOptFmax), right, rightIn.in);

  // State
  Reg#(Bool) prevChoiceWasLeft <- mkReg(False);

  // Rules
  rule merge (outPort.canPut);
    Bool chooseRight = 
        rightIn.canGet &&
          (!leftIn.canGet || prevChoiceWasLeft);
    // Consume input
    if (chooseRight) begin
      if (rightIn.canGet) begin
        rightIn.get;
        outPort.put(rightIn.value);
        prevChoiceWasLeft <= False;
      end
    end else if (leftIn.canGet) begin
      leftIn.get;
      outPort.put(leftIn.value);
      prevChoiceWasLeft <= True;
    end
  endrule

  // Interface
  return outPort.out;

endmodule

endpackage
