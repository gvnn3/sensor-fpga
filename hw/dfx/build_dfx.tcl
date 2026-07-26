# M5 DFX flow (non-project). Requires the static_region BD to exist in
# build/m2_proj (created by hw/scripts/build_static.tcl). Run from repo root:
#   vivado -mode batch -nolog -nojournal -source hw/dfx/build_dfx.tcl
# Products in build/dfx/:
#   full_a.bit                 full config with RM-A (power-on image)
#   rm_a_partial.{bit,bin}     partials for HWICAP/JTAG loading
#   rm_b_partial.{bit,bin}
#   pr_verify PASS is a hard gate.

set part xc7a100tcsg324-1
set out build/dfx
file mkdir $out

# --- RM out-of-context synthesis ------------------------------------------
foreach {rm src} {a hw/dfx/rm/rm_slot_a.v b hw/dfx/rm/rm_slot_b.v \
                  pdm hw/dfx/rm/rm_pdm.v sonar hw/dfx/rm/rm_sonar.v} {
    create_project -in_memory -part $part
    read_verilog $src
    synth_design -top rm_slot -mode out_of_context
    write_checkpoint -force $out/rm_$rm.dcp
    report_utilization -file $out/rm_${rm}_util.rpt
    close_project
}

# --- Static synthesis (BD + M5 top, rm_slot black-boxed) ------------------
create_project -in_memory -part $part
set bd [glob build/m2_proj/m2_static.srcs/sources_1/bd/static_region/static_region.bd]
read_bd $bd
if {![get_property IS_GENERATED [get_files $bd]]} { generate_target all [get_files $bd] }
read_verilog build/m2_proj/m2_static.gen/sources_1/bd/static_region/hdl/static_region_wrapper.v
read_verilog hw/rtl/tricorder_m5_top.v
read_verilog hw/dfx/rm_slot_bb.v
read_xdc hw/constraints/m2-static-nexys-a7.xdc
read_xdc hw/dfx/mic.xdc
read_xdc hw/dfx/ultra.xdc
synth_design -top tricorder_m5_top -part $part
write_checkpoint -force $out/static_synth.dcp

# --- Configuration A: link, floorplan, implement --------------------------
read_checkpoint -cell u_rm $out/rm_a.dcp
set_property HD.RECONFIGURABLE true [get_cells u_rm]
read_xdc hw/dfx/pblock_rm.xdc
opt_design
place_design
route_design
report_timing_summary -file $out/config_a_timing.rpt
if {[get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]] < 0} {
    puts "DFX: FAIL (config A negative slack)"
    exit 1
}
write_checkpoint -force $out/config_a_routed.dcp
# Compressed bitstreams (MFWR frames): smaller partials load
# proportionally faster through the DMA->ICAPE2 path
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
write_bitstream -force -no_partial_bitfile $out/full_a.bit
write_bitstream -force -bin_file -cell u_rm $out/rm_a_partial.bit

# --- Carve out the RM, lock static, swap in each remaining RM -------------
# Order matters: black-box BEFORE lock_design, else the RM's own routing is
# locked and cannot be unrouted.
update_design -cell u_rm -black_box
lock_design -level routing
foreach rm {b pdm sonar} {
    read_checkpoint -cell u_rm $out/rm_$rm.dcp
    opt_design
    place_design
    route_design
    report_timing_summary -file $out/config_${rm}_timing.rpt
    if {[get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]] < 0} {
        puts "DFX: FAIL (config $rm negative slack)"
        exit 1
    }
    write_checkpoint -force $out/config_${rm}_routed.dcp
    set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
    write_bitstream -force -bin_file -cell u_rm $out/rm_${rm}_partial.bit
    if {$rm ne "sonar"} { update_design -cell u_rm -black_box }
}

# --- Verify all configurations are swap-compatible ------------------------
pr_verify -initial $out/config_a_routed.dcp \
    -additional "$out/config_b_routed.dcp $out/config_pdm_routed.dcp $out/config_sonar_routed.dcp"
puts "DFX: PASS (pr_verify ok; partials in $out)"
exit 0
