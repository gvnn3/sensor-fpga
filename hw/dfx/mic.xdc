# On-board ADMP421 PDM microphone (dedicated nets) — read only by the DFX
# flow (build_dfx.tcl); the BD-only build_static.tcl top has no mic ports.
# Pins verified against Digilent Nexys-A7-100T-Master.xdc.

set_property -dict { PACKAGE_PIN J5 IOSTANDARD LVCMOS33 } [get_ports pdm_m_clk];   #IO_25_35 Sch=m_clk
set_property -dict { PACKAGE_PIN H5 IOSTANDARD LVCMOS33 } [get_ports pdm_m_data];  #IO_L24N_T3_35 Sch=m_data
set_property -dict { PACKAGE_PIN F5 IOSTANDARD LVCMOS33 } [get_ports pdm_m_lrsel]; #IO_0_35 Sch=m_lrsel
