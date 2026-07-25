# Nexys A7-100T constraints for the M0 blinky.
# Pins from Digilent Nexys-A7-100T-Master.xdc (github.com/Digilent/digilent-xdc).

# 100 MHz system clock
set_property -dict { PACKAGE_PIN E3  IOSTANDARD LVCMOS33 } [get_ports clk100mhz]
create_clock -period 10.000 -name sys_clk [get_ports clk100mhz]

# CPU_RESETN pushbutton (active low)
set_property -dict { PACKAGE_PIN C12 IOSTANDARD LVCMOS33 } [get_ports cpu_resetn]

# LED[3:0]
set_property -dict { PACKAGE_PIN H17 IOSTANDARD LVCMOS33 } [get_ports {led[0]}]
set_property -dict { PACKAGE_PIN K15 IOSTANDARD LVCMOS33 } [get_ports {led[1]}]
set_property -dict { PACKAGE_PIN J13 IOSTANDARD LVCMOS33 } [get_ports {led[2]}]
set_property -dict { PACKAGE_PIN N14 IOSTANDARD LVCMOS33 } [get_ports {led[3]}]

# cpu_resetn is an async pushbutton; exempt from timing
set_false_path -from [get_ports cpu_resetn]

# Configuration voltage (absent from the Digilent master XDC)
set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
