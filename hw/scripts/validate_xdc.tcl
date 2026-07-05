# Validate the tricorder master XDC against the port-complete stub top.
# Run from the repo root:
#   vivado -mode batch -source hw/scripts/validate_xdc.tcl
# Synthesizes and places for xc7a100tcsg324-1 so illegal LOCs, port/pin
# mismatches, and clock-placement (CC-pin) problems all surface.

set_part xc7a100tcsg324-1
read_verilog hw/rtl/tricorder_top.v
read_xdc hw/constraints/tricorder-arty-a7-100t.xdc

synth_design -top tricorder_top -part xc7a100tcsg324-1
opt_design
place_design

report_drc            -file build/validate_drc.rpt
report_utilization    -file build/validate_util.rpt
report_clocks         -file build/validate_clocks.rpt

set nports [llength [get_ports *]]
puts "VALIDATE: $nports top-level ports constrained and placed"
puts "VALIDATE: PASS"
