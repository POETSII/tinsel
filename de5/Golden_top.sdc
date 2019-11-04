
#**************************************************************
# Create Clock
#**************************************************************

create_clock -period 20 [get_ports OSC_50_B3B]
create_clock -period 20 [get_ports OSC_50_B3D]
create_clock -period 20 [get_ports OSC_50_B4A]
create_clock -period 20 [get_ports OSC_50_B4D]

create_clock -period 20 [get_ports OSC_50_B7A]
create_clock -period 20 [get_ports OSC_50_B7D]
create_clock -period 20 [get_ports OSC_50_B8A]
create_clock -period 20 [get_ports OSC_50_B8D]


create_clock -period 1.5515 [get_ports SFP_REFCLK_p]

#**************************************************************
# JTAG
#**************************************************************

create_clock -period "30.303 ns" -name {altera_reserved_tck} [get_ports {altera_reserved_tck}]

set_clock_groups -asynchronous -group {altera_reserved_tck}

#**************************************************************
# False paths
#**************************************************************

# The board id register distributes over the entire chip, but is
# essentially stable the whole time.  It changes only once
# (during initialisation), and has a very long period (many clock
# cycles) before it needs to stabilise.
set_false_path -from [get_keepers {S5_DDR3_QSYS:u0|de5Top:de5top_0|debugLink_boardId*}] 

# For temperature measurement.  Limits timing due to clock crossing,
# but is a fairly stable signal, and functionality is not essential.
set_false_path -from [get_keepers {temp_display:temp_display_inst|display_reg*}]

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



#**************************************************************
# Set False Path
#**************************************************************



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





