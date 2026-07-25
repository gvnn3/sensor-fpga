## ---------------------------------------------------------------------------
## Tricorder master pin constraints — Digilent Nexys A7-100T (xc7a100tcsg324-1)
##
## Full sensor complement per docs/plan.md §2: every modality simultaneously
## wired except the AD9708 DAC, which time-shares the AD9226 bus (adcdac_*)
## through static-region IOBUFs. Every pin below terminates in the STATIC
## region; the DFX partition boundary is internal and needs no pin
## constraints. Pin LOCs verified against Digilent Nexys-A7-100T-Master.xdc
## (github.com/Digilent/digilent-xdc, current master — NOT the stale local
## copy with LVDS JXADC lines).
##
## Allocation summary (40/40 header pins, zero spares):
##   JA      slow/control: I2C bus (+OV7670 SCCB), SiPM, AFE ctrl, piezo TX
##   JB      OV7670 control/sync + data[3:0]; PCLK on JB10/H16 — the ONLY
##           P-side clock-capable header pin on the board
##   JC      OV7670 data[7:4] + adcdac_data[11:8]
##   JD      adcdac_data[7:0]
##   JXADC   PDM array (clk + 6 data) + adcdac_clk (digital use, 100 ohm)
## Board nets: 4 btn, 4 sw, 4 LED, RGB LED16 = DFX status, UART.
## Known accepted risks: AD9226 derated to <=40 MS/s (200 ohm series R);
## adcdac_clk pair-coupled with pdm_data[5] (swap with sonar_tx_b/JA10 if
## mic ch5 shows artifacts).
## ---------------------------------------------------------------------------

## Configuration (absent from the Digilent master XDC — required)
set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]

## ---------------------------------------------------------------------------
## Board infrastructure
## ---------------------------------------------------------------------------

## 100 MHz system clock
set_property -dict { PACKAGE_PIN E3    IOSTANDARD LVCMOS33 } [get_ports { clk100mhz }]; #IO_L12P_T1_MRCC_35 Sch=clk100mhz
create_clock -add -name sys_clk_pin -period 10.00 -waveform {0 5} [get_ports { clk100mhz }]

## USB-UART (FreeRTOS console)
set_property -dict { PACKAGE_PIN D4    IOSTANDARD LVCMOS33 } [get_ports { uart_rxd_out }]; #IO_L11N_T1_SRCC_35 Sch=uart_rxd_out
set_property -dict { PACKAGE_PIN C4    IOSTANDARD LVCMOS33 } [get_ports { uart_txd_in }];  #IO_L7P_T1_AD6P_35 Sch=uart_txd_in

## Buttons (BTNC/BTNU/BTNL/BTNR; BTND P18 and CPU_RESETN C12 spare)
set_property -dict { PACKAGE_PIN N17   IOSTANDARD LVCMOS33 } [get_ports { btn[0] }]; #IO_L9P_T1_DQS_14 Sch=btnc
set_property -dict { PACKAGE_PIN M18   IOSTANDARD LVCMOS33 } [get_ports { btn[1] }]; #IO_L4N_T0_D05_14 Sch=btnu
set_property -dict { PACKAGE_PIN P17   IOSTANDARD LVCMOS33 } [get_ports { btn[2] }]; #IO_L12P_T1_MRCC_14 Sch=btnl
set_property -dict { PACKAGE_PIN M17   IOSTANDARD LVCMOS33 } [get_ports { btn[3] }]; #IO_L10N_T1_D15_14 Sch=btnr

## Switches 0-3 (sw[4:15] spare; note SW8/SW9 are LVCMOS18 if ever used)
set_property -dict { PACKAGE_PIN J15   IOSTANDARD LVCMOS33 } [get_ports { sw[0] }];  #IO_L24N_T3_RS0_15 Sch=sw[0]
set_property -dict { PACKAGE_PIN L16   IOSTANDARD LVCMOS33 } [get_ports { sw[1] }];  #IO_L3N_T0_DQS_EMCCLK_14 Sch=sw[1]
set_property -dict { PACKAGE_PIN M13   IOSTANDARD LVCMOS33 } [get_ports { sw[2] }];  #IO_L6N_T0_D08_VREF_14 Sch=sw[2]
set_property -dict { PACKAGE_PIN R15   IOSTANDARD LVCMOS33 } [get_ports { sw[3] }];  #IO_L13N_T2_MRCC_14 Sch=sw[3]

## LEDs 0-3 (led[4:15] spare)
set_property -dict { PACKAGE_PIN H17   IOSTANDARD LVCMOS33 } [get_ports { led[0] }]; #IO_L18P_T2_A24_15 Sch=led[0]
set_property -dict { PACKAGE_PIN K15   IOSTANDARD LVCMOS33 } [get_ports { led[1] }]; #IO_L24P_T3_RS1_15 Sch=led[1]
set_property -dict { PACKAGE_PIN J13   IOSTANDARD LVCMOS33 } [get_ports { led[2] }]; #IO_L17N_T2_A25_15 Sch=led[2]
set_property -dict { PACKAGE_PIN N14   IOSTANDARD LVCMOS33 } [get_ports { led[3] }]; #IO_L8P_T1_D11_14 Sch=led[3]

## RGB LED16 — DFX module status (R=loading, G=module active, B=idle)
set_property -dict { PACKAGE_PIN N15   IOSTANDARD LVCMOS33 } [get_ports { dfx_status_r }]; #IO_L11P_T1_SRCC_14 Sch=led16_r
set_property -dict { PACKAGE_PIN M16   IOSTANDARD LVCMOS33 } [get_ports { dfx_status_g }]; #IO_L10P_T1_D14_14 Sch=led16_g
set_property -dict { PACKAGE_PIN R12   IOSTANDARD LVCMOS33 } [get_ports { dfx_status_b }]; #IO_L5P_T0_D06_14 Sch=led16_b

## ---------------------------------------------------------------------------
## Pmod JA — slow/control lane
## Shared I2C bus: BME688 0x76/77, SCD-41 0x62, VEML7700 0x10, AS7341 0x39,
## MAX30105 0x57, OV7670 SCCB 0x21. External 4.7k pull-ups on the harness.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN C17   IOSTANDARD LVCMOS33 } [get_ports { i2c_scl }];         #IO_L20N_T3_A19_15 Sch=ja[1]
set_property -dict { PACKAGE_PIN D18   IOSTANDARD LVCMOS33 } [get_ports { i2c_sda }];         #IO_L21N_T3_DQS_A18_15 Sch=ja[2]
set_property -dict { PACKAGE_PIN E18   IOSTANDARD LVCMOS33 } [get_ports { sipm_pulse }];      #IO_L21P_T3_DQS_15 Sch=ja[3] (async in, 4x oversampled in fabric)
set_property -dict { PACKAGE_PIN G17   IOSTANDARD LVCMOS33 } [get_ports { sipm_thresh_pwm }]; #IO_L18N_T2_A23_15 Sch=ja[4]
set_property -dict { PACKAGE_PIN D17   IOSTANDARD LVCMOS33 } [get_ports { afe_gain_pwm }];    #IO_L16N_T2_A27_15 Sch=ja[7]
set_property -dict { PACKAGE_PIN E17   IOSTANDARD LVCMOS33 } [get_ports { afe_hilo }];        #IO_L16P_T2_A28_15 Sch=ja[8]
set_property -dict { PACKAGE_PIN F18   IOSTANDARD LVCMOS33 } [get_ports { sonar_tx_a }];      #IO_L22N_T3_A16_15 Sch=ja[9]
set_property -dict { PACKAGE_PIN G18   IOSTANDARD LVCMOS33 } [get_ports { sonar_tx_b }];      #IO_L22P_T3_A17_15 Sch=ja[10]

## ---------------------------------------------------------------------------
## OV7670 camera — Pmods JB (control/sync + low nibble) + JC (high nibble)
## RST/PWDN strapped on module; SCCB on the shared I2C bus (JA).
## PCLK on JB10/H16: the only P-side clock-capable (MRCC) header pin.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN D14   IOSTANDARD LVCMOS33 } [get_ports { cam_xclk }];    #IO_L1P_T0_AD0P_15 Sch=jb[1] (FPGA-generated 24 MHz)
set_property -dict { PACKAGE_PIN F16   IOSTANDARD LVCMOS33 } [get_ports { cam_href }];    #IO_L14N_T2_SRCC_15 Sch=jb[2]
set_property -dict { PACKAGE_PIN G16   IOSTANDARD LVCMOS33 } [get_ports { cam_vsync }];   #IO_L13N_T2_MRCC_15 Sch=jb[3]
set_property -dict { PACKAGE_PIN H14   IOSTANDARD LVCMOS33 } [get_ports { cam_data[0] }]; #IO_L15P_T2_DQS_15 Sch=jb[4]
set_property -dict { PACKAGE_PIN E16   IOSTANDARD LVCMOS33 } [get_ports { cam_data[1] }]; #IO_L11N_T1_SRCC_15 Sch=jb[7]
set_property -dict { PACKAGE_PIN F13   IOSTANDARD LVCMOS33 } [get_ports { cam_data[2] }]; #IO_L5P_T0_AD9P_15 Sch=jb[8]
set_property -dict { PACKAGE_PIN G13   IOSTANDARD LVCMOS33 } [get_ports { cam_data[3] }]; #IO_0_15 Sch=jb[9]
set_property -dict { PACKAGE_PIN H16   IOSTANDARD LVCMOS33 } [get_ports { cam_pclk }];    #IO_L13P_T2_MRCC_15 Sch=jb[10] (sole P-side CC header pin)
set_property -dict { PACKAGE_PIN K1    IOSTANDARD LVCMOS33 } [get_ports { cam_data[4] }]; #IO_L23N_T3_35 Sch=jc[1]
set_property -dict { PACKAGE_PIN F6    IOSTANDARD LVCMOS33 } [get_ports { cam_data[5] }]; #IO_L19N_T3_VREF_35 Sch=jc[2]
set_property -dict { PACKAGE_PIN J2    IOSTANDARD LVCMOS33 } [get_ports { cam_data[6] }]; #IO_L22N_T3_35 Sch=jc[3]
set_property -dict { PACKAGE_PIN G6    IOSTANDARD LVCMOS33 } [get_ports { cam_data[7] }]; #IO_L19P_T3_35 Sch=jc[4]

## OV7670 pixel clock: 24 MHz in RGB modes (PCLK = XCLK)
create_clock -add -name cam_pclk_pin -period 41.66 [get_ports { cam_pclk }]
set_input_delay -clock cam_pclk_pin -max 15.0 [get_ports { cam_data[*] cam_href cam_vsync }]
set_input_delay -clock cam_pclk_pin -min  5.0 [get_ports { cam_data[*] cam_href cam_vsync }]
set_clock_groups -asynchronous -group sys_clk_pin -group cam_pclk_pin

## ---------------------------------------------------------------------------
## AD9226 ADC / AD9708 DAC shared bus — Pmods JC (bits 11:8) + JD (bits 7:0)
## IOBUFs in static region; ADC personality = 12 in, DAC personality = 8 out
## (bits 7:0). FPGA sources adcdac_clk; system-synchronous capture.
## DERATED to <=40 MS/s behind the 200 ohm Pmod series resistors — 65 MS/s
## is unsupported on this board. Short harness mandatory.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN H4    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[0] }];  #IO_L21N_T3_DQS_35 Sch=jd[1]
set_property -dict { PACKAGE_PIN H1    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[1] }];  #IO_L17P_T2_35 Sch=jd[2]
set_property -dict { PACKAGE_PIN G1    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[2] }];  #IO_L17N_T2_35 Sch=jd[3]
set_property -dict { PACKAGE_PIN G3    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[3] }];  #IO_L20N_T3_35 Sch=jd[4]
set_property -dict { PACKAGE_PIN H2    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[4] }];  #IO_L15P_T2_DQS_35 Sch=jd[7]
set_property -dict { PACKAGE_PIN G4    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[5] }];  #IO_L20P_T3_35 Sch=jd[8]
set_property -dict { PACKAGE_PIN G2    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[6] }];  #IO_L15N_T2_DQS_35 Sch=jd[9]
set_property -dict { PACKAGE_PIN F3    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[7] }];  #IO_L13N_T2_MRCC_35 Sch=jd[10]
set_property -dict { PACKAGE_PIN E7    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[8] }];  #IO_L6P_T0_35 Sch=jc[7]
set_property -dict { PACKAGE_PIN J3    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[9] }];  #IO_L22P_T3_35 Sch=jc[8]
set_property -dict { PACKAGE_PIN J4    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[10] }]; #IO_L21P_T3_DQS_35 Sch=jc[9]
set_property -dict { PACKAGE_PIN E6    IOSTANDARD LVCMOS33 } [get_ports { adcdac_data[11] }]; #IO_L5P_T0_AD13P_35 Sch=jc[10]

## ---------------------------------------------------------------------------
## JXADC (digital use, 100 ohm series) — PDM mic array + adcdac_clk
## Coupled pairs: adcdac_clk (XA_N4) pairs with pdm_data[5] (XA_P4) — watch
## mic ch5 for crosstalk during ADC runs; documented swap is sonar_tx_b/JA10.
## On-board ADMP421 7th mic joins on dedicated nets (J5/H5/F5) at M6.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN A13   IOSTANDARD LVCMOS33 } [get_ports { pdm_clk }];     #IO_L9P_T1_DQS_AD3P_15 Sch=xa_p[1]
set_property -dict { PACKAGE_PIN A14   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[0] }]; #IO_L9N_T1_DQS_AD3N_15 Sch=xa_n[1]
set_property -dict { PACKAGE_PIN A15   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[1] }]; #IO_L8P_T1_AD10P_15 Sch=xa_p[2]
set_property -dict { PACKAGE_PIN A16   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[2] }]; #IO_L8N_T1_AD10N_15 Sch=xa_n[2]
set_property -dict { PACKAGE_PIN B16   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[3] }]; #IO_L7P_T1_AD2P_15 Sch=xa_p[3]
set_property -dict { PACKAGE_PIN B17   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[4] }]; #IO_L7N_T1_AD2N_15 Sch=xa_n[3]
set_property -dict { PACKAGE_PIN B18   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[5] }]; #IO_L10P_T1_AD11P_15 Sch=xa_p[4]
set_property -dict { PACKAGE_PIN A18   IOSTANDARD LVCMOS33 } [get_ports { adcdac_clk }];  #IO_L10N_T1_AD11N_15 Sch=xa_n[4]

## ---------------------------------------------------------------------------
## Spares: zero header pins. Dedicated-net reserve only: BTND (P18),
## CPU_RESETN (C12), sw[4:15], led[4:15], LED17 RGB, 7-seg (CA-CG/DP/AN),
## VGA (12-bit + hsync/vsync), AUD_PWM/AUD_SD, on-board PDM mic (J5/H5/F5),
## ADT7420 (C14/C15 + irq), ADXL362 SPI (E15/F14/F15/D15 + irq), microSD,
## PS/2, Ethernet RMII, QSPI.
## ---------------------------------------------------------------------------
