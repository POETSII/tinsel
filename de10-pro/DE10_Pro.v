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

  input  DDR4A_REFCLK_p,
  output [16:0] DDR4A_A,
  output [1:0] DDR4A_BA,
  output [1:0] DDR4A_BG,
  output DDR4A_CK,
  output DDR4A_CK_n,
  output DDR4A_CKE,
  inout  [8:0] DDR4A_DQS,
  inout  [8:0] DDR4A_DQS_n,
  inout  [71:0] DDR4A_DQ,
  inout  [8:0] DDR4A_DBI_n,
  output DDR4A_CS_n,
  output DDR4A_RESET_n,
  output DDR4A_ODT,
  output DDR4A_PAR,
  input DDR4A_ALERT_n,
  output DDR4A_ACT_n,
  input DDR4A_EVENT_n,
  inout DDR4A_SCL,
  inout DDR4A_SDA,
  input DDR4A_RZQ,

  input  DDR4B_REFCLK_p,
  output [16:0] DDR4B_A,
  output [1:0] DDR4B_BA,
  output [1:0] DDR4B_BG,
  output DDR4B_CK,
  output DDR4B_CK_n,
  output DDR4B_CKE,
  inout  [8:0] DDR4B_DQS,
  inout  [8:0] DDR4B_DQS_n,
  inout  [71:0] DDR4B_DQ,
  inout  [8:0] DDR4B_DBI_n,
  output DDR4B_CS_n,
  output DDR4B_RESET_n,
  output DDR4B_ODT,
  output DDR4B_PAR,
  input DDR4B_ALERT_n,
  output DDR4B_ACT_n,
  input DDR4B_EVENT_n,
  inout DDR4B_SCL,
  inout DDR4B_SDA,
  input DDR4B_RZQ,

  input  DDR4C_REFCLK_p,
  output [16:0] DDR4C_A,
  output [1:0] DDR4C_BA,
  output [1:0] DDR4C_BG,
  output DDR4C_CK,
  output DDR4C_CK_n,
  output DDR4C_CKE,
  inout  [8:0] DDR4C_DQS,
  inout  [8:0] DDR4C_DQS_n,
  inout  [71:0] DDR4C_DQ,
  inout  [8:0] DDR4C_DBI_n,
  output DDR4C_CS_n,
  output DDR4C_RESET_n,
  output DDR4C_ODT,
  output DDR4C_PAR,
  input DDR4C_ALERT_n,
  output DDR4C_ACT_n,
  input DDR4C_EVENT_n,
  inout DDR4C_SCL,
  inout DDR4C_SDA,
  input DDR4C_RZQ,
  //
  input  DDR4D_REFCLK_p,
  output [16:0] DDR4D_A,
  output [1:0] DDR4D_BA,
  output [1:0] DDR4D_BG,
  output DDR4D_CK,
  output DDR4D_CK_n,
  output DDR4D_CKE,
  inout  [8:0] DDR4D_DQS,
  inout  [8:0] DDR4D_DQS_n,
  inout  [71:0] DDR4D_DQ,
  inout  [8:0] DDR4D_DBI_n,
  output DDR4D_CS_n,
  output DDR4D_RESET_n,
  output DDR4D_ODT,
  output DDR4D_PAR,
  input DDR4D_ALERT_n,
  output DDR4D_ACT_n,
  input DDR4D_EVENT_n,
  inout DDR4D_SCL,
  inout DDR4D_SDA,
  input DDR4D_RZQ,


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
  // input              QSFP28C_REFCLK_p,
  // output   [ 3: 0]   QSFP28C_TX_p,
  // input    [ 3: 0]   QSFP28C_RX_p,
  // input              QSFP28C_INTERRUPT_n,
  // output             QSFP28C_LP_MODE,
  // input              QSFP28C_MOD_PRS_n,
  // output             QSFP28C_MOD_SEL_n,
  // output             QSFP28C_RST_n,
  // inout              QSFP28C_SCL,
  // inout              QSFP28C_SDA,
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

        .ddr_a_local_reset_req_local_reset_req                         (ddr4_local_reset_req),         //   input,   width = 1,                 ddr_a_local_reset_req.local_reset_req
        .ddr_a_local_reset_status_local_reset_done                     (ddr4_a_local_reset_done),     //  output,   width = 1,              ddr_a_local_reset_status.local_reset_done
        .ddr_a_pll_ref_clk_clk                                         (DDR4A_REFCLK_p),                         //   input,   width = 1,                     ddr_a_pll_ref_clk.clk
        .ddr_a_oct_oct_rzqin                                           (DDR4A_RZQ),                           //   input,   width = 1,                             ddr_a_oct.oct_rzqin
        .ddr_a_mem_mem_ck                                              (DDR4A_CK),                              //  output,   width = 1,                             ddr_a_mem.mem_ck
        .ddr_a_mem_mem_ck_n                                            (DDR4A_CK_n),                            //  output,   width = 1,                                      .mem_ck_n
        .ddr_a_mem_mem_a                                               (DDR4A_A),                                                      //  output,  width = 17,                                      .mem_a
        .ddr_a_mem_mem_act_n                                           (DDR4A_ACT_n),                                                  //  output,   width = 1,                                      .mem_act_n
        .ddr_a_mem_mem_ba                                              (DDR4A_BA),                                                     //  output,   width = 2,                                      .mem_ba
        .ddr_a_mem_mem_bg                                              (DDR4A_BG),                                                     //  output,   width = 2,                                      .mem_bg
        .ddr_a_mem_mem_cke                                             (DDR4A_CKE),                                                    //  output,   width = 1,                                      .mem_cke
        .ddr_a_mem_mem_cs_n                                            (DDR4A_CS_n),                                                   //  output,   width = 1,                                      .mem_cs_n
        .ddr_a_mem_mem_odt                                             (DDR4A_ODT),                                                    //  output,   width = 1,                                      .mem_odt
        .ddr_a_mem_mem_reset_n                                         (DDR4A_RESET_n),                                                //  output,   width = 1,                                      .mem_reset_n
        .ddr_a_mem_mem_par                                             (DDR4A_PAR),                                                    //  output,   width = 1,                                      .mem_par
        .ddr_a_mem_mem_alert_n                                         (DDR4A_ALERT_n),                                                //   input,   width = 1,                                      .mem_alert_n
        .ddr_a_mem_mem_dqs                                             (DDR4A_DQS),                                                    //   inout,   width = 9,                                      .mem_dqs
        .ddr_a_mem_mem_dqs_n                                           (DDR4A_DQS_n),                                                  //   inout,   width = 9,                                      .mem_dqs_n
        .ddr_a_mem_mem_dq                                              (DDR4A_DQ),                                                     //   inout,  width = 72,                                      .mem_dq
        .ddr_a_mem_mem_dbi_n                                           (DDR4A_DBI_n),                                                  //   inout,   width = 9,                                      .mem_dbi_n
        .ddr_a_status_local_cal_success                                (ddr4_a_status_local_cal_success),                              //  output,   width = 1,                          ddr_a_status.local_cal_success
        .ddr_a_status_local_cal_fail                                   (ddr4_a_status_local_cal_fail),                                 //  output,   width = 1,                                      .local_cal_fail

        .ddr_b_local_reset_req_local_reset_req                         (ddr4_local_reset_req),         //                     //   input,   width = 1,                 ddr_b_local_reset_req.local_reset_req
        .ddr_b_local_reset_status_local_reset_done                     (ddr4_b_local_reset_done),     //                      //  output,   width = 1,              ddr_b_local_reset_status.local_reset_done
        .ddr_b_pll_ref_clk_clk                                         (DDR4B_REFCLK_p),                                      //   input,   width = 1,                     ddr_b_pll_ref_clk.clk
        .ddr_b_oct_oct_rzqin                                           (DDR4B_RZQ),                                           //   input,   width = 1,                             ddr_b_oct.oct_rzqin
        .ddr_b_mem_mem_ck                                              (DDR4B_CK),                                            //  output,   width = 1,                             ddr_b_mem.mem_ck
        .ddr_b_mem_mem_ck_n                                            (DDR4B_CK_n),                                          //  output,   width = 1,                                      .mem_ck_n
        .ddr_b_mem_mem_a                                               (DDR4B_A),                                             //  output,  width = 17,                                      .mem_a
        .ddr_b_mem_mem_act_n                                           (DDR4B_ACT_n),                                         //  output,   width = 1,                                      .mem_act_n
        .ddr_b_mem_mem_ba                                              (DDR4B_BA),                                            //  output,   width = 2,                                      .mem_ba
        .ddr_b_mem_mem_bg                                              (DDR4B_BG),                                            //  output,   width = 2,                                      .mem_bg
        .ddr_b_mem_mem_cke                                             (DDR4B_CKE),                                           //  output,   width = 1,                                      .mem_cke
        .ddr_b_mem_mem_cs_n                                            (DDR4B_CS_n),                                          //  output,   width = 1,                                      .mem_cs_n
        .ddr_b_mem_mem_odt                                             (DDR4B_ODT),                                           //  output,   width = 1,                                      .mem_odt
        .ddr_b_mem_mem_reset_n                                         (DDR4B_RESET_n),                                       //  output,   width = 1,                                      .mem_reset_n
        .ddr_b_mem_mem_par                                             (DDR4B_PAR),                                           //  output,   width = 1,                                      .mem_par
        .ddr_b_mem_mem_alert_n                                         (DDR4B_ALERT_n),                                       //   input,   width = 1,                                      .mem_alert_n
        .ddr_b_mem_mem_dqs                                             (DDR4B_DQS),                                           //   inout,   width = 9,                                      .mem_dqs
        .ddr_b_mem_mem_dqs_n                                           (DDR4B_DQS_n),                                         //   inout,   width = 9,                                      .mem_dqs_n
        .ddr_b_mem_mem_dq                                              (DDR4B_DQ),                                            //   inout,  width = 72,                                      .mem_dq
        .ddr_b_mem_mem_dbi_n                                           (DDR4B_DBI_n),                                         //   inout,   width = 9,                                      .mem_dbi_n
        .ddr_b_status_local_cal_success                                (ddr4_b_status_local_cal_success),                     //  output,   width = 1,                          ddr_b_status.local_cal_success
        .ddr_b_status_local_cal_fail                                   (ddr4_b_status_local_cal_fail),                          //  output,   width = 1,                                      .local_cal_fail

        .ddr_c_local_reset_req_local_reset_req                         (ddr4_local_reset_req),         //                     //   input,   width = 1,                 ddr_c_local_reset_req.local_reset_req
        .ddr_c_local_reset_status_local_reset_done                     (ddr4_c_local_reset_done),     //                      //  output,   width = 1,              ddr_c_local_reset_status.local_reset_done
        .ddr_c_pll_ref_clk_clk                                         (DDR4C_REFCLK_p),                                      //   input,   width = 1,                     ddr_c_pll_ref_clk.clk
        .ddr_c_oct_oct_rzqin                                           (DDR4C_RZQ),                                           //   input,   width = 1,                             ddr_c_oct.oct_rzqin
        .ddr_c_mem_mem_ck                                              (DDR4C_CK),                                            //  output,   width = 1,                             ddr_c_mem.mem_ck
        .ddr_c_mem_mem_ck_n                                            (DDR4C_CK_n),                                          //  output,   width = 1,                                      .mem_ck_n
        .ddr_c_mem_mem_a                                               (DDR4C_A),                                             //  output,  width = 17,                                      .mem_a
        .ddr_c_mem_mem_act_n                                           (DDR4C_ACT_n),                                         //  output,   width = 1,                                      .mem_act_n
        .ddr_c_mem_mem_ba                                              (DDR4C_BA),                                            //  output,   width = 2,                                      .mem_ba
        .ddr_c_mem_mem_bg                                              (DDR4C_BG),                                            //  output,   width = 2,                                      .mem_bg
        .ddr_c_mem_mem_cke                                             (DDR4C_CKE),                                           //  output,   width = 1,                                      .mem_cke
        .ddr_c_mem_mem_cs_n                                            (DDR4C_CS_n),                                          //  output,   width = 1,                                      .mem_cs_n
        .ddr_c_mem_mem_odt                                             (DDR4C_ODT),                                           //  output,   width = 1,                                      .mem_odt
        .ddr_c_mem_mem_reset_n                                         (DDR4C_RESET_n),                                       //  output,   width = 1,                                      .mem_reset_n
        .ddr_c_mem_mem_par                                             (DDR4C_PAR),                                           //  output,   width = 1,                                      .mem_par
        .ddr_c_mem_mem_alert_n                                         (DDR4C_ALERT_n),                                       //   input,   width = 1,                                      .mem_alert_n
        .ddr_c_mem_mem_dqs                                             (DDR4C_DQS),                                           //   inout,   width = 9,                                      .mem_dqs
        .ddr_c_mem_mem_dqs_n                                           (DDR4C_DQS_n),                                         //   inout,   width = 9,                                      .mem_dqs_n
        .ddr_c_mem_mem_dq                                              (DDR4C_DQ),                                            //   inout,  width = 72,                                      .mem_dq
        .ddr_c_mem_mem_dbi_n                                           (DDR4C_DBI_n),                                         //   inout,   width = 9,                                      .mem_dbi_n
        .ddr_c_status_local_cal_success                                (ddr4_c_status_local_cal_success),                     //  output,   width = 1,                          ddr_c_status.local_cal_success
        .ddr_c_status_local_cal_fail                                   (ddr4_c_status_local_cal_fail),                          //  output,   width = 1,                                      .local_cal_fail

        .ddr_d_local_reset_req_local_reset_req                         (ddr4_local_reset_req),         //                     //   input,   width = 1,                 ddr_c_local_reset_req.local_reset_req
        .ddr_d_local_reset_status_local_reset_done                     (ddr4_d_local_reset_done),     //                      //  output,   width = 1,              ddr_c_local_reset_status.local_reset_done
        .ddr_d_pll_ref_clk_clk                                         (DDR4D_REFCLK_p),                                      //   input,   width = 1,                     ddr_c_pll_ref_clk.clk
        .ddr_d_oct_oct_rzqin                                           (DDR4D_RZQ),                                           //   input,   width = 1,                             ddr_c_oct.oct_rzqin
        .ddr_d_mem_mem_ck                                              (DDR4D_CK),                                            //  output,   width = 1,                             ddr_c_mem.mem_ck
        .ddr_d_mem_mem_ck_n                                            (DDR4D_CK_n),                                          //  output,   width = 1,                                      .mem_ck_n
        .ddr_d_mem_mem_a                                               (DDR4D_A),                                             //  output,  width = 17,                                      .mem_a
        .ddr_d_mem_mem_act_n                                           (DDR4D_ACT_n),                                         //  output,   width = 1,                                      .mem_act_n
        .ddr_d_mem_mem_ba                                              (DDR4D_BA),                                            //  output,   width = 2,                                      .mem_ba
        .ddr_d_mem_mem_bg                                              (DDR4D_BG),                                            //  output,   width = 2,                                      .mem_bg
        .ddr_d_mem_mem_cke                                             (DDR4D_CKE),                                           //  output,   width = 1,                                      .mem_cke
        .ddr_d_mem_mem_cs_n                                            (DDR4D_CS_n),                                          //  output,   width = 1,                                      .mem_cs_n
        .ddr_d_mem_mem_odt                                             (DDR4D_ODT),                                           //  output,   width = 1,                                      .mem_odt
        .ddr_d_mem_mem_reset_n                                         (DDR4D_RESET_n),                                       //  output,   width = 1,                                      .mem_reset_n
        .ddr_d_mem_mem_par                                             (DDR4D_PAR),                                           //  output,   width = 1,                                      .mem_par
        .ddr_d_mem_mem_alert_n                                         (DDR4D_ALERT_n),                                       //   input,   width = 1,                                      .mem_alert_n
        .ddr_d_mem_mem_dqs                                             (DDR4D_DQS),                                           //   inout,   width = 9,                                      .mem_dqs
        .ddr_d_mem_mem_dqs_n                                           (DDR4D_DQS_n),                                         //   inout,   width = 9,                                      .mem_dqs_n
        .ddr_d_mem_mem_dq                                              (DDR4D_DQ),                                            //   inout,  width = 72,                                      .mem_dq
        .ddr_d_mem_mem_dbi_n                                           (DDR4D_DBI_n),                                         //   inout,   width = 9,                                      .mem_dbi_n
        .ddr_d_status_local_cal_success                                (ddr4_d_status_local_cal_success),                     //  output,   width = 1,                          ddr_c_status.local_cal_success
        .ddr_d_status_local_cal_fail                                   (ddr4_d_status_local_cal_fail),                          //  output,   width = 1,                                      .local_cal_fail


        .pcie_s10_hip_avmm_bridge_0_refclk_clk                         (PCIE_REFCLK_p),                         //   input,   width = 1,     pcie_s10_hip_avmm_bridge_0_refclk.clk
        // ninit_done -> in_init. According to   Article ID: 000078510 When pin_perst is asserted, npor must be active.
        // in_init is active at power on, so we should assert npor with ninit_done and arguably 100ms afterwards?
        // we use a further invert... TODO why.
        .pcie_s10_hip_avmm_bridge_0_npor_npor                          (~ninit_done),                          //   input,   width = 1,       pcie_s10_hip_avmm_bridge_0_npor.npor
        .pcie_s10_hip_avmm_bridge_0_npor_pin_perst                     (PCIE_PERST_n),                     //   input,   width = 1,                                      .pin_perst
        .pcie_s10_hip_avmm_bridge_0_hip_ctrl_test_in                   (hip_ctrl_test_in),                   //   input,  width = 67,                                      .test_in
        .pcie_s10_hip_avmm_bridge_0_hip_serial_rx_in0                  (PCIE_RX_p[0]),                  //   input,   width = 1, pcie_s10_hip_avmm_bridge_0_hip_serial.rx_in0
        .pcie_s10_hip_avmm_bridge_0_hip_serial_tx_out0                 (PCIE_TX_p[0]),                 //  output,   width = 1,                                      .tx_out0
        .pcie_s10_hip_avmm_bridge_0_hip_serial_rx_in1                  (PCIE_RX_p[1]),                  //   input,   width = 1, pcie_s10_hip_avmm_bridge_0_hip_serial.rx_in0
        .pcie_s10_hip_avmm_bridge_0_hip_serial_tx_out1                 (PCIE_TX_p[1]),                 //  output,   width = 1,                                      .tx_out0
        .pcie_s10_hip_avmm_bridge_0_hip_serial_rx_in2                  (PCIE_RX_p[2]),                  //   input,   width = 1, pcie_s10_hip_avmm_bridge_0_hip_serial.rx_in0
        .pcie_s10_hip_avmm_bridge_0_hip_serial_tx_out2                 (PCIE_TX_p[2]),                 //  output,   width = 1,                                      .tx_out0
        .pcie_s10_hip_avmm_bridge_0_hip_serial_rx_in3                  (PCIE_RX_p[3]),                  //   input,   width = 1, pcie_s10_hip_avmm_bridge_0_hip_serial.rx_in0
        .pcie_s10_hip_avmm_bridge_0_hip_serial_tx_out3                 (PCIE_TX_p[3]),                 //  output,   width = 1,                                      .tx_out0

        // .nic_a_mac_a_refclk_in_clk_clk                                 (QSFP28A_REFCLK_p),                                 //   input,   width = 1,             nic_a_mac_a_refclk_in_clk.clk
        // .nic_a_s10_100gmac_a_serial_lanes_tx_serial                    (QSFP28A_TX_p),                    //  output,   width = 4,      nic_a_s10_100gmac_a_serial_lanes.tx_serial
        // .nic_a_s10_100gmac_a_serial_lanes_rx_serial                    (QSFP28A_RX_p),                    //   input,   width = 4,                                      .rx_serial
        // .nic_b_mac_a_refclk_in_clk_clk                                 (QSFP28B_REFCLK_p),                                 //   input,   width = 1,             nic_a_mac_a_refclk_in_clk.clk
        // .nic_b_s10_100gmac_a_serial_lanes_tx_serial                    (QSFP28B_TX_p),                    //  output,   width = 4,      nic_a_s10_100gmac_a_serial_lanes.tx_serial
        // .nic_b_s10_100gmac_a_serial_lanes_rx_serial                    (QSFP28B_RX_p),                    //   input,   width = 4,                                      .rx_serial
        // .nic_c_mac_a_refclk_in_clk_clk                                 (QSFP28C_REFCLK_p),                                 //   input,   width = 1,             nic_a_mac_a_refclk_in_clk.clk
        // .nic_c_s10_100gmac_a_serial_lanes_tx_serial                    (QSFP28C_TX_p),                    //  output,   width = 4,      nic_a_s10_100gmac_a_serial_lanes.tx_serial
        // .nic_c_s10_100gmac_a_serial_lanes_rx_serial                    (QSFP28C_RX_p),                    //   input,   width = 4,                                      .rx_serial
        // .nic_d_mac_a_refclk_in_clk_clk                                 (QSFP28D_REFCLK_p),                                 //   input,   width = 1,             nic_a_mac_a_refclk_in_clk.clk
        // .nic_d_s10_100gmac_a_serial_lanes_tx_serial                    (QSFP28D_TX_p),                    //  output,   width = 4,      nic_a_s10_100gmac_a_serial_lanes.tx_serial
        // .nic_d_s10_100gmac_a_serial_lanes_rx_serial                    (QSFP28D_RX_p),                    //   input,   width = 4,                                      .rx_serial

        .alt_ehipc2_0_i_clk_ref_clk                                    (QSFP28A_REFCLK_p),                                    //   input,   width = 1,                alt_ehipc2_0_i_clk_ref.clk
        .alt_ehipc2_0_o_tx_serial_o_tx_serial                          (QSFP28A_TX_p),                          //  output,   width = 4,              alt_ehipc2_0_o_tx_serial.o_tx_serial
        .alt_ehipc2_0_i_rx_serial_i_rx_serial                          (QSFP28A_RX_p),                          //   input,   width = 4,              alt_ehipc2_0_i_rx_serial.i_rx_serial
        .atx_pll_dualclk_0_clock_sink_clk                              (QSFP28A_REFCLK_p),                              //   input,   width = 1,          atx_pll_dualclk_0_clock_sink.clk


        .tinsel_0_temperature_val(178),
        .iopll_0_locked_export()
    );

  assign SI5340A0_RST_n = 1'b1;
  assign SI5340A1_RST_n = 1'b1;

  assign SI5340A0_OE_n = 1'b0;
  assign SI5340A1_OE_n = 1'b0;

endmodule