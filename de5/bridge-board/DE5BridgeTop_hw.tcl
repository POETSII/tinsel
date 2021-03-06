# TCL File Generated by Component Editor 16.1
# Wed Aug 21 15:54:25 BST 2019
# DO NOT MODIFY


# 
# DE5BridgeTop "DE5BridgeTop" v1.0
#  2019.08.21.15:54:25
# 
# 

# 
# request TCL package from ACDS 16.1
# 
package require -exact qsys 16.1


# 
# module DE5BridgeTop
# 
set_module_property DESCRIPTION ""
set_module_property NAME DE5BridgeTop
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property AUTHOR ""
set_module_property DISPLAY_NAME DE5BridgeTop
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false


# 
# file sets
# 
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL de5BridgeTop
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file de5BridgeTop.v VERILOG PATH ../../rtl/de5BridgeTop.v TOP_LEVEL_FILE


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
set_interface_property clock clockRate 0
set_interface_property clock ENABLED true
set_interface_property clock EXPORT_OF ""
set_interface_property clock PORT_NAME_MAP ""
set_interface_property clock CMSIS_SVD_VARIABLES ""
set_interface_property clock SVD_ADDRESS_GROUP ""

add_interface_port clock CLK clk Input 1


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

add_interface_port reset_sink RST_N reset_n Input 1


# 
# connection point jtag
# 
add_interface jtag avalon start
set_interface_property jtag addressUnits SYMBOLS
set_interface_property jtag associatedClock clock
set_interface_property jtag associatedReset reset_sink
set_interface_property jtag bitsPerSymbol 8
set_interface_property jtag burstOnBurstBoundariesOnly false
set_interface_property jtag burstcountUnits WORDS
set_interface_property jtag doStreamReads false
set_interface_property jtag doStreamWrites false
set_interface_property jtag holdTime 0
set_interface_property jtag linewrapBursts false
set_interface_property jtag maximumPendingReadTransactions 0
set_interface_property jtag maximumPendingWriteTransactions 0
set_interface_property jtag readLatency 0
set_interface_property jtag readWaitTime 1
set_interface_property jtag setupTime 0
set_interface_property jtag timingUnits Cycles
set_interface_property jtag writeWaitTime 0
set_interface_property jtag ENABLED true
set_interface_property jtag EXPORT_OF ""
set_interface_property jtag PORT_NAME_MAP ""
set_interface_property jtag CMSIS_SVD_VARIABLES ""
set_interface_property jtag SVD_ADDRESS_GROUP ""

add_interface_port jtag jtagAvalon_uart_address address Output 3
add_interface_port jtag jtagAvalon_uart_read read Output 1
add_interface_port jtag jtagAvalon_uart_uart_readdata readdata Input 32
add_interface_port jtag jtagAvalon_uart_uart_waitrequest waitrequest Input 1
add_interface_port jtag jtagAvalon_uart_write write Output 1
add_interface_port jtag jtagAvalon_uart_writedata writedata Output 32


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
# connection point soft_reset
# 
add_interface soft_reset conduit end
set_interface_property soft_reset associatedClock clock
set_interface_property soft_reset associatedReset reset_sink
set_interface_property soft_reset ENABLED true
set_interface_property soft_reset EXPORT_OF ""
set_interface_property soft_reset PORT_NAME_MAP ""
set_interface_property soft_reset CMSIS_SVD_VARIABLES ""
set_interface_property soft_reset SVD_ADDRESS_GROUP ""

add_interface_port soft_reset resetReq val Output 1


# 
# connection point maca_source
# 
add_interface maca_source avalon_streaming start
set_interface_property maca_source associatedClock clock
set_interface_property maca_source associatedReset reset_sink
set_interface_property maca_source dataBitsPerSymbol 8
set_interface_property maca_source errorDescriptor ""
set_interface_property maca_source firstSymbolInHighOrderBits true
set_interface_property maca_source maxChannel 0
set_interface_property maca_source readyLatency 0
set_interface_property maca_source ENABLED true
set_interface_property maca_source EXPORT_OF ""
set_interface_property maca_source PORT_NAME_MAP ""
set_interface_property maca_source CMSIS_SVD_VARIABLES ""
set_interface_property maca_source SVD_ADDRESS_GROUP ""

add_interface_port maca_source macA_source_data data Output 64
add_interface_port maca_source macA_source_empty empty Output 3
add_interface_port maca_source macA_source_error error Output 1
add_interface_port maca_source macA_source_source_ready ready Input 1
add_interface_port maca_source macA_source_startofpacket startofpacket Output 1
add_interface_port maca_source macA_source_valid valid Output 1
add_interface_port maca_source macA_source_endofpacket endofpacket Output 1


# 
# connection point macb_source
# 
add_interface macb_source avalon_streaming start
set_interface_property macb_source associatedClock clock
set_interface_property macb_source associatedReset reset_sink
set_interface_property macb_source dataBitsPerSymbol 8
set_interface_property macb_source errorDescriptor ""
set_interface_property macb_source firstSymbolInHighOrderBits true
set_interface_property macb_source maxChannel 0
set_interface_property macb_source readyLatency 0
set_interface_property macb_source ENABLED true
set_interface_property macb_source EXPORT_OF ""
set_interface_property macb_source PORT_NAME_MAP ""
set_interface_property macb_source CMSIS_SVD_VARIABLES ""
set_interface_property macb_source SVD_ADDRESS_GROUP ""

add_interface_port macb_source macB_source_endofpacket endofpacket Output 1
add_interface_port macb_source macB_source_data data Output 64
add_interface_port macb_source macB_source_empty empty Output 3
add_interface_port macb_source macB_source_error error Output 1
add_interface_port macb_source macB_source_source_ready ready Input 1
add_interface_port macb_source macB_source_startofpacket startofpacket Output 1
add_interface_port macb_source macB_source_valid valid Output 1


# 
# connection point maca_sink
# 
add_interface maca_sink avalon_streaming end
set_interface_property maca_sink associatedClock clock
set_interface_property maca_sink associatedReset reset_sink
set_interface_property maca_sink dataBitsPerSymbol 8
set_interface_property maca_sink errorDescriptor ""
set_interface_property maca_sink firstSymbolInHighOrderBits true
set_interface_property maca_sink maxChannel 0
set_interface_property maca_sink readyLatency 0
set_interface_property maca_sink ENABLED true
set_interface_property maca_sink EXPORT_OF ""
set_interface_property maca_sink PORT_NAME_MAP ""
set_interface_property maca_sink CMSIS_SVD_VARIABLES ""
set_interface_property maca_sink SVD_ADDRESS_GROUP ""

add_interface_port maca_sink macA_sink_ready ready Output 1
add_interface_port maca_sink macA_sink_sink_data data Input 64
add_interface_port maca_sink macA_sink_sink_empty empty Input 3
add_interface_port maca_sink macA_sink_sink_endofpacket endofpacket Input 1
add_interface_port maca_sink macA_sink_sink_error error Input 6
add_interface_port maca_sink macA_sink_sink_startofpacket startofpacket Input 1
add_interface_port maca_sink macA_sink_sink_valid valid Input 1


# 
# connection point macb_sink
# 
add_interface macb_sink avalon_streaming end
set_interface_property macb_sink associatedClock clock
set_interface_property macb_sink associatedReset reset_sink
set_interface_property macb_sink dataBitsPerSymbol 8
set_interface_property macb_sink errorDescriptor ""
set_interface_property macb_sink firstSymbolInHighOrderBits true
set_interface_property macb_sink maxChannel 0
set_interface_property macb_sink readyLatency 0
set_interface_property macb_sink ENABLED true
set_interface_property macb_sink EXPORT_OF ""
set_interface_property macb_sink PORT_NAME_MAP ""
set_interface_property macb_sink CMSIS_SVD_VARIABLES ""
set_interface_property macb_sink SVD_ADDRESS_GROUP ""

add_interface_port macb_sink macB_sink_ready ready Output 1
add_interface_port macb_sink macB_sink_sink_data data Input 64
add_interface_port macb_sink macB_sink_sink_empty empty Input 3
add_interface_port macb_sink macB_sink_sink_endofpacket endofpacket Input 1
add_interface_port macb_sink macB_sink_sink_error error Input 6
add_interface_port macb_sink macB_sink_sink_startofpacket startofpacket Input 1
add_interface_port macb_sink macB_sink_sink_valid valid Input 1


# 
# connection point temperature
# 
add_interface temperature conduit end
set_interface_property temperature associatedClock clock
set_interface_property temperature associatedReset reset_sink
set_interface_property temperature ENABLED true
set_interface_property temperature EXPORT_OF ""
set_interface_property temperature PORT_NAME_MAP ""
set_interface_property temperature CMSIS_SVD_VARIABLES ""
set_interface_property temperature SVD_ADDRESS_GROUP ""

add_interface_port temperature setTemperature_temp temp_val Input 8

