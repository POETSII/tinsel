module DE10_Pro(
  input CLK_100_B3I,
  input CLK_50_B2C,
  input CLK_50_B2L,
  input CLK_50_B3C,
  input CLK_50_B3I,
  input CLK_50_B3L,

  input CPU_RESET_n,
  input [1:0] BUTTON,
  input [1:0] SW,
  output [3:0] LED,

  inout SI5340A0_I2C_SCL,
  inout SI5340A0_I2C_SDA,
  input SI5340A0_INTR,
  output SI5340A0_OE_n,
  output SI5340A0_RST_n,

  inout SI5340A1_I2C_SCL,
  inout SI5340A1_I2C_SDA,
  input SI5340A1_INTR,
  output SI5340A1_OE_n,
  output SI5340A1_RST_n,

  output FLASH_CLK,
  output [27:1] FLASH_A,
  inout [15:0] FLASH_D,
  output FLASH_CE_n,
  output FLASH_WE_n,
  output FLASH_OE_n,
  output FLASH_ADV_n,
  output FLASH_RESET_n,
  input FLASH_RDY_BSY_n,

  // input  DDR4A_REFCLK_p,
  // output [16:0] DDR4A_A,
  // output [1:0] DDR4A_BA,
  // output [1:0] DDR4A_BG,
  // output DDR4A_CK,
  // output DDR4A_CK_n,
  // output DDR4A_CKE,
  // inout  [8:0] DDR4A_DQS,
  // inout  [8:0] DDR4A_DQS_n,
  // inout  [71:0] DDR4A_DQ,
  // inout  [8:0] DDR4A_DBI_n,
  // output DDR4A_CS_n,
  // output DDR4A_RESET_n,
  // output DDR4A_ODT,
  // output DDR4A_PAR,
  // input DDR4A_ALERT_n,
  // output DDR4A_ACT_n,
  // input DDR4A_EVENT_n,
  // inout DDR4A_SCL,
  // inout DDR4A_SDA,
  // input DDR4A_RZQ,
  //
  // input  DDR4B_REFCLK_p,
  // output [16:0] DDR4B_A,
  // output [1:0] DDR4B_BA,
  // output [1:0] DDR4B_BG,
  // output DDR4B_CK,
  // output DDR4B_CK_n,
  // output DDR4B_CKE,
  // inout  [8:0] DDR4B_DQS,
  // inout  [8:0] DDR4B_DQS_n,
  // inout  [71:0] DDR4B_DQ,
  // inout  [8:0] DDR4B_DBI_n,
  // output DDR4B_CS_n,
  // output DDR4B_RESET_n,
  // output DDR4B_ODT,
  // output DDR4B_PAR,
  // input DDR4B_ALERT_n,
  // output DDR4B_ACT_n,
  // input DDR4B_EVENT_n,
  // inout DDR4B_SCL,
  // inout DDR4B_SDA,
  // input DDR4B_RZQ,
  //
  // input  DDR4C_REFCLK_p,
  // output [16:0] DDR4C_A,
  // output [1:0] DDR4C_BA,
  // output [1:0] DDR4C_BG,
  // output DDR4C_CK,
  // output DDR4C_CK_n,
  // output DDR4C_CKE,
  // inout  [8:0] DDR4C_DQS,
  // inout  [8:0] DDR4C_DQS_n,
  // inout  [71:0] DDR4C_DQ,
  // inout  [8:0] DDR4C_DBI_n,
  // output DDR4C_CS_n,
  // output DDR4C_RESET_n,
  // output DDR4C_ODT,
  // output DDR4C_PAR,
  // input DDR4C_ALERT_n,
  // output DDR4C_ACT_n,
  // input DDR4C_EVENT_n,
  // inout DDR4C_SCL,
  // inout DDR4C_SDA,
  // input DDR4C_RZQ,
  // //
  // input  DDR4D_REFCLK_p,
  // output [16:0] DDR4D_A,
  // output [1:0] DDR4D_BA,
  // output [1:0] DDR4D_BG,
  // output DDR4D_CK,
  // output DDR4D_CK_n,
  // output DDR4D_CKE,
  // inout  [8:0] DDR4D_DQS,
  // inout  [8:0] DDR4D_DQS_n,
  // inout  [71:0] DDR4D_DQ,
  // inout  [8:0] DDR4D_DBI_n,
  // output DDR4D_CS_n,
  // output DDR4D_RESET_n,
  // output DDR4D_ODT,
  // output DDR4D_PAR,
  // input DDR4D_ALERT_n,
  // output DDR4D_ACT_n,
  // input DDR4D_EVENT_n,
  // inout DDR4D_SCL,
  // inout DDR4D_SDA,
  // input DDR4D_RZQ,


  inout              PCIE_SMBCLK,
  inout              PCIE_SMBDAT,
  input              PCIE_REFCLK_p,
  output   [ 3: 0]   PCIE_TX_p,
  input    [ 3: 0]   PCIE_RX_p,
  input              PCIE_PERST_n,
  output             PCIE_WAKE_n,

  input              QSFP28A_REFCLK_p,
  output   [ 3: 0]   QSFP28A_TX_p,
  input    [ 3: 0]   QSFP28A_RX_p,
  input              QSFP28A_INTERRUPT_n,
  output             QSFP28A_LP_MODE,
  input              QSFP28A_MOD_PRS_n,
  output             QSFP28A_MOD_SEL_n,
  output             QSFP28A_RST_n,
  inout              QSFP28A_SCL,
  inout              QSFP28A_SDA,

  // input              QSFP28B_REFCLK_p,
  // output   [ 3: 0]   QSFP28B_TX_p,
  // input    [ 3: 0]   QSFP28B_RX_p,
  // input              QSFP28B_INTERRUPT_n,
  // output             QSFP28B_LP_MODE,
  // input              QSFP28B_MOD_PRS_n,
  // output             QSFP28B_MOD_SEL_n,
  // output             QSFP28B_RST_n,
  // inout              QSFP28B_SCL,
  // inout              QSFP28B_SDA,
  //
  input              QSFP28C_REFCLK_p,
  output   [ 3: 0]   QSFP28C_TX_p,
  input    [ 3: 0]   QSFP28C_RX_p,
  input              QSFP28C_INTERRUPT_n,
  output             QSFP28C_LP_MODE,
  input              QSFP28C_MOD_PRS_n,
  output             QSFP28C_MOD_SEL_n,
  output             QSFP28C_RST_n,
  inout              QSFP28C_SCL,
  inout              QSFP28C_SDA,
  //
  // input              QSFP28D_REFCLK_p,
  // output   [ 3: 0]   QSFP28D_TX_p,
  // input    [ 3: 0]   QSFP28D_RX_p,
  // input              QSFP28D_INTERRUPT_n,
  // output             QSFP28D_LP_MODE,
  // input              QSFP28D_MOD_PRS_n,
  // output             QSFP28D_MOD_SEL_n,
  // output             QSFP28D_RST_n,
  // inout              QSFP28D_SCL,
  // inout              QSFP28D_SDA,

  input EXP_EN,

  inout UFL_CLKIN_p,
  inout UFL_CLKIN_n,

  inout FAN_I2C_SCL,
  inout FAN_I2C_SDA,
  input FAN_ALERT_n,
  inout POWER_MONITOR_I2C_SCL,
  inout POWER_MONITOR_I2C_SDA,
  input POWER_MONITOR_ALERT_n,
  inout TEMP_I2C_SCL,
  inout TEMP_I2C_SDA

);

assign PCIE_WAKE_n = 1'b1;
wire [31:0] hip_ctrl_test_in;
assign hip_ctrl_test_in = 32'h000000A8;


// wire reset_n;
wire ddr4_local_reset_req;
wire ddr4_a_local_reset_done;
wire ddr4_a_status_local_cal_fail;
wire ddr4_a_status_local_cal_success;

// assign ddr4_local_reset_req;
wire ddr4_a_local_reset_done;
wire ddr4_a_status_local_cal_fail;
wire ddr4_a_status_local_cal_success;
wire ddr4_b_local_reset_done;
wire ddr4_b_status_local_cal_fail;
wire ddr4_b_status_local_cal_success;
wire ddr4_c_local_reset_done;
wire ddr4_c_status_local_cal_fail;
wire ddr4_c_status_local_cal_success;
wire ddr4_d_local_reset_done;
wire ddr4_d_status_local_cal_fail;
wire ddr4_d_status_local_cal_success;

wire [11:0] ddr4_status;

  // Reset release
  wire ninit_done;
  reset_release reset_release (
          .ninit_done(ninit_done)
          );

  // assign reset_n = &{!ninit_done, CPU_RESET_n};

  assign QSFP28A_LP_MODE = 0;
  assign QSFP28A_RST_n = 1;
  assign QSFP28A_SCL = 0;
  assign QSFP28A_SDA = 0;

  assign QSFP28B_LP_MODE = 0;
  assign QSFP28B_RST_n = 1;
  assign QSFP28B_SCL = 0;
  assign QSFP28B_SDA = 0;

  assign QSFP28C_LP_MODE = 0;
  assign QSFP28C_RST_n = 1;
  assign QSFP28C_SCL = 0;
  assign QSFP28C_SDA = 0;

  assign QSFP28D_LP_MODE = 0;
  assign QSFP28D_RST_n = 1;
  assign QSFP28D_SCL = 0;
  assign QSFP28D_SDA = 0;

  mkDE10FanControl fancontrol(
      .CLK(CLK_50_B3I),
      .RST_N(~ninit_done),
      .FAN_I2C_SDA(FAN_I2C_SDA),
      .FAN_I2C_SCL(FAN_I2C_SCL),
      .TEMP_I2C_SDA(TEMP_I2C_SDA),
      .TEMP_I2C_SCL(TEMP_I2C_SCL)
  );


  assign ddr4_status =
    {ddr4_b_status_local_cal_fail,
       ddr4_b_status_local_cal_success,
         ddr4_b_local_reset_done};

  DE10_Pro_QSYS DE10_Pro_QSYS_inst (
        .clk_clk(CLK_50_B3I),
        .reset_reset(ninit_done),

        .atx_pll_dualclk_0_clock_sink_clk                              (QSFP28A_REFCLK_p),                              //   input,   width = 1,          atx_pll_dualclk_0_clock_sink.clk
        .alt_e100s10_north_clk_ref_clk                                 (QSFP28A_REFCLK_p),                                 //   input,   width = 1,             alt_e100s10_north_clk_ref.clk
        .alt_e100s10_north_serial_lanes_tx_serial                      (QSFP28A_TX_p),                      //  output,   width = 4,        alt_e100s10_south_serial_lanes.tx_serial
        .alt_e100s10_north_serial_lanes_rx_serial                      (QSFP28A_RX_p),
                             //   input,   width = 4,                                      .rx_serial
        .atx_pll_dualclk_1_clock_sink_clk                              (QSFP28C_REFCLK_p),                              //   input,   width = 1,          atx_pll_dualclk_0_clock_sink.clk
        .alt_e100s10_south_serial_lanes_tx_serial                      (QSFP28C_TX_p),                      //  output,   width = 4,        alt_e100s10_south_serial_lanes.tx_serial
        .alt_e100s10_south_serial_lanes_rx_serial                      (QSFP28C_RX_p),                      //   input,   width = 4,                                      .rx_serial
        .alt_e100s10_south_clk_ref_clk                                 (QSFP28C_REFCLK_p),                                  //   input,   width = 1,             alt_e100s10_south_clk_ref.clk


        .tinsel_0_temperature_val(178),
        .iopll_0_locked_export()
    );

  assign SI5340A0_RST_n = 1'b1;
  assign SI5340A1_RST_n = 1'b1;

  assign SI5340A0_OE_n = 1'b0;
  assign SI5340A1_OE_n = 1'b0;

endmodule
