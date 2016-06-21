package Util;

import DReg :: *;

// Useful function for constructing a mux with a one-hot select
function Bit#(n) when(Bool b, Bit#(n) x)
  provisos (Add#(a__, 1, n)) = signExtend(pack(b)) & x;

// A VReg is a register that can only be read
// on the clock cycle after it is written
module mkVReg (Reg#(t)) provisos (Bits#(t, twidth));
  Reg#(t) register <- mkRegU;
  Reg#(Bool) valid <- mkDReg(False);

  method Action _write (t val);
    register <= val;
    valid    <= True;
  endmethod

  method t _read if (valid) = register;
endmodule

endpackage
