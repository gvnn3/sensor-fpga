# M0 check: is DFX (partial reconfiguration) license-free for 7-series in
# this Vivado? Synthesizes a trivial parent+child in memory and sets
# HD.RECONFIGURABLE — the point where a missing PR/DFX license errors out.
# Usage: vivado -mode batch -nolog -nojournal -source hw/scripts/check_dfx_license.tcl

set part xc7a100tcsg324-1
set tmp [file join [pwd] build dfx_check]
file mkdir $tmp

set f [open [file join $tmp child.v] w]
puts $f {module child (input wire clk, input wire d, output reg q);
  always @(posedge clk) q <= d;
endmodule}
close $f

set f [open [file join $tmp top.v] w]
puts $f {module top (input wire clk, input wire d, output wire q);
  child u_child (.clk(clk), .d(d), .q(q));
endmodule}
close $f

read_verilog [file join $tmp child.v]
read_verilog [file join $tmp top.v]
synth_design -top top -part $part

if {[catch {
    set_property HD.RECONFIGURABLE true [get_cells u_child]
    opt_design
} err]} {
    puts "DFX CHECK: FAIL — $err"
    exit 1
}
puts "DFX CHECK: PASS — HD.RECONFIGURABLE accepted, no license error (part $part, [version -short])"
exit 0
