# Program build/flash/slots.mcs into the Nexys A7's S25FL128S QSPI flash
# over JTAG. Erases only the addressed sectors. Takes a few minutes.
# Run from repo root: vivado -mode batch -source hw/scripts/program_flash.tcl

open_hw_manager
connect_hw_server
open_hw_target
set dev [current_hw_device]

create_hw_cfgmem -hw_device $dev [lindex \
    [get_cfgmem_parts {s25fl128sxxxxxx0-spi-x1_x2_x4}] 0]
set cfgmem [current_hw_cfgmem]
set_property PROGRAM.FILES [list build/flash/boot.mcs] $cfgmem
set_property PROGRAM.ADDRESS_RANGE {use_file} $cfgmem
set_property PROGRAM.ERASE 1 $cfgmem
set_property PROGRAM.CFG_PROGRAM 1 $cfgmem
set_property PROGRAM.VERIFY 1 $cfgmem
set_property PROGRAM.CHECKSUM 0 $cfgmem

create_hw_bitstream -hw_device $dev \
    [get_property PROGRAM.HW_CFGMEM_BITFILE $dev]
program_hw_devices $dev            ;# load the flash-access proxy bitstream
program_hw_cfgmem $cfgmem          ;# boot bitstream at 0x0

set_property PROGRAM.FILES [list build/flash/slots.mcs] $cfgmem
program_hw_cfgmem $cfgmem          ;# data slots (partials + boot image)

puts "PROGRAM_FLASH: PASS"
close_hw_manager
exit 0
