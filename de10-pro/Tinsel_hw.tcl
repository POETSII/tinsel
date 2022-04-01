# TCL File Generated by Component Editor 19.2
# Wed Nov 24 14:14:37 GMT 2021
# DO NOT MODIFY


#
# Tinsel "Tinsel" v1.0
#  2021.11.24.14:14:37
#
#

#
# request TCL package from ACDS 19.4
#
package require -exact qsys 19.4


#
# module Tinsel
#
set_module_property DESCRIPTION ""
set_module_property NAME Tinsel
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property AUTHOR ""
set_module_property DISPLAY_NAME Tinsel
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
add_fileset_file de5Top.v VERILOG PATH ../rtl/mkDE10Top.v TOP_LEVEL_FILE


#
# parameters
#


#
# display items
#


#
# connection point clock
#
add_interface clock clock end
set_interface_property clock ENABLED true
set_interface_property clock EXPORT_OF ""
set_interface_property clock PORT_NAME_MAP ""
set_interface_property clock CMSIS_SVD_VARIABLES ""
set_interface_property clock SVD_ADDRESS_GROUP ""
set_interface_property clock IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port clock CLK clk Input 1


#
# connection point reset_sink
#
add_interface reset_sink reset end
set_interface_property reset_sink associatedClock clock
set_interface_property reset_sink synchronousEdges DEASSERT
set_interface_property reset_sink ENABLED true
set_interface_property reset_sink EXPORT_OF ""
set_interface_property reset_sink PORT_NAME_MAP ""
set_interface_property reset_sink CMSIS_SVD_VARIABLES ""
set_interface_property reset_sink SVD_ADDRESS_GROUP ""
set_interface_property reset_sink IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port reset_sink RST_N reset_n Input 1


#
# connection point dram_master_0
#
add_interface dram_master_0 avalon start
set_interface_property dram_master_0 addressGroup 0
set_interface_property dram_master_0 addressUnits WORDS
set_interface_property dram_master_0 associatedClock clock
set_interface_property dram_master_0 associatedReset reset_sink
set_interface_property dram_master_0 bitsPerSymbol 8
set_interface_property dram_master_0 burstOnBurstBoundariesOnly false
set_interface_property dram_master_0 burstcountUnits WORDS
set_interface_property dram_master_0 doStreamReads false
set_interface_property dram_master_0 doStreamWrites false
set_interface_property dram_master_0 holdTime 0
set_interface_property dram_master_0 linewrapBursts false
set_interface_property dram_master_0 maximumPendingReadTransactions 0
set_interface_property dram_master_0 maximumPendingWriteTransactions 0
set_interface_property dram_master_0 minimumResponseLatency 1
set_interface_property dram_master_0 readLatency 0
set_interface_property dram_master_0 readWaitTime 1
set_interface_property dram_master_0 setupTime 0
set_interface_property dram_master_0 timingUnits Cycles
set_interface_property dram_master_0 waitrequestAllowance 0
set_interface_property dram_master_0 writeWaitTime 0
set_interface_property dram_master_0 ENABLED true
set_interface_property dram_master_0 EXPORT_OF ""
set_interface_property dram_master_0 PORT_NAME_MAP ""
set_interface_property dram_master_0 CMSIS_SVD_VARIABLES ""
set_interface_property dram_master_0 SVD_ADDRESS_GROUP ""
set_interface_property dram_master_0 IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port dram_master_0 dramIfcs_0_m_readdata readdata Input 256
add_interface_port dram_master_0 dramIfcs_0_m_readdatavalid readdatavalid Input 1
add_interface_port dram_master_0 dramIfcs_0_m_waitrequest waitrequest Input 1
add_interface_port dram_master_0 dramIfcs_0_m_writedata writedata Output 256
add_interface_port dram_master_0 dramIfcs_0_m_address address Output 27
add_interface_port dram_master_0 dramIfcs_0_m_read read Output 1
add_interface_port dram_master_0 dramIfcs_0_m_write write Output 1
add_interface_port dram_master_0 dramIfcs_0_m_burstcount burstcount Output 3


#
# connection point dram_master_1
#
add_interface dram_master_1 avalon start
set_interface_property dram_master_1 addressGroup 0
set_interface_property dram_master_1 addressUnits WORDS
set_interface_property dram_master_1 associatedClock clock
set_interface_property dram_master_1 associatedReset reset_sink
set_interface_property dram_master_1 bitsPerSymbol 8
set_interface_property dram_master_1 burstOnBurstBoundariesOnly false
set_interface_property dram_master_1 burstcountUnits WORDS
set_interface_property dram_master_1 doStreamReads false
set_interface_property dram_master_1 doStreamWrites false
set_interface_property dram_master_1 holdTime 0
set_interface_property dram_master_1 linewrapBursts false
set_interface_property dram_master_1 maximumPendingReadTransactions 0
set_interface_property dram_master_1 maximumPendingWriteTransactions 0
set_interface_property dram_master_1 minimumResponseLatency 1
set_interface_property dram_master_1 readLatency 0
set_interface_property dram_master_1 readWaitTime 1
set_interface_property dram_master_1 setupTime 0
set_interface_property dram_master_1 timingUnits Cycles
set_interface_property dram_master_1 waitrequestAllowance 0
set_interface_property dram_master_1 writeWaitTime 0
set_interface_property dram_master_1 ENABLED true
set_interface_property dram_master_1 EXPORT_OF ""
set_interface_property dram_master_1 PORT_NAME_MAP ""
set_interface_property dram_master_1 CMSIS_SVD_VARIABLES ""
set_interface_property dram_master_1 SVD_ADDRESS_GROUP ""
set_interface_property dram_master_1 IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port dram_master_1 dramIfcs_1_m_readdata readdata Input 256
add_interface_port dram_master_1 dramIfcs_1_m_readdatavalid readdatavalid Input 1
add_interface_port dram_master_1 dramIfcs_1_m_waitrequest waitrequest Input 1
add_interface_port dram_master_1 dramIfcs_1_m_writedata writedata Output 256
add_interface_port dram_master_1 dramIfcs_1_m_address address Output 27
add_interface_port dram_master_1 dramIfcs_1_m_read read Output 1
add_interface_port dram_master_1 dramIfcs_1_m_write write Output 1
add_interface_port dram_master_1 dramIfcs_1_m_burstcount burstcount Output 3

#
# connection point dram_master_2
#
add_interface dram_master_2 avalon start
set_interface_property dram_master_2 addressGroup 0
set_interface_property dram_master_2 addressUnits WORDS
set_interface_property dram_master_2 associatedClock clock
set_interface_property dram_master_2 associatedReset reset_sink
set_interface_property dram_master_2 bitsPerSymbol 8
set_interface_property dram_master_2 burstOnBurstBoundariesOnly false
set_interface_property dram_master_2 burstcountUnits WORDS
set_interface_property dram_master_2 doStreamReads false
set_interface_property dram_master_2 doStreamWrites false
set_interface_property dram_master_2 holdTime 0
set_interface_property dram_master_2 linewrapBursts false
set_interface_property dram_master_2 maximumPendingReadTransactions 0
set_interface_property dram_master_2 maximumPendingWriteTransactions 0
set_interface_property dram_master_2 minimumResponseLatency 1
set_interface_property dram_master_2 readLatency 0
set_interface_property dram_master_2 readWaitTime 1
set_interface_property dram_master_2 setupTime 0
set_interface_property dram_master_2 timingUnits Cycles
set_interface_property dram_master_2 waitrequestAllowance 0
set_interface_property dram_master_2 writeWaitTime 0
set_interface_property dram_master_2 ENABLED true
set_interface_property dram_master_2 EXPORT_OF ""
set_interface_property dram_master_2 PORT_NAME_MAP ""
set_interface_property dram_master_2 CMSIS_SVD_VARIABLES ""
set_interface_property dram_master_2 SVD_ADDRESS_GROUP ""
set_interface_property dram_master_2 IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port dram_master_2 dramIfcs_2_m_readdata readdata Input 256
add_interface_port dram_master_2 dramIfcs_2_m_readdatavalid readdatavalid Input 1
add_interface_port dram_master_2 dramIfcs_2_m_waitrequest waitrequest Input 1
add_interface_port dram_master_2 dramIfcs_2_m_writedata writedata Output 256
add_interface_port dram_master_2 dramIfcs_2_m_address address Output 27
add_interface_port dram_master_2 dramIfcs_2_m_read read Output 1
add_interface_port dram_master_2 dramIfcs_2_m_write write Output 1
add_interface_port dram_master_2 dramIfcs_2_m_burstcount burstcount Output 3

#
# connection point dram_master_3
#
add_interface dram_master_3 avalon start
set_interface_property dram_master_3 addressGroup 0
set_interface_property dram_master_3 addressUnits WORDS
set_interface_property dram_master_3 associatedClock clock
set_interface_property dram_master_3 associatedReset reset_sink
set_interface_property dram_master_3 bitsPerSymbol 8
set_interface_property dram_master_3 burstOnBurstBoundariesOnly false
set_interface_property dram_master_3 burstcountUnits WORDS
set_interface_property dram_master_3 doStreamReads false
set_interface_property dram_master_3 doStreamWrites false
set_interface_property dram_master_3 holdTime 0
set_interface_property dram_master_3 linewrapBursts false
set_interface_property dram_master_3 maximumPendingReadTransactions 0
set_interface_property dram_master_3 maximumPendingWriteTransactions 0
set_interface_property dram_master_3 minimumResponseLatency 1
set_interface_property dram_master_3 readLatency 0
set_interface_property dram_master_3 readWaitTime 1
set_interface_property dram_master_3 setupTime 0
set_interface_property dram_master_3 timingUnits Cycles
set_interface_property dram_master_3 waitrequestAllowance 0
set_interface_property dram_master_3 writeWaitTime 0
set_interface_property dram_master_3 ENABLED true
set_interface_property dram_master_3 EXPORT_OF ""
set_interface_property dram_master_3 PORT_NAME_MAP ""
set_interface_property dram_master_3 CMSIS_SVD_VARIABLES ""
set_interface_property dram_master_3 SVD_ADDRESS_GROUP ""
set_interface_property dram_master_3 IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port dram_master_3 dramIfcs_3_m_readdata readdata Input 256
add_interface_port dram_master_3 dramIfcs_3_m_readdatavalid readdatavalid Input 1
add_interface_port dram_master_3 dramIfcs_3_m_waitrequest waitrequest Input 1
add_interface_port dram_master_3 dramIfcs_3_m_writedata writedata Output 256
add_interface_port dram_master_3 dramIfcs_3_m_address address Output 27
add_interface_port dram_master_3 dramIfcs_3_m_read read Output 1
add_interface_port dram_master_3 dramIfcs_3_m_write write Output 1
add_interface_port dram_master_3 dramIfcs_3_m_burstcount burstcount Output 3

#
# connection point temperature
#
add_interface temperature conduit end
set_interface_property temperature associatedClock clock
set_interface_property temperature associatedReset ""
set_interface_property temperature ENABLED true
set_interface_property temperature EXPORT_OF ""
set_interface_property temperature PORT_NAME_MAP ""
set_interface_property temperature CMSIS_SVD_VARIABLES ""
set_interface_property temperature SVD_ADDRESS_GROUP ""
set_interface_property temperature IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port temperature setTemperature_temp val Input 8


#
# connection point jtag_master
#
add_interface jtag_master avalon start
set_interface_property jtag_master addressGroup 0
set_interface_property jtag_master addressUnits SYMBOLS
set_interface_property jtag_master associatedClock clock
set_interface_property jtag_master associatedReset reset_sink
set_interface_property jtag_master bitsPerSymbol 8
set_interface_property jtag_master burstOnBurstBoundariesOnly false
set_interface_property jtag_master burstcountUnits WORDS
set_interface_property jtag_master doStreamReads false
set_interface_property jtag_master doStreamWrites false
set_interface_property jtag_master holdTime 0
set_interface_property jtag_master linewrapBursts false
set_interface_property jtag_master maximumPendingReadTransactions 0
set_interface_property jtag_master maximumPendingWriteTransactions 0
set_interface_property jtag_master minimumResponseLatency 1
set_interface_property jtag_master readLatency 0
set_interface_property jtag_master readWaitTime 1
set_interface_property jtag_master setupTime 0
set_interface_property jtag_master timingUnits Cycles
set_interface_property jtag_master waitrequestAllowance 0
set_interface_property jtag_master writeWaitTime 0
set_interface_property jtag_master ENABLED true
set_interface_property jtag_master EXPORT_OF ""
set_interface_property jtag_master PORT_NAME_MAP ""
set_interface_property jtag_master CMSIS_SVD_VARIABLES ""
set_interface_property jtag_master SVD_ADDRESS_GROUP ""
set_interface_property jtag_master IPXACT_REGISTER_MAP_VARIABLES ""

add_interface_port jtag_master jtagIfc_uart_address address Output 3
add_interface_port jtag_master jtagIfc_uart_writedata writedata Output 32
add_interface_port jtag_master jtagIfc_uart_write write Output 1
add_interface_port jtag_master jtagIfc_uart_read read Output 1
add_interface_port jtag_master jtagIfc_uart_uart_waitrequest waitrequest Input 1
add_interface_port jtag_master jtagIfc_uart_uart_readdata readdata Input 32


#
# connection point pcie_host_bus
#
add_interface pcie_host_bus avalon start
set_interface_property pcie_host_bus addressUnits SYMBOLS
set_interface_property pcie_host_bus associatedClock clock
set_interface_property pcie_host_bus associatedReset reset_sink
set_interface_property pcie_host_bus bitsPerSymbol 8
set_interface_property pcie_host_bus burstOnBurstBoundariesOnly false
set_interface_property pcie_host_bus burstcountUnits WORDS
set_interface_property pcie_host_bus doStreamReads false
set_interface_property pcie_host_bus doStreamWrites false
set_interface_property pcie_host_bus holdTime 0
set_interface_property pcie_host_bus linewrapBursts false
set_interface_property pcie_host_bus maximumPendingReadTransactions 0
set_interface_property pcie_host_bus maximumPendingWriteTransactions 0
set_interface_property pcie_host_bus readLatency 0
set_interface_property pcie_host_bus readWaitTime 1
set_interface_property pcie_host_bus setupTime 0
set_interface_property pcie_host_bus timingUnits Cycles
set_interface_property pcie_host_bus writeWaitTime 0
set_interface_property pcie_host_bus ENABLED true
set_interface_property pcie_host_bus EXPORT_OF ""
set_interface_property pcie_host_bus PORT_NAME_MAP ""
set_interface_property pcie_host_bus CMSIS_SVD_VARIABLES ""
set_interface_property pcie_host_bus SVD_ADDRESS_GROUP ""

add_interface_port pcie_host_bus pcieHostBus_m_readdata readdata Input 128
add_interface_port pcie_host_bus pcieHostBus_m_readdatavalid readdatavalid Input 1
add_interface_port pcie_host_bus pcieHostBus_m_waitrequest waitrequest Input 1
add_interface_port pcie_host_bus pcieHostBus_m_writedata writedata Output 128
add_interface_port pcie_host_bus pcieHostBus_m_address address Output 64
add_interface_port pcie_host_bus pcieHostBus_m_read read Output 1
add_interface_port pcie_host_bus pcieHostBus_m_write write Output 1
add_interface_port pcie_host_bus pcieHostBus_m_burstcount burstcount Output 4
add_interface_port pcie_host_bus pcieHostBus_m_byteenable byteenable Output 16

#
# connection point controlbar_s
#
add_interface controlbar_s avalon end
set_interface_property controlbar_s addressUnits WORDS
set_interface_property controlbar_s associatedClock clock
set_interface_property controlbar_s associatedReset reset_sink
set_interface_property controlbar_s bitsPerSymbol 8
set_interface_property controlbar_s burstOnBurstBoundariesOnly false
set_interface_property controlbar_s burstcountUnits WORDS
set_interface_property controlbar_s explicitAddressSpan 0
set_interface_property controlbar_s holdTime 0
set_interface_property controlbar_s linewrapBursts false
set_interface_property controlbar_s maximumPendingReadTransactions 1
set_interface_property controlbar_s maximumPendingWriteTransactions 0
set_interface_property controlbar_s readLatency 0
set_interface_property controlbar_s readWaitTime 1
set_interface_property controlbar_s setupTime 0
set_interface_property controlbar_s timingUnits Cycles
set_interface_property controlbar_s writeWaitTime 0
set_interface_property controlbar_s ENABLED true
set_interface_property controlbar_s EXPORT_OF ""
set_interface_property controlbar_s PORT_NAME_MAP ""
set_interface_property controlbar_s CMSIS_SVD_VARIABLES ""
set_interface_property controlbar_s SVD_ADDRESS_GROUP ""

add_interface_port controlbar_s controlBAR_s_writedata writedata Input 128
add_interface_port controlbar_s controlBAR_s_address address Input 4
add_interface_port controlbar_s controlBAR_s_read read Input 1
add_interface_port controlbar_s controlBAR_s_write write Input 1
add_interface_port controlbar_s controlBAR_s_byteenable byteenable Input 16
add_interface_port controlbar_s controlBAR_s_readdata readdata Output 128
add_interface_port controlbar_s controlBAR_s_readdatavalid readdatavalid Output 1
add_interface_port controlbar_s controlBAR_s_waitrequest waitrequest Output 1
set_interface_assignment controlbar_s embeddedsw.configuration.isFlash 0
set_interface_assignment controlbar_s embeddedsw.configuration.isMemoryDevice 0
set_interface_assignment controlbar_s embeddedsw.configuration.isNonVolatileStorage 0
set_interface_assignment controlbar_s embeddedsw.configuration.isPrintableDevice 0

add_interface rst_req reset start
set_interface_property rst_req associatedClock clock
set_interface_property rst_req synchronousEdges DEASSERT
set_interface_property rst_req ENABLED true
add_interface_port rst_req resetReq reset_req Output 1


#
# connection point northmac_source
#
add_interface northmac_source avalon_streaming start
set_interface_property northmac_source associatedClock clock
set_interface_property northmac_source associatedReset reset_sink
set_interface_property northmac_source dataBitsPerSymbol 8
set_interface_property northmac_source errorDescriptor ""
set_interface_property northmac_source firstSymbolInHighOrderBits true
set_interface_property northmac_source maxChannel 0
set_interface_property northmac_source readyLatency 0
set_interface_property northmac_source ENABLED true
set_interface_property northmac_source EXPORT_OF ""
set_interface_property northmac_source PORT_NAME_MAP ""
set_interface_property northmac_source CMSIS_SVD_VARIABLES ""
set_interface_property northmac_source SVD_ADDRESS_GROUP ""

add_interface_port northmac_source northMac_0_source_endofpacket endofpacket Output 1
add_interface_port northmac_source northMac_0_source_data data Output 64
add_interface_port northmac_source northMac_0_source_source_ready ready Input 1
add_interface_port northmac_source northMac_0_source_startofpacket startofpacket Output 1
add_interface_port northmac_source northMac_0_source_valid valid Output 1
add_interface_port northmac_source northMac_0_source_empty empty Output 3
add_interface_port northmac_source northMac_0_source_error error Output 1


#
# connection point southmac_source
#
add_interface southmac_source avalon_streaming start
set_interface_property southmac_source associatedClock clock
set_interface_property southmac_source associatedReset reset_sink
set_interface_property southmac_source dataBitsPerSymbol 8
set_interface_property southmac_source errorDescriptor ""
set_interface_property southmac_source firstSymbolInHighOrderBits true
set_interface_property southmac_source maxChannel 0
set_interface_property southmac_source readyLatency 0
set_interface_property southmac_source ENABLED true
set_interface_property southmac_source EXPORT_OF ""
set_interface_property southmac_source PORT_NAME_MAP ""
set_interface_property southmac_source CMSIS_SVD_VARIABLES ""
set_interface_property southmac_source SVD_ADDRESS_GROUP ""

add_interface_port southmac_source southMac_0_source_endofpacket endofpacket Output 1
add_interface_port southmac_source southMac_0_source_data data Output 64
add_interface_port southmac_source southMac_0_source_source_ready ready Input 1
add_interface_port southmac_source southMac_0_source_startofpacket startofpacket Output 1
add_interface_port southmac_source southMac_0_source_valid valid Output 1
add_interface_port southmac_source southMac_0_source_empty empty Output 3
add_interface_port southmac_source southMac_0_source_error error Output 1

#
# connection point eastmac_source
#
add_interface eastmac_source avalon_streaming start
set_interface_property eastmac_source associatedClock clock
set_interface_property eastmac_source associatedReset reset_sink
set_interface_property eastmac_source dataBitsPerSymbol 8
set_interface_property eastmac_source errorDescriptor ""
set_interface_property eastmac_source firstSymbolInHighOrderBits true
set_interface_property eastmac_source maxChannel 0
set_interface_property eastmac_source readyLatency 0
set_interface_property eastmac_source ENABLED true
set_interface_property eastmac_source EXPORT_OF ""
set_interface_property eastmac_source PORT_NAME_MAP ""
set_interface_property eastmac_source CMSIS_SVD_VARIABLES ""
set_interface_property eastmac_source SVD_ADDRESS_GROUP ""

add_interface_port eastmac_source eastMac_0_source_endofpacket endofpacket Output 1
add_interface_port eastmac_source eastMac_0_source_data data Output 64
add_interface_port eastmac_source eastMac_0_source_source_ready ready Input 1
add_interface_port eastmac_source eastMac_0_source_startofpacket startofpacket Output 1
add_interface_port eastmac_source eastMac_0_source_valid valid Output 1
add_interface_port eastmac_source eastMac_0_source_empty empty Output 3
add_interface_port eastmac_source eastMac_0_source_error error Output 1

#
# connection point westmac_source
#
add_interface westmac_source avalon_streaming start
set_interface_property westmac_source associatedClock clock
set_interface_property westmac_source associatedReset reset_sink
set_interface_property westmac_source dataBitsPerSymbol 8
set_interface_property westmac_source errorDescriptor ""
set_interface_property westmac_source firstSymbolInHighOrderBits true
set_interface_property westmac_source maxChannel 0
set_interface_property westmac_source readyLatency 0
set_interface_property westmac_source ENABLED true
set_interface_property westmac_source EXPORT_OF ""
set_interface_property westmac_source PORT_NAME_MAP ""
set_interface_property westmac_source CMSIS_SVD_VARIABLES ""
set_interface_property westmac_source SVD_ADDRESS_GROUP ""

add_interface_port westmac_source westMac_0_source_endofpacket endofpacket Output 1
add_interface_port westmac_source westMac_0_source_data data Output 64
add_interface_port westmac_source westMac_0_source_source_ready ready Input 1
add_interface_port westmac_source westMac_0_source_startofpacket startofpacket Output 1
add_interface_port westmac_source westMac_0_source_valid valid Output 1
add_interface_port westmac_source westMac_0_source_empty empty Output 3
add_interface_port westmac_source westMac_0_source_error error Output 1


#
# connection point northmac_sink
#
add_interface northmac_sink avalon_streaming end
set_interface_property northmac_sink associatedClock clock
set_interface_property northmac_sink associatedReset reset_sink
set_interface_property northmac_sink dataBitsPerSymbol 8
set_interface_property northmac_sink errorDescriptor ""
set_interface_property northmac_sink firstSymbolInHighOrderBits true
set_interface_property northmac_sink maxChannel 0
set_interface_property northmac_sink readyLatency 0
set_interface_property northmac_sink ENABLED true
set_interface_property northmac_sink EXPORT_OF ""
set_interface_property northmac_sink PORT_NAME_MAP ""
set_interface_property northmac_sink CMSIS_SVD_VARIABLES ""
set_interface_property northmac_sink SVD_ADDRESS_GROUP ""

add_interface_port northmac_sink northMac_0_sink_ready ready Output 1
add_interface_port northmac_sink northMac_0_sink_sink_data data Input 64
add_interface_port northmac_sink northMac_0_sink_sink_endofpacket endofpacket Input 1
add_interface_port northmac_sink northMac_0_sink_sink_error error Input 6
add_interface_port northmac_sink northMac_0_sink_sink_startofpacket startofpacket Input 1
add_interface_port northmac_sink northMac_0_sink_sink_valid valid Input 1
add_interface_port northmac_sink northMac_0_sink_sink_empty empty Input 3

#
# connection point southmac_sink
#
add_interface southmac_sink avalon_streaming end
set_interface_property southmac_sink associatedClock clock
set_interface_property southmac_sink associatedReset reset_sink
set_interface_property southmac_sink dataBitsPerSymbol 8
set_interface_property southmac_sink errorDescriptor ""
set_interface_property southmac_sink firstSymbolInHighOrderBits true
set_interface_property southmac_sink maxChannel 0
set_interface_property southmac_sink readyLatency 0
set_interface_property southmac_sink ENABLED true
set_interface_property southmac_sink EXPORT_OF ""
set_interface_property southmac_sink PORT_NAME_MAP ""
set_interface_property southmac_sink CMSIS_SVD_VARIABLES ""
set_interface_property southmac_sink SVD_ADDRESS_GROUP ""

add_interface_port southmac_sink southMac_0_sink_ready ready Output 1
add_interface_port southmac_sink southMac_0_sink_sink_data data Input 64
add_interface_port southmac_sink southMac_0_sink_sink_endofpacket endofpacket Input 1
add_interface_port southmac_sink southMac_0_sink_sink_error error Input 6
add_interface_port southmac_sink southMac_0_sink_sink_startofpacket startofpacket Input 1
add_interface_port southmac_sink southMac_0_sink_sink_valid valid Input 1
add_interface_port southmac_sink southMac_0_sink_sink_empty empty Input 3

#
# connection point eastmac_sink
#
add_interface eastmac_sink avalon_streaming end
set_interface_property eastmac_sink associatedClock clock
set_interface_property eastmac_sink associatedReset reset_sink
set_interface_property eastmac_sink dataBitsPerSymbol 8
set_interface_property eastmac_sink errorDescriptor ""
set_interface_property eastmac_sink firstSymbolInHighOrderBits true
set_interface_property eastmac_sink maxChannel 0
set_interface_property eastmac_sink readyLatency 0
set_interface_property eastmac_sink ENABLED true
set_interface_property eastmac_sink EXPORT_OF ""
set_interface_property eastmac_sink PORT_NAME_MAP ""
set_interface_property eastmac_sink CMSIS_SVD_VARIABLES ""
set_interface_property eastmac_sink SVD_ADDRESS_GROUP ""

add_interface_port eastmac_sink eastMac_0_sink_ready ready Output 1
add_interface_port eastmac_sink eastMac_0_sink_sink_data data Input 64
add_interface_port eastmac_sink eastMac_0_sink_sink_endofpacket endofpacket Input 1
add_interface_port eastmac_sink eastMac_0_sink_sink_error error Input 6
add_interface_port eastmac_sink eastMac_0_sink_sink_startofpacket startofpacket Input 1
add_interface_port eastmac_sink eastMac_0_sink_sink_valid valid Input 1
add_interface_port eastmac_sink eastMac_0_sink_sink_empty empty Input 3

#
# connection point westmac_sink
#
add_interface westmac_sink avalon_streaming end
set_interface_property westmac_sink associatedClock clock
set_interface_property westmac_sink associatedReset reset_sink
set_interface_property westmac_sink dataBitsPerSymbol 8
set_interface_property westmac_sink errorDescriptor ""
set_interface_property westmac_sink firstSymbolInHighOrderBits true
set_interface_property westmac_sink maxChannel 0
set_interface_property westmac_sink readyLatency 0
set_interface_property westmac_sink ENABLED true
set_interface_property westmac_sink EXPORT_OF ""
set_interface_property westmac_sink PORT_NAME_MAP ""
set_interface_property westmac_sink CMSIS_SVD_VARIABLES ""
set_interface_property westmac_sink SVD_ADDRESS_GROUP ""

add_interface_port westmac_sink westMac_0_sink_ready ready Output 1
add_interface_port westmac_sink westMac_0_sink_sink_data data Input 64
add_interface_port westmac_sink westMac_0_sink_sink_endofpacket endofpacket Input 1
add_interface_port westmac_sink westMac_0_sink_sink_error error Input 6
add_interface_port westmac_sink westMac_0_sink_sink_startofpacket startofpacket Input 1
add_interface_port westmac_sink westMac_0_sink_sink_valid valid Input 1
add_interface_port westmac_sink westMac_0_sink_sink_empty empty Input 3
