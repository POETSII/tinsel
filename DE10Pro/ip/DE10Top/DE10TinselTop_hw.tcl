# DE10TinselTop "DE10TinselTop" v1.0
package require -exact qsys 19.4


#
# module ErrorEstimatorCombined
#
set_module_property DESCRIPTION ""
set_module_property NAME DE10TinselTop
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property AUTHOR ""
set_module_property DISPLAY_NAME DE10TinselTop
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false
set_module_property LOAD_ELABORATION_LIMIT 0


#
# file sets
#
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL mkDE10Top
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file mkDE10Top.v VERILOG PATH ../../../rtl/mkDE10Top.v TOP_LEVEL_FILE

# clocks

add_interface clk clock end
set_interface_property clk ENABLED true
add_interface_port clk CLK clk Input 1

add_interface clk_mgmt clock end
set_interface_property clk_mgmt ENABLED true
add_interface_port clk_mgmt CLK_mgmt clk Input 1

add_interface clock_rx_a clock end
set_interface_property clock_rx_a ENABLED true
add_interface_port clock_rx_a CLK_rx_390_A clk Input 1

add_interface clock_rx_b clock end
set_interface_property clock_rx_b ENABLED true
add_interface_port clock_rx_b CLK_rx_390_B clk Input 1

add_interface clock_tx_a clock end
set_interface_property clock_tx_a ENABLED true
add_interface_port clock_tx_a CLK_tx_390_A clk Input 1

add_interface clock_tx_b clock end
set_interface_property clock_tx_b ENABLED true
add_interface_port clock_tx_b CLK_tx_390_B clk Input 1


# resets

add_interface rst reset end
set_interface_property rst associatedClock clk
set_interface_property rst synchronousEdges DEASSERT
set_interface_property rst ENABLED true
add_interface_port rst RST_N reset_n Input 1

add_interface rst_mgmt reset end
set_interface_property rst_mgmt associatedClock clk_mgmt
set_interface_property rst_mgmt synchronousEdges DEASSERT
set_interface_property rst_mgmt ENABLED true
add_interface_port rst_mgmt RST_N_mgmt_reset reset_n Input 1

add_interface reset_rx_A reset end
set_interface_property reset_rx_A associatedClock clock_rx_a
set_interface_property reset_rx_A synchronousEdges DEASSERT
set_interface_property reset_rx_A ENABLED true
add_interface_port reset_rx_A RST_N_rx_rst_A reset_n Input 1

add_interface reset_rx_B reset end
set_interface_property reset_rx_B associatedClock clock_rx_b
set_interface_property reset_rx_B synchronousEdges DEASSERT
set_interface_property reset_rx_B ENABLED true
add_interface_port reset_rx_B RST_N_rx_rst_B reset_n Input 1

add_interface reset_tx_A reset end
set_interface_property reset_tx_A associatedClock clock_tx_a
set_interface_property reset_tx_A synchronousEdges DEASSERT
set_interface_property reset_tx_A ENABLED true
add_interface_port reset_tx_A RST_N_tx_rst_A reset_n Input 1

add_interface reset_tx_B reset end
set_interface_property reset_tx_B associatedClock clock_tx_b
set_interface_property reset_tx_B synchronousEdges DEASSERT
set_interface_property reset_tx_B ENABLED true
add_interface_port reset_tx_B RST_N_tx_rst_B reset_n Input 1

# avalon interface number 0 - JTAG debug
add_interface jtag_uart avalon start
set_interface_property jtag_uart addressGroup 1
set_interface_property jtag_uart addressUnits WORDS
set_interface_property jtag_uart associatedClock clk_mgmt
set_interface_property jtag_uart associatedReset rst_mgmt
set_interface_property jtag_uart bitsPerSymbol 8
set_interface_property jtag_uart ENABLED true
set_interface_property jtag_uart EXPORT_OF ""
set_interface_property jtag_uart PORT_NAME_MAP ""
set_interface_property jtag_uart CMSIS_SVD_VARIABLES ""
set_interface_property jtag_uart SVD_ADDRESS_GROUP ""
set_interface_property jtag_uart IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port jtag_uart jtagIfc_uart_address address Output 1
add_interface_port jtag_uart jtagIfc_uart_writedata writedata Output 32
add_interface_port jtag_uart jtagIfc_uart_write write Output 1
add_interface_port jtag_uart jtagIfc_uart_read read Output 1
add_interface_port jtag_uart jtagIfc_uart_uart_readdata readdata Input 32
add_interface_port jtag_uart jtagIfc_uart_uart_waitrequest waitrequest Input 1

# avalon interface number 1 - DRAM ifc 0
add_interface dram0 avalon start
set_interface_property dram0 addressGroup 2
set_interface_property dram0 addressUnits WORDS
set_interface_property dram0 associatedClock clk
set_interface_property dram0 associatedReset rst
set_interface_property dram0 bitsPerSymbol 8
set_interface_property dram0 ENABLED true
set_interface_property dram0 EXPORT_OF ""
set_interface_property dram0 PORT_NAME_MAP ""
set_interface_property dram0 CMSIS_SVD_VARIABLES ""
set_interface_property dram0 SVD_ADDRESS_GROUP ""
set_interface_property dram0 IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port dram0 dramIfcs_0_m_address address Output 27
add_interface_port dram0 dramIfcs_0_m_writedata writedata Output 256
add_interface_port dram0 dramIfcs_0_m_write write Output 1
add_interface_port dram0 dramIfcs_0_m_read read Output 1
add_interface_port dram0 dramIfcs_0_m_burstcount burstcount Output 3
add_interface_port dram0 dramIfcs_0_m_readdata readdata Input 256
add_interface_port dram0 dramIfcs_0_m_readdatavalid readdatavalid Input 1
add_interface_port dram0 dramIfcs_0_m_waitrequest waitrequest Input 1

# avalon interface number 2 - DRAM ifc 1
add_interface dram1 avalon start
set_interface_property dram1 addressGroup 3
set_interface_property dram1 addressUnits WORDS
set_interface_property dram1 associatedClock clk
set_interface_property dram1 associatedReset rst
set_interface_property dram1 bitsPerSymbol 8
set_interface_property dram1 ENABLED true
set_interface_property dram1 EXPORT_OF ""
set_interface_property dram1 PORT_NAME_MAP ""
set_interface_property dram1 CMSIS_SVD_VARIABLES ""
set_interface_property dram1 SVD_ADDRESS_GROUP ""
set_interface_property dram1 IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port dram1 dramIfcs_1_m_address address Output 27
add_interface_port dram1 dramIfcs_1_m_writedata writedata Output 256
add_interface_port dram1 dramIfcs_1_m_write write Output 1
add_interface_port dram1 dramIfcs_1_m_read read Output 1
add_interface_port dram1 dramIfcs_1_m_burstcount burstcount Output 3
add_interface_port dram1 dramIfcs_1_m_readdata readdata Input 256
add_interface_port dram1 dramIfcs_1_m_readdatavalid readdatavalid Input 1
add_interface_port dram1 dramIfcs_1_m_waitrequest waitrequest Input 1


# avalon interface number 1
#
# connection point av_peripheral_s0
#
add_interface latency_tester_av_peripheral avalon end
set_interface_property latency_tester_av_peripheral addressGroup 0
set_interface_property latency_tester_av_peripheral addressUnits WORDS
set_interface_property latency_tester_av_peripheral associatedClock clk
set_interface_property latency_tester_av_peripheral associatedReset rst
set_interface_property latency_tester_av_peripheral bitsPerSymbol 8
set_interface_property latency_tester_av_peripheral bridgedAddressOffset ""
set_interface_property latency_tester_av_peripheral bridgesToMaster ""
set_interface_property latency_tester_av_peripheral burstOnBurstBoundariesOnly false
set_interface_property latency_tester_av_peripheral burstcountUnits WORDS
set_interface_property latency_tester_av_peripheral explicitAddressSpan 0
set_interface_property latency_tester_av_peripheral holdTime 0
set_interface_property latency_tester_av_peripheral linewrapBursts false
set_interface_property latency_tester_av_peripheral maximumPendingReadTransactions 0
set_interface_property latency_tester_av_peripheral maximumPendingWriteTransactions 0
set_interface_property latency_tester_av_peripheral minimumResponseLatency 1
set_interface_property latency_tester_av_peripheral readLatency 0
set_interface_property latency_tester_av_peripheral readWaitTime 1
set_interface_property latency_tester_av_peripheral setupTime 0
set_interface_property latency_tester_av_peripheral timingUnits Cycles
set_interface_property latency_tester_av_peripheral transparentBridge false
set_interface_property latency_tester_av_peripheral waitrequestAllowance 0
set_interface_property latency_tester_av_peripheral writeWaitTime 0
set_interface_property latency_tester_av_peripheral ENABLED true
set_interface_property latency_tester_av_peripheral EXPORT_OF ""
set_interface_property latency_tester_av_peripheral PORT_NAME_MAP ""
set_interface_property latency_tester_av_peripheral CMSIS_SVD_VARIABLES ""
set_interface_property latency_tester_av_peripheral SVD_ADDRESS_GROUP ""
set_interface_property latency_tester_av_peripheral IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port latency_tester_av_peripheral tester_s0_address address Input 4
add_interface_port latency_tester_av_peripheral tester_s0_writedata writedata Input 32
add_interface_port latency_tester_av_peripheral tester_s0_write write Input 1
add_interface_port latency_tester_av_peripheral tester_s0_read read Input 1
add_interface_port latency_tester_av_peripheral tester_s0_readdata readdata Output 32
add_interface_port latency_tester_av_peripheral tester_s0_waitrequest waitrequest Output 1
set_interface_assignment latency_tester_av_peripheral embeddedsw.configuration.isFlash 0
set_interface_assignment latency_tester_av_peripheral embeddedsw.configuration.isMemoryDevice 0
set_interface_assignment latency_tester_av_peripheral embeddedsw.configuration.isNonVolatileStorage 0
set_interface_assignment latency_tester_av_peripheral embeddedsw.configuration.isPrintableDevice 0
# MAC interfaces

# PORT A

#
# connection point rx
#
add_interface rx_a avalon_streaming end
set_interface_property rx_a associatedClock clock_rx_a
set_interface_property rx_a associatedReset reset_rx_A
set_interface_property rx_a dataBitsPerSymbol 8
set_interface_property rx_a errorDescriptor ""
set_interface_property rx_a firstSymbolInHighOrderBits true
set_interface_property rx_a maxChannel 0
set_interface_property rx_a readyAllowance 0
set_interface_property rx_a readyLatency 0
set_interface_property rx_a ENABLED true
set_interface_property rx_a EXPORT_OF ""
set_interface_property rx_a PORT_NAME_MAP ""
set_interface_property rx_a CMSIS_SVD_VARIABLES ""
set_interface_property rx_a SVD_ADDRESS_GROUP ""
set_interface_property rx_a IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port rx_a macA_l8_rx_error_error error Input 6
add_interface_port rx_a macA_l8_rx_valid_valid valid Input 1
add_interface_port rx_a macA_l8_rx_startofpacket_sop startofpacket Input 1
add_interface_port rx_a macA_l8_rx_endofpacket_eop endofpacket Input 1
add_interface_port rx_a macA_l8_rx_empty_empty empty Input 6
add_interface_port rx_a macA_l8_rx_data_data data Input 512


#
# connection point link_meta
#
add_interface link_meta_a conduit end
set_interface_property link_meta_a associatedClock ""
set_interface_property link_meta_a associatedReset ""
set_interface_property link_meta_a ENABLED true

add_interface_port link_meta_a macA_rx_block_lock_lock rx_block_lock Input 1
add_interface_port link_meta_a macA_tx_lanes_stable_stable tx_lanes_stable Input 1
add_interface_port link_meta_a macA_rx_pcs_ready_ready rx_pcs_ready Input 1
add_interface_port link_meta_a macA_rx_am_lock_lock rx_am_lock Input 1


#
# connection point tx
#
add_interface tx_a avalon_streaming start
set_interface_property tx_a associatedClock clock_tx_a
set_interface_property tx_a associatedReset reset_tx_A
set_interface_property tx_a dataBitsPerSymbol 8
set_interface_property tx_a errorDescriptor ""
set_interface_property tx_a firstSymbolInHighOrderBits true
set_interface_property tx_a maxChannel 0
set_interface_property tx_a readyAllowance 0
set_interface_property tx_a readyLatency 0
set_interface_property tx_a ENABLED true
set_interface_property tx_a EXPORT_OF ""
set_interface_property tx_a PORT_NAME_MAP ""
set_interface_property tx_a CMSIS_SVD_VARIABLES ""
set_interface_property tx_a SVD_ADDRESS_GROUP ""
set_interface_property tx_a IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port tx_a macA_l8_tx_endofpacket endofpacket Output 1
add_interface_port tx_a macA_l8_tx_startofpacket startofpacket Output 1
add_interface_port tx_a macA_l8_tx_valid valid Output 1
add_interface_port tx_a macA_l8_tx_ready_ready ready Input 1
add_interface_port tx_a macA_l8_tx_error error Output 1
add_interface_port tx_a macA_l8_tx_empty empty Output 6
add_interface_port tx_a macA_l8_tx_data data Output 512

# PORT B

#
# connection point rx
#
add_interface rx_b avalon_streaming end
set_interface_property rx_b associatedClock clock_rx_b
set_interface_property rx_b associatedReset reset_rx_B
set_interface_property rx_b dataBitsPerSymbol 8
set_interface_property rx_b errorDescriptor ""
set_interface_property rx_b firstSymbolInHighOrderBits true
set_interface_property rx_b maxChannel 0
set_interface_property rx_b readyAllowance 0
set_interface_property rx_b readyLatency 0
set_interface_property rx_b ENABLED true
set_interface_property rx_b EXPORT_OF ""
set_interface_property rx_b PORT_NAME_MAP ""
set_interface_property rx_b CMSIS_SVD_VARIABLES ""
set_interface_property rx_b SVD_ADDRESS_GROUP ""
set_interface_property rx_b IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port rx_b macB_l8_rx_error_error error Input 6
add_interface_port rx_b macB_l8_rx_valid_valid valid Input 1
add_interface_port rx_b macB_l8_rx_startofpacket_sop startofpacket Input 1
add_interface_port rx_b macB_l8_rx_endofpacket_eop endofpacket Input 1
add_interface_port rx_b macB_l8_rx_empty_empty empty Input 6
add_interface_port rx_b macB_l8_rx_data_data data Input 512


#
# connection point link_meta
#
add_interface link_meta_b conduit end
set_interface_property link_meta_b associatedClock ""
set_interface_property link_meta_b associatedReset ""
set_interface_property link_meta_b ENABLED true

add_interface_port link_meta_b macB_rx_block_lock_lock rx_block_lock Input 1
add_interface_port link_meta_b macB_tx_lanes_stable_stable tx_lanes_stable Input 1
add_interface_port link_meta_b macB_rx_pcs_ready_ready rx_pcs_ready Input 1
add_interface_port link_meta_b macB_rx_am_lock_lock rx_am_lock Input 1


#
# connection point tx
#
add_interface tx_b avalon_streaming start
set_interface_property tx_b associatedClock clock_tx_b
set_interface_property tx_b associatedReset reset_tx_B
set_interface_property tx_b dataBitsPerSymbol 8
set_interface_property tx_b errorDescriptor ""
set_interface_property tx_b firstSymbolInHighOrderBits true
set_interface_property tx_b maxChannel 0
set_interface_property tx_b readyAllowance 0
set_interface_property tx_b readyLatency 0
set_interface_property tx_b ENABLED true
set_interface_property tx_b EXPORT_OF ""
set_interface_property tx_b PORT_NAME_MAP ""
set_interface_property tx_b CMSIS_SVD_VARIABLES ""
set_interface_property tx_b SVD_ADDRESS_GROUP ""
set_interface_property tx_b IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port tx_b macB_l8_tx_endofpacket endofpacket Output 1
add_interface_port tx_b macB_l8_tx_startofpacket startofpacket Output 1
add_interface_port tx_b macB_l8_tx_valid valid Output 1
add_interface_port tx_b macB_l8_tx_ready_ready ready Input 1
add_interface_port tx_b macB_l8_tx_error error Output 1
add_interface_port tx_b macB_l8_tx_empty empty Output 6
add_interface_port tx_b macB_l8_tx_data data Output 512

# PCIe endpoint
add_interface axs_pcie2f axi4 end
set_interface_property axs_pcie2f associatedClock clk
set_interface_property axs_pcie2f associatedReset rst
set_interface_property axs_pcie2f readAcceptanceCapability 1
set_interface_property axs_pcie2f writeAcceptanceCapability 1
set_interface_property axs_pcie2f combinedAcceptanceCapability 1
set_interface_property axs_pcie2f readDataReorderingDepth 1
set_interface_property axs_pcie2f bridgesToMaster ""
set_interface_property axs_pcie2f ENABLED true
set_interface_property axs_pcie2f EXPORT_OF ""
set_interface_property axs_pcie2f PORT_NAME_MAP ""
set_interface_property axs_pcie2f CMSIS_SVD_VARIABLES ""
set_interface_property axs_pcie2f SVD_ADDRESS_GROUP ""
set_interface_property axs_pcie2f IPXACT_REGISTER_MAP_VARIABLES ""


add_interface_port axs_pcie2f axs_pcie2f_awready awready             Output 1
add_interface_port axs_pcie2f axs_pcie2f_wready wready              Output     1
add_interface_port axs_pcie2f axs_pcie2f_bid bid                 Output     4
add_interface_port axs_pcie2f axs_pcie2f_bresp bresp               Output     2
add_interface_port axs_pcie2f axs_pcie2f_bvalid bvalid              Output     1
add_interface_port axs_pcie2f axs_pcie2f_arready arready             Output     1
add_interface_port axs_pcie2f axs_pcie2f_rid rid                 Output     4
add_interface_port axs_pcie2f axs_pcie2f_rdata rdata               Output   32
add_interface_port axs_pcie2f axs_pcie2f_rresp rresp               Output     2
add_interface_port axs_pcie2f axs_pcie2f_rlast rlast               Output     1
add_interface_port axs_pcie2f axs_pcie2f_rvalid rvalid              Output     1
add_interface_port axs_pcie2f axs_pcie2f_awvalid awvalid             Input     1
add_interface_port axs_pcie2f axs_pcie2f_awid awid                Input     4
add_interface_port axs_pcie2f axs_pcie2f_awaddr awaddr              Input    35
add_interface_port axs_pcie2f axs_pcie2f_awlen awlen               Input     8
add_interface_port axs_pcie2f axs_pcie2f_awsize awsize              Input     3
add_interface_port axs_pcie2f axs_pcie2f_awburst awburst             Input     2
add_interface_port axs_pcie2f axs_pcie2f_awlock awlock              Input     1
add_interface_port axs_pcie2f axs_pcie2f_awcache awcache             Input     4
add_interface_port axs_pcie2f axs_pcie2f_awprot awprot              Input     3
add_interface_port axs_pcie2f axs_pcie2f_awqos awqos               Input     4
add_interface_port axs_pcie2f axs_pcie2f_awregion awregion            Input     4
add_interface_port axs_pcie2f axs_pcie2f_wvalid wvalid              Input     1
add_interface_port axs_pcie2f axs_pcie2f_wdata wdata               Input   32
add_interface_port axs_pcie2f axs_pcie2f_wstrb wstrb               Input    4
add_interface_port axs_pcie2f axs_pcie2f_wlast wlast               Input     1
add_interface_port axs_pcie2f axs_pcie2f_bready bready              Input     1
add_interface_port axs_pcie2f axs_pcie2f_arvalid arvalid             Input     1
add_interface_port axs_pcie2f axs_pcie2f_arid arid                Input     4
add_interface_port axs_pcie2f axs_pcie2f_araddr araddr              Input    35
add_interface_port axs_pcie2f axs_pcie2f_arlen arlen               Input     8
add_interface_port axs_pcie2f axs_pcie2f_arsize arsize              Input     3
add_interface_port axs_pcie2f axs_pcie2f_arburst arburst             Input     2
add_interface_port axs_pcie2f axs_pcie2f_arlock arlock              Input     1
add_interface_port axs_pcie2f axs_pcie2f_arcache arcache             Input     4
add_interface_port axs_pcie2f axs_pcie2f_arprot arprot              Input     3
add_interface_port axs_pcie2f axs_pcie2f_arqos arqos               Input     4
add_interface_port axs_pcie2f axs_pcie2f_arregion arregion            Input     4
add_interface_port axs_pcie2f axs_pcie2f_rready rready              Input     1
