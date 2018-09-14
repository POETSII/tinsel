// Copyright (c) 2017 Matthew Naylor
// Copyright (c) 2016-2017 Alex Forencich
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

// We reuse some of the (free) PHY logic for the DE5 from
//   https://github.com/alexforencich/verilog-ethernet/

module DE5(
  input wire [3:0] BUTTON,

  output wire CLOCK_SCL,
  inout wire CLOCK_SDA,

  input wire CPU_RESET_n,

  input wire OSC_50_B3B,
  input wire OSC_50_B3D,
  input wire OSC_50_B4A,
  input wire OSC_50_B4D,
  input wire OSC_50_B7A,
  input wire OSC_50_B7D,
  input wire OSC_50_B8A,
  input wire OSC_50_B8D,

  output wire [6:0] HEX0_D,
  output wire HEX0_DP,

  output wire [6:0] HEX1_D,
  output wire HEX1_DP,

  input wire PCIE_PERST_n,
  input wire PCIE_REFCLK_p,
  input wire [7:0] PCIE_RX_p,
  input wire PCIE_SMBCLK,
  inout wire PCIE_SMBDAT,
  output wire [7:0] PCIE_TX_p,
  output wire PCIE_WAKE_n,

  input wire SFP_REFCLK_p,

  input wire SFPA_LOS,
  input wire SFPA_MOD0_PRSNT_n,
  output wire SFPA_MOD1_SCL,
  inout wire SFPA_MOD2_SDA,
  output wire [1:0] SFPA_RATESEL,
  input wire SFPA_RX_p,
  output wire SFPA_TXDISABLE,
  input wire SFPA_TXFAULT,
  output wire SFPA_TX_p,

  input wire SFPB_LOS,
  input wire SFPB_MOD0_PRSNT_n,
  output wire SFPB_MOD1_SCL,
  inout wire SFPB_MOD2_SDA,
  output wire [1:0] SFPB_RATESEL,
  input wire SFPB_RX_p,
  output wire SFPB_TXDISABLE,
  input wire SFPB_TXFAULT,
  output wire SFPB_TX_p,

  input wire SFPC_LOS,
  input wire SFPC_MOD0_PRSNT_n,
  output wire SFPC_MOD1_SCL,
  inout wire SFPC_MOD2_SDA,
  output wire [1:0] SFPC_RATESEL,
  input wire SFPC_RX_p,
  output wire SFPC_TXDISABLE,
  input wire SFPC_TXFAULT,
  output wire SFPC_TX_p,

  input wire SFPD_LOS,
  input wire SFPD_MOD0_PRSNT_n,
  output wire SFPD_MOD1_SCL,
  inout wire SFPD_MOD2_SDA,
  output wire [1:0] SFPD_RATESEL,
  input wire SFPD_RX_p,
  output wire SFPD_TXDISABLE,
  input wire SFPD_TXFAULT,
  output wire SFPD_TX_p,

  output wire [3:0] LED,
  input wire [3:0] SW
);

wire clk_50mhz = OSC_50_B7A;
wire rst_50mhz = 0;

wire clk_156mhz;
wire phy_pll_locked;
wire soft_reset;

wire [7:0] ts_out;
wire ts_done;
wire ts_enable;
wire ts_clear;

wire si570_scl_i;
wire si570_scl_o;
wire si570_scl_t;
wire si570_sda_i;
wire si570_sda_o;
wire si570_sda_t;

assign si570_sda_i = CLOCK_SDA;
assign CLOCK_SDA = si570_sda_t ? 1'bz : si570_sda_o;
assign si570_scl_i = CLOCK_SCL;
assign CLOCK_SCL = si570_scl_t ? 1'bz : si570_scl_o;

wire [6:0] si570_i2c_cmd_address;
wire si570_i2c_cmd_start;
wire si570_i2c_cmd_read;
wire si570_i2c_cmd_write;
wire si570_i2c_cmd_write_multiple;
wire si570_i2c_cmd_stop;
wire si570_i2c_cmd_valid;
wire si570_i2c_cmd_ready;

wire [7:0] si570_i2c_data;
wire si570_i2c_data_valid;
wire si570_i2c_data_ready;
wire si570_i2c_data_last;

si570_i2c_init
si570_i2c_init_inst (
    .clk(clk_50mhz),
    .rst(rst_50mhz),
    .cmd_address(si570_i2c_cmd_address),
    .cmd_start(si570_i2c_cmd_start),
    .cmd_read(si570_i2c_cmd_read),
    .cmd_write(si570_i2c_cmd_write),
    .cmd_write_multiple(si570_i2c_cmd_write_multiple),
    .cmd_stop(si570_i2c_cmd_stop),
    .cmd_valid(si570_i2c_cmd_valid),
    .cmd_ready(si570_i2c_cmd_ready),
    .data_out(si570_i2c_data),
    .data_out_valid(si570_i2c_data_valid),
    .data_out_ready(si570_i2c_data_ready),
    .data_out_last(si570_i2c_data_last),
    .busy(),
    .start(1)
);

i2c_master
si570_i2c_master_inst (
    .clk(clk_50mhz),
    .rst(rst_50mhz),
    .cmd_address(si570_i2c_cmd_address),
    .cmd_start(si570_i2c_cmd_start),
    .cmd_read(si570_i2c_cmd_read),
    .cmd_write(si570_i2c_cmd_write),
    .cmd_write_multiple(si570_i2c_cmd_write_multiple),
    .cmd_stop(si570_i2c_cmd_stop),
    .cmd_valid(si570_i2c_cmd_valid),
    .cmd_ready(si570_i2c_cmd_ready),
    .data_in(si570_i2c_data),
    .data_in_valid(si570_i2c_data_valid),
    .data_in_ready(si570_i2c_data_ready),
    .data_in_last(si570_i2c_data_last),
    .data_out(),
    .data_out_valid(),
    .data_out_ready(1),
    .data_out_last(),
    .scl_i(si570_scl_i),
    .scl_o(si570_scl_o),
    .scl_t(si570_scl_t),
    .sda_i(si570_sda_i),
    .sda_o(si570_sda_o),
    .sda_t(si570_sda_t),
    .busy(),
    .bus_control(),
    .bus_active(),
    .missed_ack(),
    .prescale(312),
    .stop_on_idle(1)
);

wire [71:0] sfp_a_tx_dc;
wire [71:0] sfp_a_rx_dc;
wire [71:0] sfp_b_tx_dc;
wire [71:0] sfp_b_rx_dc;
wire [71:0] sfp_c_tx_dc;
wire [71:0] sfp_c_rx_dc;
wire [71:0] sfp_d_tx_dc;
wire [71:0] sfp_d_rx_dc;

wire [367:0] phy_reconfig_from_xcvr;
wire [559:0] phy_reconfig_to_xcvr;

assign SFPA_MOD1_SCL = 1'bz;
assign SFPA_MOD2_SDA = 1'bz;
assign SFPA_TXDISABLE = 1'b0;
assign SPFA_RATESEL = 2'b00;

assign SFPB_MOD1_SCL = 1'bz;
assign SFPB_MOD2_SDA = 1'bz;
assign SFPB_TXDISABLE = 1'b0;
assign SPFB_RATESEL = 2'b00;

assign SFPC_MOD1_SCL = 1'bz;
assign SFPC_MOD2_SDA = 1'bz;
assign SFPC_TXDISABLE = 1'b0;
assign SPFC_RATESEL = 2'b00;

assign SFPD_MOD1_SCL = 1'bz;
assign SFPD_MOD2_SDA = 1'bz;
assign SFPD_TXDISABLE = 1'b0;
assign SPFD_RATESEL = 2'b00;

phy4 phy_inst (
  .pll_ref_clk(SFP_REFCLK_p),
  .pll_locked(phy_pll_locked),

  .tx_serial_data_0(SFPA_TX_p),
  .rx_serial_data_0(SFPA_RX_p),
  .tx_serial_data_1(SFPB_TX_p),
  .rx_serial_data_1(SFPB_RX_p),
  .tx_serial_data_2(SFPC_TX_p),
  .rx_serial_data_2(SFPC_RX_p),
  .tx_serial_data_3(SFPD_TX_p),
  .rx_serial_data_3(SFPD_RX_p),

  .xgmii_tx_dc_0(sfp_a_tx_dc),
  .xgmii_rx_dc_0(sfp_a_rx_dc),
  .xgmii_tx_dc_1(sfp_b_tx_dc),
  .xgmii_rx_dc_1(sfp_b_rx_dc),
  .xgmii_tx_dc_2(sfp_c_tx_dc),
  .xgmii_rx_dc_2(sfp_c_rx_dc),
  .xgmii_tx_dc_3(sfp_d_tx_dc),
  .xgmii_rx_dc_3(sfp_d_rx_dc),

  .xgmii_rx_clk(clk_156mhz),
  .xgmii_tx_clk(clk_156mhz),

  .tx_ready(),
  .rx_ready(),

  .rx_data_ready(),

  .phy_mgmt_clk(clk_50mhz),
  .phy_mgmt_clk_reset(rst_50mhz),
  .phy_mgmt_address(9'd0),
  .phy_mgmt_read(1'b0),
  .phy_mgmt_readdata(),
  .phy_mgmt_waitrequest(),
  .phy_mgmt_write(1'b0),
  .phy_mgmt_writedata(32'd0),

  .reconfig_from_xcvr(phy_reconfig_from_xcvr),
  .reconfig_to_xcvr(phy_reconfig_to_xcvr)
);

phy_reconfig4 phy_reconfig_inst (
  .reconfig_busy(),

  .mgmt_clk_clk(clk_50mhz),
  .mgmt_rst_reset(rst_50mhz),

  .reconfig_mgmt_address(7'd0),
  .reconfig_mgmt_read(1'b0),
  .reconfig_mgmt_readdata(),
  .reconfig_mgmt_waitrequest(),
  .reconfig_mgmt_write(1'b0),
  .reconfig_mgmt_writedata(32'd0),

  .reconfig_to_xcvr(phy_reconfig_to_xcvr),
  .reconfig_from_xcvr(phy_reconfig_from_xcvr)
);

SoC system (
  .clk_clk                                   (clk_50mhz),
  .reset_reset_n                             (~soft_reset),

  .pcie_mm_hip_ctrl_test_in                    (),
  .pcie_mm_hip_ctrl_simu_mode_pipe             (1'b0),
  .pcie_mm_hip_serial_rx_in0                   (PCIE_RX_p[0]),
  .pcie_mm_hip_serial_rx_in1                   (PCIE_RX_p[1]),
  .pcie_mm_hip_serial_rx_in2                   (PCIE_RX_p[2]),
  .pcie_mm_hip_serial_rx_in3                   (PCIE_RX_p[3]),
  .pcie_mm_hip_serial_rx_in4                   (PCIE_RX_p[4]),
  .pcie_mm_hip_serial_rx_in5                   (PCIE_RX_p[5]),
  .pcie_mm_hip_serial_rx_in6                   (PCIE_RX_p[6]),
  .pcie_mm_hip_serial_rx_in7                   (PCIE_RX_p[7]),
  .pcie_mm_hip_serial_tx_out0                  (PCIE_TX_p[0]),
  .pcie_mm_hip_serial_tx_out1                  (PCIE_TX_p[1]),
  .pcie_mm_hip_serial_tx_out2                  (PCIE_TX_p[2]),
  .pcie_mm_hip_serial_tx_out3                  (PCIE_TX_p[3]),
  .pcie_mm_hip_serial_tx_out4                  (PCIE_TX_p[4]),
  .pcie_mm_hip_serial_tx_out5                  (PCIE_TX_p[5]),
  .pcie_mm_hip_serial_tx_out6                  (PCIE_TX_p[6]),
  .pcie_mm_hip_serial_tx_out7                  (PCIE_TX_p[7]),
  .pcie_mm_npor_npor                           (1'b1),
  .pcie_mm_npor_pin_perst                      (PCIE_PERST_n),
  .pcie_mm_refclk_clk                          (PCIE_REFCLK_p),
  .pcie_xcvr_clk_clk                           (OSC_50_B3B),
  .pcie_xcvr_reset_reset_n                     (PCIE_PERST_n),

  .clk_156_clk(clk_156mhz),
  .reset_156_reset_n(~(soft_reset | ~phy_pll_locked)),

  .mac_a_pause_data(0),
  .mac_a_xgmii_rx_data(sfp_a_rx_dc),
  .mac_a_xgmii_tx_data(sfp_a_tx_dc),

  .soft_reset_val(soft_reset),

  .pcie_clk_reset_reset(soft_reset),

  .ts_done_tsdcaldone(ts_done),
  .ts_out_tsdcalo(ts_out),
  .ts_enable_ce(ts_enable),
  .ts_clear_reset(ts_clear)

);
 
temp_display temp_display_inst (
  .clk_50mhz(clk_50mhz),
  .temp_valid(ts_done),
  .temp_val(ts_out),
  .temp_en(ts_enable),
  .temp_clear(ts_clear),
  .HEX0_D(HEX0_D),
  .HEX0_DP(HEX0_DP),
  .HEX1_D(HEX1_D),
  .HEX1_DP(HEX1_DP)
);
 
endmodule 
