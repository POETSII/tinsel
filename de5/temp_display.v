// Copyright (c) 2017 Matthew Naylor
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

module temp_display(
  input  wire clk_50mhz,
  input  wire temp_valid,
  input  wire [7:0] temp_val,

  output wire temp_en,
  output wire temp_clear,
  output wire [6:0] HEX0_D,
  output wire HEX0_DP,
  output wire [6:0] HEX1_D,
  output wire HEX1_DP,
  output wire [7:0] sample
);

// Seven segment displays
// ----------------------

// Value displayed on the seven segs
reg [7:0] display_reg = 0;
assign sample = display_reg;

// Decimal points (active low)
assign HEX0_DP = 1;
assign HEX1_DP = 1;

// Convert 4-bit value in hex to seven segments
function [7:0] hexLEDs;
  input [3:0] nibble; 
  begin
    case (nibble)
      4'h0: hexLEDs = 7'b0111111;
      4'h1: hexLEDs = 7'b0000110;
      4'h2: hexLEDs = 7'b1011011;
      4'h3: hexLEDs = 7'b1001111;
      4'h4: hexLEDs = 7'b1100110;
      4'h5: hexLEDs = 7'b1101101;
      4'h6: hexLEDs = 7'b1111101;
      4'h7: hexLEDs = 7'b0000111;
      4'h8: hexLEDs = 7'b1111111;
      4'h9: hexLEDs = 7'b1100111;
      4'ha: hexLEDs = 7'b1110111;
      4'hb: hexLEDs = 7'b1111100;
      4'hc: hexLEDs = 7'b1011000;
      4'hd: hexLEDs = 7'b1011110;
      4'he: hexLEDs = 7'b1111001;
      4'hf: hexLEDs = 7'b1110001;
    endcase
  end
endfunction

assign HEX0_D = ~hexLEDs(display_reg[3:0]);
assign HEX1_D = ~hexLEDs(display_reg[7:4]);

// State machine
// -------------

// To get a new reading from the temperature sensor, we must assert
// the clear signal for at least one tick of its internal ADC clock.
// In QSys, we've set the internal divider at 80, so the clear signal
// must last at least 80 clock cycles.  We use 1024 cycles to be safe.
// We poll the sensor 1-2 times a second, and disable the sensor at
// other times to reduce power.

reg temp_clear_reg = 0;     // Clear sensor, allowing a new reading
reg temp_en_reg = 0;        // Enable sensor
reg [25:0] poll_timer = 0;  // Timer

assign temp_clear = temp_clear_reg;
assign temp_en = temp_en_reg;

always @(posedge clk_50mhz) begin
  if (poll_timer == 0) begin
    temp_en_reg <= 1;
    temp_clear_reg <= 1;
    poll_timer <= poll_timer+1;
  end else if (poll_timer == 1024) begin
    if (temp_valid) begin
      temp_clear_reg <= 1;
      temp_en_reg <= 0;
      poll_timer <= poll_timer+1;
      display_reg <= temp_val;
    end else
      temp_clear_reg <= 0;
  end else
    poll_timer <= poll_timer+1;
end

endmodule 
