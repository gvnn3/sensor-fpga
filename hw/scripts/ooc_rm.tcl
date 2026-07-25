# Out-of-context synthesis + utilization budget gate for every RM.
# Budget: the pblock (clock region X1Y2) offers ~9500 LUT / 40 DSP / 0 BRAM;
# gate each RM at 70% of that so partition headroom stays real.

set part xc7a100tcsg324-1
set out build/dfx
file mkdir $out

set LUT_BUDGET 6650
set DSP_BUDGET 28
set BRAM_BUDGET 0

set fail 0
foreach {rm src} {a hw/dfx/rm/rm_slot_a.v b hw/dfx/rm/rm_slot_b.v \
                  pdm hw/dfx/rm/rm_pdm.v sonar hw/dfx/rm/rm_sonar.v} {
    create_project -in_memory -part $part
    read_verilog $src
    synth_design -top rm_slot -mode out_of_context
    set luts  [expr {[llength [get_cells -hier -filter {IS_PRIMITIVE && REF_NAME =~ LUT*}]]}]
    set dsps  [llength [get_cells -hier -filter {IS_PRIMITIVE && REF_NAME =~ DSP48*}]]
    set brams [llength [get_cells -hier -filter {IS_PRIMITIVE && REF_NAME =~ RAMB*}]]
    report_utilization -file $out/rm_${rm}_util.rpt
    puts "OOC-RM $rm: LUT=$luts DSP=$dsps BRAM=$brams (budget $LUT_BUDGET/$DSP_BUDGET/$BRAM_BUDGET)"
    if {$luts > $LUT_BUDGET || $dsps > $DSP_BUDGET || $brams > $BRAM_BUDGET} {
        puts "OOC-RM: FAIL ($rm exceeds budget)"
        set fail 1
    }
    close_project
}
if {$fail} { exit 1 }
puts "OOC-RM: PASS (all RMs within budget)"
exit 0
