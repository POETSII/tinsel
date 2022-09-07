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


  input EXP_EN,

  inout UFL_CLKIN_p,
  inout UFL_CLKIN_n
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

assign ddr4_status =
  {ddr4_b_status_local_cal_fail,
     ddr4_b_status_local_cal_success,
       ddr4_b_local_reset_done};

DE10_Pro_QSYS DE10_Pro_QSYS_inst (
        .clk_clk(CLK_50_B3I),
        .reset_reset(ninit_done),
        //
        // .ddr_b_emif_s10_ddr4_b_local_reset_req_local_reset_req         (ddr4_local_reset_req),         //   input,   width = 1,    ddr_b_emif_s10_ddr4_b_local_reset_req.local_reset_req
        // .ddr_b_emif_s10_ddr4_b_local_reset_status_local_reset_done     (ddr4_b_local_reset_done),     //  output,   width = 1, ddr_b_emif_s10_ddr4_b_local_reset_status.local_reset_done
        // .ddr_b_emif_s10_ddr4_b_pll_ref_clk_clk                         (DDR4B_REFCLK_p),                         //   input,   width = 1,        ddr_b_emif_s10_ddr4_b_pll_ref_clk.clk
        // .ddr_b_emif_s10_ddr4_b_oct_oct_rzqin                           (DDR4B_RZQ),                           //   input,   width = 1,                ddr_b_emif_s10_ddr4_b_oct.oct_rzqin
        // .ddr_b_emif_s10_ddr4_b_mem_mem_ck                              (DDR4B_CK),                              //  output,   width = 1,                ddr_b_emif_s10_ddr4_b_mem.mem_ck
        // .ddr_b_emif_s10_ddr4_b_mem_mem_ck_n                            (DDR4B_CK_n),                            //  output,   width = 1,                                         .mem_ck_n
        // .ddr_b_emif_s10_ddr4_b_mem_mem_a                               (DDR4B_A),                               //  output,  width = 17,                                         .mem_a
        // .ddr_b_emif_s10_ddr4_b_mem_mem_act_n                           (DDR4B_ACT_n),                           //  output,   width = 1,                                         .mem_act_n
        // .ddr_b_emif_s10_ddr4_b_mem_mem_ba                              (DDR4B_BA),                              //  output,   width = 2,                                         .mem_ba
        // .ddr_b_emif_s10_ddr4_b_mem_mem_bg                              (DDR4B_BG),                              //  output,   width = 2,                                         .mem_bg
        // .ddr_b_emif_s10_ddr4_b_mem_mem_cke                             (DDR4B_CKE),                             //  output,   width = 1,                                         .mem_cke
        // .ddr_b_emif_s10_ddr4_b_mem_mem_cs_n                            (DDR4B_CS_n),                            //  output,   width = 1,                                         .mem_cs_n
        // .ddr_b_emif_s10_ddr4_b_mem_mem_odt                             (DDR4B_ODT),                             //  output,   width = 1,                                         .mem_odt
        // .ddr_b_emif_s10_ddr4_b_mem_mem_reset_n                         (DDR4B_RESET_n),                         //  output,   width = 1,                                         .mem_reset_n
        // .ddr_b_emif_s10_ddr4_b_mem_mem_par                             (DDR4B_PAR),                             //  output,   width = 1,                                         .mem_par
        // .ddr_b_emif_s10_ddr4_b_mem_mem_alert_n                         (DDR4B_ALERT_n),                         //   input,   width = 1,                                         .mem_alert_n
        // .ddr_b_emif_s10_ddr4_b_mem_mem_dqs                             (DDR4B_DQS),                             //   inout,   width = 9,                                         .mem_dqs
        // .ddr_b_emif_s10_ddr4_b_mem_mem_dqs_n                           (DDR4B_DQS_n),                           //   inout,   width = 9,                                         .mem_dqs_n
        // .ddr_b_emif_s10_ddr4_b_mem_mem_dq                              (DDR4B_DQ),                              //   inout,  width = 72,                                         .mem_dq
        // .ddr_b_emif_s10_ddr4_b_mem_mem_dbi_n                           (DDR4B_DBI_n),                           //   inout,   width = 9,                                         .mem_dbi_n
        // .ddr_b_emif_s10_ddr4_b_status_local_cal_success                (ddr4_b_status_local_cal_fail),                //  output,   width = 1,             ddr_b_emif_s10_ddr4_b_status.local_cal_success
        // .ddr_b_emif_s10_ddr4_b_status_local_cal_fail                   (ddr4_b_status_local_cal_success),                   //  output,   width = 1,                                         .local_cal_fail

        // .emif_s10_ddr4_b_mem_mem_ck(DDR4B_CK),
        // .emif_s10_ddr4_b_mem_mem_ck_n(DDR4B_CK_n),
        // .emif_s10_ddr4_b_mem_mem_a(DDR4B_A),
        // .emif_s10_ddr4_b_mem_mem_act_n(DDR4B_ACT_n),
        // .emif_s10_ddr4_b_mem_mem_ba(DDR4B_BA),
        // .emif_s10_ddr4_b_mem_mem_bg(DDR4B_BG),
        // .emif_s10_ddr4_b_mem_mem_cke(DDR4B_CKE),
        // .emif_s10_ddr4_b_mem_mem_cs_n(DDR4B_CS_n),
        // .emif_s10_ddr4_b_mem_mem_odt(DDR4B_ODT),
        // .emif_s10_ddr4_b_mem_mem_reset_n(DDR4B_RESET_n),
        // .emif_s10_ddr4_b_mem_mem_par(DDR4B_PAR),
        // .emif_s10_ddr4_b_mem_mem_alert_n(DDR4B_ALERT_n),
        // .emif_s10_ddr4_b_mem_mem_dqs(DDR4B_DQS),
        // .emif_s10_ddr4_b_mem_mem_dqs_n(DDR4B_DQS_n),
        // .emif_s10_ddr4_b_mem_mem_dq(DDR4B_DQ),
        // .emif_s10_ddr4_b_mem_mem_dbi_n(DDR4B_DBI_n),
        // .emif_s10_ddr4_b_oct_oct_rzqin(DDR4B_RZQ),
        // .emif_s10_ddr4_b_pll_ref_clk_clk(DDR4B_REFCLK_p),
        // .emif_s10_ddr4_b_status_local_cal_success(ddr4_b_status_local_cal_success),
        // .emif_s10_ddr4_b_status_local_cal_fail(ddr4_b_status_local_cal_fail),
        // .ddr_c_emif_s10_ddr4_c_local_reset_req_local_reset_req(ddr4_local_reset_req),                   //   input,   width = 1,           ddr_c_emif_s10_ddr4_c_local_reset_req.local_reset_req
        // .ddr_c_emif_s10_ddr4_c_local_reset_status_local_reset_done(ddr4_c_local_reset_done),               //  output,   width = 1,        ddr_c_emif_s10_ddr4_c_local_reset_status.local_reset_done
        // .ddr_c_emif_s10_ddr4_c_mem_mem_ck(DDR4C_CK),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_ck_n(DDR4C_CK_n),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_a(DDR4C_A),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_act_n(DDR4C_ACT_n),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_ba(DDR4C_BA),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_bg(DDR4C_BG),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_cke(DDR4C_CKE),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_cs_n(DDR4C_CS_n),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_odt(DDR4C_ODT),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_reset_n(DDR4C_RESET_n),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_par(DDR4C_PAR),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_alert_n(DDR4C_ALERT_n),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_dqs(DDR4C_DQS),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_dqs_n(DDR4C_DQS_n),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_dq(DDR4C_DQ),
        // .ddr_c_emif_s10_ddr4_c_mem_mem_dbi_n(DDR4C_DBI_n),
        // .ddr_c_emif_s10_ddr4_c_oct_oct_rzqin(DDR4C_RZQ),
        // .ddr_c_emif_s10_ddr4_c_pll_ref_clk_clk(DDR4C_REFCLK_p),
        // .ddr_c_emif_s10_ddr4_c_status_local_cal_success(ddr4_c_status_local_cal_success),
        // .ddr_c_emif_s10_ddr4_c_status_local_cal_fail(ddr4_c_status_local_cal_fail),
        //
        // .ddr_d_emif_s10_ddr4_b_local_reset_req_local_reset_req         (ddr4_local_reset_req),         //   input,   width = 1,    ddr_b_emif_s10_ddr4_b_local_reset_req.local_reset_req
        // .ddr_d_emif_s10_ddr4_b_local_reset_status_local_reset_done     (ddr4_d_local_reset_done),     //  output,   width = 1, ddr_b_emif_s10_ddr4_b_local_reset_status.local_reset_done
        // .ddr_d_emif_s10_ddr4_b_pll_ref_clk_clk                         (DDR4D_REFCLK_p),                         //   input,   width = 1,        ddr_b_emif_s10_ddr4_b_pll_ref_clk.clk
        // .ddr_d_emif_s10_ddr4_b_oct_oct_rzqin                           (DDR4D_RZQ),                           //   input,   width = 1,                ddr_b_emif_s10_ddr4_b_oct.oct_rzqin
        // .ddr_d_emif_s10_ddr4_b_mem_mem_ck                              (DDR4D_CK),                              //  output,   width = 1,                ddr_b_emif_s10_ddr4_b_mem.mem_ck
        // .ddr_d_emif_s10_ddr4_b_mem_mem_ck_n                            (DDR4D_CK_n),                            //  output,   width = 1,                                         .mem_ck_n
        // .ddr_d_emif_s10_ddr4_b_mem_mem_a                               (DDR4D_A),                               //  output,  width = 17,                                         .mem_a
        // .ddr_d_emif_s10_ddr4_b_mem_mem_act_n                           (DDR4D_ACT_n),                           //  output,   width = 1,                                         .mem_act_n
        // .ddr_d_emif_s10_ddr4_b_mem_mem_ba                              (DDR4D_BA),                              //  output,   width = 2,                                         .mem_ba
        // .ddr_d_emif_s10_ddr4_b_mem_mem_bg                              (DDR4D_BG),                              //  output,   width = 2,                                         .mem_bg
        // .ddr_d_emif_s10_ddr4_b_mem_mem_cke                             (DDR4D_CKE),                             //  output,   width = 1,                                         .mem_cke
        // .ddr_d_emif_s10_ddr4_b_mem_mem_cs_n                            (DDR4D_CS_n),                            //  output,   width = 1,                                         .mem_cs_n
        // .ddr_d_emif_s10_ddr4_b_mem_mem_odt                             (DDR4D_ODT),                             //  output,   width = 1,                                         .mem_odt
        // .ddr_d_emif_s10_ddr4_b_mem_mem_reset_n                         (DDR4D_RESET_n),                         //  output,   width = 1,                                         .mem_reset_n
        // .ddr_d_emif_s10_ddr4_b_mem_mem_par                             (DDR4D_PAR),                             //  output,   width = 1,                                         .mem_par
        // .ddr_d_emif_s10_ddr4_b_mem_mem_alert_n                         (DDR4D_ALERT_n),                         //   input,   width = 1,                                         .mem_alert_n
        // .ddr_d_emif_s10_ddr4_b_mem_mem_dqs                             (DDR4D_DQS),                             //   inout,   width = 9,                                         .mem_dqs
        // .ddr_d_emif_s10_ddr4_b_mem_mem_dqs_n                           (DDR4D_DQS_n),                           //   inout,   width = 9,                                         .mem_dqs_n
        // .ddr_d_emif_s10_ddr4_b_mem_mem_dq                              (DDR4D_DQ),                              //   inout,  width = 72,                                         .mem_dq
        // .ddr_d_emif_s10_ddr4_b_mem_mem_dbi_n                           (DDR4D_DBI_n),                           //   inout,   width = 9,                                         .mem_dbi_n
        // .ddr_d_emif_s10_ddr4_b_status_local_cal_success                (ddr4_d_status_local_cal_success),                //  output,   width = 1,             ddr_b_emif_s10_ddr4_b_status.local_cal_success
        // .ddr_d_emif_s10_ddr4_b_status_local_cal_fail                   (ddr4_d_status_local_cal_fail),                   //  output,   width = 1,                                         .local_cal_fail


        // .ddr_a_local_reset_req_local_reset_req                         (ddr4_local_reset_req),         //   input,   width = 1,                 ddr_a_local_reset_req.local_reset_req
        // .ddr_a_local_reset_status_local_reset_done                     (ddr4_a_local_reset_done),     //  output,   width = 1,              ddr_a_local_reset_status.local_reset_done
        // .ddr_a_pll_ref_clk_clk                                         (DDR4A_REFCLK_p),                         //   input,   width = 1,                     ddr_a_pll_ref_clk.clk
        // .ddr_a_oct_oct_rzqin                                           (DDR4A_RZQ),                           //   input,   width = 1,                             ddr_a_oct.oct_rzqin
        // .ddr_a_mem_mem_ck                                              (DDR4A_CK),                              //  output,   width = 1,                             ddr_a_mem.mem_ck
        // .ddr_a_mem_mem_ck_n                                            (DDR4A_CK_n),                            //  output,   width = 1,                                      .mem_ck_n
        // .ddr_a_mem_mem_a                                               (DDR4A_A),                                                      //  output,  width = 17,                                      .mem_a
        // .ddr_a_mem_mem_act_n                                           (DDR4A_ACT_n),                                                  //  output,   width = 1,                                      .mem_act_n
        // .ddr_a_mem_mem_ba                                              (DDR4A_BA),                                                     //  output,   width = 2,                                      .mem_ba
        // .ddr_a_mem_mem_bg                                              (DDR4A_BG),                                                     //  output,   width = 2,                                      .mem_bg
        // .ddr_a_mem_mem_cke                                             (DDR4A_CKE),                                                    //  output,   width = 1,                                      .mem_cke
        // .ddr_a_mem_mem_cs_n                                            (DDR4A_CS_n),                                                   //  output,   width = 1,                                      .mem_cs_n
        // .ddr_a_mem_mem_odt                                             (DDR4A_ODT),                                                    //  output,   width = 1,                                      .mem_odt
        // .ddr_a_mem_mem_reset_n                                         (DDR4A_RESET_n),                                                //  output,   width = 1,                                      .mem_reset_n
        // .ddr_a_mem_mem_par                                             (DDR4A_PAR),                                                    //  output,   width = 1,                                      .mem_par
        // .ddr_a_mem_mem_alert_n                                         (DDR4A_ALERT_n),                                                //   input,   width = 1,                                      .mem_alert_n
        // .ddr_a_mem_mem_dqs                                             (DDR4A_DQS),                                                    //   inout,   width = 9,                                      .mem_dqs
        // .ddr_a_mem_mem_dqs_n                                           (DDR4A_DQS_n),                                                  //   inout,   width = 9,                                      .mem_dqs_n
        // .ddr_a_mem_mem_dq                                              (DDR4A_DQ),                                                     //   inout,  width = 72,                                      .mem_dq
        // .ddr_a_mem_mem_dbi_n                                           (DDR4A_DBI_n),                                                  //   inout,   width = 9,                                      .mem_dbi_n
        // .ddr_a_status_local_cal_success                                (ddr4_a_status_local_cal_success),                              //  output,   width = 1,                          ddr_a_status.local_cal_success
        // .ddr_a_status_local_cal_fail                                   (ddr4_a_status_local_cal_fail),                                 //  output,   width = 1,                                      .local_cal_fail

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

        .tinsel_0_boardid_val(0),
        .tinsel_0_temperature_val(178),
        .iopll_0_locked_export()
    );

assign SI5340A0_RST_n = 1'b1;
assign SI5340A1_RST_n = 1'b1;

assign SI5340A0_OE_n = 1'b0;
assign SI5340A1_OE_n = 1'b0;

endmodule
