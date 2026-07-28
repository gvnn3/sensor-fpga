# Build the M2 static-region bitstream + XSA (project mode, throwaway
# project under build/). Run from the repo root:
#   vivado -mode batch -nolog -nojournal -source hw/scripts/build_static.tcl
# Outputs: build/static_region.bit, build/static_region.xsa, reports.

set part xc7a100tcsg324-1
file mkdir build
create_project -force m2_static build/m2_proj -part $part

source hw/bd/static_region.tcl

make_wrapper -files [get_files static_region.bd] -top -import
# Top = DFX top with RM-A as an ordinary source (no black box here); the
# DFX flow (hw/dfx/build_dfx.tcl) does its own synthesis with rm_slot
# black-boxed. This build feeds the M4 FreeRTOS gate and exports the XSA.
add_files -norecurse {hw/rtl/tricorder_m5_top.v hw/rtl/sevenseg_scan.v \
    hw/rtl/audio_player.v hw/rtl/rmii_shim.v hw/dfx/rm/rm_slot_a.v}
add_files -fileset constrs_1 {hw/constraints/m2-static-nexys-a7.xdc \
    hw/constraints/switches-7seg.xdc hw/constraints/ethernet.xdc \
    hw/dfx/mic.xdc hw/dfx/ultra.xdc}
set_property top tricorder_m5_top [current_fileset]

launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_runs impl_1
if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {
    puts "BUILD: FAIL (impl_1 [get_property STATUS [get_runs impl_1]])"
    exit 1
}

open_run impl_1
set wns [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]]
report_timing_summary -file build/static_timing.rpt
report_utilization    -file build/static_util.rpt
if {$wns < 0} {
    puts "BUILD: FAIL (negative setup slack $wns)"
    exit 1
}

file copy -force build/m2_proj/m2_static.runs/impl_1/tricorder_m5_top.bit build/static_region.bit
write_hw_platform -fixed -include_bit -force build/static_region.xsa
puts "BUILD: PASS -> build/static_region.bit / .xsa (WNS $wns ns)"
exit 0
