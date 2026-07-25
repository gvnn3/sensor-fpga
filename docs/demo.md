# Running the Tricorder DFX Demo

The demo shows the project's core idea working end to end: a FreeRTOS
system on a MicroBlaze soft CPU that **loads and unloads sensor hardware
at runtime**, swapping four FPGA "personalities" through one
reconfigurable partition while the operating system and its sensor tasks
keep running.

Verified on: Digilent Nexys A7-100T, Vivado/Vitis 2025.2
(`~/Vivado/2025.2`), Ubuntu, branch `gnn-a7-tricorder`
(notebook entries 11–20 record the underlying experiments).

## What you will see

Every 8 seconds the RTOS swaps the dynamic region to the next
personality and reads its registers, in this rotation:

| Personality | Signature | What it does after the swap |
|---|---|---|
| **PDM** | `9D3A0001` | Captures live audio from the on-board ADMP421 microphone (CIC decimator); prints a PCM sample and the energy of the last 27 ms window — tap or speak near the mic and energy jumps |
| **SONAR** | `50A40001` | Fires a 40 kHz piezo burst pattern and opens a 205 µs capture window; prints peak\|time. With no ultrasound harness attached the floating ADC pins read full-scale at t=0 (`07FF0000`) — that exact value is the "no hardware" signature |
| **B** | `5B5B5B5B` | Hello-swap module: register counter that decrements on read |
| **A** | `A5A5A5A5` | Hello-swap module: counter increments on read |

Interleaved throughout, uninterrupted by the swaps:

- `RTOS: tick N` — heartbeat task (LEDs 0–3 count in binary)
- `ADT7420: 3x.xx C` — on-board temperature sensor over I2C, once a second
- `I2C-EXT: scan: -> 0 device(s)` — external JA sensor bus walk
  (0 devices until the sensor harness is wired; with the harness expect
  0x76/77 BME688, 0x62 SCD-41, 0x10 VEML7700, 0x39 AS7341, 0x57 MAX30105)

## Prerequisites

1. Nexys A7-100T connected by USB (one cable: JTAG + UART). Check:
   `lsusb | grep 0403:6010` — the FT2232 must be present. No udev
   installer or sudo is needed (the `plugdev` group covers it).
2. Power switch on; board can be otherwise untouched — the demo loads
   everything volatile over JTAG, nothing is flashed.
3. Build artifacts present (all committed scripts, rebuild takes
   ~45 min total if `build/` is empty):

   ```bash
   make -C hw static    # static region bitstream + XSA  (~20 min)
   make -C hw dfx       # 4 partial bitstreams + pr_verify (~20 min)
   make -C hw rtos      # FreeRTOS platform + app          (~5 min)
   ```

## Run it

Terminal 1 — watch the console (115200 8N1 on the second FTDI channel):

```bash
stty -F /dev/ttyUSB1 115200 raw -echo && cat /dev/ttyUSB1
```

Terminal 2 — program, stage, boot (from the repo root):

```bash
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_rtos_dfx.xsdb
```

The script does four things: programs `build/dfx/full_a.bit` (static
region + personality A), copies the four ~1 MiB partial bitstreams into
DDR2 at 0x81000000–0x84000000 with their lengths at 0x80FFFFE8–F4
(JTAG staging takes a couple of minutes — be patient), downloads the
FreeRTOS ELF, and starts the CPU. It then disconnects; the board runs
free.

Expected console output within the first minute:

```
RTOS: FreeRTOS on MicroBlaze — tricorder M4
ADT7420: 32.00 C
RTOS: tick 0
DFX: swapping to RM-PDM
DFX: RM-PDM live in 16220 ms, sig=9D3A0001 OK
  PDM: pcm=FFFFFFF2 energy=83
...
DFX: RM-SONAR live ... sig=50A40001 OK
  SONAR: peak|t=07FF0000
```

Any `MISMATCH` after a swap, or a stalled rotation, is a failure.

## Variants

- **Bare-metal loader gate** (the M5–M7 measurement run — same swaps at
  390 ms each instead of 16 s, plus the PDM/sonar probes and a final
  `DFX: PASS (all swaps ok)` verdict):

  ```bash
  ~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_dfx_test.xsdb 1
  ```

  The trailing `1` is the byteswap flag — required with the committed
  `-bin_file` partials on the little-endian MicroBlaze.

- **JTAG-only partition check** (no CPU involvement — proves partials +
  partition + decoupler by themselves): program `build/dfx/full_a.bit`,
  then `fpga -partial build/dfx/rm_b_partial.bit` in xsdb and read the
  signature register at `0x44A10004` with `mrd`.

## How it works (one paragraph)

The static region (MicroBlaze + caches + DDR2 MIG + UART + I2C + timer +
interrupt controller) owns all pins and never reconfigures. One pblock —
clock region X1Y2, `RESET_AFTER_RECONFIG` — holds the current
personality behind a DFX decoupler on an AXI4-Lite boundary at
`0x44A10000` plus dedicated PDM/sonar/ADC boundary pins. To swap, the
`dfx_task` decouples the partition, streams the staged partial word by
word into a **raw ICAPE2 primitive** (one config word per GPIO
go-toggle; bit-reversal per UG470 done in fabric), re-couples, and
verifies the new personality's signature register. The stock AXI HWICAP
core is present but non-functional in this configuration (write FSM
wedges on its first transfer — notebook entry 17); the ICAPE2 writer is
the working path.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `xsdb` can't find a target | Board off / cable out; re-check `lsusb`. Kill stale `hw_server` processes if a previous session crashed |
| Console silent | Wrong tty (UART is `ttyUSB1`, JTAG side is `ttyUSB0`); or two readers fighting — only one `cat` may hold the port |
| Garbage on console | `stty -F /dev/ttyUSB1 115200 raw -echo` not applied |
| `sig=...FFFFFFFF` or `MISMATCH` | Partials not staged (script must run to completion — staging is slow) or stale partials vs bitstream: rebuild `make -C hw dfx` and rerun |
| Demo boots but no swaps | Length words unset — the app skips personalities whose staged length is 0; rerun the xsdb script rather than programming the bitstream alone |
