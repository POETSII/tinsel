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

  input wire [3:0] SW
);

wire clk_50mhz = OSC_50_B7A;
wire rst_50mhz_n;

assign rst_50mhz_n = 1;

/*
wire ddr3_local_init_done;
wire ddr3_local_cal_success;
wire ddr3_2_local_init_done;
wire ddr3_2_local_cal_success;
*/

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

reg mac_rst_n = 0;
reg [3:0] mac_rst_count = 0;
always @(posedge clk_50mhz) begin
  if (mac_rst_count == 10) mac_rst_n <= 1;
  else mac_rst_count <= mac_rst_count+1;
end

SoC system (
        .clk_clk                                   (clk_50mhz),
        .reset_reset_n                             (rst_50mhz_n),
        //.board_id_id                               (SW),

/*
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
*/

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

        .mac_mm_clk_clk(clk_50mhz),
        .mac_mm_rst_reset_n(mac_rst_n),
      
        .mac_0_rx_ready_export(),
        .mac_0_rx_serial_data_export(SFPA_RX_p),
        .mac_0_tx_ready_export(),
        .mac_0_tx_serial_data_export(SFPA_TX_p),
        .mac_0_ref_clk_clk(SFP_REFCLK_p),
        .mac_0_ref_rst_reset_n(1),
        .mac_0_tx_reset_reset_n(1)

);
  
endmodule 
