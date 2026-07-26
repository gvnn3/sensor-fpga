# M2 static-region block design: classic MicroBlaze + 32 KB LMB + UARTlite
# + GPIO LEDs. DDR2 MIG, Ethernet, I2C, HWICAP, intc arrive in M3-M5.
# Sourced by hw/scripts/build_static.tcl inside a project targeting
# xc7a100tcsg324-1. CPU decision (notebook entry 14): classic MicroBlaze —
# the 2025.2 FreeRTOS BSPs port list is ARM + MicroBlazeV9 only, no
# MicroBlaze-V/RISC-V port, so FreeRTOS (fixed requirement) forces classic.

proc externalize {pin name} {
    # make_bd_intf_pins_external returns nothing in 2025.2 batch mode;
    # recover the created port through the new interface net.
    make_bd_intf_pins_external [get_bd_intf_pins $pin]
    set net  [get_bd_intf_nets -of_objects [get_bd_intf_pins $pin]]
    set port [get_bd_intf_ports -of_objects $net]
    set_property name $name $port
}

proc ipv {name} {
    # newest xilinx.com VLNV for an IP name, so this script survives version bumps
    return [lindex [lsort -dictionary [get_ipdefs -all -filter "NAME == $name && VLNV =~ xilinx.com:ip:*"]] end]
}

create_bd_design "static_region"

# Clocking: board 100 MHz -> clk_wiz -> 100 MHz system + 200 MHz MIG ref
set clk_wiz [create_bd_cell -type ip -vlnv [ipv clk_wiz] clk_wiz_0]
set_property -dict [list \
    CONFIG.PRIM_IN_FREQ {100.000} \
    CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {100.000} \
    CONFIG.CLKOUT2_USED {true} \
    CONFIG.CLKOUT2_REQUESTED_OUT_FREQ {200.000} \
    CONFIG.CLKOUT3_USED {true} \
    CONFIG.CLKOUT3_REQUESTED_OUT_FREQ {20.000} \
    CONFIG.RESET_TYPE {ACTIVE_LOW} \
    CONFIG.RESET_PORT {resetn} \
] $clk_wiz

create_bd_port -dir I -type clk -freq_hz 100000000 clk100mhz
connect_bd_net [get_bd_ports clk100mhz] [get_bd_pins clk_wiz_0/clk_in1]

create_bd_port -dir I -type rst cpu_resetn
set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports cpu_resetn]
connect_bd_net [get_bd_ports cpu_resetn] [get_bd_pins clk_wiz_0/resetn]

# MicroBlaze + LMB + MDM + proc_sys_reset via block automation
set mb [create_bd_cell -type ip -vlnv [ipv microblaze] microblaze_0]
apply_bd_automation -rule xilinx.com:bd_rule:microblaze -config { \
    local_mem "32KB" ecc "None" cache "16KB" debug_module "Debug Only" \
    axi_periph "Enabled" axi_intc "0" clk "/clk_wiz_0/clk_out1 (100 MHz)" \
} $mb

# (Board reset reaches proc_sys_reset/ext_reset_in via the automation,
# which picks up the cpu_resetn net from clk_wiz_0/resetn.)

# UART console (115200) on the shared FT2232 channel B
set uart [create_bd_cell -type ip -vlnv [ipv axi_uartlite] axi_uartlite_0]
set_property CONFIG.C_BAUDRATE {115200} $uart
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_uartlite_0/S_AXI} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_uartlite_0/S_AXI]
make_bd_intf_pins_external [get_bd_intf_pins axi_uartlite_0/UART]
set_property name uart [get_bd_intf_ports UART_0]

# 4 green LEDs (ch1 out) + MIG init_calib_complete readback (ch2 in)
set gpio [create_bd_cell -type ip -vlnv [ipv axi_gpio] axi_gpio_0]
set_property -dict [list CONFIG.C_GPIO_WIDTH {4} CONFIG.C_ALL_OUTPUTS {1} \
    CONFIG.C_IS_DUAL {1} CONFIG.C_GPIO2_WIDTH {1} CONFIG.C_ALL_INPUTS_2 {1}] $gpio
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_gpio_0/S_AXI} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_gpio_0/S_AXI]
make_bd_intf_pins_external [get_bd_intf_pins axi_gpio_0/GPIO]
set_property name led [get_bd_intf_ports GPIO_0]

# M3: DDR2 via MIG 7-series. Config from Digilent's Nexys A7-100T board
# files (hw/mig/mig-bd.prj = their mig.prj with SystemClock -> "No Buffer"
# so clk_wiz drives sys_clk_i 100 MHz + clk_ref_i 200 MHz from fabric).
# MIG owns the XADC (XADC_En in the prj) — no separate xadc_wizard IP later.
set mig [create_bd_cell -type ip -vlnv [ipv mig_7series] mig_7series_0]
set_property CONFIG.XML_INPUT_FILE [file normalize hw/mig/mig-bd.prj] $mig

connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins mig_7series_0/sys_clk_i]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out2] [get_bd_pins mig_7series_0/clk_ref_i]
connect_bd_net [get_bd_ports cpu_resetn]        [get_bd_pins mig_7series_0/sys_rst]
connect_bd_net [get_bd_pins mig_7series_0/init_calib_complete] \
               [get_bd_pins axi_gpio_0/gpio2_io_i]
make_bd_intf_pins_external [get_bd_intf_pins mig_7series_0/DDR2]
set_property name ddr2 [get_bd_intf_ports DDR2_0]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Cached)} Slave {/mig_7series_0/S_AXI} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins mig_7series_0/S_AXI]

# --- M4: interrupts, timer, I2C ------------------------------------------
# axi_intc + concat; FreeRTOS tick comes from axi_timer_0
set intc [create_bd_cell -type ip -vlnv [ipv axi_intc] axi_intc_0]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_intc_0/s_axi} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_intc_0/s_axi]
connect_bd_intf_net [get_bd_intf_pins axi_intc_0/interrupt] \
    [get_bd_intf_pins microblaze_0/INTERRUPT]

set timer [create_bd_cell -type ip -vlnv [ipv axi_timer] axi_timer_0]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_timer_0/S_AXI} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_timer_0/S_AXI]

# External sensor bus (Pmod JA) + on-board ADT7420 bus
foreach iic {axi_iic_0 axi_iic_1} {
    set c [create_bd_cell -type ip -vlnv [ipv axi_iic] $iic]
    apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config " \
        Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
        Master {/microblaze_0 (Periph)} Slave {/$iic/S_AXI} \
        ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
    " [get_bd_intf_pins $iic/S_AXI]
}
externalize axi_iic_0/IIC iic_ext
externalize axi_iic_1/IIC iic_tmp

set concat [create_bd_cell -type ip -vlnv [ipv xlconcat] xlconcat_0]
set_property CONFIG.NUM_PORTS {4} $concat
connect_bd_net [get_bd_pins axi_timer_0/interrupt]   [get_bd_pins xlconcat_0/In0]
connect_bd_net [get_bd_pins axi_iic_0/iic2intc_irpt] [get_bd_pins xlconcat_0/In1]
connect_bd_net [get_bd_pins axi_iic_1/iic2intc_irpt] [get_bd_pins xlconcat_0/In2]
connect_bd_net [get_bd_pins axi_uartlite_0/interrupt] [get_bd_pins xlconcat_0/In3]
connect_bd_net [get_bd_pins xlconcat_0/dout] [get_bd_pins axi_intc_0/intr]

# --- M5: DFX statics — HWICAP, decoupler, RM boundary ---------------------
set hwicap [create_bd_cell -type ip -vlnv [ipv axi_hwicap] axi_hwicap_0]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_hwicap_0/S_AXI_LITE} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_hwicap_0/S_AXI_LITE]
# icap_clk at 20 MHz — one more attempt at the v3 core; the raw ICAPE2
# writer below is the working fallback either way
connect_bd_net [get_bd_pins clk_wiz_0/clk_out3] [get_bd_pins axi_hwicap_0/icap_clk]
# eos_in MUST be tied high: the HWICAP write FSM waits for end-of-startup
# and never completes a transfer if this dangles (CR write bit sticks at 1)
set eos [create_bd_cell -type ip -vlnv [ipv xlconstant] xlconstant_eos]
set_property -dict [list CONFIG.CONST_VAL {1} CONFIG.CONST_WIDTH {1}] $eos
connect_bd_net [get_bd_pins xlconstant_eos/dout] [get_bd_pins axi_hwicap_0/eos_in]

# Decouple control: 1-bit GPIO
set gpio1 [create_bd_cell -type ip -vlnv [ipv axi_gpio] axi_gpio_1]
set_property -dict [list CONFIG.C_GPIO_WIDTH {1} CONFIG.C_ALL_OUTPUTS {1}] $gpio1
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_gpio_1/S_AXI} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_gpio_1/S_AXI]

# DFX decoupler on the RM's AXI-Lite; rp side leaves the BD
set dec [create_bd_cell -type ip -vlnv [ipv dfx_decoupler] dfx_decoupler_0]
set_property -dict [list CONFIG.ALL_PARAMS { \
    HAS_SIGNAL_CONTROL 1 HAS_AXI_LITE 0 \
    INTF {rm {ID 0 MODE slave VLNV xilinx.com:interface:aximm_rtl:1.0 PROTOCOL axi4lite}} \
}] $dec
# Pass-through IP: automation refuses it, so grow the interconnect by hand
set periph [get_bd_cells microblaze_0_axi_periph]
set n [get_property CONFIG.NUM_MI $periph]
set idx [format %02d $n]
set_property CONFIG.NUM_MI [expr {$n + 1}] $periph
connect_bd_intf_net [get_bd_intf_pins microblaze_0_axi_periph/M${idx}_AXI] \
    [get_bd_intf_pins dfx_decoupler_0/s_rm]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] \
    [get_bd_pins microblaze_0_axi_periph/M${idx}_ACLK]
connect_bd_net [get_bd_pins rst_clk_wiz_0_100M/peripheral_aresetn] \
    [get_bd_pins microblaze_0_axi_periph/M${idx}_ARESETN]
connect_bd_net [get_bd_pins axi_gpio_1/gpio_io_o] [get_bd_pins dfx_decoupler_0/decouple]
externalize dfx_decoupler_0/rp_rm rm_axi

# Raw ICAPE2 writer control (fallback DFX load path): axi_gpio_2 ch1 =
# 32-bit config word, ch2 bit0 = go-toggle. The ICAPE2 + pulse logic live
# in the top-level wrapper (hw/rtl/tricorder_m5_top.v).
set gpio2 [create_bd_cell -type ip -vlnv [ipv axi_gpio] axi_gpio_2]
set_property -dict [list CONFIG.C_GPIO_WIDTH {32} CONFIG.C_ALL_OUTPUTS {1} \
    CONFIG.C_IS_DUAL {1} CONFIG.C_GPIO2_WIDTH {1} CONFIG.C_ALL_OUTPUTS_2 {1}] $gpio2
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_gpio_2/S_AXI} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_gpio_2/S_AXI]
create_bd_port -dir O -from 31 -to 0 icapw_data
connect_bd_net [get_bd_ports icapw_data] [get_bd_pins axi_gpio_2/gpio_io_o]
create_bd_port -dir O -from 0 -to 0 icapw_go
connect_bd_net [get_bd_ports icapw_go] [get_bd_pins axi_gpio_2/gpio2_io_o]

# AXIS-FIFO fast load path: axi_dma MM2S reads a staged partial from DDR2
# and streams it out of the BD; the top wrapper feeds the stream into the
# ICAPE2 sink (one config word per cycle). GPIO writer remains as fallback.
set dma [create_bd_cell -type ip -vlnv [ipv axi_dma] axi_dma_0]
set_property -dict [list \
    CONFIG.c_include_sg {0} \
    CONFIG.c_include_mm2s {1} \
    CONFIG.c_include_s2mm {0} \
    CONFIG.c_sg_length_width {26} \
    CONFIG.c_m_axi_mm2s_data_width {32} \
    CONFIG.c_m_axis_mm2s_tdata_width {32} \
    CONFIG.c_mm2s_burst_size {16} \
] $dma
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_dma_0/S_AXI_LITE} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_dma_0/S_AXI_LITE]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/axi_dma_0/M_AXI_MM2S} Slave {/mig_7series_0/S_AXI} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_dma_0/M_AXI_MM2S]
externalize axi_dma_0/M_AXIS_MM2S icap_axis

# QSPI config flash, XIP (execute-in-place) mode: the 16 MiB flash appears
# read-only at 0x60000000, so the DMA can stream a partial from flash
# straight into the ICAPE2 sink (and the CPU can read the slot table).
# Standard (single-line) SPI mode: works with the flash's factory QE=0;
# quad XIP is a later upgrade. SCK comes from STARTUPE2 (config pin).
set qspi [create_bd_cell -type ip -vlnv [ipv axi_quad_spi] axi_quad_spi_0]
set_property -dict [list \
    CONFIG.C_XIP_MODE {0} \
    CONFIG.C_SPI_MODE {2} \
    CONFIG.C_SCK_RATIO {2} \
    CONFIG.C_USE_STARTUP {0} \
    CONFIG.C_SPI_MEMORY {3} \
    CONFIG.C_FIFO_DEPTH {256} \
] $qspi
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins axi_quad_spi_0/ext_spi_clk]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_quad_spi_0/AXI_LITE} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins -quiet axi_quad_spi_0/AXI_LITE] [get_bd_intf_pins -quiet axi_quad_spi_0/S_AXI]
# Per-signal SPI ports (no intf externalization: the wrapper would emit an
# IOBUF for sck, but sck must drive the dedicated CCLK via STARTUPE2 in the
# top wrapper, which also owns the io0/io1/ss IOBUFs).
foreach {pin dir} {io0_i I io0_o O io0_t O io1_i I io1_o O io1_t O \
                   io2_i I io2_o O io2_t O io3_i I io3_o O io3_t O \
                   ss_i I ss_o O ss_t O sck_i I sck_o O} {
    create_bd_port -dir $dir qspi_${pin}
    connect_bd_net [get_bd_ports qspi_${pin}] [get_bd_pins axi_quad_spi_0/${pin}]
}

# RM clock + reset out to the top-level wrapper
create_bd_port -dir O -type clk rm_clk
connect_bd_net [get_bd_ports rm_clk] [get_bd_pins clk_wiz_0/clk_out1]
set_property CONFIG.ASSOCIATED_BUSIF rm_axi [get_bd_ports rm_clk]
create_bd_port -dir O -type rst -from 0 -to 0 rm_aresetn
connect_bd_net [get_bd_ports rm_aresetn] \
    [get_bd_pins rst_clk_wiz_0_100M/peripheral_aresetn]

# Deterministic addresses (sw/hello and sw/memtest depend on these)
assign_bd_address
set_property offset 0x40600000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_uartlite_0_Reg}]
set_property offset 0x40000000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_gpio_0_Reg}]
set_property offset 0x40010000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_gpio_1_Reg}]
set_property offset 0x40020000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_gpio_2_Reg}]
set_property offset 0x41E00000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_dma_0_Reg}]
set qreg [get_bd_addr_segs -of_objects [get_bd_addr_spaces microblaze_0/Data] -filter {NAME =~ "*quad_spi*"}]
if {[llength $qreg] == 1} { set_property offset 0x44A20000 $qreg }
# XIP segment carries usage=register; the DMA's MM2S space only auto-accepts
# usage=memory, so force-include it, then place it.
set_property offset 0x41C00000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_timer_0_Reg}]
set rm_seg [get_bd_addr_segs -of_objects [get_bd_addr_spaces microblaze_0/Data] \
    -filter {NAME =~ "*rm*"}]
if {[llength $rm_seg] == 1} { set_property offset 0x44A10000 $rm_seg }
set_property offset 0x44A00000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_hwicap_0_Reg}]
set_property offset 0x80000000 [get_bd_addr_segs {microblaze_0/Data/SEG_mig_7series_0_memaddr}]

validate_bd_design
save_bd_design
