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

# External sensor I2C bus on Pmod JA (pull-ups on harness)
set_property -dict { PACKAGE_PIN C17 IOSTANDARD LVCMOS33 } [get_ports iic_ext_scl_io]; #IO_L20N_T3_A19_15 Sch=ja[1]
set_property -dict { PACKAGE_PIN D18 IOSTANDARD LVCMOS33 } [get_ports iic_ext_sda_io]; #IO_L21N_T3_DQS_A18_15 Sch=ja[2]

# On-board ADT7420 temperature sensor (dedicated nets, on-board pull-ups)
set_property -dict { PACKAGE_PIN C14 IOSTANDARD LVCMOS33 } [get_ports iic_tmp_scl_io]; #IO_L1N_T0_AD0N_15 Sch=tmp_scl
set_property -dict { PACKAGE_PIN C15 IOSTANDARD LVCMOS33 } [get_ports iic_tmp_sda_io]; #IO_L12N_T1_MRCC_15 Sch=tmp_sda

set_property -dict { PACKAGE_PIN H17 IOSTANDARD LVCMOS33 } [get_ports {led_tri_o[0]}]; #IO_L18P_T2_A24_15 Sch=led[0]
set_property -dict { PACKAGE_PIN K15 IOSTANDARD LVCMOS33 } [get_ports {led_tri_o[1]}]; #IO_L24P_T3_RS1_15 Sch=led[1]
set_property -dict { PACKAGE_PIN J13 IOSTANDARD LVCMOS33 } [get_ports {led_tri_o[2]}]; #IO_L17N_T2_A25_15 Sch=led[2]
set_property -dict { PACKAGE_PIN N14 IOSTANDARD LVCMOS33 } [get_ports {led_tri_o[3]}]; #IO_L8P_T1_D11_14 Sch=led[3]

# QSPI config flash (S25FL128S); SCK is the dedicated CCLK pin via STARTUPE2
set_property -dict { PACKAGE_PIN K17 IOSTANDARD LVCMOS33 } [get_ports qspi_dq0]; #IO_L1P_T0_D00_MOSI_14 Sch=qspi_dq[0]
set_property -dict { PACKAGE_PIN K18 IOSTANDARD LVCMOS33 } [get_ports qspi_dq1]; #IO_L1N_T0_D01_DIN_14 Sch=qspi_dq[1]
set_property -dict { PACKAGE_PIN L14 IOSTANDARD LVCMOS33 } [get_ports qspi_dq2]; #IO_L2P_T0_D02_14 Sch=qspi_dq[2]
set_property -dict { PACKAGE_PIN M14 IOSTANDARD LVCMOS33 } [get_ports qspi_dq3]; #IO_L2N_T0_D03_14 Sch=qspi_dq[3]
set_property -dict { PACKAGE_PIN L13 IOSTANDARD LVCMOS33 } [get_ports qspi_cs];  #IO_L6P_T0_FCS_B_14 Sch=qspi_csn
