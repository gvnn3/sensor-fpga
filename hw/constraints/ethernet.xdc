## ---------------------------------------------------------------------------
## Tricorder Ethernet constraints — Digilent Nexys A7-100T (xc7a100tcsg324-1)
## SMSC LAN8720A PHY, RMII, 100 Mb fixed. Spec: docs/ethernet-spec.md §5.
##
## eth_refclk   50 MHz REFCLK to the PHY, ODDR-forwarded from clk_wiz
##              clk_out_50 (the FPGA sources REFCLK; the PHY consumes it).
## eth_rstn     active-low PHY reset, driven by axi_gpio_eth bit 0
##              (default 0 = PHY held in reset until software releases it).
## eth_mdc/mdio management interface, EmacLite MDIO engine; eth_mdio goes
##              through an IOBUF in the top wrapper.
## eth_crsdv, eth_rxerr, eth_rxd[1:0]   RMII receive, into the rmii_shim.
## eth_txen, eth_txd[1:0]               RMII transmit, from the rmii_shim.
## eth_intn     PHY interrupt, input — unused in v1 but constrained and
##              kept wired (folded into the unused-input heartbeat).
##
## Pin LOCs and IOSTANDARDs copied verbatim from the Digilent
## Nexys-A7-100T master XDC ("SMSC Ethernet PHY" section); all LVCMOS33.
## ---------------------------------------------------------------------------

## ---------------------------------------------------------------------------
## Management (MDIO) + reset
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN C9    IOSTANDARD LVCMOS33 } [get_ports { eth_mdc }];    #IO_L11P_T1_SRCC_16 Sch=eth_mdc
set_property -dict { PACKAGE_PIN A9    IOSTANDARD LVCMOS33 } [get_ports { eth_mdio }];   #IO_L14N_T2_SRCC_16 Sch=eth_mdio
set_property -dict { PACKAGE_PIN B3    IOSTANDARD LVCMOS33 } [get_ports { eth_rstn }];   #IO_L10P_T1_AD15P_35 Sch=eth_rstn

## ---------------------------------------------------------------------------
## RMII receive (PHY -> shim)
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN D9    IOSTANDARD LVCMOS33 } [get_ports { eth_crsdv }];  #IO_L6N_T0_VREF_16 Sch=eth_crsdv
set_property -dict { PACKAGE_PIN C10   IOSTANDARD LVCMOS33 } [get_ports { eth_rxerr }];  #IO_L13N_T2_MRCC_16 Sch=eth_rxerr
set_property -dict { PACKAGE_PIN C11   IOSTANDARD LVCMOS33 } [get_ports { eth_rxd[0] }]; #IO_L13P_T2_MRCC_16 Sch=eth_rxd[0]
set_property -dict { PACKAGE_PIN D10   IOSTANDARD LVCMOS33 } [get_ports { eth_rxd[1] }]; #IO_L19N_T3_VREF_16 Sch=eth_rxd[1]

## ---------------------------------------------------------------------------
## RMII transmit (shim -> PHY)
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN B9    IOSTANDARD LVCMOS33 } [get_ports { eth_txen }];   #IO_L11N_T1_SRCC_16 Sch=eth_txen
set_property -dict { PACKAGE_PIN A10   IOSTANDARD LVCMOS33 } [get_ports { eth_txd[0] }]; #IO_L14P_T2_SRCC_16 Sch=eth_txd[0]
set_property -dict { PACKAGE_PIN A8    IOSTANDARD LVCMOS33 } [get_ports { eth_txd[1] }]; #IO_L12N_T1_MRCC_16 Sch=eth_txd[1]

## ---------------------------------------------------------------------------
## REFCLK out + interrupt in
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN D5    IOSTANDARD LVCMOS33 SLEW FAST } [get_ports { eth_refclk }]; #IO_L11P_T1_SRCC_35 Sch=eth_refclk (50 MHz, ODDR-forwarded)
set_property -dict { PACKAGE_PIN B8    IOSTANDARD LVCMOS33 } [get_ports { eth_intn }];   #IO_L12P_T1_MRCC_16 Sch=eth_intn (unused in v1, kept wired)

## ---------------------------------------------------------------------------
## Timing
##
## No create_clock here: eth_refclk is an OUTPUT (the FPGA sources the
## PHY's 50 MHz reference from clk_wiz clk_out_50 through an ODDR); no
## external clock enters on these pins.
##
## RMII I/O timing at 50 MHz is left unconstrained deliberately: the
## 20 ns bit period against LAN8720A tco/tsu leaves roughly 10 ns of
## margin, and the shim's 2-flop IOB capture (spec §4.6) tolerates a
## uniform one-cycle-late arrival. If timing ever needs to be pinned
## down, start from these stubs (values per spec §5.3):
##
## set_input_delay  -clock [get_clocks -of_objects [get_pins */clk_wiz_*/clk_out_50]] \
##     -max 14.0 [get_ports {eth_crsdv eth_rxerr eth_rxd[*]}]
## set_input_delay  -clock [get_clocks -of_objects [get_pins */clk_wiz_*/clk_out_50]] \
##     -min  2.0 [get_ports {eth_crsdv eth_rxerr eth_rxd[*]}]
## set_output_delay -clock [get_clocks -of_objects [get_pins */clk_wiz_*/clk_out_50]] \
##     -max  4.0 [get_ports {eth_txen eth_txd[*]}]
## set_output_delay -clock [get_clocks -of_objects [get_pins */clk_wiz_*/clk_out_50]] \
##     -min -2.0 [get_ports {eth_txen eth_txd[*]}]
## ---------------------------------------------------------------------------

## 25 MHz MII clock generated in the shim by divide-by-2 (spec section 5.3)
create_generated_clock -name mii_clk25 -divide_by 2 \
    -source [get_pins */rmii_shim_i/clk25_r_reg/C] \
    [get_pins */rmii_shim_i/clk25_r_reg/Q]
