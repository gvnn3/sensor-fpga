# Tricorder on the Nexys A7-100T — Project Plan

Status: proposed (branch `gnn-a7-tricorder`, 24 Jul 2026). Supersedes the
Arty A7-100T pin plan (notebook entries 8–10) after the board change to the
**Digilent Nexys A7-100T** (XC7A100T-1CSG324C — same die/package/speed grade
as the Arty, so all fabric-resource math carries over). A Nexys A7 is
connected to this host (FT2232, `/dev/ttyUSB0` = JTAG side, `/dev/ttyUSB1` =
UART).

This plan was produced by a multi-agent (ultracode) workflow: three recon
agents (board facts from Digilent primary sources, repo audit, host
toolchain audit), three planners (pins, architecture, roadmap), and two
adversarial verifiers. The verifiers refuted 4 claims and flagged 5 as
suspect; this document is the reconciliation — where sections disagreed,
the XDC-grounded pin plan wins.

## 1. What changes with the board swap

| | Arty A7-100T (old) | Nexys A7-100T (new) |
|---|---|---|
| FPGA | XC7A100T-1CSG324C | same |
| Sensor-usable I/O | ~77 (shield + 4 Pmod) | **40** (JA/JB/JC/JD = 32 @ 200 Ω series, JXADC = 8 @ 100 Ω, coupled pairs) |
| DRAM | 256 MB DDR3L | **128 MB DDR2** (MT47H64M16HR-25E, 16-bit) — MIG 7 supports it natively |
| Ethernet PHY | MII | **RMII** (FPGA supplies 50 MHz REFCLK on D5) |
| On-board extras | — | PDM mic (ADMP421), ADT7420 temp, ADXL362 accel, 16 sw/16 LED, 8-digit 7-seg, 12-bit VGA, audio PWM, microSD, USB-HID |
| Toolchain on host | (notebook: 2023.1 in /usr/local/cad) | **Only `/home/gnn/Vivado/2025.2`** — /usr/local/cad is now an ACT toolchain, not Xilinx. Standardize on Vivado/Vitis 2025.2 |

Governing constraint: prior sensor demand was 55 external pins; supply is
40. Something must give — resolved in §2 without losing any sensing
modality except simultaneous DAC.

Critical XDC fact (verified against Digilent's Nexys-A7-100T master XDC):
**JB10 (H16, `IO_L13P_T2_MRCC_15`) is the only P-side clock-capable pin on
any header.** JB2/JB7/JB3/JD10 are CC but N-side; a single-ended clock
there needs a `CLOCK_DEDICATED_ROUTE` waiver. So the board has exactly one
clean external clock input, and **OV7670 PCLK gets it — JB10, full stop.**

## 2. Pin plan (canonical)

Everything stays simultaneously wired — "loading a sensor" remains purely a
DFX swap, no re-cabling — with one exception: **AD9708 DAC shares the
AD9226's pins** (IOBUFs in static; ADC personality = 12 in + clk out, DAC
personality = 8 out + clk out; swap the harness when swapping the RM). Pin
savings that make it fit:

- Camera SCCB (SIOC/SIOD) merges onto the shared sensor I2C bus (OV7670 =
  0x21). Risk: SCCB's nonstandard ACK — verify with AXI IIC, fall back to
  bit-banging via GPIO on the same two pins.
- Camera RST/PWDN strapped on the module; soft reset via COM7 register.
- AD9226 OTR pin dropped; detect 0x000/0xFFF rail codes in fabric instead.
- AD8332 `enable` strapped high on the daughterboard.
- I2C pull-up-enable ports (`i2c_*_pup`) deleted — Arty-only hardware; use
  resistors on the harness.

Allocation (all package pins verified against the Digilent master XDC):

| Header | Signals |
|---|---|
| JA (slow/control) | i2c_scl C17, i2c_sda D18, sipm_pulse E18, sipm_thresh_pwm G17, afe_gain_pwm D17, afe_hilo E17, sonar_tx_a F18, sonar_tx_b G18 |
| JB (camera ctrl + low nibble) | cam_xclk D14, cam_href F16, cam_vsync G16, cam_data[0] H14, cam_data[1] E16, cam_data[2] F13, cam_data[3] G13, **cam_pclk H16 (sole P-side MRCC)** |
| JC (camera hi + ADC hi) | cam_data[4] K1, cam_data[5] F6, cam_data[6] J2, cam_data[7] G6, adc_data[8] E7, adc_data[9] J3, adc_data[10] J4, adc_data[11] E6 |
| JD (ADC low byte) | adc_data[0] H4, adc_data[1] H1, adc_data[2] G1, adc_data[3] G3, adc_data[4] H2, adc_data[5] G4, adc_data[6] G2, adc_data[7] F3 |
| JXADC (PDM + ADC clk) | pdm_clk A13, pdm_data[0] A14, pdm_data[1] A15, pdm_data[2] A16, pdm_data[3] B16, pdm_data[4] B17, pdm_data[5] B18, **adc_clk A18 (out)** |

Budget: **40/40, zero header spares.** Spare capacity exists only on
dedicated board nets (BTND, CPU_RESETN, 12 switches, 12 LEDs, 7-seg, VGA,
audio, on-board mic, SD).

Board infra remap: clk100mhz E3, UART C4/D4, btn → N17/M18/P17/M17, sw →
J15/L16/M13/R15, led → H17/K15/J13/N14, DFX status RGB → N15/M16/R12. Add
`CFGBVS VCCO` + `CONFIG_VOLTAGE 3.3` (absent from the Digilent master XDC).
Note SW8/SW9 are LVCMOS18 if ever used.

Known weak points, accepted with mitigations:

1. **AD9226 behind 200 Ω series R**: ~2 ns RC → derate to **≤40 MS/s**
   (65 MS/s unsupported on this board); characterize empirically at M7.
2. **adc_clk pair-coupled with pdm_data[5]** on JXADC: if mic ch5 shows
   artifacts during ADC runs, swap adc_clk with sonar_tx_b (JA10).
3. SiPM pulse has no CC pin: capture by 4× oversampling at 400 MHz on a
   plain pin (TLV3501 pulses ≥10 ns are safely caught).
4. XADC external analog inputs forfeited (JXADC used digitally); internal
   XADC telemetry still available.

Gained free: ADT7420 (I2C 0x4B) + ADXL362 (SPI) as always-present static
sensors, on-board ADMP421 as a 7th reference PDM channel on dedicated pins,
and a far richer UI (7-seg, VGA, 16 sw/LED) than the Arty plan, which had
to reserve shield pins for a display.

Fallback alternative (only if imaging demos are expendable): drop the
OV7670 wiring, freeing 12 pins — DAC becomes simultaneous, OTR and
afe_enable return, VGA becomes the display surface.

## 3. Architecture

**CPU: MicroBlaze-V + FreeRTOS** (honors the notebook decision
"MicroBlaze(-V) + FreeRTOS"; MicroBlaze-V is available in 2025.2 and has
FreeRTOS BSPs in the SDT flow). Fall back to classic MicroBlaze only if
BSP/DFX friction appears at M2 — record either way in the notebook.
100 MHz, 16 KB/16 KB caches, 32 KB local BRAM for boot; code in DDR2.

**Static region** (~22–23k LUT ≈ 36%, ~25 BRAM36, ~6 DSP):

- MIG 7 DDR2: MT47H64M16HR-25E, 650 MT/s, 4:1 → ui_clk 81.25 MHz; import
  Digilent's MIG project/UCF (DDR2 pins are not in the master XDC). All AXI
  stays at 100 MHz; one contained CDC in the interconnect.
- MMCM: 100 MHz sys, 200 MHz MIG ref, 50 MHz RMII REFCLK out, 24 MHz cam
  XCLK, 400 MHz SiPM sampling.
- EthernetLite + **hand-written ~100-line MII↔RMII shim** (`mii_to_rmii` IP
  is gone from 2025.2; own RTL removes the dependency).
- AXI: UARTlite 115200, IIC (sensor bus + SCCB), Quad SPI ×2 (ADXL362;
  config flash via STARTUPE2 for partial storage), HWICAP, DMA (S2MM +
  MM2S), XADC, GPIO + 7-seg scanner, timer, intc, DFX Decoupler.
- All sensor IOBs live in static; the ADC/DAC bus is exposed to the RP as
  `bay_i/o/t` through IOBUFs so the RM sets pin direction via `t`.

**DFX partition**: boundary = S_AXIL (4 KB ctrl) + M_AXIS 32-bit → DMA S2MM
(tlast framing, tuser[0] = SOF) + S_AXIS ← MM2S (DAC/sonar) + irq + clk100 +
clk_aux (per-personality: 400 MHz, or cam_pclk from JB10 via static BUFG) +
rst_n + the ADC/DAC bay bundle. One DFX Decoupler; safe state = AXI held
off, bay Hi-Z, irq 0. Pblock = **2 full clock regions** (of 6) →
**≈20k LUT / ~45 BRAM36 / ~80 DSP** — larger than any planned pipeline.
`RESET_AFTER_RECONFIG=TRUE`, `SNAPPING_MODE=ON`; exclude the DDR2 PHY bank
(34) and config center; finalize placement after a first heat check. Room
to grow to 3 CRs.

**Partial bitstream path**: full bitstream 3.83 MB; 2/6-CR partial
≈ 1.3 MB. QSPI (16 MiB) holds the golden image + **8 partial slots at
1.5 MiB spacing** with CRC32 each (verifier corrected the earlier
"9 slots @ 4 MiB" math). Load v1 = MicroBlaze PIO through HWICAP
(QSPI→DDR→ICAP, plan on ~0.5 s end-to-end); DMA-fed HWICAP halves that;
DFX Controller IP (~4 ms) only if latency ever matters. Ethernet TFTP →
DDR → ICAP for development iteration, no reflash cycle.

**First two RMs**: (1) **PDM beamformer** — lowest risk, permanent JXADC
pins, no external clock domain, bring-up possible with the on-board mic
alone before any harness exists (~3–6k LUT). (2) **OV7670 capture** —
deliberately the hard case: external clock domain (cam_pclk JB10 →
BUFG → clk_aux), video framing over AXIS into a DDR2 ring. Note the pin
rate is **18.4 MB/s** (RGB565/YUV422, 2 bytes/pixel at VGA@30) — 9.2 MB/s
only if the RM stores Y-only; size the DMA ring for 18.4. Still trivial vs
DDR2 bandwidth. The ultrasound chain (adds S_AXIS TX + derated ADC capture)
comes third.

**Software** (FreeRTOS, Vitis 2025.2 unified/SDT flow), tasks high→low:
dfx_loader (quiesce→decouple→load→init; owns HWICAP + flash), acq (DMA
IRQs → buffers), comms (lwIP TCP control + UDP telemetry + TFTP), sensor_mgr
(1–10 Hz I2C poll of the 5 externals + ADT7420 + ADXL362 → telemetry
blackboard), ui (buttons/7-seg/RGB status).

## 4. Roadmap (risk-retirement order)

| M | Deliverable | Gate |
|---|---|---|
| M0 | Blinky via JTAG (0.5 d) | LED blinks from a bitstream programmed by 2025.2 hw_server as user gnn; George runs the udev cable-driver installer (sudo) first. Also: 10-min check that DFX is license-free in 2025.2 |
| M1 | `tricorder-nexys-a7-100t.xdc` + per-board top wrappers + parameterized `validate_xdc.tcl` (1–2 d) | `-tclargs nexys-a7` and `-tclargs arty-a7` both PASS under **2025.2** (re-validates the old Arty result on the new tool); 0 DRC; script exits nonzero on failure instead of unconditional PASS |
| M2 | Static block design + MicroBlaze-V hello (1–2 d) | "hello" on /dev/ttyUSB1; BD checked in as Tcl (`write_bd_tcl`), rebuildable from clean clone; CPU choice ratified here |
| M3 | DDR2 MIG calibration (1–3 d) | MIG example design first, then in-BD; `init_calib_complete`=1, full-range memtest 0 errors across 3 power cycles |
| M4 | FreeRTOS boot + I2C scan (2–3 d) | Scheduler stable ≥1 hr; on-board ADT7420 proves the I2C stack with zero external wiring; external scan finds 0x76/77, 0x62, 0x10, 0x39, 0x57 |
| M5 | DFX partition + hello-swap RM pair + ICAP (3–5 d) | Runtime A→B→A swap, UART heartbeat never glitches, swap time measured vs theory |
| M6 | PDM beamformer RM (1–2 wk) | Test tone localized to correct sector; single on-board mic first, then 6-mic array; RM fits pblock with ≥30% headroom |
| M7 | Ultrasound RM + swap demo (1–2 wk) | Scripted boot → beam data → swap → sonar range → swap back, twice without reset; measured max reliable adc_clk through the Pmod closes the M1 derating assumption |

**CI story**: no cloud Vivado — deterministic local make targets, all exit
nonzero on failure: `make validate` (both boards), `make ooc-rm`
(out-of-context synth per RM against a `budget.tcl` derived from the pblock),
`make sim` (self-checking xsim testbenches), `make static`/`make dfx`, lint
(Verilator `--lint-only`). `VIVADO=` overridable, no hardcoded paths.

**Repo conventions**: one notebook entry per experiment (5-section format);
every prompt logged in `docs/prompts.md`; George pushes — the agent never
pushes.

## 5. Session status — 25 Jul 2026 (M0–M7 executed)

All eight roadmap milestones were driven to their gates in one overnight
session (24 Jul 22:00 → 25 Jul 09:30), notebook entries 11–19, branch
`gnn-a7-tricorder`. Everything below ran on the physically attached
Nexys A7-100T.

| M | Result | Measured |
|---|---|---|
| M0 | PASS | Blinky via JTAG; 3.83 MB bitstream; die 48.6 °C; **no udev installer needed** (plugdev covers FT2232); DFX license-free in 2025.2 |
| M1 | PASS | Nexys XDC 58/58 ports placed, 0 DRC; Arty control 75/75 under 2025.2; cam_pclk clean on H16 (sole P-side MRCC) |
| M2 | PASS | MicroBlaze hello on /dev/ttyUSB1; BSP-free 250 B ELF; **CPU ratified: classic MicroBlaze** (no MB-V FreeRTOS port in 2025.2) |
| M3 | PASS | DDR2 MIG from Digilent prj: init_calib + **128 MiB memtest 0 errors × 3 config cycles** |
| M4 | PASS | FreeRTOS ticks 60 s; ADT7420 31.2–31.5 °C on-board; external scan correctly empty (no harness) |
| M5 | PASS | **Runtime swaps 390 ms**/1.07 MB partial, MicroBlaze-driven, static uninterrupted; pr_verify across 4 configs |
| M6 | PASS | PDM RM live audio from on-board mic (PCM −4326…+1064, energy windows); RM = 1.7% of partition |
| M7 | PASS* | Sonar RM swaps in, 3 bursts fired/counted, floating-ADC rail signature as predicted; *echo ranging + ADC derating measurement need the harness |

**Deviations from the plan as written:**

1. **AXI HWICAP v3.0 does not work here** — write FSM wedges on the first
   transfer regardless of eos_in (must be tied high — first bug found),
   icap_clk speed/asyncness, JTAG state, or data content. Replaced by a
   **raw ICAPE2 writer** (second ICAP site, GPIO-paced, UG470 bit-reversal
   in RTL) — 390 ms/swap, GPIO-bound; a FIFO front-end reaches ~11 ms.
2. **MicroBlaze classic, not -V**: 2025.2 ships no FreeRTOS port for
   MicroBlaze-V (BSP ports = ARM + MicroBlazeV9 only).
3. **Caches are mandatory, not optional**: `cache "None"` block automation
   leaves no CPU instruction path to DDR; FreeRTOS (104 KB) cannot fit
   LMB. 16 KB I/D caches, MIG behind the cached port; `.vectors.*` must
   stay in LMB when relinking to DDR.
4. **MIG owns the XADC** (Digilent prj `XADC_En`): drop the separate
   xadc_wizard idea; program.tcl's hw_server temp read still works.
5. **Pblock needed explicit site ranges** (SLICE_X52Y100:X89Y149 +
   DSP48_X1Y40:X2Y59): CLOCKREGION+SNAPPING snapped X1Y2 to empty. X1Y2
   has no BRAM — move to X1Y1 when an RM needs BRAM.
6. **Partial format**: `write_bitstream -bin_file` + software byteswap is
   the working combination; `write_cfgmem SMAPx32` output is bit-swapped
   for SelectMAP and never syncs the ICAP.
7. **RM boundary evolved to v3** (AXI-Lite + PDM + sonar/AFE/adcdac pins)
   before first hardware swap — cheap now, expensive later.

**Hardware-blocked (next bench session):** JA sensor harness (5×I2C,
pull-ups) → M4 full scan; 6-mic array → beamforming/localization;
ultrasound chain → echo ranging + 200 Ω derating sweep; camera + DAC per
master XDC. **Software next:** FIFO-fronted ICAPE2 writer, QSPI partial
slots + CRC, FreeRTOS dfx_loader task folding in `sw/dfx_load` logic,
Ethernet/RMII shim, 1 hr FreeRTOS soak, HWICAP minimal repro for AMD.

### Continuation status — 27 Jul 2026 (post-milestone work, entries 20–30)

The two days after the M-gates turned the milestone platform into an
operating instrument. All results measured on the attached board;
21 commits on `gnn-a7-tricorder`.

**Swap-time arc (entries 20–24):** 16.2 s → 1.65 s (the Vitis template's
-O0 was the culprit, not preemption — proven by byte-identical timings
under a critical section) → **2.7 ms** (DMA-fed ICAPE2: wire speed to
the microsecond, 267,763 words at one per 100 MHz cycle) → **0.78–1.06
ms** with compressed partials (2.5–3.4x smaller, MFWR streams at full
rate). A personality change now costs less than one FreeRTOS tick.

**Flash residency (entries 25–26):** QSPI slot table — partials live in
the S25FL128S in 1 MiB slots; one script emits both the MCS image and
the compiled-in manifest. Command-mode quad SPI (QE set persistently,
0x6B framing measured on hardware: 8 overhead bytes, not 5) reads slots
at 4.9–6.1 s — **CPU-bound, not wire-bound**: a 32x wire speedup moved
totals under 10%. The board changes personalities with no host attached.

**XIP investigation (entries 27–28, closed):** three configurations,
ILA-instrumented. Verdict: the AXI Quad SPI XIP core clocks the bus with
perfect quad timing while the flash never answers its command sequence
(RDATA constant 0xCC = idle lanes); standard mode transfers but never
frames. Command-mode stays the production path; the ILA capture recipe
(`hw/scripts/ila_capture.tcl` + debug-rule automation) is committed for
any future attempt. A regression from these experiments — dq2/dq3 left
floating in the top wrapper — was found at the final gate and fixed.

**Test-mode UI + sounds (entries 29–30, George's feature requests):**
SW0–SW14 select 15 exclusive functions with 7-seg readouts (live: temp,
I2C count, swap count/time, uptime, RM registers), idle banner, `Err
SEL` on multi-select; SW0–SW3 **load their DFX personality for real**
(auto-rotation removed — the dynamic region is operator state), sonar
auto-fires while selected. TOS tricorder sounds from a 48 KiB BRAM ROM:
scan on every function-on (= the load sound) and at boot; alert on
`Err SEL` and on any signature mismatch. Built via an ultracode workflow
whose adversarial verifier proved the three-layer display protocol
coherent pre-synthesis; final display gate was register-exact
(`trICordr` = 0x5C505E50/0x78503039). `docs/manual.md` is the user
manual; `docs/demo.md` stays the runbook.

**Standing next steps:** quad-command read speedup if flash swaps ever
need sub-second (interrupt-driven SPI ~1 s, or XIP with a different
memory-vendor table + the ILA method); QSPI full self-boot (golden image
already in slot 0); switch debounce; the sensor harness — SW1 becomes a
live rangefinder with zero further software once the piezo chain is
wired.

## 6. Standing risks

| Risk | Mitigation point |
|---|---|
| 200 Ω series R vs ADC rate | Derate at M1, measure at M7 |
| SCCB-on-shared-I2C ACK quirk | M2/M6: AXI IIC first, bit-bang fallback |
| adc_clk/pdm_data[5] crosstalk on coupled JXADC pair | Documented pin swap with JA10 |
| DFX license behavior on 2025.2 unverified | Checked in M0 |
| `mii_to_rmii` gone from 2025.2 | Own ~100-line RMII shim at M2 (or defer Ethernet; UART suffices through M7) |
| DDR2 MIG UCF import friction in 2025.2 | M3 example-design-first isolates it |
| Prior validation done on 2019.2/2023.1, host now has only 2025.2 | M1 re-runs the Arty XDC as a control |
| Old local Nexys master-XDC copy (tutorial-chp-fpga repo) has stale LVDS lines on JXADC | Source constraints from current Digilent GitHub master only |
