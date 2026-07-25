# Build the M0 blinky bitstream for the Nexys A7-100T (non-project batch flow).
# Usage: vivado -mode batch -nolog -nojournal -source hw/scripts/build_blinky.tcl
# Output: build/blinky.bit + reports under build/

set part xc7a100tcsg324-1
set root [file normalize [file join [file dirname [info script]] .. ..]]
set build [file join $root build]
file mkdir $build

read_verilog [file join $root hw/bringup/blinky.v]
read_xdc     [file join $root hw/bringup/blinky-nexys-a7.xdc]

synth_design -top blinky -part $part
opt_design
place_design
route_design

report_timing_summary -file [file join $build blinky_timing.rpt]
report_utilization    -file [file join $build blinky_util.rpt]
report_drc            -file [file join $build blinky_drc.rpt]

# Fail loudly on timing
if {[get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]] < 0} {
    puts "BUILD: FAIL (negative setup slack)"
    exit 1
}

write_bitstream -force [file join $build blinky.bit]
puts "BUILD: PASS -> build/blinky.bit"
exit 0
