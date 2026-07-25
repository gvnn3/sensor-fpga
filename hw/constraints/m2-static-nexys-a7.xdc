# M2 static-region hello constraints — Nexys A7-100T.
# Port names match the static_region BD wrapper. Pins from the Digilent
# Nexys-A7-100T-Master.xdc. Master tricorder XDC takes over from M4 on.

set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]

set_property -dict { PACKAGE_PIN E3  IOSTANDARD LVCMOS33 } [get_ports clk100mhz];    #IO_L12P_T1_MRCC_35 Sch=clk100mhz
set_property -dict { PACKAGE_PIN C12 IOSTANDARD LVCMOS33 } [get_ports cpu_resetn];   #IO_L3P_T0_DQS_AD1P_15 Sch=cpu_resetn

# uartlite rxd = FTDI->FPGA (board net UART_TXD_IN), txd = FPGA->FTDI
set_property -dict { PACKAGE_PIN C4  IOSTANDARD LVCMOS33 } [get_ports uart_rxd];     #IO_L7P_T1_AD6P_35 Sch=uart_txd_in
set_property -dict { PACKAGE_PIN D4  IOSTANDARD LVCMOS33 } [get_ports uart_txd];     #IO_L11N_T1_SRCC_35 Sch=uart_rxd_out

set_property -dict { PACKAGE_PIN H17 IOSTANDARD LVCMOS33 } [get_ports {led_tri_o[0]}]; #IO_L18P_T2_A24_15 Sch=led[0]
set_property -dict { PACKAGE_PIN K15 IOSTANDARD LVCMOS33 } [get_ports {led_tri_o[1]}]; #IO_L24P_T3_RS1_15 Sch=led[1]
set_property -dict { PACKAGE_PIN J13 IOSTANDARD LVCMOS33 } [get_ports {led_tri_o[2]}]; #IO_L17N_T2_A25_15 Sch=led[2]
set_property -dict { PACKAGE_PIN N14 IOSTANDARD LVCMOS33 } [get_ports {led_tri_o[3]}]; #IO_L8P_T1_D11_14 Sch=led[3]
