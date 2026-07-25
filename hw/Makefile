# Tricorder CI targets (plan section 4 "CI story"). Run from repo root:
#   make -C hw validate | ooc-rm | static | dfx | rtos
# Every target exits nonzero on failure. Only requirement: Vivado 2025.2
# (override VIVADO= / VITIS= for other installs).

VIVADO ?= $(HOME)/Vivado/2025.2/Vivado/bin/vivado
VITIS  ?= $(HOME)/Vivado/2025.2/Vitis/bin/vitis
VFLAGS  = -mode batch -nolog -nojournal
ROOT    = $(abspath ..)

.PHONY: validate ooc-rm static dfx rtos all

# Pin-plan gate for both boards (synth+place against the real XDCs)
validate:
	cd $(ROOT) && $(VIVADO) $(VFLAGS) -source hw/scripts/validate_xdc.tcl -tclargs nexys-a7
	cd $(ROOT) && $(VIVADO) $(VFLAGS) -source hw/scripts/validate_xdc.tcl -tclargs arty-a7

# Out-of-context synth of every RM (utilization reports land in build/dfx)
ooc-rm:
	cd $(ROOT) && $(VIVADO) $(VFLAGS) -source hw/scripts/ooc_rm.tcl

# Full static-region build (BD + DFX top with RM-A resolved) + XSA export
static:
	cd $(ROOT) && $(VIVADO) $(VFLAGS) -source hw/scripts/build_static.tcl

# Full DFX flow: 4 configs, pr_verify, partial .bit/.bin set
dfx:
	cd $(ROOT) && $(VIVADO) $(VFLAGS) -source hw/dfx/build_dfx.tcl

# FreeRTOS platform + app (needs build/static_region.xsa from `static`)
rtos:
	cd $(ROOT) && $(VITIS) -s sw/scripts/build_rtos.py

all: validate static dfx rtos
