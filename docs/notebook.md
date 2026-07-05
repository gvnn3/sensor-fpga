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

3. [EXPERIMENT  4 Jul 2026 07:34:03 OHWR Cores for the Dynamic Region of This Host (Alveo U250)](#4-jul-2026-073403) :complete:
2. [EXPERIMENT  4 Jul 2026 07:26:08 Vitis Libraries Suitability for the FPGA Dynamic Region](#4-jul-2026-072608) :complete:
1. [EXPERIMENT  4 Jul 2026 07:20:01 Repository and Notebook Initialization](#4-jul-2026-072001) :complete:

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
