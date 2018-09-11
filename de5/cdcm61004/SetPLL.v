module SetPLL (
  input  wire clk,  // 50 Mhz
  input  wire reset_n,
  output wire i2c_scl,
  inout  wire i2c_sda,
  output wire conf_ready
);

reg [7:0] counter = 0;
reg write_trigger = 0;

always @ (posedge clk) begin
  if (reset_n == 0) begin
    counter <= 0;
    write_trigger <= 0;
  end else begin
    if (counter < 32)
      counter <= counter+1;
    else if (counter == 32) begin
      counter <= counter+1;
      write_trigger <= 1;
    end else
      write_trigger <= 0;
  end
end

// SFP 1G REF CLK: diabled
// SATA CLK: 625 MHz
wire [11:0] val = 12'd193; 

ext_pll_ctrl ext_pll_ctrl_inst
(
    // system input
    .osc_50(clk),                
    .rstn(reset_n),
    // device 1
    .clk1_set_wr(val[3:0]),
    // device 2
    .clk2_set_wr(val[7:4]),
    // device 3
    .clk3_set_wr(val[11:8]),
    // setting trigger
    .conf_wr(write_trigger),
    .conf_rd(0),
    // status 
    .conf_ready(conf_ready),
    // 2-wire interface 
    .max_sclk(i2c_scl),
    .max_sdat(i2c_sda)
);


endmodule
