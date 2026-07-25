# Program a bitstream into the attached board over JTAG (reusable helper).
# Usage: vivado -mode batch -nolog -nojournal -source hw/scripts/program.tcl \
#            -tclargs <path/to/file.bit>
# Exits 0 only if programming succeeds and DONE is high.

if {$argc < 1} {
    puts "usage: program.tcl <bitfile>"
    exit 1
}
set bitfile [lindex $argv 0]
if {![file exists $bitfile]} {
    puts "PROGRAM: FAIL (no such file: $bitfile)"
    exit 1
}

open_hw_manager
connect_hw_server
open_hw_target

set dev [current_hw_device]
puts "PROGRAM: device [get_property PART $dev]"

set_property PROGRAM.FILE $bitfile $dev
program_hw_devices $dev
refresh_hw_device $dev

set done [get_property REGISTER.IR.BIT5_DONE $dev]
if {$done ne "1"} {
    puts "PROGRAM: FAIL (DONE=$done)"
    exit 1
}

# Bonus sanity: read on-die temperature via XADC/sysmon
set sysmon [lindex [get_hw_sysmons -of_objects $dev] 0]
if {$sysmon ne ""} {
    puts "PROGRAM: die temp [get_property TEMPERATURE $sysmon] C"
}

puts "PROGRAM: PASS (DONE=1)"
close_hw_manager
exit 0
