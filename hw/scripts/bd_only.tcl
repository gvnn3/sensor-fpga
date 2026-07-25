# Fast BD elaboration check (no synthesis). Run from repo root:
#   vivado -mode batch -nolog -nojournal -source hw/scripts/bd_only.tcl
create_project -force bd_check build/bd_check -part xc7a100tcsg324-1
source hw/bd/static_region.tcl
puts "BD_ONLY: PASS"
exit 0
