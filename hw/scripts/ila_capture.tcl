# Capture XIP AXI read beats on the system ILA while the RTOS demo runs
# (flash swap fires every ~8 s; trigger = RVALID high).
# Prereq: board already booted via run_rtos_dfx.xsdb with the ILA build.
# Run from repo root:
#   vivado -mode batch -nolog -nojournal -source hw/scripts/ila_capture.tcl
# Output: build/dfx/ila_capture.csv

open_hw_manager
connect_hw_server
open_hw_target
set dev [current_hw_device]
set_property PROBES.FILE build/dfx/full_a.ltx $dev
set_property FULL_PROBES.FILE build/dfx/full_a.ltx $dev
refresh_hw_device $dev

set ila [lindex [get_hw_ilas] 0]
if {$ila eq ""} { puts "ILA: FAIL (no ILA found)"; exit 1 }
puts "ILA: [get_property NAME $ila]"
foreach p [list] {
    puts "PROBE: [get_property NAME $p]"
}

set rvalid [lindex [get_hw_probes -of_objects $ila -filter {NAME =~ *axi_rvalid*}] 0]
if {$rvalid eq ""} { puts "ILA: FAIL (no RVALID probe)"; exit 1 }
set_property TRIGGER_COMPARE_VALUE eq1'b1 $rvalid
set_property CONTROL.TRIGGER_POSITION 8 $ila

run_hw_ila $ila
puts "ILA: armed, waiting for flash swap trigger (<=10 s)..."
wait_on_hw_ila $ila
display_hw_ila_data [upload_hw_ila_data $ila]
write_hw_ila_data -force -csv_file build/dfx/ila_capture.csv [current_hw_ila_data]
puts "ILA: PASS -> build/dfx/ila_capture.csv"
close_hw_manager
exit 0
