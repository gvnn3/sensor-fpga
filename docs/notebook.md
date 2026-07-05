# Laboratory Notebook — sensor-fpga

Experiments on dynamic sensor FPGA programs: each sensor type can be loaded
or removed at runtime from the FPGA dynamic (partial reconfiguration) region.

**Conventions**

- Entries are kept in **reverse chronological order** (newest first, directly
  below the Table of Contents).
- Every entry title carries the **date and time of creation**:
  `# EXPERIMENT DD Mon YYYY HH:MM:SS Title :status:`
- Status tags: `:complete:` or `:in_progress:`.
- Entries are separated by `---` and are self-contained (repeat hardware and
  software context as needed).
- Section 3 records raw observations; Section 4 interprets them. Always
  include units; bold key results.

# Table of Contents

7. [EXPERIMENT  5 Jul 2026 04:30:06 CPU Options on the Artix-7](#5-jul-2026-043006) :complete:
6. [EXPERIMENT  5 Jul 2026 04:11:47 Prototyping the Tricorder on the Alveo U250 Without the Artix-7](#5-jul-2026-041147) :complete:
5. [EXPERIMENT  5 Jul 2026 03:53:38 Tricorder Parts List Received (Artix-7 Target)](#5-jul-2026-035338) :complete:
4. [EXPERIMENT  5 Jul 2026 03:41:44 Extraction of Shared Conversation: FPGA as a Sensor Platform](#5-jul-2026-034144) :complete:
3. [EXPERIMENT  4 Jul 2026 07:34:03 OHWR Cores for the Dynamic Region of This Host (Alveo U250)](#4-jul-2026-073403) :complete:
2. [EXPERIMENT  4 Jul 2026 07:26:08 Vitis Libraries Suitability for the FPGA Dynamic Region](#4-jul-2026-072608) :complete:
1. [EXPERIMENT  4 Jul 2026 07:20:01 Repository and Notebook Initialization](#4-jul-2026-072001) :complete:

---

# EXPERIMENT  5 Jul 2026 04:30:06 CPU Options on the Artix-7 :complete:

## 1. Hypothesis

Does the Artix-7 provide a CPU capable of running a typical program (the
tricorder's control/UI/sensor-management software)?

## 2. How

- **Equipment:** target Artix-7 board (entry 5); exact board TBD, Arty
  A7-100T assumed (xc7a100t, 256 MB DDR3).
- **Software:** design analysis; Vivado/Vitis, LiteX, OHWR cores.
- **Benchmarks:** none.

### Key commands

```bash
# none run; candidate flows listed in section 4
```

## 3. Observations

- Artix-7 is **pure FPGA fabric — no hard CPU**. (The 7-series part with a
  hard CPU is Zynq-7000: dual Cortex-A9 + Artix-class fabric.)
- A CPU must therefore be a **soft core** in the static region. Typical
  clock ~100 MHz on A7; DDR3 on Arty A7 gives enough memory for an OS.
- Candidates: Xilinx MicroBlaze / MicroBlaze-V (RISC-V, Vitis toolchain),
  VexRiscv or NEORV32 via LiteX (Linux-capable with litedram), OHWR uRV /
  Mock Turtle (entry 3 — deterministic real-time, no OS), PicoRV32 (tiny).

## 4. Data analysis

Yes in practice: a soft CPU in the static region runs the "typical program",
and this dovetails with the DFX architecture — the soft CPU is the natural
owner of the low-rate I2C sensors (entry 5), the UI/host link, and crucially
the **ICAP controller for loading/unloading the DFX sensor modules**, making
the tricorder self-reconfiguring without a PC. Options by need:

| Need | Choice | Program environment |
|---|---|---|
| Bare-metal/FreeRTOS C, vendor-supported | MicroBlaze(-V) | Vitis SDK, newlib, ~100 MHz |
| Full Linux (files, net, Python) | VexRiscv + LiteX | mainline-ish Linux on Arty A7, slow but real |
| Hard real-time sensor firmware | uRV / Mock Turtle | per-node firmware, no OS jitter |

Fabric cost on xc7a100t (~63k LUT): MicroBlaze or VexRiscv + DDR controller
+ peripherals ≈ 10-20% — leaves ample room for the DFX partition, but must
be counted in the entry-6 partition budget. If Linux-class software is a
hard requirement, switching the board to a Zynq-7000 (e.g., PYNQ-Z2, same
price class, hard A9 + identical DFX story) is the cleaner path.

## 5. Ideas for future experiments

- Decide software tier: bare-metal MicroBlaze-V vs. LiteX/Linux vs. Zynq
  board swap; prototype the choice on the U250 first (soft cores are
  portable).
- Measure ICAP-driven partial reconfiguration initiated from the soft CPU.
- Out-of-context build: MicroBlaze-V + DDR3 + I2C + ICAP static region on
  xc7a100t; record utilization to fix the DFX partition size.

---

# EXPERIMENT  5 Jul 2026 04:11:47 Prototyping the Tricorder on the Alveo U250 Without the Artix-7 :complete:

## 1. Hypothesis

Can the tricorder design (entry 5) be prototyped on this host's Alveo U250
before the Artix-7 board and sensors are in hand?

## 2. How

- **Equipment:** Alveo U250 in this host (entry 3: OpenNIC shell flashed,
  Vitis XDMA DFX platform installed); no sensor hardware present.
- **Software:** analysis against entries 3-5; Vivado/Vitis DFX flows.
- **Benchmarks:** none (design analysis).

### Key commands

```bash
# none run; proposed flows referenced in section 4
```

## 3. Observations

- The U250 dynamic region receives only AXI4-Lite + AXI4-MM (XDMA shell) or
  AXI4-Stream packet ports (OpenNIC); **no external pins** — none of the 13
  BOM parts can attach physically (no Pmod, DVP, I2C header, or analog I/O).
- Everything *behind* the front-end pins is plain streaming RTL/HLS: the
  seven DFX sensor pipelines consume sample streams (PDM bits, pixels, ADC
  words, photon timestamps) that can equally come from host DMA.
- DFX mechanics exist on both devices but differ: UltraScale+ (U250) vs.
  7-series (A7) have different reconfiguration frame granularity, ICAP
  bandwidth, and DFX controller behavior — absolute numbers won't transfer.
- A7 *fit* can be checked without any hardware: Vivado synthesizes/implements
  for an Artix-7 part out of context from just the RTL.

## 4. Data analysis

Yes — as a **processing-first prototype with emulated sensors**; the only
things that cannot be prototyped are the physical front ends themselves.
Practical split:

| Prototypable on U250 now | Not prototypable without A7/sensors |
|---|---|
| All 7 DFX pipeline modules (beamformer, vision, TDC, sonar DSP, hls4ml) fed by host-DMA'd recorded/synthetic streams at real sample rates | Analog front-end behavior (AD9226/AD8332/SiPM signal quality) |
| Full DFX lifecycle: partition, partial bitstreams, runtime load/swap, SDB discovery, host tooling | I/O timing closure: DVP capture, PDM clocking, I2C bring-up |
| Module-boundary spec (AXI4-Lite + AXI4-Stream) — identical wrapper reused on A7 | A7 partition fit/timing (but checkable via A7 out-of-context builds, no board needed) |
| Sensor bus-functional models (PDM bitstream gen, DVP pixel player, SiPM pulse statistics, echo models) — become the A7 verification testbench | Real-world latency of sensor→pin→pipeline path |

A hybrid is also available: real sensors on any cheap MCU/small-FPGA board
streaming over Ethernet into the OpenNIC 250 MHz user box, so live data
reaches U250-resident modules before the A7 exists. Main risk to track:
U250's abundance hides A7 scarcity — every module must carry an A7
out-of-context utilization report as a gate.

## 5. Ideas for future experiments

- Stand up the XDMA DFX platform: one reconfigurable partition, hello-world
  module pair, measure swap time on UltraScale+ as a baseline.
- Build the sensor-emulator host tool (replay PDM/DVP/ADC traces via DMA at
  true rates).
- CI gate: `synth_design -mode out_of_context -part xc7a100t...` for every
  module; fail if utilization exceeds the planned A7 partition.
- Pick the A7 board (Arty A7-100T likely) to fix the partition budget.

---

# EXPERIMENT  5 Jul 2026 03:53:38 Tricorder Parts List Received (Artix-7 Target) :complete:

## 1. Hypothesis

What sensors does the tricorder parts list (`docs/tricorder_parts.xlsx`,
committed by George as 078441b "A list of parts for a tricorder projects
with the Xilinx A7") specify, and how do they map onto static-region vs.
dynamic-region (DFX) processing?

## 2. How

- **Equipment:** target board is a **Xilinx Artix-7** (per commit message) —
  not the Alveo U250 surveyed in entry 3; the A7 board supplies the physical
  sensor I/O the U250 lacks.
- **Software:** xlsx parsed by unzipping and reading the sheet XML directly
  (no openpyxl installed on this host).
- **Benchmarks:** none.

### Key commands

```bash
unzip -o tricorder_parts.xlsx -d /tmp/xlsx   # then parse xl/worksheets/sheet1.xml
```

## 3. Observations

13 line items, **total $392** (prices dated 2026-07-05, mix of VERIFIED and
ESTIMATE; unit prices editable, totals are formulas). The sheet itself
categorizes parts as **"High-rate / DFX"** vs. "Low-rate / quality":

*Revision (5 Jul 2026 04:19, commit 568ed36 "Updated with the columns
fixed."):* the original file had a mis-aligned Price Status column (URLs
shifted into it); the fixed sheet shows only **BME688 and VEML7700 are
VERIFIED** (DigiKey, Jul 2026) — all other prices are ESTIMATEs to confirm
before ordering, incl. the $150 SiPM eval (hobby SiPM alternative $30-60).
Parts, quantities, and the $392 total are unchanged.

| Category | Part | Qty | Total | Tricorder function |
|---|---|---|---|---|
| Low-rate | Adafruit BME688 #5046 (gas+T+RH+P, I2C) | 1 | $31.11 | air composition/environment |
| Low-rate | Adafruit SCD-41 #5190 (NDIR true CO2, I2C) | 1 | $49.50 | atmospheric CO2 |
| Low-rate | Adafruit VEML7700 #4162 (lux, I2C) | 1 | $7.24 | light scan |
| Low-rate | Adafruit AS7341 #4698 (11-ch spectral, I2C) | 1 | $15.95 | crude spectroscopy |
| **DFX** | Adafruit PDM MEMS mic #3492 | 6 | $29.70 | acoustic beamforming array |
| Low-rate opt. | SparkFun MAX30105 #14045 (pulse-ox, I2C) | 1 | $19.50 | medical pulse/SpO2 |
| **DFX** | AD9226 ADC module (12-bit 65 MS/s, Pmod) | 1 | $25 | EM/RF scan, sonar RX, transients |
| **DFX** | AD9708 DAC module (8-bit 125 MS/s) | 1 | $20 | stimulus source / ultrasound TX |
| **DFX** | OV7670 camera (DVP 640x480@30, no-FIFO version) | 1 | $8 | vision/motion |
| **DFX** | onsemi MICROFC-SMA-10035 SiPM eval | 1 | $150 | photon counting/ToF (needs TLV3501 comparator) |
| **DFX** | AD8332 VGA board (ultrasound AFE) | 1 | $30 | sonar/structure probing |
| **DFX** | 40 kHz piezo pair (TX/RX) | 2 | $6 | pulse-echo sonar |

## 4. Data analysis

The sheet's own "High-rate / DFX" tag confirms the architecture direction:
slow I2C environmental sensors (BME688, SCD-41, VEML7700, AS7341, MAX30105)
belong in an always-present static-region I2C subsystem (or soft-CPU
firmware, cf. Mock Turtle in entry 3), while the seven high-rate front ends
(PDM mic array, AD9226, AD9708, OV7670, SiPM, AD8332, piezo) each demand a
dedicated processing pipeline — exactly the per-sensor loadable DFX modules
this project is about. The Artix-7 (commit message "Xilinx A7") resolves
entry 4's open question of how physical sensors attach: the A7 board owns
the front-end pins; DFX on 7-series is supported by Vivado (though with
coarser reconfigurable-frame granularity than UltraScale+, and the smaller
A7 makes partition sizing tighter). Candidate module sources line up:
Vitis Vision (OV7670 pipeline), CIC/FIR decimators (PDM array beamforming),
hls4ml classifier (acoustic/vision inference), OHWR-style TDC (SiPM photon
timestamps), DDS + matched filter (AD9708/AD8332/piezo sonar).

## 5. Ideas for future experiments

- Identify the exact Artix-7 board (Arty A7? Nexys?) and its Pmod/DVP pin
  budget vs. this sensor set.
- Define the DFX partition boundary on the A7: one reconfigurable partition
  with AXI4-Lite + AXI4-Stream, sized for the largest candidate module.
- First swap pair: OV7670 capture pipeline vs. PDM 6-mic beamformer —
  measure partition utilization and partial-bitstream load time via ICAP.
- Static region: MicroBlaze/uRV + I2C for the low-rate sensors, running
  regardless of which DFX module is loaded.

---

# EXPERIMENT  5 Jul 2026 03:41:44 Extraction of Shared Conversation: FPGA as a Sensor Platform :complete:

## 1. Hypothesis

What background research does the shared claude.ai conversation
(https://claude.ai/share/3ff13c9b-f27d-47c6-ba76-99bd91a29c7b) contain that
is relevant to this project? (Direct fetch failed — client-side rendering +
Cloudflare; content obtained by pasting the transcript.)

## 2. How

- **Equipment:** none (transcript extraction)
- **Software:** transcript pasted by George on 5 Jul 2026; full excerpt in
  `docs/prompts.md` (entry 5 Jul 2026 03:41:44)
- **Benchmarks:** none

### Key commands

```bash
# Both fetch attempts failed before the paste:
#   WebFetch https://claude.ai/share/3ff13c9b-...  -> SPA shell only
#   curl https://claude.ai/api/chat_snapshots/...  -> Cloudflare challenge
```

## 3. Observations

The conversation covered three questions:

**(a) Sensor data classes where FPGA custom circuitry pays off** — common
thread: fixed dataflow, fine-grained parallelism, hard deadlines that an
interrupt-driven CPU path (~10 µs floor, jitter) cannot meet:

| Domain | Data / task | Latency scale |
|---|---|---|
| RF / SDR, radar | ADC streams (100s MS/s–GS/s); DDC, FFT, matched filter, pulse compression | keep up with ADC, deterministic |
| Machine vision | per-pixel Bayer demosaic, defect correction, blob/edge, pre-DRAM | control loops in tens of µs |
| Physics instrumentation | detector L1 triggers (LHC: keep/discard on TB/s), photon counting, TDC | ~µs decisions, **sub-ns** timestamps |
| Control loops | current sensing → PWM, adaptive optics, plasma control | **< 1 µs**, jitter-free |
| (Deprioritized) networks | line-rate 100G+ filtering, HFT tick-to-trade | sub-µs |

**(b) Open-source circuit libraries** (physics/vision prioritized over
network): CERN **OHWR** (TDC cores, FMC ADC/DAC gateware, White Rabbit),
**hls4ml** (NN → HLS for ~100 ns–1 µs trigger inference), **FINN** (AMD,
quantized-NN dataflow), **Vitis Vision** (Apache-2.0 OpenCV-equivalent HLS
kernels), **PYNQ overlays**, **Vitis DSP** (FFT/FIR/CORDIC), **LiteX** /
Migen/Amaranth + **FuseSoC** packaging, **ZipCPU** formally verified
FFT/filter cores. Caveat from the conversation: outside OHWR and the Vitis
libraries, quality drops fast; OpenCores is "scavenging territory".

**(c) Artifacts produced there but NOT included in the paste:** a 14-entry
BibTeX bibliography ("Fpga sensor libs", with verified entries: Duarte 2018
JINST 13 P07027; Schulte 2026 TRETS hls4ml; FINN FPGA'17; FINN-R TRETS 2018;
White Rabbit ISPCS 2009/2011; LiteX arXiv:2005.02506; FuseSoC OSDA 2019;
flagged-unverified: Fahim 2021 hls4ml codesign, FINN-R DOI, OHWR/ICALEPCS
per-core papers) and a "Tricorder parts" XLSX (Adafruit-first sensor BOM
incl. AD9226 ADC, AD9708 DAC, OV7670 camera, MAX30105, SiPM, AD8332 AFE,
piezo transducers).

## 4. Data analysis

The conversation converges with notebook entries 2–3: OHWR + Vitis libraries
are the two defensible open-source pools, now motivated from the *workload*
side (physics timing, vision pipelines, tight control loops). New additions
to our candidate list are the ML-inference generators (**hls4ml**, **FINN**)
— attractive for the dynamic region because each trained sensor model
compiles to a self-contained streaming dataflow core, a natural per-sensor
reconfigurable module. The tricorder BOM implies analog front-ends (ADC,
camera, pulse-ox, SiPM) that the Alveo U250 cannot attach directly (entry
3's I/O constraint) — a carrier/mezzanine or a second, I/O-capable board
would be needed for real front-ends, with the U250 doing the heavy
processing.

## 5. Ideas for future experiments

- Obtain the actual BibTeX file and XLSX from the original conversation and
  commit them under `docs/` (bibliography.bib, tricorder-parts).
- Trial hls4ml: train a small model, generate an HLS core, synthesize for
  `xcu250`, and measure resources vs. a DFX partition budget.
- Compare a Vitis Vision pipeline vs. an hls4ml classifier as the first two
  swappable "sensor programs".
- Decide how physical sensors attach (separate I/O board + network into the
  U250 OpenNIC path vs. a different carrier with FMC).

---

# EXPERIMENT  4 Jul 2026 07:34:03 OHWR Cores for the Dynamic Region of This Host (Alveo U250) :complete:

## 1. Hypothesis

Which gateware modules hosted on the Open Hardware Repository (ohwr.org) can
run in the FPGA dynamic (DFX) region of the FPGA card installed in this host?

## 2. How

- **Equipment (discovered in this experiment):** AMD/Xilinx **Alveo U250**
  (UltraScale+ XCU250) at PCIe `af:00.0/.1`, currently flashed with the
  **OpenNIC shell** (device IDs `10ee:903f`/`913f`, kernel driver `onic`).
  Vitis XDMA platform files also installed:
  `/opt/xilinx/platforms/xilinx_u250_gen3x16_xdma_4_1_202210_1`.
- **Software:** ohwr.org catalog (Hugo static site; full project index at
  `https://ohwr.org/index.json`, 188 projects), web search.
- **Benchmarks:** none (desk survey).

### Key commands

```bash
lspci -vvnn -s af:00.0            # -> Xilinx 10ee:903f, driver onic (OpenNIC)
ls /opt/xilinx/platforms          # -> xilinx_u250_gen3x16_xdma_4_1_202210_1
curl -sL https://ohwr.org/index.json   # full project catalog (188 entries)
```

## 3. Observations

- Host FPGA: Alveo U250; **no FMC connector, no VME/SPEC/SVEC carrier**; I/O
  is PCIe + QSFP28 network cages + on-card DDR4, all owned by the static
  shell in both available shells (OpenNIC, Vitis XDMA).
- ohwr.org hosts 188 projects; 21 tagged `Gateware`. Candidates surveyed:

| OHWR project | Form | Dynamic-region fit on U250 |
|---|---|---|
| general-cores | VHDL lib (Wishbone/AXI utils, UART, SPI, I2C, FIFOs, CDC) | **Yes** — platform-independent RTL |
| urv-core (uRV RISC-V) | Verilog soft CPU | **Yes** |
| Mock Turtle | Multi-core (uRV) real-time SoC framework | **Yes** — strong fit for per-sensor "programs" |
| EtherBone core | Remote Wishbone over Ethernet | **Yes**, esp. behind OpenNIC AXI-Stream boxes |
| fpga-config-space (SDB) | Self-describing bus metadata core | **Yes** — useful for module discovery after DFX load |
| VME64x core, GN4124 core | VME / Gennum PCIe bridges | No — bus hardware absent (shell owns PCIe) |
| wr-cores / WRPC (White Rabbit) | Timing core | No — needs GT transceivers, SFP and DMTD clocking owned by static region |
| FMC ADC 100M/250M/500M, FMC TDC 1ns 5cha, FMC DEL, FMC DIO gateware | Mezzanine-specific DAQ stacks | No as-is — front-end pins/mezzanines absent; internal DAQ blocks reusable |
| TDC core (Spartan-6 carry-chain TDC) | Device-specific RTL | No without porting — Spartan-6 primitives + needs physical input pins |
| absenc (absolute encoder) | VHDL core | Logic portable, but needs encoder pins the shell does not expose |

## 4. Data analysis

The deciding factor is not synthesis compatibility (most OHWR cores are
plain, vendor-neutral VHDL) but the **partition boundary**: on the U250 the
dynamic region only receives what the static shell forwards — AXI4-Lite
control + AXI4-MM host/DDR paths (XDMA shell) or 250/322 MHz AXI4-Stream
packet ports + AXI4-Lite (OpenNIC shell). Therefore *processing and
infrastructure* cores qualify, while cores whose value lies in dedicated
front-end silicon or pins (WR transceiver timing, FMC mezzanine interfaces,
carry-chain TDCs) do not. OHWR cores are Wishbone-centric, so each module
needs a Wishbone↔AXI bridge at the boundary; general-cores itself provides
these bridges. **Mock Turtle + uRV** is the standout: loadable per-sensor
firmware on soft CPUs inside a DFX module gives two nested levels of runtime
reconfigurability. Licences (CERN-OHL, LGPL) permit reuse.

## 5. Ideas for future experiments

- Synthesize general-cores + urv-core for `xcu250` out of context; record
  LUT/FF/BRAM to size the reconfigurable partition.
- Build a Mock Turtle node as a DFX reconfigurable module on the XDMA
  platform; measure partial-bitstream load time and firmware-swap time.
- Prototype an EtherBone endpoint inside an OpenNIC 250 MHz user box to
  expose Wishbone sensor registers over the network.
- Decide the standard module boundary: AXI4-Lite + AXI4-Stream, with SDB
  (fpga-config-space) records for post-load module discovery.

---

# EXPERIMENT  4 Jul 2026 07:26:08 Vitis Libraries Suitability for the FPGA Dynamic Region :complete:

## 1. Hypothesis

Can the kernels in the AMD/Xilinx Vitis Libraries repository
(https://github.com/Xilinx/Vitis_Libraries) be used inside the FPGA dynamic
(partial-reconfiguration / DFX) region of this system, where each sensor-type
program is loaded and removed at runtime?

## 2. How

- **Equipment:** none (desk investigation, no hardware target selected yet)
- **Software:** Vitis Libraries GitHub repository (state as of 2025.2 release)
- **Benchmarks:** none

### Key commands

```bash
# Repository reviewed via web fetch of:
#   https://github.com/Xilinx/Vitis_Libraries
```

## 3. Observations

- Libraries are organized in three layers: **L1** = HLS C++ building blocks,
  **L2** = Vitis kernels (compiled by `v++` into `.xo` objects, linked into an
  `xclbin`), **L3** = host-side software APIs.
- Component forms: HLS C++ kernels (PL) and **AIE graphs** (Versal AI Engine
  only, not PL logic).
- Active PL library domains relevant to sensors: **vision, DSP, BLAS,
  security, solver, ultrasound, motor control, data-mover utilities**.
- As of release **2025.2**, these PL libraries are deprecated/unmaintained:
  codec, data_analytics, data_compression, graph, hpc, quantitative_finance,
  sparse. Alveo **U200/U250/U280 dropped**; U50/U50LV/U55C still supported.
- In the standard Vitis/XRT flow, an `xclbin` is itself a **partial bitstream
  loaded at runtime into the platform shell's dynamic region** — i.e., the
  normal deployment mode of L2 kernels is already dynamic-region execution.

## 4. Data analysis

Yes — the PL (HLS) libraries are usable in the dynamic region, and in the
Vitis/XRT flow that is in fact their *default* placement: the shell (static
region) owns I/O, DMA and control, and every kernel `xclbin` is swapped into
the dynamic region at runtime. For a custom Vivado DFX flow on an embedded
device, L1/L2 components synthesize to ordinary RTL with AXI interfaces, so
they can populate a reconfigurable partition provided every sensor module is
wrapped to present the **identical partition-boundary interface** (DFX
requirement). Exceptions: AIE-graph libraries target the AI Engine array, not
the PL dynamic region; L3 APIs assume XRT/xclbin loading; deprecated
libraries should be avoided; each kernel must fit the partition floorplan.

## 5. Ideas for future experiments

- Choose platform: Vitis/XRT shell flow (dynamic region managed for us) vs.
  custom Vivado DFX flow (we define the partition).
- Define the common sensor-module wrapper interface (AXI4-Lite control +
  AXI4-Stream/MM data) all library-based kernels must conform to.
- Build one Vitis Vision (e.g., ISP or filter) kernel and one DSP (e.g., FFT)
  kernel as interchangeable reconfigurable modules; measure resource use vs.
  partition size and partial-bitstream load time.

---

# EXPERIMENT  4 Jul 2026 07:20:01 Repository and Notebook Initialization :complete:

## 1. Hypothesis

Not an experiment — administrative entry. Establishes the laboratory notebook
for the sensor-fpga project, which will investigate dynamically loading and
removing per-sensor-type FPGA programs from the FPGA dynamic region at runtime.

## 2. How

- **Equipment:** Linux workstation (kernel 6.8.0-124-generic)
- **Software:** git, Claude Code (notebook skill)
- **Benchmarks:** none

### Key commands

```bash
mkdir docs   # created implicitly by writing docs/notebook.md and docs/prompts.md
```

## 3. Observations

- Repository `sensor-fpga` was empty (fresh git repo, clean tree, no commits).
- Created `docs/notebook.md` (this file) and `docs/prompts.md`.

## 4. Data analysis

Notebook conventions fixed at project start: reverse-chronological entries
with full timestamps (date + HH:MM:SS) so the ordering of rapid successive
experiments remains unambiguous. All user prompts are recorded verbatim in
`docs/prompts.md`.

## 5. Ideas for future experiments

- Select the target FPGA platform and toolchain (e.g., Xilinx/AMD DFX partial
  reconfiguration flow vs. Intel PR flow).
- Define the static-region interface (bus, clocking, isolation) that all
  sensor-type modules must conform to.
- Prototype a first loadable sensor module and measure partial-bitstream load
  time.
