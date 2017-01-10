// Copyright (c) Matthew Naylor

package Mult;

interface Mult#(numeric type n);
  method Bit#(TAdd#(n,n)) mult(Bit#(n) x, Bit#(n) y);
endinterface

`ifdef SIMULATE

module mkSignedMult (Mult#(n));

  method Bit#(TAdd#(n,n)) mult(Bit#(n) x, Bit#(n) y) =
    signExtend(x) * signExtend(y);

endmodule

`else

import "BVI" AlteraSignedMult =
  module mkSignedMult (Mult#(n));
    method res mult(dataa, datab);

    parameter WIDTH = valueOf(n);

    default_clock clk(CLK, (*unused*) clk_gate);
    default_reset no_reset;

    schedule (mult) C (mult);
  endmodule
                      
`endif
                   
endpackage
