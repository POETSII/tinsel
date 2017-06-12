
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

create_clock -period "100.0 MHz" [get_ports PCIE_REFCLK_p]

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





