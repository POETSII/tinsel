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

module Golden_top(
  input wire [3:0] BUTTON,
  input wire CPU_RESET_n,

  output wire CLOCK_SCL,
  inout wire CLOCK_SDA,

  output wire [15:0] DDR3A_A,
  output wire [2:0] DDR3A_BA,
  output wire DDR3A_CAS_n,
  output wire [1:0] DDR3A_CK,
  output wire [1:0] DDR3A_CKE,
  output wire [0:0] DDR3A_CK_n,
  output wire [1:0] DDR3A_CS_n,
  output wire [7:0] DDR3A_DM,
  inout wire [63:0] DDR3A_DQ,
  inout wire [7:0] DDR3A_DQS,
  inout wire [7:0] DDR3A_DQS_n,
  input wire DDR3A_EVENT_n,
  output wire [1:0] DDR3A_ODT,
  output wire DDR3A_RAS_n,
  output wire DDR3A_RESET_n,
  output wire DDR3A_SCL,
  inout wire DDR3A_SDA,
  output wire DDR3A_WE_n,

  output wire [15:0] DDR3B_A,
  output wire [2:0] DDR3B_BA,
  output wire DDR3B_CAS_n,
  output wire [1:0] DDR3B_CK,
  output wire [1:0] DDR3B_CKE,
  output wire [0:0] DDR3B_CK_n,
  output wire [1:0] DDR3B_CS_n,
  output wire [7:0] DDR3B_DM,
  inout wire [63:0] DDR3B_DQ,
  inout wire [7:0] DDR3B_DQS,
  inout wire [7:0] DDR3B_DQS_n,
  input wire DDR3B_EVENT_n,
  output wire [1:0] DDR3B_ODT,
  output wire DDR3B_RAS_n,
  output wire DDR3B_RESET_n,
  output wire DDR3B_SCL,
  inout wire DDR3B_SDA,
  output wire DDR3B_WE_n,

  inout wire FAN_CTRL,

  output wire FLASH_ADV_n,
  output wire [1:0] FLASH_CE_n,
  output wire FLASH_CLK,
  output wire FLASH_OE_n,
  input wire [1:0] FLASH_RDY_BSY_n,
  output wire FLASH_RESET_n,
  output wire FLASH_WE_n,

  output wire [26:0] FSM_A,
  inout wire  [31:0] FSM_D,

  output wire [6:0] HEX0_D,
  output wire HEX0_DP,

  output wire [6:0] HEX1_D,
  output wire HEX1_DP,

  output wire [3:0] LED,
  output wire [3:0] LED_BRACKET,
  output wire LED_RJ45_L,
  output wire LED_RJ45_R,

  input wire OSC_50_B3B,
  input wire OSC_50_B3D,
  input wire OSC_50_B4A,
  input wire OSC_50_B4D,
  input wire OSC_50_B7A,
  input wire OSC_50_B7D,
  input wire OSC_50_B8A,
  input wire OSC_50_B8D,

  output wire PLL_SCL,
  inout wire PLL_SDA,

  output wire RS422_DE,
  input wire RS422_DIN,
  output wire RS422_DOUT,
  output wire RS422_RE_n,
  output wire RS422_TE,

  input wire RZQ_0,
  input wire RZQ_1,
  input wire RZQ_4,
  input wire RZQ_5,

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

  input wire SMA_CLKIN,
  output wire SMA_CLKOUT,

  input wire [3:0] SW,

  output wire TEMP_CLK,
  inout wire TEMP_DATA,
  input wire TEMP_INT_n,
  input wire TEMP_OVERT_n

);

wire clk_50mhz = OSC_50_B7A;
wire rst_50mhz = 0;
wire rst_50mhz_n = 1;

wire clk_156mhz;
wire phy_pll_locked;

wire ddr3_local_init_done;
wire ddr3_local_cal_success;
wire ddr3_2_local_init_done;
wire ddr3_2_local_cal_success;

assign LED[3:0] = {ddr3_local_init_done, ddr3_local_cal_success,
                   ddr3_2_local_init_done, ddr3_2_local_cal_success};

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

phy phy_inst (
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

phy_reconfig phy_reconfig_inst (
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

S5_DDR3_QSYS u0 (
  .clk_clk                                   (clk_50mhz),
  .reset_reset_n                             (rst_50mhz_n),
  .board_id_id                               (SW),

  .memory_mem_a                              (DDR3A_A),
  .memory_mem_ba                             (DDR3A_BA),
  .memory_mem_ck                             (DDR3A_CK),
  .memory_mem_ck_n                           (DDR3A_CK_n),
  .memory_mem_cke                            (DDR3A_CKE),
  .memory_mem_cs_n                           (DDR3A_CS_n),
  .memory_mem_dm                             (DDR3A_DM),
  .memory_mem_ras_n                          (DDR3A_RAS_n),
  .memory_mem_cas_n                          (DDR3A_CAS_n),
  .memory_mem_we_n                           (DDR3A_WE_n),
  .memory_mem_reset_n                        (DDR3A_RESET_n),
  .memory_mem_dq                             (DDR3A_DQ),
  .memory_mem_dqs                            (DDR3A_DQS),
  .memory_mem_dqs_n                          (DDR3A_DQS_n),
  .memory_mem_odt                            (DDR3A_ODT),
  .oct_rzqin                                 (RZQ_4),
  .mem_if_ddr3_emif_status_local_init_done   (ddr3_local_init_done),   
  .mem_if_ddr3_emif_status_local_cal_success (ddr3_local_cal_success), 
  .mem_if_ddr3_emif_status_local_cal_fail    (ddr3_local_cal_fail),
      
  .memory_2_mem_a                              (DDR3B_A),
  .memory_2_mem_ba                             (DDR3B_BA),
  .memory_2_mem_ck                             (DDR3B_CK),
  .memory_2_mem_ck_n                           (DDR3B_CK_n),
  .memory_2_mem_cke                            (DDR3B_CKE),
  .memory_2_mem_cs_n                           (DDR3B_CS_n),
  .memory_2_mem_dm                             (DDR3B_DM),
  .memory_2_mem_ras_n                          (DDR3B_RAS_n),
  .memory_2_mem_cas_n                          (DDR3B_CAS_n),
  .memory_2_mem_we_n                           (DDR3B_WE_n),
  .memory_2_mem_reset_n                        (DDR3B_RESET_n),
  .memory_2_mem_dq                             (DDR3B_DQ),
  .memory_2_mem_dqs                            (DDR3B_DQS),
  .memory_2_mem_dqs_n                          (DDR3B_DQS_n),
  .memory_2_mem_odt                            (DDR3B_ODT),
  .mem_if_ddr3_emif_2_status_local_init_done   (ddr3_2_local_init_done),
  .mem_if_ddr3_emif_2_status_local_cal_success (ddr3_2_local_cal_success),
  .mem_if_ddr3_emif_2_status_local_cal_fail    (ddr3_2_local_cal_fail),

  .clk_156_clk(clk_156mhz),
  .reset_156_reset_n(phy_pll_locked),

  .mac_a_pause_data(0),
  .mac_a_xgmii_rx_data(sfp_a_rx_dc),
  .mac_a_xgmii_tx_data(sfp_a_tx_dc),

  .mac_b_pause_data(0),
  .mac_b_xgmii_rx_data(sfp_b_rx_dc),
  .mac_b_xgmii_tx_data(sfp_b_tx_dc),

  .mac_c_pause_data(0),
  .mac_c_xgmii_rx_data(sfp_c_rx_dc),
  .mac_c_xgmii_tx_data(sfp_c_tx_dc),

  .mac_d_pause_data(0),
  .mac_d_xgmii_rx_data(sfp_d_rx_dc),
  .mac_d_xgmii_tx_data(sfp_d_tx_dc),

);

endmodule 
