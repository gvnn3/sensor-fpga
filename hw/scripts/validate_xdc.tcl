# Validate a tricorder master XDC against its port-complete stub top.
# Run from the repo root:
#   vivado -mode batch -nolog -nojournal -source hw/scripts/validate_xdc.tcl \
#       -tclargs <board>
# Boards: arty-a7 | nexys-a7. Synthesizes and places for the target part so
# illegal LOCs, port/pin mismatches, and clock-placement (CC-pin) problems
# all surface. Exits nonzero on any error, critical warning, or DRC
# violation — usable as a CI gate.

if {$argc < 1} {
    puts "VALIDATE: FAIL (usage: validate_xdc.tcl -tclargs <arty-a7|nexys-a7>)"
    exit 1
}
set board [lindex $argv 0]

# board -> {part top rtl xdc}
array set cfg {
    arty-a7  {xc7a100tcsg324-1 tricorder_top       hw/rtl/tricorder_top.v       hw/constraints/tricorder-arty-a7-100t.xdc}
    nexys-a7 {xc7a100tcsg324-1 tricorder_top_nexys hw/rtl/tricorder_top_nexys.v hw/constraints/tricorder-nexys-a7-100t.xdc}
}
if {![info exists cfg($board)]} {
    puts "VALIDATE: FAIL (unknown board '$board'; know: [array names cfg])"
    exit 1
}
lassign $cfg($board) part top rtl xdc

file mkdir build

read_verilog $rtl
read_xdc $xdc

synth_design -top $top -part $part
opt_design
place_design

report_drc            -file build/validate_${board}_drc.rpt
report_utilization    -file build/validate_${board}_util.rpt
report_clocks         -file build/validate_${board}_clocks.rpt

# Gate 1: no unplaced ports
set unplaced {}
foreach p [get_ports *] {
    if {[get_property PACKAGE_PIN $p] eq ""} { lappend unplaced $p }
}
if {[llength $unplaced]} {
    puts "VALIDATE: FAIL ($board: unplaced ports: $unplaced)"
    exit 1
}

# Gate 2: no DRC violations of severity Error (or worse) in the report
set drc_errors 0
set f [open build/validate_${board}_drc.rpt r]
while {[gets $f line] >= 0} {
    if {[regexp {^\s*\S+\s+(Error|Critical)} $line]} { incr drc_errors }
}
close $f
if {$drc_errors} {
    puts "VALIDATE: FAIL ($board: $drc_errors DRC errors — see build/validate_${board}_drc.rpt)"
    exit 1
}

# Gate 3: no critical warnings accumulated during the run
set ncrit [get_msg_config -count -severity {CRITICAL WARNING}]
if {$ncrit > 0} {
    puts "VALIDATE: FAIL ($board: $ncrit critical warnings)"
    exit 1
}

set nports [llength [get_ports *]]
puts "VALIDATE: $board — $nports top-level ports constrained and placed, 0 DRC errors"
puts "VALIDATE: PASS"
exit 0
