# DFX reconfigurable-partition floorplan for the M5 hello-swap RM.
# One full clock region (RESET_AFTER_RECONFIG requires full CR height on
# 7-series). Region chosen away from the DDR2/MIG banks and config center;
# SNAPPING_MODE trims to legal reconfigurable-frame boundaries. Override
# region via -generic? No: edit here if placement conflicts appear.

# Explicit site ranges for clock region X1Y2 (full 50-row height satisfies
# RESET_AFTER_RECONFIG). A CLOCKREGION range + SNAPPING snapped to empty on
# this device/tool combo; explicit ranges do not. X1Y2 has no BRAM — fine
# for the current RMs (0 BRAM); move to X1Y1 if a future RM needs BRAM.
create_pblock pblock_rm
add_cells_to_pblock [get_pblocks pblock_rm] [get_cells u_rm]
resize_pblock [get_pblocks pblock_rm] -add {SLICE_X52Y100:SLICE_X89Y149 DSP48_X1Y40:DSP48_X2Y59}
set_property RESET_AFTER_RECONFIG true [get_pblocks pblock_rm]
set_property SNAPPING_MODE ON [get_pblocks pblock_rm]
