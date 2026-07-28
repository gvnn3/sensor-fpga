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

36. [EXPERIMENT 28 Jul 2026 01:00:00 DFX Swap Telemetry: From/To Personality, Partial Sizes, Swap Time on the Wire](#28-jul-2026-010000) :complete:
35. [EXPERIMENT 27 Jul 2026 23:59:00 QSPI Re-Flash: Programmer Clears QE, Stale Bitstream Kills TX, Fresh DFX Build Fixes Both](#27-jul-2026-235900) :complete:
34. [EXPERIMENT 27 Jul 2026 22:45:00 Full-Schema Sensor Telemetry: Two AXI IIC Driver Bugs Between the Code and the Wire](#27-jul-2026-224500) :complete:
33. [EXPERIMENT 27 Jul 2026 21:05:00 DHCP Lease Acquired: REFCLK Phase Was the Bug](#27-jul-2026-210500) :complete:
32. [EXPERIMENT 27 Jul 2026 20:24:50 Ethernet Bring-Up: Telemetry on the Wire, DHCP Still Silent](#27-jul-2026-202450) :complete:

31. [EXPERIMENT 27 Jul 2026 07:28:20 Standalone QSPI Boot: Bitstream + Bootloader + App, No Host Required](#27-jul-2026-072820) :complete:
30. [EXPERIMENT 27 Jul 2026 06:56:01 Switches Command the Hardware: Personality Loads, Boot Chirp, Mismatch Alert](#27-jul-2026-065601) :complete:
29. [EXPERIMENT 26 Jul 2026 22:48:43 Test-Mode UI: 15 Switch Functions, 7-Seg Readouts, Tricorder Sounds](#26-jul-2026-224843) :complete:
28. [EXPERIMENT 26 Jul 2026 22:40:00 ILA Verdict on XIP: Bus Clocks Perfectly, Flash Never Answers — Case Closed](#26-jul-2026-224000) :complete:
27. [EXPERIMENT 26 Jul 2026 13:37:02 XIP Retry: 21 MB/s Transfers, Data Alignment Unresolved — Reverted](#26-jul-2026-133702) :complete:
26. [EXPERIMENT 26 Jul 2026 10:46:52 Quad SPI: Correct at 4 Lanes x 50 MHz — and Proof the Reader Is CPU-Bound](#26-jul-2026-104652) :complete:
25. [EXPERIMENT 26 Jul 2026 01:55:20 QSPI Slot Table: Personalities Load from Flash](#26-jul-2026-015520) :complete:
24. [EXPERIMENT 26 Jul 2026 00:16:23 Compressed Partials: Sub-Millisecond Swaps (0.78–1.06 ms)](#26-jul-2026-001623) :complete:
23. [EXPERIMENT 25 Jul 2026 18:05:04 DMA-Fed ICAPE2: Swaps at Wire Speed — 2.7 ms](#25-jul-2026-180504) :complete:
22. [EXPERIMENT 25 Jul 2026 16:57:03 RTOS Swap-Time Optimization: 15.9 s → 1.65 s](#25-jul-2026-165703) :complete:
21. [EXPERIMENT 25 Jul 2026 15:21:12 Demo Runbook Published (docs/demo.md)](#25-jul-2026-152112) :complete:
20. [EXPERIMENT 25 Jul 2026 09:55:00 Integrated Demo: FreeRTOS Rotates All Four DFX Personalities + CI Targets](#25-jul-2026-095500) :complete:
19. [EXPERIMENT 25 Jul 2026 09:30:00 M7: Sonar RM + Full Runtime Swap Demo (Ranging Pending Harness)](#25-jul-2026-093000) :complete:
18. [EXPERIMENT 25 Jul 2026 09:28:00 M6: PDM Pipeline RM — Live Audio from the On-Board Mic via DFX Swap](#25-jul-2026-092800) :complete:
17. [EXPERIMENT 25 Jul 2026 09:26:00 M5: MicroBlaze-Driven DFX Swaps at 390 ms — HWICAP Broken, Raw ICAPE2 Writer Works](#25-jul-2026-092600) :complete:
16. [EXPERIMENT 25 Jul 2026 08:30:00 M4: FreeRTOS Boots on MicroBlaze — I2C Stack Proven via On-Board ADT7420](#25-jul-2026-083000) :complete:
15. [EXPERIMENT 24 Jul 2026 23:20:00 M3: DDR2 via MIG — 128 MiB Memtest Clean](#24-jul-2026-232000) :complete:
14. [EXPERIMENT 24 Jul 2026 22:57:00 M2: MicroBlaze Static Region — Hello over UART (CPU Decision Ratified)](#24-jul-2026-225700) :complete:
13. [EXPERIMENT 24 Jul 2026 22:52:30 M1: Nexys A7 Master XDC + Two-Board Validation Gate](#24-jul-2026-225230) :complete:
12. [EXPERIMENT 24 Jul 2026 22:38:43 M0 Bring-Up: Blinky on the Nexys A7-100T via JTAG](#24-jul-2026-223843) :complete:
11. [EXPERIMENT 24 Jul 2026 22:31:09 Board Retarget Plan: Nexys A7-100T (Ultracode Multi-Agent Planning)](#24-jul-2026-223109) :complete:
10. [EXPERIMENT  5 Jul 2026 05:14:19 Vivado Validation of the Master XDC](#5-jul-2026-051419) :complete:
9. [EXPERIMENT  5 Jul 2026 05:08:00 Master XDC for the Full Sensor Complement](#5-jul-2026-050800) :complete:
8. [EXPERIMENT  5 Jul 2026 05:01:07 Board Decision: Arty A7-100T — Pin and Resource Budget](#5-jul-2026-050107) :complete:
7. [EXPERIMENT  5 Jul 2026 04:30:06 CPU Options on the Artix-7](#5-jul-2026-043006) :complete:
6. [EXPERIMENT  5 Jul 2026 04:11:47 Prototyping the Tricorder on the Alveo U250 Without the Artix-7](#5-jul-2026-041147) :complete:
5. [EXPERIMENT  5 Jul 2026 03:53:38 Tricorder Parts List Received (Artix-7 Target)](#5-jul-2026-035338) :complete:
4. [EXPERIMENT  5 Jul 2026 03:41:44 Extraction of Shared Conversation: FPGA as a Sensor Platform](#5-jul-2026-034144) :complete:
3. [EXPERIMENT  4 Jul 2026 07:34:03 OHWR Cores for the Dynamic Region of This Host (Alveo U250)](#4-jul-2026-073403) :complete:
2. [EXPERIMENT  4 Jul 2026 07:26:08 Vitis Libraries Suitability for the FPGA Dynamic Region](#4-jul-2026-072608) :complete:
1. [EXPERIMENT  4 Jul 2026 07:20:01 Repository and Notebook Initialization](#4-jul-2026-072001) :complete:

---

# EXPERIMENT 28 Jul 2026 01:00:00 DFX Swap Telemetry: From/To Personality, Partial Sizes, Swap Time on the Wire :complete:

## 1. Hypothesis

Can each DFX personality swap emit a UDP event frame carrying the RM
switched from and to, the byte size of each partial bitstream, and the
reconfiguration time?

## 2. How

- **Equipment:** Nexys A7-100T, DHCP lease 192.168.1.126, host listener
  on UDP 5001
- **Software:** Vivado/Vitis 2025.2, FreeRTOS app on branch gnn-dfx-ctx;
  `full_a.bit` + JTAG-loaded ELF; swaps triggered by poking
  `ui_live.req_rm` over JTAG
- **Benchmarks:** two chained swaps A->PDM and PDM->SONAR

### Key commands

```bash
vitis -s sw/scripts/build_rtos.py
xsdb hw/scripts/run_elf.xsdb build/dfx/full_a.bit .../tricorder_rtos.elf
xsdb -eval '...; mwr <ui_live> 0; after 4000; mwr <ui_live> 1'   # req_rm
python3 listen.py 26
```

## 3. Observations

Both frames captured on the first attempt:

```
type=dfx dfx=1 from=3 from_name=A   to=0 to_name=PDM   from_bytes=311452 to_bytes=409856 swap_us=1043 sig=ok swaps=1 uptime=37
type=dfx dfx=2 from=0 from_name=PDM to=1 to_name=SONAR from_bytes=409856 to_bytes=371724 swap_us=1132 sig=ok swaps=2 uptime=43
```

- **swap_us=1043 / 1132** — consistent with every sub-1.2 ms DMA->ICAPE2
  measurement since entry 27
- Sizes match the post-ILA-removal slot manifest exactly
  (`SLOT_A_LEN=311452`, `SLOT_PDM_LEN=409856`, `SLOT_SONAR_LEN=371724`)
- Chained `from`/`to` are correct across events (to of dfx=1 = from of
  dfx=2); `swaps` tracks the lifetime counter

## 4. Data analysis

Same single-writer pattern as the switch events: `dfx_task` fills the
record (`dfx_from/to/from_len/to_len/us/ok`) and bumps `dfx_seq`;
`net_task` notices the sequence change and emits — no second MAC writer,
no locking. Failed swaps are reported too (`sig=mismatch`), which turns
the signature-check alert into an observable network event rather than
just a sound. The shared UDP tail was factored out of `net_send_event`
into `udp_bcast_finish()`, so all three frame types (telemetry, event,
dfx) build headers through one code path. Build also flushed out two F5
stragglers the earlier regex missed: `bme_gas_ohm` and `sonar_range_mm`
were still clamping below-floor values into range (dead code caught by
`-Wunused-function` on `fu_floor`).

## 5. Ideas for future experiments

- Re-flash QSPI so standalone boot carries the dfx frames
- Aggregate swap statistics key (mean/max swap_us) in the 1 Hz frame
- Host-side listener that renders swap history as a timeline

---

# EXPERIMENT 27 Jul 2026 23:59:00 QSPI Re-Flash: Programmer Clears QE, Stale Bitstream Kills TX, Fresh DFX Build Fixes Both :complete:

## 1. Hypothesis

Does re-flashing QSPI with the new telemetry app give a standalone boot
that passes the same listener gate as the JTAG-loaded build?

## 2. How

- **Equipment:** Nexys A7-100T, S25FL128S QSPI flash, LAN8720A PHY, DHCP
  network, host listener on UDP 5001
- **Software:** Vivado/Vitis 2025.2; hw/dfx/build_dfx.tcl (full DFX flow),
  hw/scripts/build_flash.tcl + program_flash.tcl; flash-boot exercised by
  loading `full_a.bit` over JTAG (bootloader in BRAM then pulls the app
  from flash — identical to power-on boot minus FPGA config from flash,
  proven separately in entry 31)

### Key commands

```bash
vivado -mode batch -source hw/dfx/build_dfx.tcl      # full rebuild, ~25 min
vivado -mode batch -source hw/scripts/build_flash.tcl
vivado -mode batch -source hw/scripts/program_flash.tcl
xsdb hw/scripts/run_elf.xsdb ...                     # QE restore (see §4)
xsdb -eval 'connect; ...; fpga build/dfx/full_a.bit' # flash boot
python3 listen.py 15                                 # gate
```

## 3. Observations

Three distinct failures before the gate passed:

| Attempt | Symptom | Cause |
|---------|---------|-------|
| 1 | `BOOT: bad magic eeeeeeee` | Vivado flash programmer **cleared the QE bit**; quad read returned float garbage |
| 2 | Boots, telemetry emits, `NET: loopback 0/60 bytes FAIL`, DHCP discover forever | `full_a.bit` in flash was built 19:29 — **before the REFCLK ODDR inversion fix** (21:03); TX dead on the wire |
| 3 (fresh DFX build) | — | ILA debug block aborted the flow (`'1' vacant channels insufficient`); removed it — never worked, ethernet proven |

After rebuild + re-flash + QE restore, flash boot in one pass:

```
BOOT: qspi bootloader / BOOT: launching app
SENS: ADT7420 UNKNOWN->PRESENT / init mask=00004001
NET: loopback 60/60 bytes OK
NET: DHCP IP 192.168.1.126
NET: telemetry seq=0 keys=75 len=1081
DFX: RM-PDM live in 1040 us, sig=9D3A0001 OK
```

- Listener gate: **12 frames / 15 s**, `temp_cc=3250` live, full 75-key
  schema, correct sentinels
- ILA removal shrank every partial (~15%): `slots.h` lengths changed →
  **app rebuild required between build_flash passes** (slot table is
  compiled in)
- JTAG-triggered swap from the new flash slots: **RM-PDM live in
  1040 µs**, signature verified
- **Cold power cycle** (user, JP1 on QSPI): full standalone boot — config
  from flash, app from flash, DHCP lease reacquired, **16 frames / 20 s**
  on the listener with live `temp_cc=3306`. QE survived the power cycle
  (non-volatile CR1 bit, as expected)

## 4. Data analysis

Vivado's flash programmer writes the status/config registers during
program-verify and leaves QE=0 — every `program_flash.tcl` run must be
followed by a QE restore before quad reads work. The app's QSPI init
already repairs it (`QSPI: QE set, CR1=02`), so one JTAG run of the app
(or any future boot.c QE-write) heals the flash; a cold standalone boot
straight after programming would hang on bad magic. The loopback
0/60 → 60/60 flip between attempts 2 and 3 is a clean A/B on the same
board, same PHY, same app — only the bitstream differed — closing the
case on entry 33's REFCLK phase diagnosis. The slots.h drift is a
build-order trap: partial sizes are compiled into the app, so any
change to RM contents forces app rebuild → bootimg repack → re-flash,
in that order.

## 5. Ideas for future experiments

- Teach boot.c to check/set QE itself (single-mode WRR before first quad
  read) — removes the post-programming JTAG step entirely
- Add QE restore pass to program_flash.tcl via JTAG-poked SPI commands
- Background-stage flash partials into DDR at boot so first swap skips
  the 5.9 s CPU-bound quad read

---

# EXPERIMENT 27 Jul 2026 22:45:00 Full-Schema Sensor Telemetry: Two AXI IIC Driver Bugs Between the Code and the Wire :complete:

## 1. Hypothesis

Can the tricorder emit the full frozen 75-key telemetry schema — live values
for wired sensors, out-of-range sentinels for everything else — plus
switch-event frames, all captured by the UDP listener on the host?

## 2. How

- **Equipment:** Digilent Nexys A7-100T (XC7A100T), on-board ADT7420 @ 0x4B
  on axi_iic_1, LAN8720A RMII PHY, host 192.168.1.112, board DHCP lease
  192.168.1.126
- **Software:** Vivado/Vitis 2025.2, FreeRTOS on classic MicroBlaze, new
  `sensors.c` discovery/poll layer + `net_telemetry.c` 75-key emitter
  (ultracode workflow, 9 verifier findings F1–F9 fixed pre-hardware)
- **Benchmarks:** UDP listener (`listen.py`, port 5001) frame capture; UART
  discovery log; JTAG register pokes for I2C ground truth

### Key commands

```bash
vitis -s sw/scripts/build_rtos.py
xsdb hw/scripts/run_elf.xsdb build/static_region.bit \
    build/vitis-ws/tricorder_rtos/build/tricorder_rtos.elf
timeout 40 cat /dev/ttyUSB1 > sens_uart.log   # discovery transitions
python3 listen.py 25                          # telemetry gate
# ground truth: JTAG dynamic-mode read of ADT7420 on axi_iic_1
xsdb iicpoke.xsdb    # mwr 0x40810108 0x197 ; mwr 0x40810108 0x201
```

## 3. Observations

First run: every I2C device ABSENT, including the soldered-on ADT7420:

```
SENS: init mask=00004000 i2c=0
SENS: ADT7420 UNKNOWN->ABSENT
I2C-EXT: walk -> 0 device(s)
```

JTAG poke of the same core with the same dynamic-mode sequence: device
ACKs and delivers data — `DRR=0x10` (temp MSB ≈ 32 °C), but ISR reads
`0xD2` — **TX_ERROR set after a fully successful read**.

After the two driver fixes (§4), discovery and the wire gate both pass:

```
SENS: ADT7420 UNKNOWN->PRESENT
SENS: init mask=00004001 i2c=0
```

| Gate | Result |
|------|--------|
| 75-key frame on listener | **PASS** — `seq=27 … temp_cc=3343 … sonar_bursts_cnt=-3` |
| Live sensor value | **`temp_cc=3343`** (33.43 °C, ADT7420) |
| `sens_mask` | **16385** = ADT7420 (bit 0) + PDM (bit 14) |
| Unwired Type U keys | `-1` ABSENT / `-3` N-A as specified |
| Unwired Type S keys | `-1000001` / `-1000003` as specified |
| Switch-event frame (JTAG-injected) | **PASS** — `type=event evt=99 sw=7 name=SWAP state=active mask=128` |

## 4. Data analysis

Two independent AXI IIC (PG090) driver bugs conspired to fake a dead bus:

1. **IISR is toggle-on-write, not write-1-to-clear.** `IISR = mask` with a
   bit currently 0 *sets* it. The transfer preamble "cleared" TX_ERROR by
   fabricating it, so every transaction returned instant-NAK and every
   device probed ABSENT. Clearing must write back only the bits that read
   as 1 (`i2c_isr_clear()`, the xiic_l idiom).
2. **TX_ERROR doubles as "slave transmit complete".** In a master read the
   master's *own* end-of-read NAK raises the same bit, racing the final
   byte into the RX FIFO. Treating it as address-NAK dropped completed
   reads. Discriminator: TX_ERROR with data arriving = completion;
   TX_ERROR with the FIFO still empty after a bounded grace = real NAK.

The JTAG register poke was the pivotal experiment: it separated "bus/device
broken" from "driver misreads status" in one 6-word sequence. XADC and
ADXL362 sentinels are as-designed (deviation D1: MIG owns the XADC
primitive; no accel SPI core in the BD). Verifier findings F1–F9 all
landed: fabricated-value window (F4) closed, clamp floors (F5) now emit
error sentinels, RM counters (F8) capability-gated, NAK FIFO flush (F9),
tri-state link (F3), single emitter (F2), bounded poll loop (F1/F6),
doc examples match as-built truth (F7).

## 5. Ideas for future experiments

- Wire a JA-bus sensor (VEML7700 is the simplest: no ID register, config
  echo check already in place) and watch its keys go live without a rebuild
- Soak test: hours of frames, assert `tele_overflow` stays 0
- Re-flash QSPI boot image so the standalone boot carries the new app
- Feed `light_lux` / `co2_ppm` to the 7-seg via the existing switch UI
- Grafana/InfluxDB ingest of the key=value stream on the host

---

# EXPERIMENT 27 Jul 2026 21:05:00 DHCP Lease Acquired: REFCLK Phase Was the Bug :complete:

## 1. Hypothesis

Entry 32 left Ethernet half-working: telemetry reached the host but no
DHCP OFFER ever arrived and ping never answered. Two hypotheses were
open - EmacLite MAC filtering, or something in the RMII datapath.

## 2. How

- Removed a self-inflicted blocker first: the bitstream still carried
  `TX_LOOPBACK=1` from an earlier bisect, so the board had been **deaf to
  the wire** (RX fed from its own TX) during every entry-32 receive
  measurement. With it gone, real LAN traffic appeared immediately
  (ARP broadcasts, UDP 58277->5002) - and the "unicast never received"
  and "MAC filter" theories evaporated.
- Then TX died on the clean build while it had worked on the loopback
  build - the same code, different placement. **Root cause: the TX
  output flops launch on the clk50 rising edge and the ODDR forwarded
  REFCLK rising at the same instant, so the PHY sampled TXD exactly as it
  changed - zero setup margin, working or failing on placement luck.**
  Fix: forward REFCLK inverted (ODDR D1=0/D2=1), putting the PHY's
  sampling edge in the middle of our data eye (~10 ns each side).
- Also added DHCP option 61 (client identifier) for router
  compatibility, matching what this network's router issues to hosts.

### Key commands

```bash
make -C hw static && make -C hw rtos
xsdb hw/scripts/run_elf.xsdb build/static_region.bit .../tricorder_rtos.elf
ping -c4 192.168.1.126 ; nc -ulp 5001
```

## 3. Observations

- **`NET: DHCP discover` -> `DHCP request 192.168.1.126` -> `NET: DHCP IP
  192.168.1.126`** - a real lease from the router at 192.168.1.1.
- **Ping answered from this host: rtt 11.8-172 ms**, 2-4 replies per run
  (loss is this shared LAN, not the board).
- **Telemetry streaming continuously**: `seq=6 uptime=43 temp_cc=3337
  i2c=0 rm=3 swaps=0 ...` captured with `nc -ulp 5001`.
- Before the REFCLK fix: telemetry silent, ping 100% loss. After: both
  work on the first try, and DHCP completed within two DISCOVERs.

## 4. Data analysis

Two bugs stacked, and each masked the other. The debug-only
`TX_LOOPBACK` left in the build made receive measurements meaningless
for hours - the lesson being that a bisect switch must be reverted the
moment its question is answered, and that "it works" evidence collected
under a debug build proves only what that build wired. Underneath it sat
a genuine hardware-timing defect: source-synchronous RMII with the clock
forwarded in phase with the data is a coin flip, and it flipped heads on
one build and tails on the next, which is exactly why intermittent
"works on my build" Ethernet bugs are notorious. Inverting the forwarded
clock is the standard remedy and cost one line. The tricorder now has an
IP, answers ping, and streams sensor telemetry to any listener on the
network.

## 5. Ideas for future experiments

- Add the `set_output_delay` constraints the spec drafted (currently
  commented out) so the margin is enforced by timing analysis rather
  than by construction.
- Lease renewal (T1/T2 timers) - the client currently takes a lease and
  keeps it until reboot.
- Telemetry to a fixed collector IP as well as broadcast; add the DFX
  personality name and switch state to the payload.
- SW15 now shows the leased address; verify the alternating octet-pair
  render at the board.

---

# EXPERIMENT 27 Jul 2026 20:24:50 Ethernet Bring-Up: Telemetry on the Wire, DHCP Still Silent :complete:

## 1. Hypothesis

Can the tricorder push telemetry off-board over the Nexys A7's RMII
Ethernet, obtain an address by DHCP on a live network, and show that
address on the 7-seg (SW15), sounding the alert when there is no lease?

## 2. How

- **Equipment:** Nexys A7-100T, cable into a live DHCP network; this host
  on the same LAN at 192.168.1.112/24. No root here, so no tcpdump.
- **Design (ultracode workflow: spec agent, 3 implementers, verifier):**
  `docs/ethernet-spec.md` (854 lines) froze every cross-implementer
  interface; then `hw/rtl/rmii_shim.v` (MII<->RMII, 25 MHz nibbles vs
  50 MHz dibits, CRS_DV framing), `hw/constraints/ethernet.xdc` (12 PHY
  pins from the Digilent master), `sw/tricorder-rtos/src/net.{c,h}`
  (EmacLite driver, ARP, ICMP, UDP telemetry - no lwIP). The verifier
  caught the two integration gaps (top wrapper, main.c) and a missing
  `create_generated_clock`; all fixed.
- **Integration:** axi_ethernetlite at 0x40E00000 (polled, ping-pong
  off), axi_gpio_eth PHY reset at 0x40E10000, clk_wiz 50 MHz output,
  shim + REFCLK ODDR in the top wrapper. DHCP client (DISCOVER/OFFER/
  REQUEST/ACK) added to net.c; `ui_live.ip` drives the new SW15 display.

### Key commands

```bash
make -C hw static dfx && make -C hw rtos
xsdb hw/scripts/run_elf.xsdb build/static_region.bit .../tricorder_rtos.elf
nc -ulp 5001            # telemetry listener on this host
```

## 3. Observations

- **Telemetry reaches this host over real Ethernet:**
  `seq=13 uptime=78 temp_cc=3331 i2c=0 rm=3 swaps=0 ...` captured with
  `nc -ulp 5001`. The datapath MAC -> shim -> PHY -> switch -> host works.
- PHY: ID 0007 C0F1 @ addr 1; autoneg completes; **BMSR 0x782D, ANLPAR
  0xC1E1, reg31 0x1058 = 100BASE-TX full duplex**; internal loopback
  self-test **60/60 bytes OK**, proving the shim's TX serializer and RX
  deserializer both correct.
- **Bug found and fixed:** the PHY stayed in internal loopback after the
  self-test - every frame came back to us and nothing reached the wire
  (the "received ARP" was our own gratuitous ARP). net_init now verifies
  BMCR bit 14 is clear and retries. This was the whole reason the first
  hours of DHCP attempts saw no reply.
- Counters (`net_stats` read live over JTAG) are the workhorse
  instrument: rx_frames / rx_arp / rx_icmp / rx_dropped / tx_frames.
  Present state: RX from the wire works (host ARP requests match,
  rx_arp increments), TX works (telemetry lands), **but no unicast frame
  has ever been received** - rx_icmp stays 0, ping gets no reply, and no
  DHCP OFFER arrives.
- MAC-filter programming is the prime suspect for the unicast gap. Two
  variants tried and reverted: adding `TX_LEN=6` per PG135 killed RX
  entirely (rx_frames 0); PROG-bit-only polling never self-clears and
  hangs init. The committed form (PROG|GO, poll GO) is the one that
  leaves broadcast RX/TX working.

## 4. Data analysis

Ethernet is two thirds delivered: the tricorder now streams live sensor
telemetry onto a real network, which was the actual request, and the
hardware datapath is proven in both directions by an in-PHY loopback and
by frames arriving at this host. What remains is a MAC-address filter
question, not a datapath question: EmacLite accepts broadcast but no
unicast, which blocks ping replies from being useful and - if this
network's DHCP server unicasts its OFFER - blocks the lease. The
loopback-stuck discovery is the transferable lesson: a PHY left in
internal loopback looks *exactly* like a working link (link up, frames
"received") while nothing reaches the wire; always verify BMCR bit 14
after any loopback self-test.

## 5. Ideas for future experiments

- Resolve the unicast filter: compare against `xemaclite.c`
  `XEmacLite_SetMacAddress` on this install (the sequence that works
  here differs from the PG135 text), or set the EmacLite promiscuous bit
  (RX_CTRL bit 3 in some configurations) and re-test ping.
- If the OFFER is being broadcast and simply not parsed, log every RX
  frame's ethertype/ports for a few seconds - the parser filter
  (`rxf[36..37] == 68`, `flen >= 42+240`) is unverified against a real
  OFFER.
- Static-IP mode is a usable fallback today: set our_ip and have_ip=1 and
  the board answers on the LAN with telemetry streaming.
- SW15 currently shows `nO IP` with the alert sound, which is correct
  behaviour for the present state.

---

# EXPERIMENT 27 Jul 2026 07:28:20 Standalone QSPI Boot: Bitstream + Bootloader + App, No Host Required :complete:

## 1. Hypothesis

Can the board run with no host at all: FPGA configured from QSPI at
power-on, and the FreeRTOS app loaded from flash by a bootloader baked
into the bitstream — completing the standalone instrument?

## 2. How

- **Config side:** `full_a.bit` written to flash 0x0 via `write_cfgmem
  -loadbit` (proper config formatting; the old raw `-loaddata` placement
  would not boot), `BITSTREAM.CONFIG.SPI_BUSWIDTH 4` + `CONFIGRATE 33`.
  Two MCS images now: `boot.mcs` (bitstream, standard bit order) and
  `slots.mcs` (data slots, `-disablebitswap` per entry 25) — write_cfgmem
  flags are global, so they cannot share a file; `program_flash.tcl`
  programs both ranges.
- **Software side:** `sw/boot` — a 1.4 KB bootloader linked into the LMB
  BRAM and baked into the bitstream post-route with `updatemem` (MMI from
  `write_mem_info`, proc `u_static/static_region_i/microblaze_0`). It
  waits for DDR2 calibration, quad-reads a scatter-load image from flash
  0xD00000 (`tools/mkbootimg.py` packs the app ELF's PT_LOAD segments:
  magic/nseg/entry + {addr,len,data}...), loads each segment — the app's
  LMB vectors overwrite the bootloader's own, by design — and jumps.
- Iteration trick worth keeping: boot.c changes need only mb-gcc +
  standalone `updatemem` on the kept `full_a_raw.bit` (~1 min), no
  Vivado flow re-run.

### Key commands

```bash
vivado -mode batch -source hw/dfx/build_dfx.tcl      # raw bit + updatemem
vitis -s sw/scripts/build_rtos.py
vivado -mode batch -source hw/scripts/build_flash.tcl # boot.mcs + slots.mcs
vivado -mode batch -source hw/scripts/program_flash.tcl
# gate: configure ONLY, no ELF download:
vivado -mode batch -source hw/scripts/program.tcl -tclargs build/dfx/full_a.bit
```

## 3. Observations

- **Gate (twice, consecutively): configure-only → `BOOT: qspi
  bootloader` → `BOOT: launching app` → full tricorder** (banner, JEDEC
  01 20 18, QE persistent, scheduler ticks, display banner
  register-exact, boot chirp fired). Zero software over JTAG.
- Boot image: 3 PT_LOAD segments, 42,906 payload bytes, entry 0x0
  (vectors → crt); flash 0xD00000–0xD0A7BF.
- One transient: the first bootloader run after a flash-programming
  session read 0xEEEEEEEE (flash-silent fill) — consistent with the
  cfgmem proxy leaving the S25FL128S in a latched read mode; clean
  reconfigurations since then boot correctly every time.

## 4. Data analysis

The instrument no longer needs this workstation: flash holds the
bitstream (with the bootloader inside), the app, and all four
personalities — power alone brings up the full switch-driven,
sound-effected tricorder. The two-MCS split is the load-bearing subtle
bit: config data and slot data need opposite bit-ordering treatments,
which one write_cfgmem invocation cannot express. What remained for true
power-on proof was physical: JP1 to QSPI and a power cycle.

*Revision (27 Jul 2026 07:40, George):* **JP1 moved to QSPI, power
cycled — it works.** The board boots the full tricorder from flash with
no host attached. Every gate in this line of work is now closed.

## 5. Ideas for future experiments

- George: move JP1 to QSPI, power-cycle, expect the chirp and banner
  with no cable. (JTAG still works for development regardless.)
- Harden boot.c against the post-programming flash state: issue a mode
  reset (0xFF continuous-read exit + 0xF0 software reset) before the
  first real read.
- Boot time budget: config at x4/33 MHz ≈ 0.5 s + app load ≈ 1.5 s;
  measure for the notebook once the jumper is set.
- CRC32 over the boot image (mkbootimg already has the layout to carry
  it).

---

# EXPERIMENT 27 Jul 2026 06:56:01 Switches Command the Hardware: Personality Loads, Boot Chirp, Mismatch Alert :complete:

## 1. Hypothesis

**DECISION (George):** the entry-29 test-mode functions should not be
display-only — SW0–SW3 must actually load their DFX personalities; the
scan sound should also play at boot; the alert sound should also fire on
a DFX signature mismatch. All three are software-only changes on the
entry-29 hardware.

## 2. How

- **Equipment:** Nexys A7-100T, entry-29 image (no fabric change; three
  Vitis-only rebuilds, commits 693ffbe, c3e982f, 02f7e6d).
- **Switch→personality:** `ui_task` publishes `ui_live.req_rm` for
  SW0–SW3 (mapping trap handled: HELA = hello-A sig 0xA5A5A5A5 = rms
  index 3, HELB = index 2); `dfx_task` **drops the 8 s auto-rotation**
  and swaps reactively on a 250 ms poll, holding `cur_rm = -1` during
  the load. S0–S3 readouts track it (`----` → `run`); PCM/energy (S10,
  S11) read the live RM registers when PDM is loaded, sonar peak (S12)
  when sonar is; the sonar personality auto-fires one ranging burst per
  second while selected. The scan sound, already tied to switch-on, is
  now literally the personality load sound.
- **Boot chirp:** `main()` pulses the scan trigger right after the boot
  banner; `ui_task` seeds its toggle state from the register.
- **Mismatch alert:** `dfx_task` pulses the alert trigger whenever a
  loaded personality fails its signature check. Both tasks moved to
  read-modify-XOR of the audio register inside `taskENTER_CRITICAL` —
  single-writer discipline so the two trigger sources cannot revert each
  other's toggles (the earlier local shadow copy would have).

### Key commands

```bash
make -C hw rtos && xsdb hw/scripts/run_rtos_dfx.xsdb
```

## 3. Observations

- Boot: **no auto-rotation lines on UART** (dynamic region now changes
  only on request); banner register-exact (0x5C505E50/0x78503039 =
  `trICordr`); **AUDIO trigger register reads 1 after boot** — the chirp
  fired; scheduler and sensors streaming.
- Hand gates at the board (George): switch-flip loads with the scan
  sound per selection, `run` readouts, `Err SEL` + alert on multiple
  switches.
- The mismatch-alert path cannot be provoked on a healthy board — it is
  code-reviewed and boot-gated, and will announce the first real load
  failure.

## 4. Data analysis

The instrument loop is closed: physical control → OS decision → fabric
reconfiguration → live readout → sound, all on the flash-resident
personality store. Removing the rotation demo in favor of switch-driven
loads is the semantic shift from "demo" to "instrument": the dynamic
region is now operator state. The audio-register single-writer fix is
worth remembering as a pattern — toggle-signaling across tasks needs
either one owner or atomic RMW; the shadow-copy version worked only
while exactly one task pulsed.

## 5. Ideas for future experiments

- Debounce/settle on switch transitions (a slow flip across two switch
  positions transits Err SEL and fires the alert — arguably correct,
  possibly chatty).
- Show the ACTIVE personality on the display's unused left digits during
  non-S0-S3 functions (e.g. dot on digit 7 while PDM loaded).
- Sonar burst cadence + peak into the S12 readout is ready for the real
  harness: when the piezo chain arrives, SW1 becomes a live rangefinder
  with zero further software.

---

# EXPERIMENT 26 Jul 2026 22:48:43 Test-Mode UI: 15 Switch Functions, 7-Seg Readouts, Tricorder Sounds :complete:

## 1. Hypothesis

Can the board become a self-explanatory instrument: SW0–SW14 each select
one of 15 mutually exclusive test-mode functions with a readout on the
8-digit 7-seg display (error display on multiple selection), and the TOS
tricorder scan sound playing whenever a function is switched on?

## 2. How

- **Hardware:** axi_gpio_3 (16 switch inputs, 0x40030000), axi_gpio_4
  (dual 32-bit = 64-bit raw segment register, 0x40040000/8), axi_gpio_5
  (2 toggle-to-play audio triggers, 0x40050000); top-level
  `sevenseg_scan.v` (1 ms/digit multiplex, register active-high →
  pins active-low inverted once) and `audio_player.v` (48 KiB audio ROM
  in BRAM — scan 4.1 s + alert 1.2 s at 8 kHz/8-bit — sigma-delta PWM
  into AUD_PWM/A11, amp enable D12). `sounds/*.mp3` committed;
  `tools/mp3_to_mem.py` regenerates the ROM (miniaudio, no ffmpeg).
- **Software:** `ui.c`/`ui.h` — font, right-aligned compose, 15-function
  table (live: temp, I2C count, swap count, uptime, last-swap-µs), 100 ms
  task; scan sound on each new single-switch selection, alert sound on
  entering `Err SEL`. `ui_live` fed by the sensor and DFX tasks.
- **Process:** ultracode workflow — 4 parallel implementers (RTL, XDC,
  UI C, manual) + adversarial verifier, which confirmed pins/protocol/
  digit-order coherence across all three layers and caught 4 defects
  before hardware (unrenderable tags, W-glyph/manual mismatch, two
  manual wording bugs). Docs: `docs/manual.md` (user manual).

### Key commands

```bash
make -C hw static dfx && vivado -source hw/scripts/build_flash.tcl
make -C hw rtos && vivado -source hw/scripts/program_flash.tcl
xsdb hw/scripts/run_rtos_dfx.xsdb
```

## 3. Observations

- Final image WNS +1.268 ns; all five chain stages PASS.
- **Register-level display gate: byte-exact.** With all switches down,
  DISP_LO=0x5C505E50 DISP_HI=0x78503039 — decoding to the idle banner
  `trICordr` through the committed font, matching the computed
  expectation to the last bit. Switch inputs read 0; audio triggers idle.
- Full demo unchanged around it: flash-sourced swaps 4/4 with signatures
  (see entry 28), PDM live audio, sonar bursts, ADT7420 stream.
- **Human-only gates remain:** physically flipping switches (readouts,
  Err SEL, exclusivity) and hearing the scan/alert sounds — the register
  proxies verify the full digital path up to the pins.

## 4. Data analysis

The three-layer protocol (software font/compose → 64-bit register →
scanner) was specified once in the workflow prompt and implemented by
three different agents — the verifier proving digit order and polarity
coherent across all three before synthesis is what made this land in one
hardware iteration. The audio path reuses the toggle-signaling idiom
from the ICAPE2 writer, and baking sounds into BRAM (12 of ~110 free)
costs no CPU and no DDR bandwidth. Test mode is honest: placeholders say
`On`, live values are live, and the manual documents which is which.

## 5. Ideas for future experiments

- George: flip switches (SW4 temp, SW9 uptime live; two up → Err SEL +
  alert), listen for the scan sound on each function-on.
- Wire function switches to actual DFX personality selection (SW0-SW3
  choosing the loaded RM) — the ui and dfx tasks already share state.
- Alert sound on SIGNATURE MISMATCH too, not just Err SEL.

---

# EXPERIMENT 26 Jul 2026 22:40:00 ILA Verdict on XIP: Bus Clocks Perfectly, Flash Never Answers — Case Closed :complete:

## 1. Hypothesis

Entry 27 left XIP data framing unexplained. Does a System ILA on the XIP
core's AXI read channel show a byte shift that can be corrected?

## 2. How

- system_ila (via the `xilinx.com:bd_rule:debug` automation — manual
  monitor-net connects fail obscurely in batch) on the axi_quad_spi
  AXI_FULL net; `write_debug_probes` added to the DFX flow;
  `hw/scripts/ila_capture.tcl` arms on RVALID and dumps CSV while the
  demo's flash swap fires.
- XIP re-tried in BOTH quad and standard SPI modes on the fixed
  STARTUPE2/CCLK path.

## 3. Observations

- **Quad XIP capture:** ARs march perfectly (0xC00040/80/C0, 16-beat
  INCR), R beats exactly every 16 cycles — 32 bits/4 lanes at 50 MHz,
  i.e. the SPI bus clocking at full quad speed — but **every RDATA beat
  is 0xCCCCCCCC**: dq3/dq2 pulled high, dq1/dq0 low. The flash never
  drives — it does not recognize the XIP core's quad command sequence.
  Not a shift; silence. No error in XIPSR, none in the DMA.
- **Standard XIP:** transfers complete at single-line speed (73 ms/slot)
  but the ICAP never syncs either — same disease, milder symptom.
- Regression found during the final rebuild: the XIP-standard experiment
  had left dq2/dq3 Hi-Z and disconnected in the top wrapper — the
  restored quad-command core was then sampling two floating lanes.
  Reconnected; **flash-sourced swaps pass again end-to-end**
  (JEDEC 01 20 18, QE persistent, 4/4 personalities from FLASH,
  4.8–6.1 s reads + ~1 ms ICAP).

## 4. Data analysis

Closed: the axi_quad_spi XIP mode as configured (2025.2, Spansion table,
S25FL128S) issues a command set this flash ignores — in quad it is
outright silent, and no software-visible knob changes the command
protocol. The working architecture is command-mode quad reads (software
framing, entry 26) + DMA→ICAPE2; XIP would only be worth revisiting with
a different memory-vendor table or IP version, armed with this exact ILA
method (the capture script is committed). The dq2/dq3 float regression
is a lesson in experiment hygiene: hardware experiments that mutate
shared wrappers need the revert to be as tested as the change.

## 5. Ideas for future experiments

- If flash swaps ever need to be faster than ~5 s: try C_SPI_MEMORY
  {1,2} (winbond/numonyx command tables) in XIP against the ILA method,
  or interrupt-driven command-mode reads (~1 s reachable).
- The ILA + debug-automation recipe is reusable for the AD9226 capture
  path when the harness arrives.

---

# EXPERIMENT 26 Jul 2026 13:37:02 XIP Retry: 21 MB/s Transfers, Data Alignment Unresolved — Reverted :complete:

## 1. Hypothesis

Entry 26 predicted XIP mode would work on the fixed STARTUPE2/CCLK path
(the original XIP failure sharing the dead-bus root cause) and give DMA
flash→ICAP at ~20–40 ms. Test it.

## 2. How

- **Hardware:** axi_quad_spi back to XIP (quad, C_USE_STARTUP=0,
  window 0x60000000/16M pinned via C_S_AXI4_BASEADDR); AXI_FULL on a new
  axi_mem_intercon port; `create_bd_addr_seg` for the DMA space +
  excludes for the CPU spaces (recipe from entry 25). External
  STARTUPE2/IOBUF path retained. QE already persistent.
- **Software:** dfx_task DMA SA = XIP slot address directly — no bounce,
  no SPI driver (XIP has no command interface; only XIPSR visibility).
- Two flash byte orders tried: raw and word-byte-swapped slot images.

### Key commands

```bash
# full chain x2 (raw + swab flash) + revert chain
make -C hw static dfx && vivado -source hw/scripts/build_flash.tcl \\
  && make -C hw rtos && vivado -source hw/scripts/program_flash.tcl
```

## 3. Observations

- **The CCLK-fix prediction was right in part: XIP transfers now
  complete** — `DFX: RM-PDM live in 20908 us` for a 443,936 B slot =
  **21.2 MB/s**, byte-identical timing across runs. (The original XIP
  attempt never completed data at all.)
- But the RM signature stayed at the previous personality in **both**
  byte orders — the ICAP never saw a sync word, implying the stream is
  misaligned *within* words (a byteswap cannot fix an odd-byte shift).
  Prime suspect: the XIP core's quad command sequence on Spansion
  (0xEB continuous-read mode bits / dummy-cycle framing) offsetting the
  data phase — the same class of framing bug found empirically in the
  command-mode driver (entry 26: 8 overhead bytes, not 5), but here the
  framing is inside the IP and invisible.
- No error ever surfaces: XIPSR=1 (RX empty, no flags), DMA completes
  Idle, signature check is the only tripwire — again.
- **Reverted to the entry-26 quad-command configuration** and re-gated:
  all four personalities from FLASH, sigs OK, reads 4.9–6.1 s.

## 4. Data analysis

Half the prediction confirmed (transfers at wire-class speed once CCLK
exists), half open (data framing). The debugging asymmetry decided the
revert: command mode exposes every byte to software, XIP exposes
nothing — closing this needs an ILA on the AXIS sink to see the actual
beats, a bench session rather than blind iteration. The stack keeps the
proven flash path; the 20.9 ms measurement stands as the prize for
fixing XIP framing: ~250x faster flash swaps with zero CPU.

## 5. Ideas for future experiments

- ILA (or 64-beat capture register) on the icap_axis stream; compare
  beats against the .bin — the shift amount will identify the framing
  culprit directly.
- Try XIP standard (single-line) mode first: simpler command sequence
  (0x0B), lower speed but if it aligns, the quad framing is isolated.
- Check PG153/AR for Spansion XIP dummy-cycle configuration parameters
  (C_XIP_PERF_MODE interaction).

---

# EXPERIMENT 26 Jul 2026 10:46:52 Quad SPI: Correct at 4 Lanes x 50 MHz — and Proof the Reader Is CPU-Bound :complete:

## 1. Hypothesis

Entry 25 measured 6.4–7.6 s flash reads on single-line SPI at
6.25 MHz. Does quad mode (C_SPI_MODE=2, C_SCK_RATIO=2 → 50 MHz, 4 data
lanes ≈ 25 MB/s wire) bring flash-sourced swaps toward the ~15 ms class?

## 2. How

- **Hardware:** quad axi_quad_spi; dq2/dq3 (L14/M14) added through the
  BD/top/XDC; SCK ratio 2.
- **Flash:** S25FL128S **QE bit** (CR1 bit 1, non-volatile) set once at
  boot by the app — WREN + WRR(SR1=0, CR1=0x02) + WIP poll — verified by
  RDCR readback; QE also releases the HOLD#/WP# functions from dq3/dq2.
- **Software:** reads via 0x6B (quad-output fast read); exact-count RX
  drain (no per-byte status polls).

### Key commands

```bash
make -C hw static && make -C hw dfx
vivado -mode batch -source hw/scripts/build_flash.tcl
vivado -mode batch -source hw/scripts/program_flash.tcl
make -C hw rtos && xsdb hw/scripts/run_rtos_dfx.xsdb
```

## 3. Observations

- `QSPI: QE set, CR1=02` first boot; `QE already set` thereafter.
- First quad reads returned data **shifted +3 bytes** (and sig
  mismatches caught it): with 0x6B, RX overhead is **8 bytes** — cmd +
  3 addr at 1 byte/8 cycles, then 8 dummy cycles at quad = **4 bytes** —
  not the naive 5; TX must supply matching entries. Fixed; bounce header
  then byte-perfect and **all four personalities load from FLASH with
  signatures OK.**
- **Timing: 4.9–6.1 s per slot — essentially unchanged from single-line
  6.25 MHz.** Wire time at 25 MB/s is ~17 ms of that; the rest is the
  polled driver: per-128-byte FIFO churn and per-byte DTR/DRR AXI
  accesses. The exact-count drain bought only ~8%.

## 4. Data analysis

Quad mode is functionally landed (correct lane handling, QE persistent,
framing understood and documented) but bought almost nothing, because
the measurement in entry 25 was mis-attributed: the reader was never
wire-bound — a 32x wire speedup moved the total by <10%. The real
lesson reaches back further: the XIP failure in entry 25's debug trail
almost certainly shares the same root cause as the dead command-mode
bus (C_USE_STARTUP=1 expecting an external STARTUP_IO connection →
CCLK never toggling). With today's working STARTUPE2 path, XIP mode +
quad + the existing DMA→ICAPE2 sink would stream flash to ICAP with no
CPU in the loop at ~25 MB/s: **~20–40 ms flash swaps** — the identified
next hardware pass, not attempted tonight.

## 5. Ideas for future experiments

- Re-try XIP (C_USE_STARTUP=0 + external STARTUPE2, quad, QE now set):
  DMA SA = XIP window → flash→ICAP direct; expect ~20–40 ms.
- Alternative same-goal: axi_quad_spi interrupt-driven with a small
  RX-drain ISR, or a custom SPI-to-AXIS bridge into the existing sink.
- Bounce-dump debug print in dfx_task can go once flash path is boring.

---

# EXPERIMENT 26 Jul 2026 01:55:20 QSPI Slot Table: Personalities Load from Flash :complete:

## 1. Hypothesis

Can the compressed partials live in the on-board 16 MiB QSPI flash
(S25FL128S) in a slot layout, with the FreeRTOS system pulling
personalities from flash at runtime — no JTAG staging of partials?

## 2. How

- **Equipment:** Nexys A7-100T; QSPI DQ0/DQ1/CSN on K17/K18/L13, SCK on
  the dedicated CCLK pin (only reachable through STARTUPE2).
- **Hardware:** axi_quad_spi in **command mode** (single-line, FIFO 256,
  no internal STARTUP), per-signal ports out of the BD; the top wrapper
  owns the three IOBUFs and a STARTUPE2 whose USRCCLKO carries sck. XIP
  mode was tried first and abandoned: reads "succeed" with zero
  observability while the bus is dead — command mode buys a JEDEC-ID
  liveness probe.
- **Flash layout:** `hw/scripts/build_flash.tcl` → MCS with full_a.bit @0
  and 1 MiB slots (a@0x900000 b@0xA00000 pdm@0xB00000 sonar@0xC00000),
  plus the generated slot manifest `sw/tricorder-rtos/src/slots.h` — one
  script is the single source of truth for both. Programmed with
  `hw/scripts/program_flash.tcl` (erase+program+verify over JTAG).
- **Software:** boot JEDEC probe; per swap: SPI fast-read (0x0B) of the
  slot into a DDR2 bounce buffer, then the entry-23 DMA→ICAPE2 path;
  signature check with automatic fallback to DDR-staged partials.

### Key commands

```bash
make -C hw static && make -C hw dfx
vivado -mode batch -source hw/scripts/build_flash.tcl
vivado -mode batch -source hw/scripts/program_flash.tcl
make -C hw rtos && xsdb hw/scripts/run_rtos_dfx.xsdb
```

## 3. Observations

- **`QSPI: JEDEC 01 20 18`** — S25FL128S answers; then **all four
  personalities load from FLASH**: PDM (live audio), SONAR (bursts),
  B, A — signatures verified every swap, zero fallbacks.
- Timing per flash swap: ICAP stream ~1 ms (unchanged, wire speed);
  **flash read 6.4–7.6 s per slot** (~17 µs/byte: CPU-polled single-line
  SPI with per-128-byte FIFO churn at SCK = ext_spi_clk/16).
- Debug trail worth keeping: (1) XIP-mode reads returned idle-bus 0xFF
  forever with XIPSR clean — unobservable dead end; (2) JTAG-poked JEDEC
  through the same core returned `FF 01 20 18`, proving bus + STARTUPE2
  CCLK path and isolating the C driver; (3) driver bug #1: soft-reset
  needs settle time (first-xfer-after-reset garbage); (4) driver bug #2:
  the slot read used the post-increment rotation index — the "PDM" swap
  cleanly loaded the SONAR slot, and the signature mismatch caught it
  (defense-in-depth via per-RM signatures paying off exactly as
  designed).

## 4. Data analysis

The tricorder no longer needs a host to change personalities: partials
live in flash, the OS reads a slot, streams it through the DMA→ICAPE2
path, and verifies the result — the fallback chain (flash → DDR staging)
never triggered in the final run. The 6.5 s flash read dominates and is
pure driver ceiling, not architecture: single-line polled SPI at /16
clocking. Known ladder: SCK ratio /2 (~8x), quad I/O (4x more),
interrupt/DMA-driven SPI — sub-second flash swaps are reachable without
new silicon. Full self-boot (golden image from flash + ELF in flash)
is the remaining step to a standalone instrument; the golden image is
already in slot 0.

## 5. Ideas for future experiments

- SCK ratio /2 + quad fast-read (set flash QE) → target <1 s flash swap.
- Boot from QSPI: BITSTREAM.CONFIG.SPI_BUSWIDTH + srec bootloader in
  LMB reading the ELF from a flash slot — no JTAG at all.
- CRC32 per slot in the manifest, checked before ICAP streaming.
- Wear note: slots rewritten per DFX build; fine for development
  cadence, add erase counters if this ever ships.

---

# EXPERIMENT 26 Jul 2026 00:16:23 Compressed Partials: Sub-Millisecond Swaps (0.78–1.06 ms) :complete:

## 1. Hypothesis

Entry 23 hit the configuration port's wire speed (2.7 ms for a 1.07 MB
partial), leaving partial size as the only lever. Does
`BITSTREAM.GENERAL.COMPRESS` shrink the partials — and do compressed
(MFWR multi-frame-write) partials load correctly through the
DMA→ICAPE2 path?

## 2. How

- **Equipment:** Nexys A7-100T; hardware unchanged from entry 23.
- **Software:** `BITSTREAM.GENERAL.COMPRESS TRUE` set before every
  `write_bitstream` in `hw/dfx/build_dfx.tcl` (full config A + each
  partial). Loaders untouched — they already take length from file size.
- **Benchmarks:** full bare-metal gate sequence + RTOS rotation.

### Key commands

```bash
~/Vivado/2025.2/Vivado/bin/vivado -mode batch -source hw/dfx/build_dfx.tcl
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_dfx_test.xsdb 1
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_rtos_dfx.xsdb
```

## 3. Observations

| Artifact | Uncompressed | Compressed | Ratio |
|---|---|---|---|
| full_a.bit | 3,825,917 B | 1,910,779 B | 2.0x |
| rm_a partial | 1,071,052 B | 316,832 B | 3.4x |
| rm_b partial | 1,071,052 B | 311,976 B | 3.4x |
| rm_pdm partial | 1,071,052 B | 421,740 B | 2.5x |
| rm_sonar partial | 1,071,052 B | 356,780 B | 3.0x |

- pr_verify PASS unchanged; per-RM sizes now differ (compression tracks
  content: the near-empty hello RMs compress hardest).
- **Bare-metal swaps: 781/793/1055/893 µs** — still exactly
  words/100 MHz (311,976/4 = 77,994 words → 780 µs), so MFWR frames
  stream at full rate through the ICAPE2 sink.
- **FreeRTOS: PDM 1067 µs, sonar 903 µs.** All signatures OK, PDM live
  audio and sonar bursts unchanged, `DFX: PASS (all swaps ok)`.
- Side benefit: JTAG staging drops from ~4.3 MB to ~1.4 MB of dow
  traffic; full-image programming also halves.

## 4. Data analysis

Compression is free performance on every axis measured: 2.5-3.4x
smaller partials, proportionally faster swaps (still wire-limited, now
sub-millisecond), no correctness impact through either load path.
**Swap-time history closes at 16.2 s → 1.65 s → 2.7 ms → ~0.8-1.1 ms
(≈20,000x)** — reconfiguration now costs less than one FreeRTOS tick,
i.e. a personality change is cheaper than a 1 ms scheduler quantum. The
remaining QSPI-sourced swap estimate improves to ~13-17 ms flash-read
dominated (320-420 KB at 25 MB/s), well within interactive feel.

## 5. Ideas for future experiments

- QSPI slot table with compressed partials: measure flash→ICAP
  end-to-end (~15 ms expected).
- Per-RM size now varies — the slot table should store lengths, not
  assume uniform slots.
- Interrupt-driven DMA completion; at 0.8 ms it hardly matters, but it
  removes the last busy-wait.

---

# EXPERIMENT 25 Jul 2026 18:05:04 DMA-Fed ICAPE2: Swaps at Wire Speed — 2.7 ms :complete:

## 1. Hypothesis

Entry 22 left swaps CPU-pacing-bound (1.65 s RTOS / 390 ms bare-metal)
and predicted an AXIS-fed ICAPE2 writer would reach the ICAP's wire
speed. Target: under 20 ms per swap.

## 2. How

- **Equipment:** Nexys A7-100T.
- **Software/hardware:** BD gains **axi_dma_0** (simple MM2S only,
  26-bit length, 32-bit stream, S_AXI_LITE at 0x41E00000; M_AXI_MM2S
  into the MIG through connection automation). The MM2S stream leaves
  the BD (`icap_axis`) and terminates in the top wrapper's ICAPE2 sink:
  tready tied high, one config word per beat, CSIB low exactly while a
  registered beat is valid. Bit-ordering identity: software byteswap ∘
  UG470 per-byte bit-reverse = **full 32-bit reversal**, done in wiring
  — the DMA streams raw .bin bytes from DDR2 with zero software
  preprocessing. GPIO writer kept in hardware as fallback; both loaders
  (bare-metal + RTOS dfx_task) switched to: DMACR=1, SA=addr,
  LENGTH=bytes, poll Idle. RTOS path dropped its critical section —
  nothing to protect, the CPU is idle during the stream.
- **Benchmarks:** full bare-metal gate sequence + RTOS rotation demo.

### Key commands

```bash
make -C hw static && make -C hw dfx && make -C hw rtos
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_dfx_test.xsdb 1
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_rtos_dfx.xsdb
```

## 3. Observations

- Static rebuild WNS +0.901 ns; DFX flow pr_verify PASS.
- **Bare-metal: 2,679 µs per swap** — identical across all six swaps.
  Sanity: 1,071,052 B / 4 = 267,763 words at 100 MHz one-word-per-cycle
  = 2,678 µs theoretical. **The DMA path saturates the ICAP: wire
  speed, to the microsecond.**
- **FreeRTOS: 2,688 µs** (+9 µs DMA register setup/poll overhead), no
  critical section, ticks and ADT7420 reads continue through the swap.
- All signatures verified; PDM live audio (energy peak 3,743,367) and
  sonar bursts unchanged; `DFX: PASS (all swaps ok)`.

## 4. Data analysis

Swap-time history closes: **16.2 s → 1.65 s (compiler) → 2.7 ms (DMA)**
— 6,000x total, and the endpoint is the configuration port's physical
limit at 100 MHz x32, so there is nothing left to win in this direction
(a faster icap_clk is out of spec; compression would shrink the partial
itself). Reconfiguration is now invisible at demo timescales — under
three MicroBlaze ticks — which retires the "swap latency" concern from
the original plan outright. The bit-reversal-in-wiring trick also
removed the last software byte-order dependency: the staged partial is
byte-exact `write_bitstream -bin_file` output.

## 5. Ideas for future experiments

- Bitstream compression (`BITSTREAM.GENERAL.COMPRESS`) on partials:
  smaller staging + proportionally faster swaps.
- QSPI slot table: at 2.7 ms ICAP cost, QSPI read (~52 ms at x4 50 MHz)
  now dominates a flash-sourced swap — measure end-to-end.
- Interrupt-driven DMA completion (mm2s_introut → intc) instead of
  polling, freeing the dfx_task during the 2.7 ms.

---

# EXPERIMENT 25 Jul 2026 16:57:03 RTOS Swap-Time Optimization: 15.9 s → 1.65 s :complete:

## 1. Hypothesis

Entry 20 measured 16.2 s per DFX swap under FreeRTOS vs 390 ms
bare-metal and blamed tick preemption. Does `taskENTER_CRITICAL` around
the ICAP stream close the gap?

## 2. How

- **Equipment:** Nexys A7-100T, same bitstream/partials as entries 17-21
  (software-only change).
- **Software:** `dfx_task` wrapped the word loop in
  `taskENTER_CRITICAL`/`EXIT`; timing moved from `xTaskGetTickCount`
  (frozen when interrupts are off) to the axi_timer's **second counter**
  (counter 0 belongs to the FreeRTOS tick).
- **Benchmarks:** demo rotation, per-swap microsecond timer readings.

### Key commands

```bash
~/Vivado/2025.2/Vitis/bin/vitis -s sw/scripts/build_rtos.py
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_rtos_dfx.xsdb
```

## 3. Observations

- Critical section alone: **15,908,081 µs — no improvement**, but
  byte-identical times across swaps proved the loop ran uninterrupted;
  the tick-preemption hypothesis was WRONG.
- Real culprit: the Vitis app template compiles at **-O0 -g3**; the
  bare-metal loader was -O2. At -O0 the word loop is ~60 µs/word of
  stack traffic.
- `__attribute__((optimize("O2")))` on `dfx_task` +
  `always_inline` bswap: **1,651,043 µs = 1.65 s/swap — 9.6x faster.**
  All signatures still OK, sensors/heartbeat resume cleanly after each
  swap.
- Residual 4.2x vs bare-metal (6.2 vs 1.46 µs/word): cold-dcache reads
  of the 1 MiB partial from DDR plus instruction fetch sharing the same
  memory, vs the bare-metal loader's LMB-resident code.

## 4. Data analysis

Classic mis-attributed bottleneck: the deterministic (identical-to-the-
microsecond) timings under the critical section were the decisive
evidence that preemption contributed nothing — compiler optimization
level dominated. The critical section still earns its keep (swaps are
now atomic w.r.t. the scheduler; the tick loses ~1.65 s of wall time per
swap, acceptable at demo cadence). Next real step change needs hardware:
the AXIS-FIFO front on the ICAPE2 writer (~11 ms theoretical), which
also removes the critical-section tick loss entirely.

## 5. Ideas for future experiments

- AXIS-FIFO/DMA-fed ICAPE2 writer: target <20 ms, no interrupt blackout.
- Set -O2 project-wide in build_rtos.py app config rather than
  per-function attributes.
- Account tick loss: resync FreeRTOS time after swaps if wall-clock
  accuracy ever matters.

---

# EXPERIMENT 25 Jul 2026 15:21:12 Demo Runbook Published (docs/demo.md) :complete:

## 1. Hypothesis

Administrative entry — no new experiment. Records the publication of the
demo runbook so anyone (including future us) can reproduce the entry-20
integrated demo without reading the notebook history.

## 2. How

- **Equipment:** none (documentation).
- **Software:** `docs/demo.md`, commit 141b287 on `gnn-a7-tricorder`.
- **Benchmarks:** procedure cross-checked against the commands actually
  used in entries 17-20.

### Key commands

```bash
# The runbook's two-terminal core:
stty -F /dev/ttyUSB1 115200 raw -echo && cat /dev/ttyUSB1
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_rtos_dfx.xsdb
```

## 3. Observations

`docs/demo.md` contains: the four-personality rotation table with
signatures and how to provoke each (mic tap for PDM energy; `07FF0000`
as the documented no-harness sonar value); prerequisites — **no sudo**
(plugdev covers the FT2232), clean-clone rebuild via
`make -C hw static dfx rtos` (~45 min); run procedure; expected
transcript; two variants (bare-metal 390 ms gate run
`run_dfx_test.xsdb 1` — byteswap flag mandatory with `-bin_file`
partials — and the CPU-free JTAG partial check); one-paragraph
architecture summary including the HWICAP-broken → raw-ICAPE2-writer
story; troubleshooting table sourced from real failures hit this
session (stale hw_server, dueling tty readers, unstaged partials).

## 4. Data analysis

Documentation is the cheapest reproducibility instrument this project
has: every gate in entries 11-20 was command-driven, so the runbook is a
faithful executable summary rather than a narrative. Kept deliberately
separate from plan.md (intent) and the notebook (evidence): demo.md is
operations.

## 5. Ideas for future experiments

- Update demo.md when the sensor harness lands (external scan expected
  addresses become observable) and when the swap-time optimization
  replaces the 16.2 s RTOS figure.
- Add a `make -C hw demo` target wrapping the xsdb invocation.

---

# EXPERIMENT 25 Jul 2026 09:55:00 Integrated Demo: FreeRTOS Rotates All Four DFX Personalities + CI Targets :complete:

## 1. Hypothesis

Can the M4 FreeRTOS system and the M5 DFX loader merge — the RTOS itself
swapping sensor personalities while its sensor/heartbeat tasks keep
running — completing the tricorder's core operating loop?

## 2. How

- **Equipment:** Nexys A7-100T; same full-config-A bitstream and partials
  as entries 17-19 (no hardware rebuild — the ICAPE2 writer, decoupler,
  and RM registers were already in the static region).
- **Software:** `dfx_task` added to `sw/tricorder-rtos` (priority above
  sensors): every 8 s rotates PDM → SONAR → B → A from the DDR2-staged
  partials via the raw ICAPE2 writer, verifies the signature, and reads
  the personality's own data (PDM PCM/energy; sonar burst + peak).
  `hw/scripts/run_rtos_dfx.xsdb` stages and boots the whole demo.
  Also added: `hw/Makefile` CI targets (validate / ooc-rm / static /
  dfx / rtos, all nonzero-exit) + `hw/scripts/ooc_rm.tcl` per-RM
  utilization budget gate (70% of the X1Y2 pblock).

### Key commands

```bash
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_rtos_dfx.xsdb
cat /dev/ttyUSB1
make -C hw validate ooc-rm     # CI gates
```

## 3. Observations

- Rotation transcript (excerpt): `DFX: RM-PDM live ... sig=9D3A0001 OK`
  → `PDM: pcm=FFFFFFF2 energy=83` → `DFX: RM-SONAR ... 50A40001 OK` →
  `SONAR: peak|t=07FF0000` → `RM-B ... OK` → `RM-A ...` — **all four
  personalities swapped by the RTOS, signatures verified, while
  ADT7420 readings (32.0-32.2 °C) and heartbeat ticks continued
  uninterrupted through every swap.**
- Swap time under FreeRTOS: **16.2 s** vs 390 ms bare-metal — the GPIO
  pacing loop takes tick interrupts and scheduler overhead per word.
  Functional, not yet fast; fixes: taskENTER_CRITICAL around the stream,
  or the planned AXIS-FIFO writer front-end.
- Cached full-range memtest: **PASS 0 errors** again; no wall-clock
  speedup vs the uncached M3 run — expected, a 128 MiB linear walk is
  100% miss on a 16 KB cache. The caches' real work is instruction fetch
  (FreeRTOS executes from DDR at all).

## 4. Data analysis

This closes the loop the project was defined around: a self-contained
instrument whose OS loads and unloads sensor hardware at runtime. The
demo is one xsdb invocation away at any time, and the CI make targets
freeze the gates (pin plans, RM budgets, full builds) for every future
change. The 40x swap-time regression under the RTOS is pure software
pacing — the fabric path is identical — so it is an optimization queue
item, not a design risk.

## 5. Ideas for future experiments

- taskENTER_CRITICAL (or DMA/FIFO writer) around the ICAP stream; target
  sub-second RTOS swaps.
- Personality selection from switches/buttons via the ui task instead of
  fixed rotation; 7-seg shows active RM.
- QSPI slot table so the demo boots without a host (golden image +
  partials in flash).
- Wire the harness: full sensor scan, mic array, echo ranging — the only
  gates this bench session could not reach.

---

# EXPERIMENT 25 Jul 2026 09:30:00 M7: Sonar RM + Full Runtime Swap Demo (Ranging Pending Harness) :complete:

## 1. Hypothesis

Does the ultrasound sonar RM (40 kHz burst TX, 20 MS/s capture window,
peak-hold) load and run via a runtime DFX swap, and does the full
multi-personality demo (hello-A/B, PDM, sonar) survive repeated swapping?

## 2. How

- **Equipment:** Nexys A7-100T. No AD9226/AD8332/piezo harness attached —
  ADC pins float; echo ranging physically impossible this session.
- **Software:** `hw/dfx/rm/rm_sonar.v` (boundary v3: adds sonar_tx pair,
  AFE PWM/hilo, 12-bit adcdac bus, 20 MHz adc clock), 4-config DFX flow,
  loader phase 3.
- **Benchmarks:** scripted sequence B→A→B→PDM(probe)→SONAR(3 bursts)→PDM.

### Key commands

```bash
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_dfx_test.xsdb 1
cat /dev/ttyUSB1   # 115200
```

## 3. Observations

- Sonar RM loaded by the in-fabric ICAPE2 writer in **390 ms**; signature
  0x50A40001 correct.
- **3 bursts fired and counted** (`bursts=...0003`); each capture window
  completed; `peak|t = 07ff0000` — peak magnitude 0x7FF at t=0, i.e. the
  floating ADC bus reads full-scale rail immediately. **Exactly the
  expected signature for an unconnected AD9226** (and incidentally proves
  the plan's OTR-replacement rail-detection idea works).
- PDM→sonar→PDM round trip clean; final verdict `DFX: PASS (all swaps
  ok)`; static UART/heartbeat never glitched across 6 swaps.

## 4. Data analysis

The swap demo — the session-achievable core of M7 — is complete: four
distinct personalities rotate through one partition at runtime under
MicroBlaze control, each self-identifying and functional immediately
after reconfiguration (RESET_AFTER_RECONFIG delivering clean per-load
state, e.g. the burst counter restarting). What remains of M7 is purely
physical: piezo/AFE/ADC harness → real echo, and the M1 derating
assumption (≤40 MS/s through 200 Ω) measured with an eye scan. The
firmware and fabric sides need no further work for that — fire-and-read
registers are already in place.

## 5. Ideas for future experiments

- Wire the ultrasound chain; expect peak_t to move off 0 to the echo
  round-trip time (at 20 MS/s: 58.8 µs/cm of range → verify against tape
  measure).
- Sweep adc_clk 20→40+ MS/s with a live AD9226 to close the derating
  measurement.
- Promote the ICAPE2 writer from GPIO-paced to a small AXI-Stream FIFO
  frontend — projected to cut swap time from 390 ms toward the ~11 ms
  theoretical (1.07 MB @ 100 MHz x32).

---

# EXPERIMENT 25 Jul 2026 09:28:00 M6: PDM Pipeline RM — Live Audio from the On-Board Mic via DFX Swap :complete:

## 1. Hypothesis

Does a real sensor pipeline — PDM capture + CIC decimation — work as a
reconfigurable module, fed by the on-board ADMP421 microphone, with zero
external hardware?

## 2. How

- **Equipment:** Nexys A7-100T; on-board ADMP421 PDM mic (M_CLK J5,
  M_DATA H5, M_LRSEL F5 tied 0 in static).
- **Software:** `hw/dfx/rm/rm_pdm.v`: 2.38 MHz PDM clock, CIC N=4 R=64
  (→ ~37 kHz PCM), leaky DC removal, 1024-sample energy windows; AXI-Lite
  regs (PCM, energy, window count). 344 LUT / 0 BRAM out of context —
  **1.7% of the ~20k-LUT partition, far beyond the ≥30% headroom gate.**
- **Benchmarks:** loader swaps PDM RM in at runtime, then a 200k-read
  register probe.

### Key commands

```bash
# part of the run_dfx_test.xsdb sequence (loader phase 2)
```

## 3. Observations

- Swap-in via ICAPE2 writer: 390 ms, sig 0x9D3A0001.
- Probe: **PCM range [0xFFFFEF1A, 0x00000428] = [-4326, +1064]** — live
  audio (room noise) with DC removed; **energy window peak 3,666,747**;
  window counter advancing. `PDM: PASS (live audio)`.
- Window counter advanced only 2 during the probe (expected ~15-70
  depending on AXI read pacing) — the probe loop's uncached AXI reads
  dominate; not a pipeline fault (PCM variance proves data flow).

## 4. Data analysis

First real sensor pipeline running as a DFX module — the project's core
thesis demonstrated end to end: physical transducer → RM DSP →
AXI registers → CPU, loaded at runtime into a partition that moments
before held a different personality. The on-board-mic-first strategy paid
off exactly as the roadmap intended: zero wiring risk isolated the RTL
bring-up. The 6-mic JA array is now a connector-plugging exercise plus a
6-channel elaboration of this RM (CIC per channel + delay-and-sum).

## 5. Ideas for future experiments

- 6-mic array RM on JXADC; beam steering; tone-localization gate.
- Stream PCM to DDR2 via AXIS/DMA (boundary already reserves the plan's
  M_AXIS) instead of register polling.
- Feed energy telemetry into the FreeRTOS sensor blackboard + 7-seg VU.

---

# EXPERIMENT 25 Jul 2026 09:26:00 M5: MicroBlaze-Driven DFX Swaps at 390 ms — HWICAP Broken, Raw ICAPE2 Writer Works :complete:

## 1. Hypothesis

Can the MicroBlaze load partial bitstreams into the DFX partition at
runtime through the configuration port, with the static region running
uninterrupted — and what does a swap cost?

## 2. How

- **Equipment:** Nexys A7-100T on JTAG (staging only; TAP quiesced during
  ICAP access).
- **Software:** DFX flow (`hw/dfx/build_dfx.tcl`): static synth with
  black-box `rm_slot` stub, 4 RM configs, pblock = explicit site ranges
  for clock region X1Y2 (SLICE_X52Y100:X89Y149 + DSP48_X1Y40:X2Y59,
  RESET_AFTER_RECONFIG + SNAPPING), `pr_verify` gate, partials via
  `write_bitstream -bin_file`. Loader `sw/dfx_load` stages partials in
  DDR2, decouples, streams to the config port, verifies RM signatures,
  measures with axi_timer.
- **Benchmarks:** B→A→B hello-swap sequence + 4-personality rotation.

### Key commands

```bash
~/Vivado/2025.2/Vivado/bin/vivado -mode batch -source hw/dfx/build_dfx.tcl
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_dfx_test.xsdb 1
```

## 3. Observations

- Flow: `pr_verify` PASS across all 4 configs; partials **1,071,052 B**
  each (plan estimated ~1.3 MB for a 2-CR partition; actual pblock = 1 CR).
- **AXI HWICAP v3.0 is non-functional on this design**: every write
  transfer wedges at the first 16-word chunk — CR write bit stuck at 1,
  FIFO drained (WFV=63), DALIGN never set — with dummy-pad words, so
  content-independent. Reproduced identically across: eos_in dangling
  (first bug found — must be tied high, but insufficient), eos_in tied,
  100 MHz sync icap_clk, 20 MHz async icap_clk, JTAG active, JTAG fully
  quiesced, CPU-driven and JTAG-poked register sequences.
- **Workaround that works: raw ICAPE2 writer** — second ICAP site
  instantiated in the top wrapper, GPIO-paced (32-bit word + go-toggle →
  one-cycle CSIB pulse), UG470 bit-reversal in RTL. **Swap = 390,191 µs**
  (1.07 MB, ~1.46 µs/word, 2 AXI writes each).
- Independent validation: JTAG `fpga -partial` swaps A→B→PDM with correct
  signatures — partition/partials/decoupler proven separate from the
  loader path.
- Gate transcript: `sig=5b5b5b5b` after B (counter decrements),
  `sig=a5a5a5a5` after A (increments), static heartbeat printing between
  swaps, `DFX: PASS (all swaps ok)`.

## 4. Data analysis

M5's substance — runtime self-reconfiguration under soft-CPU control with
measured cost — is delivered at **0.39 s/swap**, squarely inside the
plan's 260-430 ms PIO estimate, despite arriving via a different port
than intended. The HWICAP failure is the session's biggest tool finding:
symptom is a complete write-FSM stall, insensitive to every documented
configuration axis we varied; the raw-ICAPE2 fallback both unblocks the
project and isolates the fault to the IP core rather than the device,
clocking, bitstream format, or arbitration. 390 ms is GPIO-pacing bound,
not ICAP bound — a FIFO-fronted writer reaches ~11 ms theoretical.
RESET_AFTER_RECONFIG semantics confirmed by per-load counter resets.

## 5. Ideas for future experiments

- File/repro-case the HWICAP wedge against 2025.2 (minimal design:
  MicroBlaze + HWICAP only); test 2023.x for regression.
- AXIS-FIFO front the ICAPE2 writer: target <20 ms swaps.
- QSPI slot table + CRC per plan §3 (partials currently staged over JTAG).
- Cached-CPU ICAP streaming: measure loader speedup with dcache on DDR
  reads (this run's loader executes uncached from LMB).

---

# EXPERIMENT 25 Jul 2026 08:30:00 M4: FreeRTOS Boots on MicroBlaze — I2C Stack Proven via On-Board ADT7420 :complete:

## 1. Hypothesis

Does FreeRTOS (Vitis 2025.2 SDT flow, freertos10_xilinx BSP) boot on the
classic MicroBlaze and run concurrent tasks — and does the AXI IIC stack
find real sensors (on-board ADT7420 as always-present ground truth; the
external JA bus scanned as-found)?

## 2. How

- **Equipment:** Nexys A7-100T on JTAG. No external sensor harness wired
  yet — the JA bus is expected empty.
- **Software:** Vivado/Vitis 2025.2. BD grew M4 IPs: axi_intc (wired to
  the MicroBlaze interrupt), axi_timer (FreeRTOS tick), axi_iic_0 (JA
  C17/D18), axi_iic_1 (ADT7420 C14/C15); plus M5 statics (HWICAP,
  DFX decoupler, RM AXI port). **Two hardware lessons this milestone:**
  (1) the M2 BD had no instruction path to DDR — MicroBlaze block
  automation with `cache "None"` gives only a data-port AXI, so an ELF
  linked to 0x80000000 left the PC frozen at the entry point; fixed with
  16 KB I/D caches and the MIG behind `(Cached)`. (2) The blanket linker
  edit that moved sections to DDR also moved the vector sections — MB
  vectors must stay in LMB at 0x0; the build script now patches all
  regions to `mig_7series_0_memory_0` EXCEPT `.vectors.*`.
- **Benchmarks:** `sw/tricorder-rtos/src/main.c` (heartbeat task +
  sensors task: full-bus probe scan + ADT7420 13-bit read),
  `sw/scripts/build_rtos.py` (Vitis python: platform from XSA,
  os=freertos, app build, lscript patch).

### Key commands

```bash
~/Vivado/2025.2/Vitis/bin/vitis -s sw/scripts/build_rtos.py
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_elf.xsdb \
    build/static_region.bit build/vitis-ws/tricorder_rtos/build/tricorder_rtos.elf
timeout 60 cat /dev/ttyUSB1
```

## 3. Observations

- Hardware: **WNS +0.810 ns** with caches; ELF 104,848 B (34 KB text) —
  3.2x the 32 KB LMB, confirming DDR execution was mandatory.
- Boot banner, then steady interleave for the full 60 s capture:
  `RTOS: tick N` (heartbeat, LEDs counting) alternating with
  `I2C-EXT: scan: -> 0 device(s)` and **`ADT7420: 31.18–31.50 C`** —
  plausible enclosure temperature, drifting up ~0.3 C as the die warms.
- External scan found **0 devices — correct**: nothing is wired to JA.
  Bus error handling clean (probe NAKs, no hangs).
- FreeRTOS-on-DDR first attempt (no caches) failed silently: PC pinned at
  0x80000000, all registers zero — the debugger made the missing
  instruction port obvious.

## 4. Data analysis

The M4 gate is met in substance: scheduler + two tasks + interrupt-driven
tick prove the FreeRTOS port; the ADT7420 read proves the I2C driver path
end-to-end on real silicon. The external-scan portion of the gate
(finding BME688/SCD-41/VEML7700/AS7341/MAX30105) is **pending the sensor
harness** — the scan loop is demonstrably correct (clean NAK sweep), so
wiring the harness is the only remaining step, no software change
expected. Stability sampled 60 s, not the roadmap's 1 hr; the app can be
left running for the long soak whenever the board is idle.

## 5. Ideas for future experiments

- Wire the JA sensor harness (pull-ups!) and re-run the scan — expected
  addresses 0x76/77, 0x62, 0x10, 0x39, 0x57.
- 1 hr scheduler soak (leave running; check tick monotonicity).
- ADXL362 (SPI) join the sensors task; lwIP/Ethernet when the RMII shim
  lands.
- Re-run cached memtest and record the speedup vs entry 15's uncached run.

---

# EXPERIMENT 24 Jul 2026 23:20:00 M3: DDR2 via MIG — 128 MiB Memtest Clean :complete:

## 1. Hypothesis

Does MIG 7-series in Vivado 2025.2, configured from Digilent's Nexys
A7-100T board-file mig.prj, calibrate the on-board 128 MiB DDR2
(MT47H64M16HR-25E) and pass a full-range software memtest from the
MicroBlaze?

## 2. How

- **Equipment:** Nexys A7-100T on JTAG; DDR2 MT47H64M16HR-25E (16-bit,
  650 MT/s, 3077 ps, 4:1 PHY ratio).
- **Software:** Vivado 2025.2. MIG config fetched from
  github.com/Digilent/vivado-boards `nexys-a7-100t/D.0/1.3/mig.prj`
  (committed as `hw/mig/mig.prj`); BD variant `hw/mig/mig-bd.prj` differs
  only in `SystemClock: Single-Ended -> No Buffer` (+ InputClkFreq 100)
  so the fabric clk_wiz drives sys_clk_i (100 MHz) and clk_ref_i
  (200 MHz, new clk_wiz output) instead of MIG owning the clock pin.
- **Benchmarks:** `sw/memtest` (bare-metal, 1425 B): waits for
  init_calib_complete (GPIO ch2), address-in-address + inverted full-range
  passes, walking-ones — all through the uncached MicroBlaze data port.

### Key commands

```bash
V=~/Vivado/2025.2/Vivado/bin
$V/vivado -mode batch -nolog -nojournal -source hw/scripts/build_static.tcl
$V/xsdb hw/scripts/run_elf.xsdb build/static_region.bit sw/memtest/memtest.elf
timeout 150 cat /dev/ttyUSB1   # 115200 raw
```

## 3. Observations

- Build: **PASS, WNS +1.313 ns**. Connection automation inserted the
  AXI clock converter (`auto_cc`) and an 81.25 MHz ui_clk reset domain
  (`rst_mig_7series_0_81M`) — the plan's "one contained CDC" appeared
  exactly as predicted.
- **Run 1: `init_calib_complete=1`, then `MEMTEST: PASS errors=0
  (128 MiB)`.** Repeat across two further full configuration cycles
  (JTAG reprogram → fresh calibration): **3/3 PASS, 0 errors.** (True
  power-cycles need hands on the board — config-cycle recalibration is
  the proxy used here.)
- Full 4-pass test over the uncached DP takes ~1-2 min — usable but slow;
  caches (M4+) or DMA will be the real data path.
- Digilent's prj carries `XADC_En Enabled`: **the MIG owns the die XADC**
  — the plan's separate xadc_wizard idea is off the table; internal
  telemetry reads go through MIG's temperature path / hw_server.

## 4. Data analysis

The DDR2-vs-DDR3 retarget risk is retired: MIG 7-series handled the DDR2
part natively from Digilent's own configuration with zero hand-edits
beyond the No-Buffer clocking change, and pin constraints came from the
prj (DDR2 pins never touch our XDC, as planned). init_calib_complete
visible to software via GPIO ch2 proved the right observability hook —
memtest gates on it rather than racing calibration. Address map now:
DDR2 128 MiB at 0x80000000, UART 0x40600000, GPIO 0x40000000 — pinned in
the BD so bare-metal programs stay header-free.

## 5. Ideas for future experiments

- M4: AXI IIC x2 + timer + intc into the BD; FreeRTOS via Vitis SDT.
- Enable MicroBlaze caches against DDR2 once FreeRTOS runs from it
  (M4+); measure memtest speedup as a cache sanity check.
- George: when convenient, 3 real power cycles of the board re-running
  memtest closes the letter of the roadmap gate.

---

# EXPERIMENT 24 Jul 2026 22:57:00 M2: MicroBlaze Static Region — Hello over UART (CPU Decision Ratified) :complete:

## 1. Hypothesis

Can a Tcl-scripted block design with a soft CPU, built board-file-free for
xc7a100tcsg324-1 in Vivado 2025.2, print over the Nexys A7's USB-UART —
and which MicroBlaze (classic vs -V) does the FreeRTOS requirement
actually permit?

## 2. How

- **Equipment:** Nexys A7-100T on JTAG (FT2232), /dev/ttyUSB1 = UART.
- **Software:** Vivado 2025.2 (BD project mode, throwaway project under
  build/), xsdb for program+load, mb-gcc from the bundled toolchain
  (`~/Vivado/2025.2/gnu/microblaze/lin/bin`). No Vitis platform/BSP used.
- **Benchmarks:** `hw/bd/static_region.tcl` (BD: clk_wiz 100 MHz, classic
  MicroBlaze via block automation — 32 KB LMB, MDM debug-only, no caches —
  AXI UARTlite 115200, AXI GPIO x4 LED; deterministic addresses UART
  0x40600000, GPIO 0x40000000), `hw/constraints/m2-static-nexys-a7.xdc`,
  `hw/scripts/build_static.tcl`, `hw/scripts/run_elf.xsdb`, `sw/hello/`
  (250-byte bare-metal ELF: own vectors/crt, polled UARTlite, LED counter).

### Key commands

```bash
V=~/Vivado/2025.2/Vivado/bin
$V/vivado -mode batch -nolog -nojournal -source hw/scripts/build_static.tcl
GNU=~/Vivado/2025.2/gnu/microblaze/lin/bin
$GNU/mb-gcc -O2 -Wall -mlittle-endian -nostdlib -nostartfiles -ffreestanding \
    -Wl,-T,link.ld -o hello.elf start.S hello.c   # in sw/hello
$V/xsdb hw/scripts/run_elf.xsdb build/static_region.bit sw/hello/hello.elf
stty -F /dev/ttyUSB1 115200 raw -echo && timeout 3 cat /dev/ttyUSB1
```

## 3. Observations

- **CPU decision evidence:** 2025.2 ships MicroBlaze-V IP
  (`microblaze_riscv_v1_0`) but every bundled FreeRTOS BSP
  (freertos10_xilinx v1_15–v1_18) has ports for **ARM_CA53/CA9/CR5 and
  MicroBlazeV9 only — no RISC-V/MicroBlaze-V port**.
- Build: **PASS, WNS +2.699 ns**, bitstream + XSA produced. One BD
  scripting fix: the MicroBlaze block automation already ties the
  `cpu_resetn` net to `ext_reset_in`; an explicit `connect_bd_net` errors
  with "all ports/pins are already connected".
- Run: `RUN_ELF: PASS`; **"hello tricorder" repeating on /dev/ttyUSB1 at
  115200**; LEDs count.
- Toolchain quirk: user shell's `make` shim broken under the harness
  (`function definition file not found`) — compiled with a direct mb-gcc
  invocation; Makefile kept for normal use.

## 4. Data analysis

**CPU ratified: classic MicroBlaze.** The entry-7 decision was
"MicroBlaze(-V) + FreeRTOS"; with FreeRTOS fixed and no RISC-V port in the
2025.2 embeddedsw tree, classic is the only in-support path (revisit only
if AMD ships a MB-V FreeRTOS port or we write one). The
BSP-free hello deliberately sidesteps the Vitis unified flow for M2 —
one fewer variable while proving BD → bitstream → xsdb → UART; Vitis/BSP
enters at M4 where FreeRTOS actually needs it. Everything is rebuildable
from a clean clone: BD is Tcl (no .bd binary committed), addresses are
pinned so `sw/hello` needs no generated headers.

## 5. Ideas for future experiments

- M3: add MIG 7-series DDR2 (Digilent mig.prj fetched from vivado-boards
  D.0/1.3: MT47H64M16HR-25E, 3077 ps, 4:1) + software memtest.
- M4: Vitis SDT platform from the XSA; FreeRTOS BSP; AXI IIC + intc into
  the BD.
- Add `sw/hello` compile + BD elaboration to the CI make targets.

---

# EXPERIMENT 24 Jul 2026 22:52:30 M1: Nexys A7 Master XDC + Two-Board Validation Gate :complete:

## 1. Hypothesis

Can the docs/plan.md §2 pin plan (40/40 header pins, full sensor
complement, ADC/DAC bus shared) be expressed as a concrete XDC for the
Nexys A7-100T that survives synthesis + placement in Vivado 2025.2 — and
does the old Arty XDC still validate under 2025.2 as a control?

## 2. How

- **Equipment:** none needed (placement-only validation); target
  xc7a100tcsg324-1 for both boards.
- **Software:** Vivado 2025.2. Pin LOCs sourced from the current Digilent
  Nexys-A7-100T-Master.xdc (github.com/Digilent/digilent-xdc) — every plan
  §2 pin re-checked against it before writing; zero discrepancies. (The
  stale local copy in tutorial-chp-fpga with LVDS JXADC lines was NOT
  used, per plan risk table.)
- **Benchmarks:** new `hw/rtl/tricorder_top_nexys.v` (58-port stub:
  18 infra + 40 sensor pins; heartbeat folds all inputs; sw[3] gates the
  shared-bus tristate so both IOBUF directions exist),
  `hw/constraints/tricorder-nexys-a7-100t.xdc`, and rewritten
  `hw/scripts/validate_xdc.tcl` (parameterized, CI-gate exit codes).

### Key commands

```bash
V=~/Vivado/2025.2/Vivado/bin/vivado
$V -mode batch -nolog -nojournal -source hw/scripts/validate_xdc.tcl -tclargs nexys-a7
$V -mode batch -nolog -nojournal -source hw/scripts/validate_xdc.tcl -tclargs arty-a7
```

## 3. Observations

- **nexys-a7: PASS — 58/58 ports constrained and placed, 0 DRC errors,
  0 critical warnings, exit 0.** Both clocks accepted: sys_clk_pin 10.000
  ns on E3, **cam_pclk_pin 41.660 ns on H16** (`IO_L13P_T2_MRCC_15`, the
  board's only P-side clock-capable header pin) — no
  `CLOCK_DEDICATED_ROUTE` waiver needed.
- **arty-a7: PASS — 75/75 ports, 0 DRC errors, exit 0** under 2025.2 —
  the entry-10 result (2019.2/2023.1) reproduces on the new tool.
- Script bug found during bring-up: `set_msg_config -severity {...} -count 1`
  is not valid 2025.2 syntax (`ERROR: [Common 17-170] Unknown option
  '-count'`); correct form for counting is
  `get_msg_config -count -severity {CRITICAL WARNING}`.
- Validator now enforces three gates with nonzero exit: unplaced ports,
  DRC errors parsed from the report, accumulated critical warnings.
  Reports land at `build/validate_<board>_{drc,util,clocks}.rpt`.

## 4. Data analysis

The plan §2 allocation is now toolchain-proven: all 40 header pins place
legally at LVCMOS33, the shared adcdac bus synthesizes with real IOBUFs
(tristate exercised both ways), and the single-P-side-MRCC finding holds
up — cam_pclk on H16 was accepted as a clean dedicated clock route, which
would have failed on any of the N-side CC pins the draft plans proposed.
The Arty control run doubles as the 2025.2 regression check the roadmap
demanded: no silent behavior drift between Vivado generations for this
flow. The validator's exit-code gates make `make validate` (CI story)
a one-liner per board. Port-count arithmetic confirms the plan: 58 Nexys
ports = 75 Arty ports − 2 pup − 2 cam rst/pwdn − 2 SCCB − 1 OTR −
1 afe_enable − 9 DAC (bus shared).

## 5. Ideas for future experiments

- M2: static-region block design (MicroBlaze-V + FreeRTOS decision to
  ratify), Tcl-scripted BD, hello over /dev/ttyUSB1.
- Wire `validate_xdc.tcl` both-board run into a `hw/Makefile` target.
- Add AD9226 `create_clock`/input delays for the system-synchronous
  capture once the derated rate is fixed (plan: measure at M7).
- Consider `tricorder_top` core-module refactor (per-board wrappers) when
  real static-region logic replaces the stubs at M2 — premature while
  both tops are stubs.

---

# EXPERIMENT 24 Jul 2026 22:38:43 M0 Bring-Up: Blinky on the Nexys A7-100T via JTAG :complete:

## 1. Hypothesis

Does the full toolchain-to-board path work — can Vivado 2025.2 (the host's
only Xilinx install) build a bitstream for the attached Nexys A7-100T and
program it over JTAG as user gnn — and is DFX license-free in 2025.2?
(Roadmap milestone M0 from `docs/plan.md`.)

## 2. How

- **Equipment:** Digilent Nexys A7-100T (XC7A100T-1CSG324C) on USB, FT2232
  serial 210292BF705D; Linux 6.17.0-22-generic.
- **Software:** Vivado 2025.2 (`/home/gnn/Vivado/2025.2`), non-project
  batch flow. No cable/udev rules installed (`/etc/udev/rules.d` has no
  Xilinx/Digilent entries).
- **Benchmarks:** new files `hw/bringup/blinky.v` (27-bit counter →
  led[3:0], CPU_RESETN reset), `hw/bringup/blinky-nexys-a7.xdc`,
  `hw/scripts/build_blinky.tcl`, `hw/scripts/program.tcl` (reusable JTAG
  helper, exits nonzero unless DONE=1), `hw/scripts/check_dfx_license.tcl`.

### Key commands

```bash
V=~/Vivado/2025.2/Vivado/bin/vivado
$V -mode batch -nolog -nojournal -source hw/scripts/build_blinky.tcl
$V -mode batch -nolog -nojournal -source hw/scripts/program.tcl -tclargs build/blinky.bit
$V -mode batch -nolog -nojournal -source hw/scripts/check_dfx_license.tcl
```

## 3. Observations

- **udev installer NOT needed:** `/dev/bus/usb/003/011` is root:plugdev
  0660 and gnn is in plugdev — hw_server found the cable as-is:
  `localhost:3121/xilinx_tcf/Digilent/210292BF705DA`, device **xc7a100t_0**.
- Build: **0 errors, 0 warnings, 0 critical warnings; WNS +7.202 ns**
  (all constraints met); 2 LUT / 27 FF; `build/blinky.bit` =
  **3,825,907 bytes** — matching the plan's 3.83 MB full-bitstream figure.
- Program: `PROGRAM: PASS (DONE=1)`; XADC read through the programmed
  device: **die temp 48.6 °C** (plausible idle value).
- DFX check: `HD.RECONFIGURABLE` accepted on a synthesized cell and
  `opt_design` ran — **no license error in 2025.2** for xc7a100t.
- No libtinfo shim needed (2019.2/2023.1-era workaround obsolete on this
  kernel with 2025.2).

## 4. Data analysis

Every M0 gate passed on the first attempt, and the milestone's two
standing risks are retired cheaper than planned: (1) JTAG works without
the sudo udev-installer step — Ubuntu's plugdev default covers the FT2232,
so nothing in the flow requires root; (2) DFX is confirmed license-free in
2025.2, unblocking M5 without a toolchain change. The 3.83 MB bitstream
size empirically anchors the plan's QSPI slot math (16 MiB flash − golden
image ≈ 12 MiB for partials). `program.tcl` is now the standing
program-and-verify helper for all later milestones; the XADC temp read
doubles as proof that post-configuration JTAG register access works, which
M5's swap-time measurements will rely on.

## 5. Ideas for future experiments

- M1: write `hw/constraints/tricorder-nexys-a7-100t.xdc` (plan §2 pin
  table), split per-board top wrappers, parameterize `validate_xdc.tcl`
  with `-tclargs <board>` and nonzero exit on DRC; re-run Arty as control.
- Verify blink visually / count LED period against wall clock (LED0
  ≈ 0.75 Hz) — only gate not machine-checkable.
- M2 prep: confirm MicroBlaze-V + FreeRTOS BSP presence in this exact
  2025.2 install (Vitis SDT flow) before committing the block design.

---

# EXPERIMENT 24 Jul 2026 22:31:09 Board Retarget Plan: Nexys A7-100T (Ultracode Multi-Agent Planning) :complete:

## 1. Hypothesis

**DECISION (George):** the tricorder target board changes from the Arty
A7-100T to the **Digilent Nexys A7** (one is connected to this host). Can
the full sensor complement (entries 5, 8) be replanned onto the Nexys A7's
much smaller header I/O budget, and what else must change?

## 2. How

- **Equipment:** Nexys A7-100T (XC7A100T-1CSG324C — same die/package/speed
  as the Arty) attached via FT2232 (`lsusb` 0403:6010; /dev/ttyUSB0 = JTAG
  side, /dev/ttyUSB1 = UART).
- **Software:** Claude Code ultracode Workflow, 8 agents in 3 phases:
  Recon (Nexys A7 fact sheet from Digilent reference manual + master XDC;
  repo audit of `hw/`; host toolchain audit) → Plan (pin map, architecture,
  roadmap) → Verify (2 adversarial reviewers refuting claims against the
  Digilent master XDC and each other).
- **Benchmarks:** none (planning); pin claims cross-checked against
  `Nexys-A7-100T-Master.xdc` fetched from github.com/Digilent/digilent-xdc.

### Key commands

```bash
git checkout -b gnn-a7-tricorder
lsusb | grep -i ftdi          # FT2232 present -> board attached
# Workflow: 3 recon + 3 plan + 2 verify agents; results reconciled into docs/plan.md
```

## 3. Observations

- Header I/O supply: **40 pins total** (JA–JD = 32 @ 200 Ω series, JXADC =
  8 @ 100 Ω, coupled pairs) vs. Arty sensor demand of **55** — shortfall 15.
- **JB10 (H16, `IO_L13P_T2_MRCC_15`) is the only P-side clock-capable pin
  on any header.** JB2/JB3/JB7/JD10 are CC but N-side (single-ended clocks
  there need `CLOCK_DEDICATED_ROUTE` waivers).
- Host toolchain changed: `/usr/local/cad` no longer holds Xilinx tools
  (now an ACT/async toolchain); the **only install is
  `/home/gnn/Vivado/2025.2`** (Vivado + Vitis). Entry-10's 2023.1 is gone.
- Nexys A7 vs Arty deltas: 128 MB **DDR2** (not DDR3; MIG 7 supports it,
  pins via Digilent MIG UCF import — not in master XDC), **RMII** PHY (MII
  `mii_to_rmii` IP deprecated/gone in 2025.2), no shield header, no I2C
  pull-up enables; gains: on-board PDM mic (ADMP421), ADT7420, ADXL362,
  16 sw/16 LED, 8-digit 7-seg, 12-bit VGA, audio PWM, microSD.
- Adversarial verify caught **9 issues (4 REFUTED)** in the planners' drafts:
  three mutually incompatible pin strategies; "cam_pclk on JB3" (N-side —
  invalid without waiver); a Personality-A packing that put 9 signals on an
  8-pin connector; QSPI slot math (9 slots @ 4 MiB spacing in 12 MiB);
  OV7670 bandwidth understated 2× (18.4 MB/s RGB565, not 9.2); stale local
  Nexys XDC copy with LVDS JXADC lines; CPU choice drifting between
  MicroBlaze classic and -V across sections.

## 4. Data analysis

The reconciled plan (committed as `docs/plan.md`) keeps **every sensing
modality simultaneously wired except the AD9708 DAC**, which time-shares
the AD9226's 13 pins through static-region IOBUFs (aligned with DFX: ADC
and DAC personalities never coexist). The 15-pin shortfall closes via:
SCCB merged onto the sensor I2C bus (−2), camera RST/PWDN strapped (−2),
ADC OTR dropped for fabric rail-detection (−1), AFE enable strapped (−1),
DAC sharing (−9), pull-up-enable ports deleted (−2). Result: **40/40 pins,
zero header spares**; cam_pclk takes the board's single clean clock input
(JB10/H16); SiPM pulse is 4× oversampled at 400 MHz instead of clocked;
AD9226 derated to **≤40 MS/s** behind the 200 Ω series resistors. The
verify phase proved its worth — the planners' three sections could not
have been implemented together; reconciliation rule was "the XDC-grounded
pin plan wins". Toolchain standardizes on Vivado/Vitis 2025.2 (sole
install), which also reopens **MicroBlaze-V** (plan recommends it per the
entry-7 decision, ratify at M2). Roadmap M0–M7 ordered by risk retirement:
blinky/JTAG → XDC + validate gate (both boards, as 2025.2 control) →
static BD + hello → DDR2 MIG → FreeRTOS + I2C scan → DFX hello-swap →
PDM beamformer RM → ultrasound RM + swap demo.

## 5. Ideas for future experiments

- M0: George runs the 2025.2 cable-driver udev installer (sudo), then
  blinky via hw_server; 10-min check that DFX is license-free in 2025.2.
- M1: write `hw/constraints/tricorder-nexys-a7-100t.xdc` + per-board top
  wrappers; parameterize `validate_xdc.tcl` (`-tclargs <board>`, nonzero
  exit on DRC); re-run the Arty XDC under 2025.2 as a control.
- Write the ~100-line MII↔RMII shim RTL (replaces the deprecated IP).
- Measure the real AD9226 max rate through the 200 Ω Pmod at M7; watch
  adc_clk↔pdm_data[5] coupling on the JXADC pair (swap with JA10 if noisy).

---

# EXPERIMENT  5 Jul 2026 05:14:19 Vivado Validation of the Master XDC :complete:

## 1. Hypothesis

Does the entry-9 master XDC survive real toolchain validation — synthesis
and placement for xc7a100tcsg324-1 — with no LOC, port, or clock-placement
errors?

## 2. How

- **Equipment:** this host (no FPGA needed; placement-only validation).
- **Software:** **Vivado 2019.2** found at `/tools/Xilinx/Vivado/2019.2`
  (xc7a100t is a free WebPACK part). Required a `libtinfo.so.5 ->
  libtinfo.so.6` shim on Ubuntu 24.04 (kernel 6.8) — Vivado 2019.2 predates
  the libtinfo6 transition.
- **Benchmarks:** new files `hw/rtl/tricorder_top.v` (port-complete stub
  top, all 43 named ports / 75 pins, inputs folded into a heartbeat so
  nothing is pruned) and `hw/scripts/validate_xdc.tcl`.

### Key commands

```bash
ln -sf /lib/x86_64-linux-gnu/libtinfo.so.6 $SHIM/libtinfo.so.5
LD_LIBRARY_PATH=$SHIM vivado -mode batch -nolog -nojournal \
    -source hw/scripts/validate_xdc.tcl
```

## 3. Observations

- `synth_design`, `opt_design`, `place_design` all completed:
  **75/75 ports constrained and placed, 0 errors, 0 critical warnings,
  1 benign warning; DRC: 0 violations.**
- Both `create_clock` constraints (100 MHz sys, 24 MHz cam_pclk on SRCC pin
  E15) were accepted; clock and input-delay constraints applied cleanly.
- Whole run takes well under a minute on this machine — cheap enough to be
  a CI gate.
- **Vivado 2019.2 version limits discovered:** no MicroBlaze-V (RISC-V soft
  core arrived ~2023.2 — entry 7's plan falls back to classic MicroBlaze
  unless Vivado is upgraded), and partial reconfiguration in 2019.2 needs a
  separate PR license (DFX became free in Vivado ML 2021.1+). **An upgrade
  to a current Vivado (2023.2+) is strongly indicated for the DFX phase.**

## 4. Data analysis

The pin plan is now toolchain-proven, not just paper-checked: placement
accepting all 75 LOCs confirms no package-pin typos, no I/O-bank conflicts
at LVCMOS33, and legal clock routing for cam_pclk. The stub top doubles as
the growing point for the real static region. The significant new risk is
toolchain vintage: staying on 2019.2 means classic MicroBlaze and a PR
license requirement, while upgrading buys MicroBlaze-V + free DFX at the
cost of a multi-GB install — decision needed before the static-region block
design.

*Revision (5 Jul 2026 05:33, after George pointed out 2025.2 is on this
host):* `/usr/local/cad` holds Vivado **2020.2, 2023.1, and 2025.2** (my
first search only covered `/tools` and `/opt`). Findings: **2025.2 has no
Artix-7 device support installed** (`data/parts/xilinx/` lacks `artix7/`;
adding it needs the installer and write access — dir is root:sno, gn262 is
only in snoadmin). **2023.1 has Artix-7 and free DFX**, and the validation
re-run there **PASSES: 75/75 ports, 0 errors, 0 critical warnings** (same
libtinfo.so.5 shim required). Toolchain decision resolved: **use Vivado
2023.1** — classic MicroBlaze (MicroBlaze-V arrived in 2023.2) but nothing
else blocks; optionally have the admin add Artix-7 device files to 2025.2
later for MicroBlaze-V.

## 5. Ideas for future experiments

- Decide: upgrade Vivado (2023.2+ / 2025.x) for free DFX + MicroBlaze-V, or
  verify a PR license exists for 2019.2.
- Wire `validate_xdc.tcl` into CI as the standing pin-plan gate.
- Begin the static-region block design in the chosen Vivado version.

---

# EXPERIMENT  5 Jul 2026 05:08:00 Master XDC for the Full Sensor Complement :complete:

## 1. Hypothesis

Can the full entry-8 pin plan be expressed as a concrete, conflict-free XDC
for the Arty A7-100T using only verified pin locations?

## 2. How

- **Equipment:** Arty A7-100T pinout (xc7a100tcsg324-1).
- **Software:** Digilent `Arty-A7-100-Master.xdc` (Rev. D/E) fetched from
  github.com/Digilent/digilent-xdc as the LOC ground truth; every pin in our
  file copied from it, none from memory.
- **Benchmarks:** duplicate-pin check via grep/uniq.

### Key commands

```bash
curl -sL -O https://raw.githubusercontent.com/Digilent/digilent-xdc/master/Arty-A7-100-Master.xdc
grep -oE 'PACKAGE_PIN [A-V][0-9]+' hw/constraints/tricorder-arty-a7-100t.xdc | sort | uniq -d  # -> empty
```

## 3. Observations

Wrote `hw/constraints/tricorder-arty-a7-100t.xdc`: **75 pin assignments, no
duplicates**. Final allocation (refines entry 8):

| Interface | Pins | Location | Notes |
|---|---|---|---|
| Board infra (clk, UART, 4 btn, 4 sw, 4 LED, RGB0) | 17 | fixed | RGB LED 0 = DFX status |
| I2C bus + pull-up enables | 4 | ck_scl/sda + scl/sda_pup | Arty has on-board pull-up enable pins |
| PDM array (clk + 6 data) | 7 | JA | ja[10] spare |
| OV7670 (ctrl/SCCB + 8 data) | 16 | JB + JC | PCLK on clock-capable E15 (SRCC); input clock + delays constrained, async to sys_clk |
| AD9226 (12 data + OTR + clk) | 14 | ck_io0-13 | OTR (out-of-range) pin gained a home; clk on MRCC P17 |
| AD9708 (8 data + clk) | 9 | JD + ck_io26 | 200 Ω on JD acceptable for DAC data |
| SiPM (pulse + threshold PWM) | 2 | ck_io33/34 | pulse on clock-capable P15 |
| AD8332 ctrl (gain PWM, hi/lo, en) | 3 | ck_io35-37 | RX signal enters via AD9226 |
| Piezo TX pair | 2 | ck_io38/39 | push-pull via external driver |
| **Total** | **74** + CFGBVS/CONFIG_VOLTAGE | | Spares: ck_io27-32, 40/41, ck_a0-a11, SPI (reserved for display), XADC vaux12 |

## 4. Data analysis

The full complement fits with 15+ digital spares remaining, confirming entry
8's budget with real pins. Design choices encoded in the file: camera PCLK
and SiPM pulse sit on clock-capable (SRCC/MRCC) pins; the AD9226 got the
resistor-free ChipKit header including its OTR flag; only static-region pins
are constrained — the DFX boundary is internal AXI and needs no XDC. Timing
honesty: only sys_clk and cam_pclk have clock constraints so far; AD9226
capture is system-synchronous with derating expected over jumpers (input
delays to be added after entry-8's max-rate measurement).

## 5. Ideas for future experiments

- Validate with Vivado: `read_xdc` against an empty top with matching ports
  (catches typos grep cannot).
- Add AD9226 input delays once the derated sample rate is measured.
- Static-region block design (MicroBlaze-V + MIG + EthernetLite + I2C +
  ICAP) consuming this XDC; out-of-context build to fix the DFX pblock.

---

# EXPERIMENT  5 Jul 2026 05:01:07 Board Decision: Arty A7-100T — Pin and Resource Budget :complete:

## 1. Hypothesis

**DECISION (George):** the tricorder target board is the **Digilent Arty
A7-100T**. Does its I/O and fabric budget accommodate the entry-5 sensor set
(all front ends wired simultaneously) plus the entry-7 static region
(MicroBlaze(-V) + FreeRTOS) and a useful DFX partition?

## 2. How

- **Equipment:** Digilent Arty A7-100T: XC7A100T-1CSG324C (63,400 LUT /
  126,800 FF / 135 BRAM36 / 240 DSP48E1), 256 MB DDR3L (16-bit), 100 MHz
  osc, 10/100 Ethernet PHY, USB-UART/JTAG, 4x Pmod (JA/JD standard with
  200 Ω series protection, JB/JC high-speed direct), ChipKit/Arduino shield
  header (~45 digital I/O incl. dedicated I2C, plus XADC analog inputs).
  Figures from the Digilent reference manual, to be re-verified at design
  entry.
- **Software:** desk analysis against notebook entries 5-7.
- **Benchmarks:** none.

### Key commands

```bash
# none run; budget analysis only
```

## 3. Observations

Pin plan — wiring is a **static-region resource**: every physically attached
front end consumes pins permanently, even though only one DFX processing
module is loaded at a time.

| Front end | Pins | Placement |
|---|---|---|
| I2C bus (BME688, SCD-41, VEML7700, AS7341, MAX30105) | 2 | shield SCL/SDA |
| PDM mic array (6 mics, shared clock, L/R paired) | 4-7 | 1 Pmod (JA) |
| OV7670 DVP (8 data, PCLK/HREF/VSYNC/XCLK, SCCB, rst/pwdn) | ~14-16 | JB+JC (high-speed pair, the standard Arty OV7670 hookup) |
| AD9226 ADC (12 data + clk) | 13 | shield header |
| AD9708 DAC (8 data + clk) | 9 | JD + shield spill |
| SiPM via TLV3501 (pulse + PWM threshold) | 2 | shield |
| AD8332 gain ctrl (piezo RX goes through AD9226) | 2-3 | shield |
| **Total** | **~46-52** | vs. 32 Pmod + ~45 shield ≈ **77 available** |

Fabric budget: static region (MicroBlaze + MIG DDR3 ≈ 6k + EthernetLite +
I2C + ICAP/DFX controller + UART/SPI ≈ 12-18k LUT total) leaves roughly
**40k LUT, ~150-200 DSP, ~90-100 BRAM36 for the DFX partition** — larger
than any single candidate sensor pipeline from entry 5.

Constraints noted: JA/JD 200 Ω series resistors cap toggle rates (fine for
PDM/DAC clocking at tens of MHz, marginal for AD9226 at 65 MS/s — hence
AD9226 on the shield header); Ethernet is only 10/100 (U250 streaming link
per entry 6 will bottleneck there); XADC (1 MS/s, 12-bit) is a free extra
"sensor" for battery/AFE monitoring; DFX for 7-series is included in the
free Vivado ML Standard license.

## 4. Data analysis

The Arty A7-100T fits with margin: ~46-52 static pins against ~77 available,
and a DFX partition of roughly two-thirds of the fabric. The deciding
allocation is giving the OV7670 both high-speed Pmods (JB/JC) and routing
the AD9226's 65 MS/s parallel bus to the series-resistor-free shield pins.
Everything can stay wired at once, so "loading a sensor" is purely a DFX
module swap plus enabling the front end — no re-cabling, which is the
demo behavior the project wants. Residual risks: AD9226 signal integrity
over shield jumpers at full 65 MS/s (may need to derate to ~20-40 MS/s),
and the exact shield pin count/XDC assignments need verification against
the Digilent master XDC before committing a constraints file.

## 5. Ideas for future experiments

- Write the master pin-assignment XDC for the full sensor complement and
  check it against Digilent's Arty-A7-100 master XDC.
- Entry-6 CI gate now has a concrete part: `xc7a100tcsg324-1`, partition
  budget ≈ 40k LUT / 90 BRAM / 150 DSP.
- Floorplan the DFX pblock: right-half die, keeping MIG + PHY-adjacent
  banks in the static region.
- Measure AD9226 max reliable sample rate over the shield header.

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

**DECISION (5 Jul 2026 04:58:12, George):** FreeRTOS is sufficient for this
application. Software tier fixed: **MicroBlaze(-V) + FreeRTOS** in the
static region; no Linux requirement, Artix-7 target stands (no Zynq swap).

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
