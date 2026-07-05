## ---------------------------------------------------------------------------
## Tricorder master pin constraints — Digilent Arty A7-100T (xc7a100tcsg324-1)
##
## Full sensor complement, all front ends wired simultaneously (notebook
## entry 8). Every pin below terminates in the STATIC region; the DFX
## partition boundary is internal (AXI4-Lite + AXI4-Stream) and needs no
## pin constraints. Pin LOCs verified against Digilent Arty-A7-100-Master.xdc
## (Rev. D/E, github.com/Digilent/digilent-xdc).
##
## Allocation summary:
##   JA (std)        PDM microphone array (shared clk + 6 data)
##   JB (high-speed) OV7670 control/sync/SCCB, PCLK on clock-capable pin
##   JC (high-speed) OV7670 8-bit DVP data
##   JD (std)        AD9708 DAC data
##   ChipKit outer   AD9226 ADC (12 data + OTR + clk) — no series resistors
##   ChipKit inner   DAC clk, SiPM, AD8332 control, piezo TX, spares
##   ChipKit I2C     BME688 + SCD-41 + VEML7700 + AS7341 + MAX30105 bus
##   ChipKit SPI     reserved for UI display (unassigned)
## ---------------------------------------------------------------------------

## Configuration (required for Arty A7)
set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]

## ---------------------------------------------------------------------------
## Board infrastructure
## ---------------------------------------------------------------------------

## 100 MHz system clock
set_property -dict { PACKAGE_PIN E3    IOSTANDARD LVCMOS33 } [get_ports { clk100mhz }]; #IO_L12P_T1_MRCC_35 Sch=gclk[100]
create_clock -add -name sys_clk_pin -period 10.00 -waveform {0 5} [get_ports { clk100mhz }]

## USB-UART (FreeRTOS console)
set_property -dict { PACKAGE_PIN D10   IOSTANDARD LVCMOS33 } [get_ports { uart_rxd_out }]; #IO_L19N_T3_VREF_16 Sch=uart_rxd_out
set_property -dict { PACKAGE_PIN A9    IOSTANDARD LVCMOS33 } [get_ports { uart_txd_in }];  #IO_L14N_T2_SRCC_16 Sch=uart_txd_in

## Buttons (btn[0] = system reset by convention), switches, green LEDs
set_property -dict { PACKAGE_PIN D9    IOSTANDARD LVCMOS33 } [get_ports { btn[0] }]; #IO_L6N_T0_VREF_16 Sch=btn[0]
set_property -dict { PACKAGE_PIN C9    IOSTANDARD LVCMOS33 } [get_ports { btn[1] }]; #IO_L11P_T1_SRCC_16 Sch=btn[1]
set_property -dict { PACKAGE_PIN B9    IOSTANDARD LVCMOS33 } [get_ports { btn[2] }]; #IO_L11N_T1_SRCC_16 Sch=btn[2]
set_property -dict { PACKAGE_PIN B8    IOSTANDARD LVCMOS33 } [get_ports { btn[3] }]; #IO_L12P_T1_MRCC_16 Sch=btn[3]
set_property -dict { PACKAGE_PIN A8    IOSTANDARD LVCMOS33 } [get_ports { sw[0] }];  #IO_L12N_T1_MRCC_16 Sch=sw[0]
set_property -dict { PACKAGE_PIN C11   IOSTANDARD LVCMOS33 } [get_ports { sw[1] }];  #IO_L13P_T2_MRCC_16 Sch=sw[1]
set_property -dict { PACKAGE_PIN C10   IOSTANDARD LVCMOS33 } [get_ports { sw[2] }];  #IO_L13N_T2_MRCC_16 Sch=sw[2]
set_property -dict { PACKAGE_PIN A10   IOSTANDARD LVCMOS33 } [get_ports { sw[3] }];  #IO_L14P_T2_SRCC_16 Sch=sw[3]
set_property -dict { PACKAGE_PIN H5    IOSTANDARD LVCMOS33 } [get_ports { led[0] }]; #IO_L24N_T3_35 Sch=led[4]
set_property -dict { PACKAGE_PIN J5    IOSTANDARD LVCMOS33 } [get_ports { led[1] }]; #IO_25_35 Sch=led[5]
set_property -dict { PACKAGE_PIN T9    IOSTANDARD LVCMOS33 } [get_ports { led[2] }]; #IO_L24P_T3_A01_D17_14 Sch=led[6]
set_property -dict { PACKAGE_PIN T10   IOSTANDARD LVCMOS33 } [get_ports { led[3] }]; #IO_L24N_T3_A00_D16_14 Sch=led[7]

## RGB LED 0 — DFX module status (R=loading, G=module active, B=idle)
set_property -dict { PACKAGE_PIN G6    IOSTANDARD LVCMOS33 } [get_ports { dfx_status_r }]; #IO_L19P_T3_35 Sch=led0_r
set_property -dict { PACKAGE_PIN F6    IOSTANDARD LVCMOS33 } [get_ports { dfx_status_g }]; #IO_L19N_T3_VREF_35 Sch=led0_g
set_property -dict { PACKAGE_PIN E1    IOSTANDARD LVCMOS33 } [get_ports { dfx_status_b }]; #IO_L18N_T2_35 Sch=led0_b

## ---------------------------------------------------------------------------
## Low-rate sensor I2C bus (static region, FreeRTOS-managed)
## BME688 0x76/0x77, SCD-41 0x62, VEML7700 0x10, AS7341 0x39, MAX30105 0x57
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN L18   IOSTANDARD LVCMOS33 } [get_ports { i2c_scl }]; #IO_L4P_T0_D04_14 Sch=ck_scl
set_property -dict { PACKAGE_PIN M18   IOSTANDARD LVCMOS33 } [get_ports { i2c_sda }]; #IO_L4N_T0_D05_14 Sch=ck_sda
## Drive high to enable the Arty's on-board I2C pull-ups
set_property -dict { PACKAGE_PIN A14   IOSTANDARD LVCMOS33 } [get_ports { i2c_scl_pup }]; #IO_L9N_T1_DQS_AD3N_15 Sch=scl_pup
set_property -dict { PACKAGE_PIN A13   IOSTANDARD LVCMOS33 } [get_ports { i2c_sda_pup }]; #IO_L9P_T1_DQS_AD3P_15 Sch=sda_pup

## ---------------------------------------------------------------------------
## PDM microphone array — 6x Adafruit #3492, Pmod JA
## Shared FPGA-generated clock (~2.4 MHz); one data line per mic. ja[7] spare.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN G13   IOSTANDARD LVCMOS33 } [get_ports { pdm_clk }];     #IO_0_15  Sch=ja[1]
set_property -dict { PACKAGE_PIN B11   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[0] }]; #IO_L4P_T0_15 Sch=ja[2]
set_property -dict { PACKAGE_PIN A11   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[1] }]; #IO_L4N_T0_15 Sch=ja[3]
set_property -dict { PACKAGE_PIN D12   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[2] }]; #IO_L6P_T0_15 Sch=ja[4]
set_property -dict { PACKAGE_PIN D13   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[3] }]; #IO_L6N_T0_VREF_15 Sch=ja[7]
set_property -dict { PACKAGE_PIN B18   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[4] }]; #IO_L10P_T1_AD11P_15 Sch=ja[8]
set_property -dict { PACKAGE_PIN A18   IOSTANDARD LVCMOS33 } [get_ports { pdm_data[5] }]; #IO_L10N_T1_AD11N_15 Sch=ja[9]

## ---------------------------------------------------------------------------
## OV7670 camera — DVP, Pmods JB (control) + JC (data), high-speed pair
## PCLK on clock-capable SRCC pin; XCLK is FPGA-generated 24 MHz.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN E15   IOSTANDARD LVCMOS33 } [get_ports { cam_pclk }];  #IO_L11P_T1_SRCC_15 Sch=jb_p[1] (clock-capable)
set_property -dict { PACKAGE_PIN E16   IOSTANDARD LVCMOS33 } [get_ports { cam_xclk }];  #IO_L11N_T1_SRCC_15 Sch=jb_n[1]
set_property -dict { PACKAGE_PIN D15   IOSTANDARD LVCMOS33 } [get_ports { cam_href }];  #IO_L12P_T1_MRCC_15 Sch=jb_p[2]
set_property -dict { PACKAGE_PIN C15   IOSTANDARD LVCMOS33 } [get_ports { cam_vsync }]; #IO_L12N_T1_MRCC_15 Sch=jb_n[2]
set_property -dict { PACKAGE_PIN J17   IOSTANDARD LVCMOS33 } [get_ports { cam_rst_n }]; #IO_L23P_T3_FOE_B_15 Sch=jb_p[3]
set_property -dict { PACKAGE_PIN J18   IOSTANDARD LVCMOS33 } [get_ports { cam_pwdn }];  #IO_L23N_T3_FWE_B_15 Sch=jb_n[3]
set_property -dict { PACKAGE_PIN K15   IOSTANDARD LVCMOS33 } [get_ports { cam_sioc }];  #IO_L24P_T3_RS1_15 Sch=jb_p[4] (SCCB clock)
set_property -dict { PACKAGE_PIN J15   IOSTANDARD LVCMOS33 } [get_ports { cam_siod }];  #IO_L24N_T3_RS0_15 Sch=jb_n[4] (SCCB data)
set_property -dict { PACKAGE_PIN U12   IOSTANDARD LVCMOS33 } [get_ports { cam_data[0] }]; #IO_L20P_T3_A08_D24_14 Sch=jc_p[1]
set_property -dict { PACKAGE_PIN V12   IOSTANDARD LVCMOS33 } [get_ports { cam_data[1] }]; #IO_L20N_T3_A07_D23_14 Sch=jc_n[1]
set_property -dict { PACKAGE_PIN V10   IOSTANDARD LVCMOS33 } [get_ports { cam_data[2] }]; #IO_L21P_T3_DQS_14 Sch=jc_p[2]
set_property -dict { PACKAGE_PIN V11   IOSTANDARD LVCMOS33 } [get_ports { cam_data[3] }]; #IO_L21N_T3_DQS_A06_D22_14 Sch=jc_n[2]
set_property -dict { PACKAGE_PIN U14   IOSTANDARD LVCMOS33 } [get_ports { cam_data[4] }]; #IO_L22P_T3_A05_D21_14 Sch=jc_p[3]
set_property -dict { PACKAGE_PIN V14   IOSTANDARD LVCMOS33 } [get_ports { cam_data[5] }]; #IO_L22N_T3_A04_D20_14 Sch=jc_n[3]
set_property -dict { PACKAGE_PIN T13   IOSTANDARD LVCMOS33 } [get_ports { cam_data[6] }]; #IO_L23P_T3_A03_D19_14 Sch=jc_p[4]
set_property -dict { PACKAGE_PIN U13   IOSTANDARD LVCMOS33 } [get_ports { cam_data[7] }]; #IO_L23N_T3_A02_D18_14 Sch=jc_n[4]

## OV7670 pixel clock: 24 MHz in RGB modes (PCLK = XCLK)
create_clock -add -name cam_pclk_pin -period 41.66 [get_ports { cam_pclk }]
set_input_delay -clock cam_pclk_pin -max 15.0 [get_ports { cam_data[*] cam_href cam_vsync }]
set_input_delay -clock cam_pclk_pin -min  5.0 [get_ports { cam_data[*] cam_href cam_vsync }]
set_clock_groups -asynchronous -group sys_clk_pin -group cam_pclk_pin

## ---------------------------------------------------------------------------
## AD9226 ADC — 12-bit parallel, ChipKit outer header (no series resistors)
## FPGA sources adc_clk; data captured on the same clock (system-synchronous).
## Start derated (20-40 MS/s) over jumper wiring; 65 MS/s needs short leads.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN V15   IOSTANDARD LVCMOS33 } [get_ports { adc_data[0] }];  #IO_L16P_T2_CSI_B_14 Sch=ck_io[0]
set_property -dict { PACKAGE_PIN U16   IOSTANDARD LVCMOS33 } [get_ports { adc_data[1] }];  #IO_L18P_T2_A12_D28_14 Sch=ck_io[1]
set_property -dict { PACKAGE_PIN P14   IOSTANDARD LVCMOS33 } [get_ports { adc_data[2] }];  #IO_L8N_T1_D12_14 Sch=ck_io[2]
set_property -dict { PACKAGE_PIN T11   IOSTANDARD LVCMOS33 } [get_ports { adc_data[3] }];  #IO_L19P_T3_A10_D26_14 Sch=ck_io[3]
set_property -dict { PACKAGE_PIN R12   IOSTANDARD LVCMOS33 } [get_ports { adc_data[4] }];  #IO_L5P_T0_D06_14 Sch=ck_io[4]
set_property -dict { PACKAGE_PIN T14   IOSTANDARD LVCMOS33 } [get_ports { adc_data[5] }];  #IO_L14P_T2_SRCC_14 Sch=ck_io[5]
set_property -dict { PACKAGE_PIN T15   IOSTANDARD LVCMOS33 } [get_ports { adc_data[6] }];  #IO_L14N_T2_SRCC_14 Sch=ck_io[6]
set_property -dict { PACKAGE_PIN T16   IOSTANDARD LVCMOS33 } [get_ports { adc_data[7] }];  #IO_L15N_T2_DQS_DOUT_CSO_B_14 Sch=ck_io[7]
set_property -dict { PACKAGE_PIN N15   IOSTANDARD LVCMOS33 } [get_ports { adc_data[8] }];  #IO_L11P_T1_SRCC_14 Sch=ck_io[8]
set_property -dict { PACKAGE_PIN M16   IOSTANDARD LVCMOS33 } [get_ports { adc_data[9] }];  #IO_L10P_T1_D14_14 Sch=ck_io[9]
set_property -dict { PACKAGE_PIN V17   IOSTANDARD LVCMOS33 } [get_ports { adc_data[10] }]; #IO_L18N_T2_A11_D27_14 Sch=ck_io[10]
set_property -dict { PACKAGE_PIN U18   IOSTANDARD LVCMOS33 } [get_ports { adc_data[11] }]; #IO_L17N_T2_A13_D29_14 Sch=ck_io[11]
set_property -dict { PACKAGE_PIN R17   IOSTANDARD LVCMOS33 } [get_ports { adc_otr }];      #IO_L12N_T1_MRCC_14 Sch=ck_io[12] (out-of-range flag)
set_property -dict { PACKAGE_PIN P17   IOSTANDARD LVCMOS33 } [get_ports { adc_clk }];      #IO_L12P_T1_MRCC_14 Sch=ck_io[13]

## ---------------------------------------------------------------------------
## AD9708 DAC — 8-bit parallel, Pmod JD (data) + ChipKit inner (clock)
## 200 ohm series resistors on JD are acceptable for the DAC data bus.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN D4    IOSTANDARD LVCMOS33 } [get_ports { dac_data[0] }]; #IO_L11N_T1_SRCC_35 Sch=jd[1]
set_property -dict { PACKAGE_PIN D3    IOSTANDARD LVCMOS33 } [get_ports { dac_data[1] }]; #IO_L12N_T1_MRCC_35 Sch=jd[2]
set_property -dict { PACKAGE_PIN F4    IOSTANDARD LVCMOS33 } [get_ports { dac_data[2] }]; #IO_L13P_T2_MRCC_35 Sch=jd[3]
set_property -dict { PACKAGE_PIN F3    IOSTANDARD LVCMOS33 } [get_ports { dac_data[3] }]; #IO_L13N_T2_MRCC_35 Sch=jd[4]
set_property -dict { PACKAGE_PIN E2    IOSTANDARD LVCMOS33 } [get_ports { dac_data[4] }]; #IO_L14P_T2_SRCC_35 Sch=jd[7]
set_property -dict { PACKAGE_PIN D2    IOSTANDARD LVCMOS33 } [get_ports { dac_data[5] }]; #IO_L14N_T2_SRCC_35 Sch=jd[8]
set_property -dict { PACKAGE_PIN H2    IOSTANDARD LVCMOS33 } [get_ports { dac_data[6] }]; #IO_L15P_T2_DQS_35 Sch=jd[9]
set_property -dict { PACKAGE_PIN G2    IOSTANDARD LVCMOS33 } [get_ports { dac_data[7] }]; #IO_L15N_T2_DQS_35 Sch=jd[10]
set_property -dict { PACKAGE_PIN U11   IOSTANDARD LVCMOS33 } [get_ports { dac_clk }];     #IO_L19N_T3_A09_D25_VREF_14 Sch=ck_io[26]

## ---------------------------------------------------------------------------
## SiPM photon front end — TLV3501 comparator output + PWM threshold DAC
## Pulse input on clock-capable pin for clean async capture into the TDC.
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN P15   IOSTANDARD LVCMOS33 } [get_ports { sipm_pulse }];       #IO_L13P_T2_MRCC_14 Sch=ck_io[33] (clock-capable)
set_property -dict { PACKAGE_PIN R16   IOSTANDARD LVCMOS33 } [get_ports { sipm_thresh_pwm }];  #IO_L15P_T2_DQS_RDWR_B_14 Sch=ck_io[34]

## ---------------------------------------------------------------------------
## AD8332 ultrasound AFE control (RX signal path goes into the AD9226)
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN N16   IOSTANDARD LVCMOS33 } [get_ports { afe_gain_pwm }]; #IO_L11N_T1_SRCC_14 Sch=ck_io[35]
set_property -dict { PACKAGE_PIN N14   IOSTANDARD LVCMOS33 } [get_ports { afe_hilo }];     #IO_L8P_T1_D11_14 Sch=ck_io[36]
set_property -dict { PACKAGE_PIN U17   IOSTANDARD LVCMOS33 } [get_ports { afe_enable }];   #IO_L17P_T2_A14_D30_14 Sch=ck_io[37]

## ---------------------------------------------------------------------------
## Piezo sonar TX — 40 kHz push-pull drive pair (through external driver)
## ---------------------------------------------------------------------------
set_property -dict { PACKAGE_PIN T18   IOSTANDARD LVCMOS33 } [get_ports { sonar_tx_a }]; #IO_L7N_T1_D10_14 Sch=ck_io[38]
set_property -dict { PACKAGE_PIN R18   IOSTANDARD LVCMOS33 } [get_ports { sonar_tx_b }]; #IO_L7P_T1_D09_14 Sch=ck_io[39]

## ---------------------------------------------------------------------------
## Spares (unassigned, for future front ends)
##   JA pin 10 (K16); ck_io27-32 (V16,M13,R10,R11,R13,R15); ck_io40/41 (P18,N17);
##   ck_a0-a5 as digital (F5,D8,C7,E7,D7,D5); ck_ioa (M17)
##   ChipKit SPI (G1,H1,F1,C1) reserved for UI display
##   XADC: vaux12 pair (B7/B6) available for AFE envelope / battery monitor
## ---------------------------------------------------------------------------
