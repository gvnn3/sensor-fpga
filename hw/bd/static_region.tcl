# M2 static-region block design: classic MicroBlaze + 32 KB LMB + UARTlite
# + GPIO LEDs. DDR2 MIG, Ethernet, I2C, HWICAP, intc arrive in M3-M5.
# Sourced by hw/scripts/build_static.tcl inside a project targeting
# xc7a100tcsg324-1. CPU decision (notebook entry 14): classic MicroBlaze —
# the 2025.2 FreeRTOS BSPs port list is ARM + MicroBlazeV9 only, no
# MicroBlaze-V/RISC-V port, so FreeRTOS (fixed requirement) forces classic.

proc ipv {name} {
    # newest xilinx.com VLNV for an IP name, so this script survives version bumps
    return [lindex [lsort -dictionary [get_ipdefs -all -filter "NAME == $name && VLNV =~ xilinx.com:ip:*"]] end]
}

create_bd_design "static_region"

# Clocking: board 100 MHz -> clk_wiz -> 100 MHz system clock
set clk_wiz [create_bd_cell -type ip -vlnv [ipv clk_wiz] clk_wiz_0]
set_property -dict [list \
    CONFIG.PRIM_IN_FREQ {100.000} \
    CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {100.000} \
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
    local_mem "32KB" ecc "None" cache "None" debug_module "Debug Only" \
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

# 4 green LEDs
set gpio [create_bd_cell -type ip -vlnv [ipv axi_gpio] axi_gpio_0]
set_property -dict [list CONFIG.C_GPIO_WIDTH {4} CONFIG.C_ALL_OUTPUTS {1}] $gpio
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { \
    Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/microblaze_0 (Periph)} Slave {/axi_gpio_0/S_AXI} \
    ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0} \
} [get_bd_intf_pins axi_gpio_0/S_AXI]
make_bd_intf_pins_external [get_bd_intf_pins axi_gpio_0/GPIO]
set_property name led [get_bd_intf_ports GPIO_0]

# Deterministic peripheral addresses (sw/hello depends on these)
assign_bd_address
set_property offset 0x40600000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_uartlite_0_Reg}]
set_property offset 0x40000000 [get_bd_addr_segs {microblaze_0/Data/SEG_axi_gpio_0_Reg}]

validate_bd_design
save_bd_design
