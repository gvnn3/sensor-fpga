# Ultrasound-chain pins for the DFX top (boundary v3). Read only by
# build_dfx.tcl. Pins copied from hw/constraints/tricorder-nexys-a7-100t.xdc
# (plan section 2, verified against the Digilent master XDC).

set_property -dict { PACKAGE_PIN F18 IOSTANDARD LVCMOS33 } [get_ports sonar_tx_a];      #IO_L22N_T3_A16_15 Sch=ja[9]
set_property -dict { PACKAGE_PIN G18 IOSTANDARD LVCMOS33 } [get_ports sonar_tx_b];      #IO_L22P_T3_A17_15 Sch=ja[10]
set_property -dict { PACKAGE_PIN D17 IOSTANDARD LVCMOS33 } [get_ports afe_gain_pwm];    #IO_L16N_T2_A27_15 Sch=ja[7]
set_property -dict { PACKAGE_PIN E17 IOSTANDARD LVCMOS33 } [get_ports afe_hilo];        #IO_L16P_T2_A28_15 Sch=ja[8]

set_property -dict { PACKAGE_PIN H4 IOSTANDARD LVCMOS33 } [get_ports {adc_data[0]}];    #IO_L21N_T3_DQS_35 Sch=jd[1]
set_property -dict { PACKAGE_PIN H1 IOSTANDARD LVCMOS33 } [get_ports {adc_data[1]}];    #IO_L17P_T2_35 Sch=jd[2]
set_property -dict { PACKAGE_PIN G1 IOSTANDARD LVCMOS33 } [get_ports {adc_data[2]}];    #IO_L17N_T2_35 Sch=jd[3]
set_property -dict { PACKAGE_PIN G3 IOSTANDARD LVCMOS33 } [get_ports {adc_data[3]}];    #IO_L20N_T3_35 Sch=jd[4]
set_property -dict { PACKAGE_PIN H2 IOSTANDARD LVCMOS33 } [get_ports {adc_data[4]}];    #IO_L15P_T2_DQS_35 Sch=jd[7]
set_property -dict { PACKAGE_PIN G4 IOSTANDARD LVCMOS33 } [get_ports {adc_data[5]}];    #IO_L20P_T3_35 Sch=jd[8]
set_property -dict { PACKAGE_PIN G2 IOSTANDARD LVCMOS33 } [get_ports {adc_data[6]}];    #IO_L15N_T2_DQS_35 Sch=jd[9]
set_property -dict { PACKAGE_PIN F3 IOSTANDARD LVCMOS33 } [get_ports {adc_data[7]}];    #IO_L13N_T2_MRCC_35 Sch=jd[10]
set_property -dict { PACKAGE_PIN E7 IOSTANDARD LVCMOS33 } [get_ports {adc_data[8]}];    #IO_L6P_T0_35 Sch=jc[7]
set_property -dict { PACKAGE_PIN J3 IOSTANDARD LVCMOS33 } [get_ports {adc_data[9]}];    #IO_L22P_T3_35 Sch=jc[8]
set_property -dict { PACKAGE_PIN J4 IOSTANDARD LVCMOS33 } [get_ports {adc_data[10]}];   #IO_L21P_T3_DQS_35 Sch=jc[9]
set_property -dict { PACKAGE_PIN E6 IOSTANDARD LVCMOS33 } [get_ports {adc_data[11]}];   #IO_L5P_T0_AD13P_35 Sch=jc[10]
set_property -dict { PACKAGE_PIN A18 IOSTANDARD LVCMOS33 } [get_ports adc_clk];         #IO_L10N_T1_AD11N_15 Sch=xa_n[4]
