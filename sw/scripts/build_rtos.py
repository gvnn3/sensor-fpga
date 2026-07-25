# Vitis (SDT/unified) build for the M4 FreeRTOS app.
# Run from the repo root:
#   ~/Vivado/2025.2/Vitis/bin/vitis -s sw/scripts/build_rtos.py
# Consumes build/static_region.xsa; produces
# build/vitis-ws/tricorder_rtos/build/tricorder_rtos.elf

import os
import shutil
import vitis

REPO = os.getcwd()
WS = os.path.join(REPO, "build", "vitis-ws")
XSA = os.path.join(REPO, "build", "static_region.xsa")
SRC = os.path.join(REPO, "sw", "tricorder-rtos", "src")

if os.path.isdir(WS):
    shutil.rmtree(WS)

client = vitis.create_client()
client.set_workspace(path=WS)

platform = client.create_platform_component(
    name="tricorder_pf",
    hw_design=XSA,
    cpu="microblaze_0",
    os="freertos",
)
platform.build()

app = client.create_app_component(
    name="tricorder_rtos",
    platform=os.path.join(WS, "tricorder_pf", "export", "tricorder_pf",
                          "tricorder_pf.xpfm"),
    domain="freertos_microblaze_0",
)

# Replace template sources with ours
comp_src = os.path.join(WS, "tricorder_rtos", "src")
for f in os.listdir(comp_src):
    if f.endswith((".c", ".h")):
        os.remove(os.path.join(comp_src, f))
for f in os.listdir(SRC):
    shutil.copy(os.path.join(SRC, f), comp_src)

# FreeRTOS + drivers (~100 KB) cannot fit the 32 KB LMB: run from DDR2.
# Vectors stay in LMB (base region default); everything else to the MIG.
ls = os.path.join(comp_src, "lscript.ld")
lines = open(ls).read().splitlines(keepends=True)
sect = ""
out = []
for ln in lines:
    stripped = ln.strip()
    if stripped.startswith("."):
        sect = stripped.split()[0]
    if "> microblaze_0_local_memory" in ln and not sect.startswith(".vectors"):
        ln = ln.replace("> microblaze_0_local_memory_dlmb_bram_if_cntlr_memory_0",
                        "> mig_7series_0_memory_0")
    out.append(ln)
open(ls, "w").write("".join(out))

app.build()
print("BUILD_RTOS: done")
vitis.dispose()
