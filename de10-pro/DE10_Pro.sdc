#**************************************************************
# This .sdc file is created by Terasic Tool.
# Users are recommended to modify this file to match users logic.
#**************************************************************

#**************************************************************
# Create Clock
#**************************************************************
# CLOCK
create_clock -period 10 [get_ports CLK_100_B3I]
create_clock -period 20 [get_ports CLK_50_B2C]
create_clock -period 20 [get_ports CLK_50_B2L]
create_clock -period 20 [get_ports CLK_50_B3C]
create_clock -period 20 [get_ports CLK_50_B3I]
create_clock -period 20 [get_ports CLK_50_B3L]
set CLK_100 [get_ports CLK_100_B3I]

create_clock -period "266.666666 MHz" [get_ports DDR4A_REFCLK_p]
create_clock -period "166.666666 MHz" [get_ports DDR4B_REFCLK_p]
create_clock -period "166.666666 MHz" [get_ports DDR4C_REFCLK_p]
create_clock -period "166.666666 MHz" [get_ports DDR4D_REFCLK_p]
create_clock -period "100.000000 MHz" [get_ports PCIE_REFCLK_p]

# create_clock -period "30.303 ns" -name {altera_reserved_tck} [get_ports {altera_reserved_tck}]
# set_clock_groups -asynchronous -group {altera_reserved_tck}


#**************************************************************
# Create Generated Clock
#**************************************************************
derive_pll_clocks


#**************************************************************
# Set Clock Latency
#**************************************************************



#**************************************************************
# Set Clock Uncertainty
#**************************************************************
derive_clock_uncertainty


#**************************************************************
# Set Input Delay
#**************************************************************



#**************************************************************
# Set Output Delay
#**************************************************************



#**************************************************************
# Set Clock Groups
#**************************************************************
set_clock_groups -asynchronous -group {[get_clocks { CLK_50_B3I }]}
set_clock_groups -asynchronous -group {[get_clocks { DDR4A_REFCLK_p }]}
set_clock_groups -asynchronous -group {[get_clocks { DDR4B_REFCLK_p }]}
set_clock_groups -asynchronous -group {[get_clocks { DDR4C_REFCLK_p }]}
set_clock_groups -asynchronous -group {[get_clocks { DDR4D_REFCLK_p }]}



#**************************************************************
# Set False Path
#**************************************************************
set_false_path -from * -to [get_cells {DE10_Pro_QSYS_inst|ddr4_status|pio_0|readdata*} ]
set_false_path -from [get_ports CPU_RESET_n*] -to *
set_false_path -from [get_ports BUTTON*] -to *

# The board id register distributes over the entire chip, but is
# essentially stable the whole time.  It changes only once
# (during initialisation), and has a very long period (many clock
# cycles) before it needs to stabilise.
set_false_path -from [get_keepers {*|debugLink_boardId*}]

set RX_CORE_CLK [get_clocks {*|xcvr|rx_clkout2|ch1}]
set TX_CORE_CLK [get_clocks {*|xcvr|tx_clkout2|ch1}]
set_clock_groups -exclusive -group $TX_CORE_CLK -group $RX_CORE_CLK -group $CLK_100

for {set chNum 0} {$chNum < 4} {incr chNum} {
  set RX_CLK [get_clocks *|xcvr|rx_pcs_x2_clk|ch$chNum]
  set TX_CLK [get_clocks *|xcvr|tx_pcs_x2_clk|ch$chNum]

  set_clock_groups -exclusive -group $TX_CLK -group $CLK_100
  set_clock_groups -exclusive -group $RX_CLK -group $CLK_100
}

set_false_path -from [get_clocks {ALTERA_INSERTED_INTOSC_FOR_TRS|divided_osc_clk}] -to [get_clocks {DE10_Pro_QSYS_inst|*|s10_100gmac|s10_100gmac|xcvr|*|*|*}]
set_false_path -from [get_clocks {DE10_Pro_QSYS_inst|*|s10_100gmac|s10_100gmac|xcvr|*|*|*}] -to [get_clocks {DE10_Pro_QSYS_inst|*|s10_100gmac|s10_100gmac|xcvr|*|*|*}]

#**************************************************************
# Set Multicycle Path
#**************************************************************



#**************************************************************
# Set Maximum Delay
#**************************************************************



#**************************************************************
# Set Minimum Delay
#**************************************************************



#**************************************************************
# Set Input Transition
#**************************************************************



#**************************************************************
# Set Load
#**************************************************************
