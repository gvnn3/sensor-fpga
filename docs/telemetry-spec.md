# Tricorder Telemetry Schema — Frozen Specification (schema 1)

Status: **frozen for implementation** — branch `gnn-tricorder-telemetry`,
27 Jul 2026. Extends `docs/ethernet-spec.md` §7.3 from 10 keys to the
**full 75-key fixed-shape frame** covering every sensor in `docs/plan.md`
§2, wired or not.

This document is the single source of truth for the telemetry payload.
`docs/ethernet-spec.md` §7 continues to own the Ethernet/IP/UDP framing
(§7.3 header table, §7.4 checksum); this document replaces §7.3.1
(payload keys) in its entirety and **freezes schema version 1**.

Governing requirement from the project owner:

> When no data is present, send a constant value **outside the sensor's
> valid range** so a consumer can always distinguish "absent" from a real
> reading. Every key is present in every frame — the schema is
> fixed-shape, never sparse.

Nothing here requires floats, dynamic allocation, lwIP, or C11. Target is
the running system: Nexys A7-100T, classic MicroBlaze + FreeRTOS,
Vivado/Vitis 2025.2, standalone QSPI boot, DHCP lease, UDP broadcast to
port 5001 at 1 Hz.

---

## 1. Design rules

### 1.1 Wire format

* Payload is **US-ASCII**, printable characters plus `\n` (0x0A) only.
  **No CR.** No NUL terminator on the wire.
* **One key per line**, `key=value\n`. No spaces anywhere. No comments,
  no blank lines, no trailing separator other than the final `\n`.
* Key charset: `[a-z0-9_]`, first character `[a-z]`, **length ≤ 16**.
* Value charset: `[-0-9]`, **length ≤ 11** (`-2147483648` is the widest
  representable token; §4 shows the actual per-key bound is far tighter).
* **Fixed shape.** Every key in §2 appears in every frame, in the exact
  order of §2, exactly once. A consumer that counts lines gets 75 every
  time, forever, at schema 1. There is no sparse mode, no optional key,
  no conditional block.
* Frame is one UDP datagram, never fragmented (§4).

### 1.2 Integer-only values

Every value is a **decimal signed 32-bit integer** (`s32`). No floats, no
hex, no fixed-point notation, no exponent, no thousands separators, no
leading zeros, no leading `+`. Negative values carry a single leading `-`.

Rationale: `xil_printf` has no float support, MicroBlaze has no FPU in
this build, and the existing `fmt_u32`/`fmt_i32`/`kv_u`/`kv_i` helpers in
`sw/tricorder-rtos/src/net.c` already produce exactly this form. Scaling
is pushed into the **unit suffix** so the integer is always the natural
resolution of the device.

### 1.3 Unit suffixes

The suffix is part of the key name and is **normative**: it fully
determines the scale factor. A consumer never needs a units table beyond
this one.

| Suffix | Unit | Note |
|---|---|---|
| `_cc` | centi-degrees Celsius | 3128 = 31.28 °C |
| `_ppm` | parts per million | |
| `_lux` | lux | integer lux; sub-lux resolution via the companion `_raw` key |
| `_mg` | milli-g | 1004 = 1.004 g |
| `_hz` | hertz | |
| `_us` | microseconds | |
| `_ms` | milliseconds | |
| `_pct` | whole percent | 0..100 |
| `_mv` | millivolts | |
| `_pa` | pascals | absolute pressure |
| `_ohm` | ohms | |
| `_cps` | counts per second | |
| `_mm` | millimetres | |
| `_db` | decibels | integer dB |
| `_bpm` | beats per minute | |
| `_ksps` | kilosamples per second | |
| `_cnt` | dimensionless count | raw ADC counts, event totals |
| `_code` | raw converter LSB code | 0..4095 for the AD9226 |
| `_deg` | degrees of arc | −180..180 |
| `_s` | seconds | |
| *(none)* | dimensionless / enumerated | `seq`, `rm`, `link`, `ip`, `schema`, `i2c`, `pcm`, `energy`, `peak`, `swaps` — frozen legacy or enum keys |

Keys added in future schema revisions **must** carry a suffix from this
table or extend it in a spec revision.

### 1.4 The SENTINEL convention

Three conditions must be distinguishable from data and from each other:

| Condition | Meaning |
|---|---|
| **ABSENT** | The device is not wired, not detected, or has never been probed. There is no data and none is expected. |
| **ERROR** | The device *is* present but the most recent read failed, NAKed, or hit the bounded timeout. Data is expected but missing right now. |
| **N/A** | The value is produced by the reconfigurable partition and the currently loaded DFX personality does not provide it (including "a swap is in progress"). The question is not meaningful in this configuration. |

Exactly **one sentinel literal per condition per value type**. There are
two value types, because a single set of literals cannot be out-of-range
for both:

| Value type | Definition | ABSENT | ERROR | N/A |
|---|---|---|---|---|
| **U** — non-negative quantity | Declared valid range ⊆ `[0, 2^31-1]`. The quantity is physically incapable of being negative (counts, ppm, lux, pascals, percent, milliseconds, hertz, ohms). | **`-1`** | **`-2`** | **`-3`** |
| **S** — signed quantity | Declared valid range straddles zero (temperatures in `_cc`, accelerations in `_mg`, PCM samples, SNR in `_db`, bearings in `_deg`, AFE gain in `_db`). | **`-1000001`** | **`-1000002`** | **`-1000003`** |

Two further types exist and **never carry a sentinel**:

| Value type | Definition |
|---|---|
| **Y** — system | Produced by software that is always running (`seq`, `uptime`, `swaps`, `swap_us`, `ip`, `schema`, `i2c`, `probe_s`, `sens_mask`). It cannot be absent; there is nothing to sentinel. |
| **E** — enumeration with legacy semantics | `rm` only. `rm=-1` already means "swap in progress" and is frozen by `docs/ethernet-spec.md` §7.3.1. `rm` is **exempt** from §1.4 and keeps its existing encoding. |

#### Why these literals are outside every planned sensor's range

**Type U** (`-1`, `-2`, `-3`): trivially outside range — the quantity is
non-negative by construction. A CO₂ concentration, a lux level, a photon
count, a pressure in pascals, a percentage and a frame counter are all
`≥ 0` for every device in `docs/plan.md` §2. Any negative reading is a
sentinel; no ambiguity is possible.

**Type S** (`-1000001`, `-1000002`, `-1000003`): the naive `-1` is
*wrong* here and must not be used. `temp_cc=-1` is a perfectly valid
ADT7420 reading of −0.01 °C, and `acc_x_mg=-1` is a valid ADXL362
reading. The sentinels are therefore placed at `-1000001 … -1000003`,
i.e. **magnitude > 1,000,000**, which exceeds the worst-case magnitude of
every signed quantity on this board by at least 66×:

| Signed quantity | Widest planned range | Max magnitude |
|---|---|---|
| ADT7420 / BME688 / SCD-41 / MAX30105 / ADXL362 / XADC die temperature `_cc` | −40 … +150 °C | 15,000 |
| ADXL362 acceleration `_mg` (±8 g range) | −8,000 … +8,000 mg | 8,000 |
| PDM PCM sample (`pcm`, 16-bit signed) | −32,768 … +32,767 | 32,768 |
| Beam bearing `doa_deg` | −180 … +180 | 180 |
| Sonar SNR `sonar_snr_db` | −20 … +60 dB | 60 |
| AD8332 AFE gain `afe_gain_db` | −5 … +48 dB | 48 |
| **Bound enforced by this spec** | | **< 1,000,000** |

The clamp in §6.4 makes this structural rather than aspirational: any
real value is clamped into its declared range before printing, so a real
reading can never collide with a sentinel even if a driver misbehaves.
The three literals `-1000001 … -1000003` mirror the `-1 … -3` pattern
offset by −1,000,000, so the same "last digit is the reason code" mental
model works for both types.

The `-1000001 … -1000003` literals still fit comfortably in `s32` and
print in 8 characters, which is what §4's size arithmetic assumes.

#### Sentinel precedence (deterministic)

Evaluated in this order; the first match wins:

1. The key's **capability gate** (§3.4) is not satisfied by the loaded
   personality — including `ui_live.cur_rm == -1` (swap in progress) —
   → **N/A** (`-3` / `-1000003`). *(Applies only to F- and C-class keys;
   see §2.1.)*
2. The device's state is `SENS_ERROR` → **ERROR** (`-2` / `-1000002`).
3. The device's state is `SENS_ABSENT` or `SENS_UNKNOWN` → **ABSENT**
   (`-1` / `-1000001`).
4. Otherwise the clamped measured value.

`SENS_UNKNOWN` (never probed yet — only possible in the first ~10 ms
after `sensors_init()`) deliberately maps to ABSENT rather than getting a
fourth literal: to a consumer, "we have not looked yet" and "it is not
there" are the same actionable state, and the boot sweep collapses the
window to well under one telemetry period.

### 1.5 Consumer decoding rule

The complete decoder. `SIGNED_KEYS` is the set of keys whose Type column
in §2 reads **S**.

```python
SENTINEL_REASON = {-1: "absent", -2: "error", -3: "n/a"}

def decode(key, text):
    """-> (value_or_None, reason_or_None)"""
    v = int(text)
    if key in SIGNED_KEYS:
        if -1000003 <= v <= -1000001:
            return None, SENTINEL_REASON[v + 1000000]
    elif -3 <= v <= -1 and key not in EXEMPT_KEYS:   # EXEMPT_KEYS = {"rm"}
        return None, SENTINEL_REASON[v]
    return v, None
```

`ip` decodes as a big-endian IPv4 address:
`socket.inet_ntoa(struct.pack('!I', v & 0xFFFFFFFF))`.

### 1.6 Schema evolution

* `schema=1` is frozen by this document. A consumer **must** read
  `schema` and refuse to interpret a frame whose value it does not know.
* **Append-only.** New keys are added *after* key #75, never inserted,
  never reordered, never renamed, never re-unitised. Adding keys does not
  bump `schema`; a consumer that ignores unknown keys keeps working.
* Removing a key, changing a key's units, changing a sentinel literal, or
  changing the order of keys #1–#75 **requires bumping `schema`** and a
  revision of this document.
* The 10 keys of `docs/ethernet-spec.md` §7.3.1 (`seq` … `peak`) keep
  their names, meanings, and positions #1–#10 unchanged. Two behavioural
  refinements are called out in §2.3 and are the only permitted changes.

### 1.7 Relationship to the event frame

`net.h` also declares `net_send_event(seq, sw, mask)` — an aperiodic
UDP frame emitted on a UI switch change. It is **out of scope** for this
schema and must not be confused with it:

* The event frame is aperiodic and variable-shape; the telemetry frame is
  1 Hz and fixed-shape.
* An event frame **must** carry `schema=` and `seq=` so a consumer can
  tell the two apart from the first line, and **must not** reuse any key
  name from §2 with a different meaning.
* If an event frame carries sensor values, it uses the §1.4 sentinel
  convention unchanged.
* Event frames never advance `tele_seq`; telemetry `seq` stays gap-free
  (gate G6).

### 1.8 The DFX swap frame (`type=dfx`)

A second aperiodic frame, emitted by `net_task` once per attempted
personality swap (`dfx_task` publishes the record and bumps
`ui_live.dfx_seq`; same single-writer pattern as the switch events).
Fixed shape, 12 lines:

```
type=dfx
dfx=4                 swap event sequence, wraps
from=3                RM id loaded before the swap (-1 = none/unknown)
from_name=A           NONE when from=-1
to=0                  RM id requested
to_name=PDM
from_bytes=311452     partial bitstream sizes, bytes (0 = unknown)
to_bytes=409856
swap_us=1040          decouple->signature ICAP stream time
sig=ok                or "mismatch" - the swap failed, cur_rm is -1
swaps=5               lifetime successful-swap counter (same as telemetry)
uptime=62
```

RM ids use the `req_rm` encoding: 0 PDM, 1 SONAR, 2 B, 3 A. Sizes come
from the source actually streamed (QSPI slot manifest, or the DDR staging
length when flash fell back). Mismatched swaps ARE reported — `sig=mismatch`
with the sizes and time of the failed attempt. Like switch events, dfx
frames never advance `tele_seq`.

---

## 2. The key table (frozen, emit order = table order)

### 2.1 Column definitions

**Type** — `U` / `S` / `Y` / `E` per §1.4.

**Class** — determines which sentinel rules apply:

| Class | Meaning | Can be N/A? | Can be ABSENT? | Written by |
|---|---|---|---|---|
| **Y** | System / software | no | no | `net_task` / `ui_task` |
| **S** | Static-region peripheral (AXI IIC, AXI Quad SPI, XADC). Always in the design, independent of the loaded personality. | **no** | yes | `sensors_task` |
| **F** | Fabric measurement produced by the reconfigurable module | yes | yes | `dfx_task` / acq |
| **C** | Commanded/derived state (we know what we asked the hardware to do) | yes | **no** | `dfx_task` / acq |

**Gate** — the capability the loaded personality must advertise (§3.4).
Blank = ungated.

**Status** — `LIVE` (emitted with real data by the current build),
`SW` (hardware is on the board, software driver is the only thing
missing), `HW` (hardware is not wired yet).

### 2.2 Block 0 — the frozen legacy ten (#1–#10)

Positions, names and meanings frozen by `docs/ethernet-spec.md` §7.3.1.

| # | Key | Type | Class | Source | Units | Valid range | Gate | Status |
|---|---|---|---|---|---|---|---|---|
| 1 | `seq` | Y | Y | `tele_seq` in `net.c` | — | 0 … 4294967295, wraps | | LIVE |
| 2 | `uptime` | Y | Y | `xTaskGetTickCount()/configTICK_RATE_HZ` | seconds | 0 … 4294967295 | | LIVE |
| 3 | `temp_cc` | **S** | S | ADT7420 @ I2C 0x4B on `axi_iic_1`, regs 0x00/0x01 (13-bit, 1/16 °C) | centi-°C | −4000 … +15000 | | LIVE |
| 4 | `i2c` | Y | Y | Count of JA-bus devices in state `SENS_PRESENT` (§3) | count | 0 … 8 | | LIVE |
| 5 | `rm` | **E** | Y | `ui_live.cur_rm` | enum | −1 swapping, 0 PDM, 1 SONAR, 2 Hello-B, 3 Hello-A, 4–7 reserved | | LIVE |
| 6 | `swaps` | Y | Y | `ui_live.swaps` | count | 0 … 4294967295 | | LIVE |
| 7 | `swap_us` | Y | Y | `ui_live.last_swap_us` | µs | 0 … 4294967295 (0 = none yet) | | LIVE |
| 8 | `pcm` | **S** | F | PDM RM register `RM_CTRL` @ 0x44A10008 | signed sample | −32768 … +32767 | `CAP_PDM` | LIVE (RM 0) |
| 9 | `energy` | U | F | PDM RM register `RM_AUX` @ 0x44A1000C | count | 0 … 2147483647 | `CAP_PDM` | LIVE (RM 0) |
| 10 | `peak` | U | F | Sonar RM register `RM_AUX` @ 0x44A1000C | count | 0 … 2147483647 | `CAP_SONAR` | LIVE (RM 1) |

### 2.3 Permitted behavioural refinements to the legacy ten

Two changes are required to satisfy the ABSENT/ERROR requirement. Both
preserve name, position, units and meaning; both are the only changes
allowed to keys #1–#10.

1. **`energy` and `peak` move from `kv_u` to `kv_i`.** They are still
   non-negative Type-U quantities with the same meaning; the signed
   emitter is what lets them carry `-1/-2/-3`. Values are clamped to
   `0 … 2147483647` (§6.4), so the printed text is byte-identical to
   today's for every real reading.
2. **`i2c` is computed from the discovery table** (up to 8 known JA
   addresses) instead of a blocking 0x08–0x77 walk performed once per
   second. Meaning — "how many devices answer on the external JA bus" —
   is unchanged, and both definitions give 0 today and 5–6 with the
   planned harness. The exhaustive walk is retained as a **UART-only
   diagnostic every 30 s** (§3.5); its count is published separately as
   `i2c_scan_cnt` (#80, appended) so an unexpected device on the bus is
   still visible. The 112-probe-per-second walk is removed because it is
   precisely the unbounded blocking behaviour the cadence constraint
   forbids.

`temp_cc` gains a sentinel: on ADT7420 read failure it now emits
`-1000002` instead of silently repeating the last good value. Consumers
plotting `temp_cc` must apply §1.5 (a naive consumer sees an obviously
absurd −10000.01 °C spike rather than a plausible-but-stale value, which
is the intended failure mode).

### 2.4 Block 1 — system, appended (#11–#15)

| # | Key | Type | Class | Source | Units | Valid range | Gate | Status |
|---|---|---|---|---|---|---|---|---|
| 11 | `schema` | Y | Y | compile-time constant `TELE_SCHEMA` | — | 1 | | LIVE |
| 12 | `ip` | Y | Y | `ui_live.ip` (DHCP lease, big-endian IPv4 as u32) | — | 0 = no lease, else 1 … 4294967295 | | LIVE |
| 13 | `link` | U | Y | `net_link_up()` (LAN8720A BMSR bit 2) | — | 0 down, 1 up, `-2` MDIO dead | | LIVE |
| 14 | `sens_mask` | Y | Y | `sensors.mask`; bit *i* = `enum sensor_id` *i* is `SENS_PRESENT` | bitmap | 0 … 32767 | | LIVE |
| 15 | `probe_s` | Y | Y | `sensors.probe_s` — seconds since the last complete discovery sweep | seconds | 0 … 65535 | | LIVE |

`link` is the one Type-U system key that can carry a sentinel: `-2` when
`net_init()` never got a PHY ID (MDIO hard failure), because "the link
state is unknown" is materially different from "the link is down".

### 2.5 Block 2 — on-board devices (#16–#27)

All four on-board devices are physically present on the Nexys A7-100T; no
harness is required. `HW` never applies here — only `SW`.

| # | Key | Type | Class | Source | Units | Valid range | Gate | Status |
|---|---|---|---|---|---|---|---|---|
| 16 | `acc_x_mg` | **S** | S | ADXL362 @ `axi_quad_spi_1`, regs 0x0E/0x0F (XDATA_L/H) | milli-g | −8000 … +8000 | | SW |
| 17 | `acc_y_mg` | **S** | S | ADXL362 regs 0x10/0x11 | milli-g | −8000 … +8000 | | SW |
| 18 | `acc_z_mg` | **S** | S | ADXL362 regs 0x12/0x13 | milli-g | −8000 … +8000 | | SW |
| 19 | `acc_mag_mg` | U | S | `isqrt(x²+y²+z²)`, integer only | milli-g | 0 … 13857 | | SW |
| 20 | `acc_temp_cc` | **S** | S | ADXL362 regs 0x14/0x15 (0.065 °C/LSB) | centi-°C | −4000 … +8500 | | SW |
| 21 | `xadc_temp_cc` | **S** | S | XADC DRP 0x00 (die temperature) | centi-°C | −4000 … +12500 | | SW |
| 22 | `xadc_vccint_mv` | U | S | XADC DRP 0x01 | mV | 0 … 3000 | | SW |
| 23 | `xadc_vccaux_mv` | U | S | XADC DRP 0x02 | mV | 0 … 3000 | | SW |
| 24 | `xadc_vbram_mv` | U | S | XADC DRP 0x06 | mV | 0 … 3000 | | SW |
| 25 | `mic_n_cnt` | U | F | PDM RM: number of active PDM channels | count | 0 … 7 | `CAP_PDM` | HW (array) |
| 26 | `mic_spl_db` | U | F | PDM RM: A-weighted SPL estimate | dB SPL | 0 … 130 | `CAP_PDM` | HW |
| 27 | `doa_deg` | **S** | F | PDM RM beamformer: bearing of the dominant source | degrees | −180 … +180 | `CAP_PDM` | HW (array) |

**XADC access note.** `docs/plan.md` §5 deviation 4 records that the MIG
instantiates the XADC primitive (`XADC_En` in the Digilent project), so
there is currently **no software DRP path**. Two options, decided by the
hardware implementer:

* **(A) recommended** — reconfigure MIG for a user-instantiated XADC, put
  an `xadc_wiz` in the block design in DRP-only mode, and drive MIG's
  `device_temp_i` from it. Keys #21–#24 go LIVE.
* **(B)** — leave MIG owning XADC. Keys #21–#24 are permanently
  `SENS_ABSENT` and emit `-1000001` / `-1`. Legal, fixed-shape, honest.

Until (A) lands the build ships (B). No schema change either way — that
is the point of the sentinel convention.

### 2.6 Block 3 — JA I2C sensor harness (#28–#53)

Five breakouts daisy-chained on STEMMA QT into JA1/JA2
(`i2c_scl` C17, `i2c_sda` D18) plus the OV7670's SCCB port on the same
bus. **None are wired today** — every key in this block emits ABSENT.

| # | Key | Type | Class | Source (device @ addr, register) | Units | Valid range | Status |
|---|---|---|---|---|---|---|---|
| 28 | `bme_temp_cc` | **S** | S | BME688 @ 0x76 (alt 0x77), `temp_adc` 0x22–0x24 | centi-°C | −4000 … +8500 | HW |
| 29 | `bme_rh_pct` | U | S | BME688 `hum_adc` 0x25–0x26 | % RH | 0 … 100 | HW |
| 30 | `bme_pres_pa` | U | S | BME688 `press_adc` 0x1F–0x21 | Pa | 30000 … 110000 | HW |
| 31 | `bme_gas_ohm` | U | S | BME688 `gas_r` 0x2C–0x2D + `gas_range` | Ω | 10 … 50000000 | HW |
| 32 | `co2_ppm` | U | S | SCD-41 @ 0x62, `read_measurement` 0xEC05 word 0 | ppm | 0 … 40000 | HW |
| 33 | `scd_temp_cc` | **S** | S | SCD-41 `read_measurement` word 1 | centi-°C | −1000 … +6000 | HW |
| 34 | `scd_rh_pct` | U | S | SCD-41 `read_measurement` word 2 | % RH | 0 … 100 | HW |
| 35 | `light_lux` | U | S | VEML7700 @ 0x10, ALS 0x04, gain/IT-corrected | lux | 0 … 120000 | HW |
| 36 | `light_raw` | U | S | VEML7700 ALS 0x04 raw counts (keeps sub-lux resolution) | count | 0 … 65535 | HW |
| 37 | `as_415_cnt` | U | S | AS7341 @ 0x39, F1 (415 nm) | count | 0 … 65535 | HW |
| 38 | `as_445_cnt` | U | S | AS7341 F2 (445 nm) | count | 0 … 65535 | HW |
| 39 | `as_480_cnt` | U | S | AS7341 F3 (480 nm) | count | 0 … 65535 | HW |
| 40 | `as_515_cnt` | U | S | AS7341 F4 (515 nm) | count | 0 … 65535 | HW |
| 41 | `as_555_cnt` | U | S | AS7341 F5 (555 nm) | count | 0 … 65535 | HW |
| 42 | `as_590_cnt` | U | S | AS7341 F6 (590 nm) | count | 0 … 65535 | HW |
| 43 | `as_630_cnt` | U | S | AS7341 F7 (630 nm) | count | 0 … 65535 | HW |
| 44 | `as_680_cnt` | U | S | AS7341 F8 (680 nm) | count | 0 … 65535 | HW |
| 45 | `as_clear_cnt` | U | S | AS7341 CLEAR channel | count | 0 … 65535 | HW |
| 46 | `as_nir_cnt` | U | S | AS7341 NIR channel | count | 0 … 65535 | HW |
| 47 | `as_flicker_hz` | U | S | AS7341 flicker-detect (FD_STATUS 0xDB) | Hz | 0 unknown, 100, 120 | HW |
| 48 | `ppg_red_cnt` | U | S | MAX30105 @ 0x57, FIFO_DATA 0x07, RED slot (18-bit) | count | 0 … 262143 | HW |
| 49 | `ppg_ir_cnt` | U | S | MAX30105 FIFO_DATA, IR slot | count | 0 … 262143 | HW |
| 50 | `ppg_grn_cnt` | U | S | MAX30105 FIFO_DATA, GREEN slot | count | 0 … 262143 | HW |
| 51 | `hr_bpm` | U | S | derived from the MAX30105 IR channel (peak interval) | bpm | 30 … 220 | HW |
| 52 | `spo2_pct` | U | S | derived from the MAX30105 RED/IR ratio | % | 70 … 100 | HW |
| 53 | `max_temp_cc` | **S** | S | MAX30105 TEMP_INT 0x1F / TEMP_FRAC 0x20 | centi-°C | −4000 … +8500 | HW |

`hr_bpm` and `spo2_pct` are derived: they are `SENS_ERROR` (→ `-2`) while
the MAX30105 is present but no valid pulse has been detected in the last
10 s, and ABSENT (→ `-1`) when the device itself is absent. This is a
meaningful distinction for a medical readout: "sensor missing" vs "sensor
present, finger not on it".

### 2.7 Block 4 — fabric / DFX-provided instruments (#54–#75)

All of these live behind the reconfigurable partition boundary
(`docs/plan.md` §3: S_AXIL + M_AXIS + S_AXIS + bay bundle). **None of the
analog front ends are wired today.**

| # | Key | Type | Class | Source | Units | Valid range | Gate | Status |
|---|---|---|---|---|---|---|---|---|
| 54 | `adc_rate_ksps` | U | C | AD9226 capture rate commanded by the RM (`adc_clk` divider) | kSPS | 0 … 40000 (derated, `plan.md` §2) | `CAP_ADC` | HW |
| 55 | `adc_min_code` | U | F | AD9226 window minimum | LSB code | 0 … 4095 | `CAP_ADC` | HW |
| 56 | `adc_max_code` | U | F | AD9226 window maximum | LSB code | 0 … 4095 | `CAP_ADC` | HW |
| 57 | `adc_rms_code` | U | F | AD9226 window RMS about mid-scale | LSB code | 0 … 4095 | `CAP_ADC` | HW |
| 58 | `adc_rail_cnt` | U | F | Samples at 0x000/0xFFF in the last window (the dropped-OTR replacement, `plan.md` §2) | count | 0 … 2147483647 | `CAP_ADC` | HW |
| 59 | `dac_mode` | U | C | AD9708 waveform mode commanded by the RM | enum | 0 idle, 1 tone, 2 sweep, 3 sonar burst | `CAP_DAC` | HW |
| 60 | `dac_freq_hz` | U | C | AD9708 commanded output frequency | Hz | 0 … 20000000 | `CAP_DAC` | HW |
| 61 | `dac_ampl_pct` | U | C | AD9708 commanded full-scale amplitude | % | 0 … 100 | `CAP_DAC` | HW |
| 62 | `cam_pid` | U | S | OV7670 SCCB @ 0x21: PID (0x0A) in the high byte, VER (0x0B) in the low byte; 0x7673 = 30323 | — | 0 … 65535 | | HW |
| 63 | `cam_frames_cnt` | U | F | Frames written to the DDR2 ring since RM load | count | 0 … 4294967295 | `CAP_CAM` | HW |
| 64 | `cam_fps_hz` | U | F | Measured frame rate | Hz | 0 … 30 | `CAP_CAM` | HW |
| 65 | `cam_lum_pct` | U | F | Mean frame luminance | % | 0 … 100 | `CAP_CAM` | HW |
| 66 | `cam_motion_pct` | U | F | Frame-difference metric | % | 0 … 100 | `CAP_CAM` | HW |
| 67 | `sipm_cps` | U | F | SiPM pulses on `sipm_pulse` (E18) in the last 1 s window | counts/s | 0 … 5000000 | `CAP_SIPM` | HW |
| 68 | `sipm_tot_cnt` | U | F | Total SiPM counts since RM load | count | 0 … 4294967295 | `CAP_SIPM` | HW |
| 69 | `sipm_thresh_mv` | U | C | Comparator threshold commanded on `sipm_thresh_pwm` (G17) | mV | 0 … 3300 | `CAP_SIPM` | HW |
| 70 | `afe_gain_db` | **S** | C | AD8332 gain commanded on `afe_gain_pwm` (D17) | dB | −5 … +48 | `CAP_AFE` | HW |
| 71 | `afe_hilo` | U | C | AD8332 gain-range strap on `afe_hilo` (E17) | enum | 0 low, 1 high | `CAP_AFE` | HW |
| 72 | `sonar_range_mm` | U | F | 40 kHz pulse-echo range | mm | 30 … 4000 | `CAP_SONAR` | HW |
| 73 | `sonar_tof_us` | U | F | Echo time of flight | µs | 0 … 30000 | `CAP_SONAR` | HW |
| 74 | `sonar_snr_db` | **S** | F | Echo peak over noise floor | dB | −20 … +60 | `CAP_SONAR` | HW |
| 75 | `sonar_bursts_cnt` | U | F | Bursts fired since RM load | count | 0 … 4294967295 | `CAP_SONAR` | HW |

Note the class distinction that makes `-1` vs `-3` meaningful here:

* `adc_rate_ksps`, `dac_*`, `sipm_thresh_mv`, `afe_*` are **class C** —
  we always know what we commanded, so they are never ABSENT. With the
  ultrasound personality loaded they report the commanded value even with
  no daughterboard attached; with any other personality they are `-3`.
* `adc_min_code` … `sonar_snr_db` are **class F** — a measurement. With
  the right personality loaded but the front end declared absent (§3.3)
  they are `-1`; with the wrong personality they are `-3`.
* `cam_pid` is **class S**, not F: the camera's identity is read over the
  static-region I2C/SCCB bus and does not depend on any personality. It
  is therefore the camera's discovery hook and is never `-3`.

### 2.8 Reserved / appended (#76+)

Reserved for the next revision, listed so nobody re-uses the names:
`i2c_scan_cnt` (#80, exhaustive-walk device count, §3.5), `tele_drop_cnt`,
`heap_free_cnt`, `task_hwm_cnt`, `ddr_err_cnt`, `flash_crc_cnt`,
`build_cnt`. They are **not** emitted at schema 1 and MUST NOT be added
without a spec revision line here.

---

## 3. Discovery model

### 3.1 Per-sensor state enum

```c
enum sensor_state {
    SENS_UNKNOWN = 0,   /* never probed (only before the boot sweep)   */
    SENS_ABSENT  = 1,   /* probe completed with no ACK / declared off  */
    SENS_PRESENT = 2,   /* probe ACKed AND the ID register matched     */
    SENS_ERROR   = 3    /* was PRESENT; the last data read failed      */
};
```

State machine, evaluated per device per discovery turn:

```
UNKNOWN --probe ACK + ID ok--> PRESENT
UNKNOWN --probe NAK/timeout--> ABSENT
ABSENT  --probe ACK + ID ok--> PRESENT      (hot-plug of the harness)
ABSENT  --probe ACK, ID bad-> ABSENT        (foreign device at that address)
PRESENT --data read fails----> ERROR
ERROR   --data read ok-------> PRESENT
ERROR   --2 consecutive failed probes--> ABSENT
```

Every transition prints one UART line
(`SENS: <name> <old>-><new>`) so a bench session can see the harness come
up. A change in `sensors.mask` also triggers the `Err SEL`-style alert
tone? **No** — telemetry state changes are silent on the audio path; the
tone stays reserved for `docs/manual.md` conditions.

### 3.2 Probeable devices — bounded I2C address probe

Probe table (order = round-robin order). Bus 0 = `axi_iic_0` (JA
harness, `EXT_IIC_BASE`), bus 1 = `axi_iic_1` (on-board ADT7420).

| Slot | Device | Bus | Addr | ID register | Expected |
|---|---|---|---|---|---|
| 0 | ADT7420 | 1 | 0x4B | 0x0B | `0xCB` |
| 1 | VEML7700 | 0 | 0x10 | *(none)* — write ALS_CONF 0x00 = 0x0000, read back | `0x0000` |
| 2 | OV7670 (SCCB) | 0 | 0x21 | 0x0A, 0x0B | `0x76`, `0x73` |
| 3 | AS7341 | 0 | 0x39 | 0x92 | `(id & 0xFC) == 0x24` |
| 4 | MAX30105 | 0 | 0x57 | 0xFF (PART_ID) | `0x15` |
| 5 | SCD-41 | 0 | 0x62 | cmd 0x3682 (`get_serial_number`), 9 bytes | CRC-8 of each word valid |
| 6 | BME688 | 0 | 0x76 | 0xD0 (CHIP_ID) | `0x61` |
| 7 | BME688 (alt) | 0 | 0x77 | 0xD0 | `0x61` |

> These ID values are transcribed from vendor datasheets. The implementer
> **must** confirm each against the datasheet at bring-up and record any
> correction as a notebook entry plus a revision line in this file. The
> two-stage "ACK then ID match" rule exists specifically so a wrong ID
> constant fails safe (device reported ABSENT) rather than reporting
> garbage as a valid reading.

**Bounded timeout.** No I2C call may block the 1 Hz cadence. Every
transaction is wrapped:

```c
#define I2C_XFER_TIMEOUT_US   2000u   /* 2 ms hard ceiling per transaction */
#define I2C_SPIN  (I2C_XFER_TIMEOUT_US * 100u)  /* 100 MHz core, 1 poll/cycle-ish */
```

Sequence for every transaction (raw MMIO, PG090 AXI IIC register map —
verify offsets against the generated `xiic_l.h` at bring-up):

1. **Pre-check**: if `SR & BUS_BUSY` is set, spin up to `I2C_SPIN`
   iterations for it to clear. If it never clears → write `SOFTR = 0x0A`,
   re-enable the core, return `-1` (timeout).
2. Issue the transfer with an explicit iteration bound on **every** wait
   loop (TX FIFO empty, RX FIFO not empty, bus-not-busy). Exceeding the
   bound → `SOFTR` reset + return `-1`.
3. After the transfer, read `ISR`. Bit 1 (transmit error / slave NAK) set
   → return `0` (NAK, device absent). Clear `ISR` write-1-to-clear.
4. Success → return `1`.

The BSP helpers `XIic_Send()`/`XIic_Recv()` may be used **only** wrapped
in step 1's pre-check and step 2's post-failure `SOFTR` reset; the 2 ms
ceiling must be preserved by the pre-check, because those helpers do not
themselves guarantee a bound on a wedged bus. Raw MMIO is preferred and
matches the house style of `net.c` and `main.c`.

Worst-case cost of one probe: 2 ms. This is the only unbounded-in-theory
operation in the poll and it is capped at 0.2% of the telemetry period.

### 3.3 Non-probeable devices — declaration + fabric liveness

The AD9226, AD9708, SiPM, AD8332 and the piezo pair have **no electrical
presence signature** on a digital bus. They are governed by two gates,
both of which must pass for `SENS_PRESENT`:

1. **Operator declaration.** A compile-time bitmap
   `TELE_HARNESS_MASK` (in `sensors.h`, overridable by a `-D` in the
   build) declares which analog front ends are physically attached.
   **Default: `0` — nothing wired.** Bit positions are `enum sensor_id`.
   Nothing can promote a device the operator has not declared.
2. **Fabric liveness heuristic**, evaluated by the RM and published each
   window:

| Device | Liveness heuristic | Rationale |
|---|---|---|
| AD9226 | `adc_min_code == adc_max_code` **and** the value is `0x000` or `0xFFF` for 3 consecutive windows → ABSENT | Exactly the floating-input rail signature observed at M7 (`plan.md` §5) |
| AD9708 | Class C — never ABSENT | We only ever report what we commanded |
| SiPM | `sipm_cps == 0` for 30 consecutive windows with `sipm_thresh_mv > 0` → ABSENT | A biased SiPM always shows dark counts |
| AD8332 | Class C — never ABSENT | Gain is a commanded PWM |
| Piezo pair | RX energy floor below the noise threshold for 10 consecutive bursts → ABSENT | An attached transducer always couples ring-down into the RX path |
| PDM array | `mic_n_cnt` reported by the RM; on-board ADMP421 always counts as channel 0 | The on-board mic proves the path with no harness |

A device that has never been declared is ABSENT permanently — no
heuristic can promote it. A declared device that fails its heuristic
drops to ABSENT and is re-evaluated on every window (no hysteresis limit;
the counters above *are* the hysteresis).

### 3.4 Personality gating (capability bitmap)

Fabric-sourced keys (classes F and C) are gated on what the loaded
personality provides, not on which slot happens to be loaded — so adding
a personality that also drives the ADC does not require touching the
emitter.

```c
#define RM_PDM      0
#define RM_SONAR    1
#define RM_HELLO_B  2
#define RM_HELLO_A  3
#define RM_CAM      4        /* reserved, not yet built */
#define RM_SIPM     5        /* reserved */
#define RM_RF       6        /* reserved */

#define CAP_PDM     0x01u
#define CAP_SONAR   0x02u
#define CAP_ADC     0x04u
#define CAP_DAC     0x08u
#define CAP_CAM     0x10u
#define CAP_SIPM    0x20u
#define CAP_AFE     0x40u

static const u32 rm_caps[8] = {
    [RM_PDM]     = CAP_PDM,
    [RM_SONAR]   = CAP_SONAR | CAP_ADC | CAP_DAC | CAP_AFE,
    [RM_HELLO_B] = 0u,
    [RM_HELLO_A] = 0u,
    [RM_CAM]     = CAP_CAM,
    [RM_SIPM]    = CAP_SIPM,
    [RM_RF]      = CAP_ADC | CAP_DAC,
    [7]          = 0u,
};

/* Gate predicate. cur_rm == -1 (swap in progress) => no capability. */
static int cap_live(u32 cap)
{
    int r = ui_live.cur_rm;
    return (r >= 0 && r < 8) ? ((rm_caps[r] & cap) != 0) : 0;
}
```

Consequence for the current build (Hello-A boots, `rm=3`): all 22 keys of
block 4 plus `pcm`, `energy`, `peak`, `mic_*`, `doa_deg` report N/A —
except `cam_pid`, which is class S and reports ABSENT. That is the
intended, informative distinction.

### 3.5 Schedule

| Event | Action | Worst-case cost |
|---|---|---|
| `sensors_init()` (once, before the scheduler's first telemetry frame) | `SOFTR` both IIC cores; one **full sweep** of the 8-slot probe table; `mask` and `probe_s` published | 8 × 2 ms = **16 ms** |
| `sensors_poll()` (1 Hz, from `sensors_task`) | Read every device in `SENS_PRESENT`; probe **exactly one** table slot (round-robin cursor `next_probe`) | **≤ 20 ms** budget, ~11 ms measured worst case (§3.6) |
| Cursor wraps (every 8 polls) | Re-publish `mask`; reset `probe_s` to 0 | — |
| `mask` changes | Print `SENS: mask %08x` and force a full sweep across the next 8 polls (cursor unchanged, one slot per poll) | — |
| Every 30 s | Exhaustive 0x08–0x77 walk on bus 0, **UART diagnostic only**, count published as `i2c_scan_cnt` (#80, reserved) | 112 × 2 ms = 224 ms **worst case**; runs in `sensors_task` at priority 2, below `dfx_task` (3), so a DFX swap preempts it |

Effective re-probe interval **N = 8 seconds** (one slot per 1 Hz poll,
8 slots). This satisfies "re-probe every N seconds" while keeping any
single poll's probe cost at one bounded 2 ms transaction.

`probe_s` in the frame is `(now - last_wrap) / configTICK_RATE_HZ`, so a
consumer can tell how stale the presence information is.

### 3.6 Timing budget for `sensors_poll()`

Bus 0 at 100 kHz standard mode; bus 1 likewise. ~90 µs per byte including
address and ACK overhead.

| Step | Bytes | Cost |
|---|---|---|
| ADT7420 temperature (bus 1) | 3 | 0.4 ms |
| ADXL362 burst read (SPI, 5 MHz) | 8 | 0.02 ms |
| XADC DRP reads ×4 (AXI) | — | < 0.01 ms |
| BME688 burst | 8 | 1.0 ms |
| SCD-41 `read_measurement` | 9 | 1.1 ms |
| VEML7700 ALS | 4 | 0.6 ms |
| AS7341 10 channels + status | 26 | 2.7 ms |
| MAX30105 FIFO + temp | 12 | 1.4 ms |
| OV7670 SCCB PID/VER | 4 | 0.6 ms |
| One round-robin probe | — | ≤ 2.0 ms |
| **Total, all devices present** | | **≈ 9.8 ms + 2.0 ms = 11.8 ms** |
| **Hard budget** | | **20 ms** |

1.2% of the 1 Hz period with the full harness attached; ~2.5 ms today.
`sensors_task` runs at `tskIDLE_PRIORITY + 2`, below `dfx_task`
(`+3`), so a personality swap always preempts a sensor poll. `net_task`
(`+1`) never calls into the sensor module — it reads the blackboard only
(§6.5), so the telemetry cadence is structurally independent of I2C.

**Non-negotiable rule:** if any implementation of `sensors_poll()` can
exceed 20 ms, it is wrong. Split it across polls instead — the schema
tolerates a stale value (it is a latest-value blackboard), it does not
tolerate a missed telemetry frame.

---

## 4. Frame size arithmetic

### 4.1 Line cost

`len(line) = len(key) + 1 ('=') + len(value) + 1 ('\n')`.

### 4.2 Worst-case value widths

The clamp in §6.4 means a printed value is either a sentinel or a number
inside the declared valid range. So per key,
`W = max(width(range_min), width(range_max), width(sentinel))`, with
sentinel width 2 for Type U (`-1`), 8 for Type S (`-1000001`).

| Block | Keys | Σ key-name chars | Σ separators (2/key) | Σ worst-case value chars | Block total |
|---|---|---|---|---|---|
| 0 — legacy ten | 10 | 46 | 20 | 80 | **146** |
| 1 — system appended | 5 | 28 | 10 | 24 | **62** |
| 2 — on-board | 12 | 124 | 24 | 70 | **218** |
| 3 — JA I2C | 26 | 261 | 52 | 137 | **450** |
| 4 — fabric | 22 | 255 | 44 | 121 | **420** |
| **Total** | **75** | **714** | **150** | **432** | **1296** |

Worked examples of the value-width column: `seq` and `swaps` are free
u32 counters → 10 each; `temp_cc` is Type S with range ±15000 (5 chars)
but sentinel `-1000001` (8 chars) → 8; `bme_rh_pct` is Type U, range
0…100 (3 chars), sentinel `-1` (2 chars) → 3; `as_*_cnt` are Type U,
0…65535 → 5 each × 10 = 50.

### 4.3 Budget

```
worst-case payload                    1296 bytes
budget (TELE_PAYLOAD_MAX)             1400 bytes      headroom 104 bytes
UDP header                               8
IPv4 header                             20
Ethernet header                         14
------------------------------------------------
worst-case frame on the wire          1338 bytes
Ethernet MTU frame (no FCS)           1514 bytes      headroom 176 bytes
```

**Never fragmented**, always a single datagram, at any sensor
configuration. The `txf[1536]` staging array in `net.c` is already large
enough: `42 + 1400 = 1442 ≤ 1536`. `emac_send()`'s `ETH_MAX = 1514` cap
is never reached, so its silent truncation path cannot fire.

### 4.4 Guards (both mandatory)

**Compile time** — C99-compatible static assertion on the sum of the
declared key-name lengths and worst-case widths:

```c
/* sum computed by the key table in §4.2; update both together */
#define TELE_KEYS              75
#define TELE_WORST_PAYLOAD   1296
#define TELE_PAYLOAD_MAX     1400
typedef char tele_size_check[(TELE_WORST_PAYLOAD <= TELE_PAYLOAD_MAX) ? 1 : -1];
```

**Run time** — `net_send_telemetry()` checks the assembled length before
handing the frame to `emac_send()`:

```c
payload_len = (u32)(p - (char *)&txf[42]);
if (payload_len > TELE_PAYLOAD_MAX) {
    /* Truncate at the last complete line; never emit a partial line. */
    while (payload_len && txf[42 + payload_len - 1] != '\n')
        payload_len--;
    net_stats.tele_overflow++;
    xil_printf("NET: telemetry overflow, truncated to %u\r\n",
               (unsigned)payload_len);
}
```

A truncated frame is a **bug**, not a mode. `tele_overflow` must remain 0
in every soak test; a nonzero value fails the gate.

---

## 5. Worked example frame

State: Hello-A personality loaded (`rm=3`), DHCP lease 192.168.1.126,
link up, ADT7420 live, **no JA harness and no analog front ends wired**.
As built (deviations D1), the ADXL362 path and the XADC are ABSENT —
their keys carry sentinels. `sens_mask=16385` = bit 0 + bit 14 =
ADT7420 + PDM (the on-board microphone is soldered down and always
declared).

```
seq=1234
uptime=3671
temp_cc=3128
i2c=0
rm=3
swaps=7
swap_us=1042
pcm=-1000003
energy=-3
peak=-3
schema=1
ip=3232235902
link=1
sens_mask=16385
probe_s=2
acc_x_mg=-1000001
acc_y_mg=-1000001
acc_z_mg=-1000001
acc_mag_mg=-1
acc_temp_cc=-1000001
xadc_temp_cc=-1000001
xadc_vccint_mv=-1
xadc_vccaux_mv=-1
xadc_vbram_mv=-1
mic_n_cnt=-3
mic_spl_db=-3
doa_deg=-1000003
bme_temp_cc=-1000001
bme_rh_pct=-1
bme_pres_pa=-1
bme_gas_ohm=-1
co2_ppm=-1
scd_temp_cc=-1000001
scd_rh_pct=-1
light_lux=-1
light_raw=-1
as_415_cnt=-1
as_445_cnt=-1
as_480_cnt=-1
as_515_cnt=-1
as_555_cnt=-1
as_590_cnt=-1
as_630_cnt=-1
as_680_cnt=-1
as_clear_cnt=-1
as_nir_cnt=-1
as_flicker_hz=-1
ppg_red_cnt=-1
ppg_ir_cnt=-1
ppg_grn_cnt=-1
hr_bpm=-1
spo2_pct=-1
max_temp_cc=-1000001
adc_rate_ksps=-3
adc_min_code=-3
adc_max_code=-3
adc_rms_code=-3
adc_rail_cnt=-3
dac_mode=-3
dac_freq_hz=-3
dac_ampl_pct=-3
cam_pid=-1
cam_frames_cnt=-3
cam_fps_hz=-3
cam_lum_pct=-3
cam_motion_pct=-3
sipm_cps=-3
sipm_tot_cnt=-3
sipm_thresh_mv=-3
afe_gain_db=-1000003
afe_hilo=-3
sonar_range_mm=-3
sonar_tof_us=-3
sonar_snr_db=-1000003
sonar_bursts_cnt=-3
```

75 lines. Size: 714 (names) + 150 (separators) + 214 (values) =
**1078 bytes** payload, **1120 bytes** on the wire. Well inside §4.3.

Reading the sentinels in this frame:

* `bme_rh_pct=-1` — Type U ABSENT. The BME688 is not on the bus.
* `bme_temp_cc=-1000001` — same device, Type S ABSENT. Note it is *not*
  `-1`, which would have been a valid −0.01 °C.
* `energy=-3` — Type U N/A. The PDM personality is not loaded; the value
  is not merely missing, it is not defined in this configuration.
* `pcm=-1000003` — same personality gate, Type S.
* `cam_pid=-1` — class S: the camera's SCCB address 0x21 did not ACK. If
  the camera were plugged in, this would read `30323` (0x7673) *even with
  Hello-A loaded*, while `cam_fps_hz` would stay `-3` until the camera
  personality is swapped in. That is the whole point of separating the
  identity probe from the measurement.
* `afe_gain_db=-1000003` and `afe_hilo=-3` — class C, gated on
  `CAP_AFE`. With the sonar personality loaded these would report the
  commanded gain and range, wired or not, because we know what we
  commanded.

Same board with the sonar personality selected (SW1) and the harness
attached would show, among the others: `rm=1`, `peak=418`,
`sonar_range_mm=1240`, `sonar_tof_us=7235`, `sonar_snr_db=18`,
`adc_min_code=1918`, `adc_rail_cnt=0`, `afe_gain_db=24`, `afe_hilo=1`,
while every `mic_*` and `cam_*` key would move to `-3`.

Verification one-liner for a consumer on the LAN:

```
nc -ul 5001 | awk -F= 'NF==2{n++} /^seq=/{if(n)print "keys:",n; n=1}'
```

must print `keys: 75` for every frame, forever.

---

## 6. C API for implementers

Three files change. Nothing else may touch the sensor blackboard.

| File | Change |
|---|---|
| `sw/tricorder-rtos/src/sensors.h` | **new** — §6.1 |
| `sw/tricorder-rtos/src/sensors.c` | **new** — §6.2, §6.3 |
| `sw/tricorder-rtos/src/net.c` | §6.4, §6.5 — emitter rewrite of `net_send_telemetry()` only |
| `sw/tricorder-rtos/src/main.c` | `sensors_task` body replaced by `sensors_init()` + 1 Hz `sensors_poll()` |
| `sw/tricorder-rtos/src/ui.h` | unchanged — `struct ui_live` keeps its meaning and its UI consumers |

`struct ui_live` is **not** extended. It exists for the 7-seg UI; the
telemetry blackboard is a separate structure so the two concerns do not
grow into each other. `net_send_telemetry()` reads both.

### 6.1 `sensors.h`

```c
/*
 * Tricorder sensor blackboard — spec docs/telemetry-spec.md §6.
 *
 * Latest-value blackboard, no snapshot semantics. Every field is a
 * naturally aligned 32-bit word written by exactly one task and read by
 * net_task; MicroBlaze aligned word loads/stores are atomic, so no lock
 * is required and no field can tear. Fields may be skewed relative to
 * each other by up to one poll period (1 s). That is accepted.
 */
#ifndef SENSORS_H
#define SENSORS_H

#include "xil_types.h"

#define TELE_SCHEMA        1
#define TELE_KEYS          75
#define TELE_PAYLOAD_MAX   1400
#define TELE_WORST_PAYLOAD 1296

enum sensor_state {
    SENS_UNKNOWN = 0,
    SENS_ABSENT  = 1,
    SENS_PRESENT = 2,
    SENS_ERROR   = 3
};

/* Bit positions in sensors.mask and in the sens_mask telemetry key. */
enum sensor_id {
    SENS_ADT7420 = 0,   /* on-board, I2C bus 1 @ 0x4B                  */
    SENS_ADXL362,       /* on-board, axi_quad_spi_1                    */
    SENS_XADC,          /* on-board, DRP                               */
    SENS_BME688,        /* JA I2C 0x76 / 0x77                          */
    SENS_SCD41,         /* JA I2C 0x62                                 */
    SENS_VEML7700,      /* JA I2C 0x10                                 */
    SENS_AS7341,        /* JA I2C 0x39                                 */
    SENS_MAX30105,      /* JA I2C 0x57                                 */
    SENS_OV7670,        /* JA I2C (SCCB) 0x21                          */
    SENS_AD9226,        /* non-probeable, declared + liveness          */
    SENS_AD9708,        /* non-probeable, class C only                 */
    SENS_SIPM,          /* non-probeable, declared + liveness          */
    SENS_AD8332,        /* non-probeable, class C only                 */
    SENS_PIEZO,         /* non-probeable, declared + liveness          */
    SENS_PDM,           /* on-board ADMP421 + optional 6-mic array     */
    SENS_COUNT
};

/* Operator declaration of attached analog front ends (spec §3.3).
 * Default: nothing wired. Override with -DTELE_HARNESS_MASK=0x... */
#ifndef TELE_HARNESS_MASK
#define TELE_HARNESS_MASK  0u
#endif

struct sensor_state {
    /* ---- discovery ------------------------------------------------ */
    volatile u32 st[SENS_COUNT];  /* enum sensor_state, one word each  */
    volatile u32 mask;            /* bit i set iff st[i] == SENS_PRESENT */
    volatile u32 probe_s;         /* seconds since the last full sweep */
    volatile u32 next_probe;      /* round-robin cursor, 0..7          */

    /* ---- on-board (written by sensors_task) ----------------------- */
    volatile s32 adt_temp_cc;
    volatile s32 acc_x_mg, acc_y_mg, acc_z_mg, acc_mag_mg, acc_temp_cc;
    volatile s32 xadc_temp_cc, xadc_vccint_mv, xadc_vccaux_mv, xadc_vbram_mv;

    /* ---- JA I2C (written by sensors_task) ------------------------- */
    volatile s32 bme_temp_cc, bme_rh_pct, bme_pres_pa, bme_gas_ohm;
    volatile s32 co2_ppm, scd_temp_cc, scd_rh_pct;
    volatile s32 light_lux, light_raw;
    volatile s32 as_cnt[10];      /* 415,445,480,515,555,590,630,680,clear,nir */
    volatile s32 as_flicker_hz;
    volatile s32 ppg_red_cnt, ppg_ir_cnt, ppg_grn_cnt;
    volatile s32 hr_bpm, spo2_pct, max_temp_cc;
    volatile s32 cam_pid;         /* (PID << 8) | VER, class S         */

    /* ---- fabric / RM (written by dfx_task or the acq path) -------- */
    volatile s32 mic_n_cnt, mic_spl_db, doa_deg;
    volatile s32 adc_rate_ksps, adc_min_code, adc_max_code;
    volatile s32 adc_rms_code, adc_rail_cnt;
    volatile s32 dac_mode, dac_freq_hz, dac_ampl_pct;
    volatile s32 cam_frames_cnt, cam_fps_hz, cam_lum_pct, cam_motion_pct;
    volatile s32 sipm_cps, sipm_tot_cnt, sipm_thresh_mv;
    volatile s32 afe_gain_db, afe_hilo;
    volatile s32 sonar_range_mm, sonar_tof_us, sonar_snr_db, sonar_bursts_cnt;
};

extern struct sensor_state sensors;

/* Zero the blackboard, set every st[] to SENS_UNKNOWN, soft-reset both
 * AXI IIC cores, and run one full bounded probe sweep (spec §3.5).
 * Blocks for <= 20 ms. Call once from sensors_task before the loop, and
 * before net_task can emit its first frame. */
void sensors_init(void);

/* One 1 Hz service pass: read every SENS_PRESENT device, probe exactly
 * one round-robin table slot, republish mask/probe_s. MUST return in
 * <= 20 ms (spec §3.6). Never called from net_task. */
void sensors_poll(void);

/* Fabric publishers, called by dfx_task / the acq path after a window.
 * Plain word stores into the blackboard; no locking. */
void sensors_publish_pdm(s32 n_ch, s32 spl_db, s32 doa_deg);
void sensors_publish_adc(s32 min_code, s32 max_code, s32 rms_code, s32 rail_cnt);
void sensors_publish_sonar(s32 range_mm, s32 tof_us, s32 snr_db, s32 bursts);
void sensors_publish_sipm(s32 cps, s32 total);
void sensors_publish_cam(s32 frames, s32 fps_hz, s32 lum_pct, s32 motion_pct);

/* Commanded state (class C) — call when the command is issued. */
void sensors_command_dac(s32 mode, s32 freq_hz, s32 ampl_pct);
void sensors_command_afe(s32 gain_db, s32 hilo);
void sensors_command_sipm(s32 thresh_mv);
void sensors_command_adc(s32 rate_ksps);

#endif /* SENSORS_H */
```

### 6.2 `sensors.c` — required internals

```c
/* Bounded I2C primitives (spec §3.2). base = EXT_IIC_BASE or TMP_IIC_BASE. */
static int  i2c_probe(u32 base, u8 addr);                 /*  1 ACK, 0 NAK, -1 timeout */
static int  i2c_wr(u32 base, u8 addr, const u8 *tx, u32 n);        /* 0 ok, -1 fail */
static int  i2c_rd(u32 base, u8 addr, u8 reg, u8 *rx, u32 n);      /* 0 ok, -1 fail */
static void i2c_recover(u32 base);                        /* SOFTR + re-enable */

/* State machine helper: apply one probe/read outcome to st[id] and mask,
 * printing the transition. rc: 1 present, 0 absent, -1 error. */
static void sens_set(enum sensor_id id, int rc);

/* Per-device readers. Each returns 0 on success, -1 on failure, and
 * writes its blackboard fields only on success. */
static int rd_adt7420(void);
static int rd_adxl362(void);
static int rd_xadc(void);
static int rd_bme688(void);
static int rd_scd41(void);
static int rd_veml7700(void);
static int rd_as7341(void);
static int rd_max30105(void);
static int rd_ov7670_id(void);

/* Integer square root for acc_mag_mg — no libm, no floats. */
static u32 isqrt32(u32 v);
```

`sensors_poll()` skeleton (this control flow is normative):

```c
void sensors_poll(void)
{
    /* 1. Read every device we believe is present. A failure demotes it
     *    to SENS_ERROR; its next round-robin turn re-probes it.        */
    if (sensors.st[SENS_ADT7420] == SENS_PRESENT)
        sens_set(SENS_ADT7420, rd_adt7420() == 0 ? 1 : -1);
    /* ... one line per device, same shape ...                          */

    /* 2. Exactly one bounded probe, round robin.                       */
    probe_slot(sensors.next_probe);
    if (++sensors.next_probe >= PROBE_SLOTS) {
        sensors.next_probe = 0;
        sensors.probe_s = 0;              /* sweep complete             */
    }

    /* 3. Republish derived state.                                      */
    sensors.mask = compute_mask();
    ui_live.i2c_devices = popcount(sensors.mask & JA_BUS_MASK);
}
```

`sensors_task` in `main.c` becomes:

```c
static void sensors_task(void *arg)
{
    (void)arg;
    sensors_init();
    for (;;) {
        sensors_poll();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 6.3 Sentinel literals (shared header, `sensors.h` or `tele.h`)

```c
#define TS_ABSENT_U   (-1)
#define TS_ERROR_U    (-2)
#define TS_NA_U       (-3)
#define TS_ABSENT_S   (-1000001)
#define TS_ERROR_S    (-1000002)
#define TS_NA_S       (-1000003)
```

### 6.4 Emitter helpers in `net.c`

```c
static s32 clamp_s32(s32 v, s32 lo, s32 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Class S (static peripheral), Type U. */
static s32 su(enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    switch (sensors.st[id]) {
    case SENS_PRESENT: return clamp_s32(v, lo, hi);
    case SENS_ERROR:   return TS_ERROR_U;
    default:           return TS_ABSENT_U;      /* ABSENT or UNKNOWN */
    }
}

/* Class S, Type S. */
static s32 ss(enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    switch (sensors.st[id]) {
    case SENS_PRESENT: return clamp_s32(v, lo, hi);
    case SENS_ERROR:   return TS_ERROR_S;
    default:           return TS_ABSENT_S;
    }
}

/* Class F (fabric measurement), Type U / Type S — gate first (§1.4). */
static s32 fu(u32 cap, enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    if (!cap_live(cap)) return TS_NA_U;
    return su(id, v, lo, hi);
}
static s32 fs(u32 cap, enum sensor_id id, s32 v, s32 lo, s32 hi)
{
    if (!cap_live(cap)) return TS_NA_S;
    return ss(id, v, lo, hi);
}

/* Class C (commanded) — gated, never ABSENT. */
static s32 cu(u32 cap, s32 v, s32 lo, s32 hi)
{
    return cap_live(cap) ? clamp_s32(v, lo, hi) : TS_NA_U;
}
static s32 cs(u32 cap, s32 v, s32 lo, s32 hi)
{
    return cap_live(cap) ? clamp_s32(v, lo, hi) : TS_NA_S;
}
```

**Clamp before sentinel selection, never after.** A sentinel is out of
range by construction; clamping it would destroy it. The helpers above
have the order right: `clamp_s32` is applied only on the `SENS_PRESENT`
path.

### 6.5 `net_send_telemetry()`

Structure is unchanged from `docs/ethernet-spec.md` §7.3 (same Ethernet /
IPv4 / UDP header construction, same `inet_csum`, same `emac_send`); only
the payload block changes. `kv_u` and `kv_i` are unchanged. All keys that
can carry a sentinel use `kv_i`.

```c
void net_send_telemetry(void)
{
    char *p = (char *)&txf[42];
    u32 payload_len, iplen, udplen;
    u16 csum;

    /* ---- Block 0: frozen legacy ten (spec §2.2) -------------------- */
    p = kv_u(p, "seq",     tele_seq);
    p = kv_u(p, "uptime",  (u32)(xTaskGetTickCount() / configTICK_RATE_HZ));
    p = kv_i(p, "temp_cc", ss(SENS_ADT7420, sensors.adt_temp_cc, -4000, 15000));
    p = kv_i(p, "i2c",     (s32)ui_live.i2c_devices);
    p = kv_i(p, "rm",      (s32)ui_live.cur_rm);          /* type E, exempt */
    p = kv_u(p, "swaps",   ui_live.swaps);
    p = kv_u(p, "swap_us", ui_live.last_swap_us);
    p = kv_i(p, "pcm",     fs(CAP_PDM,   SENS_PDM,   ui_live.pcm, -32768, 32767));
    p = kv_i(p, "energy",  fu(CAP_PDM,   SENS_PDM,   (s32)ui_live.energy, 0, 2147483647));
    p = kv_i(p, "peak",    fu(CAP_SONAR, SENS_PIEZO, (s32)ui_live.peak,   0, 2147483647));

    /* ---- Block 1: system (spec §2.4) ------------------------------- */
    p = kv_u(p, "schema",    TELE_SCHEMA);
    p = kv_u(p, "ip",        ui_live.ip);
    p = kv_i(p, "link",      link_up_cached < 0 ? TS_ERROR_U : link_up_cached);
    p = kv_u(p, "sens_mask", sensors.mask);
    p = kv_u(p, "probe_s",   sensors.probe_s);

    /* ---- Block 2: on-board (spec §2.5) ----------------------------- */
    p = kv_i(p, "acc_x_mg",       ss(SENS_ADXL362, sensors.acc_x_mg,   -8000, 8000));
    p = kv_i(p, "acc_y_mg",       ss(SENS_ADXL362, sensors.acc_y_mg,   -8000, 8000));
    p = kv_i(p, "acc_z_mg",       ss(SENS_ADXL362, sensors.acc_z_mg,   -8000, 8000));
    p = kv_i(p, "acc_mag_mg",     su(SENS_ADXL362, sensors.acc_mag_mg,     0, 13857));
    p = kv_i(p, "acc_temp_cc",    ss(SENS_ADXL362, sensors.acc_temp_cc, -4000, 8500));
    p = kv_i(p, "xadc_temp_cc",   ss(SENS_XADC, sensors.xadc_temp_cc,  -4000, 12500));
    p = kv_i(p, "xadc_vccint_mv", su(SENS_XADC, sensors.xadc_vccint_mv,    0, 3000));
    p = kv_i(p, "xadc_vccaux_mv", su(SENS_XADC, sensors.xadc_vccaux_mv,    0, 3000));
    p = kv_i(p, "xadc_vbram_mv",  su(SENS_XADC, sensors.xadc_vbram_mv,     0, 3000));
    p = kv_i(p, "mic_n_cnt",      fu(CAP_PDM, SENS_PDM, sensors.mic_n_cnt,   0, 7));
    p = kv_i(p, "mic_spl_db",     fu(CAP_PDM, SENS_PDM, sensors.mic_spl_db,  0, 130));
    p = kv_i(p, "doa_deg",        fs(CAP_PDM, SENS_PDM, sensors.doa_deg,  -180, 180));

    /* ---- Block 3: JA I2C (spec §2.6) — 26 keys, same shape --------- */
    /* ---- Block 4: fabric (spec §2.7) — 22 keys, same shape --------- */
    /*      ... full list in the key table; every line is one kv_i()    */

    payload_len = (u32)(p - (char *)&txf[42]);
    /* §4.4 runtime guard here */

    /* Headers unchanged from ethernet-spec §7.3 */
    iplen  = 28 + payload_len;
    udplen = 8 + payload_len;
    /* ... identical to the existing implementation ... */
    (void)emac_send(14 + iplen);

    if (tele_seq % 60u == 0)
        xil_printf("NET: telemetry seq=%u keys=%u len=%u\r\n",
                   (unsigned)tele_seq, TELE_KEYS, (unsigned)payload_len);
    tele_seq++;
}
```

Concurrency: `net_send_telemetry()` performs **only word-aligned loads**
from `sensors` and `ui_live`. It never calls `sensors_poll()`, never
touches an AXI IIC or Quad SPI register, and never blocks. The 1 Hz
telemetry cadence therefore cannot be perturbed by a wedged I2C bus, an
absent sensor, or a DFX swap.

### 6.6 Acceptance gates for the implementation

| G | Gate | Evidence |
|---|---|---|
| G1 | `nc -ul 5001` shows exactly 75 `key=value` lines per frame, keys in §2 order, for 600 consecutive frames | awk one-liner in §5 |
| G2 | Payload length reported in the `NET: telemetry` UART line is stable and ≤ 1400; `tele_overflow == 0` after a 1 hr soak | UART log |
| G3 | With no harness: every block-3 key is `-1`/`-1000001`, every block-4 F/C key is `-3`/`-1000003`, `cam_pid=-1`, `i2c=0` | frame capture |
| G4 | Unplugging the ADT7420 bus (or holding SDA low) moves `temp_cc` to `-1000002` within 2 s and back to a real value within 10 s of release | bench |
| G5 | `sensors_poll()` measured with `axi_timer` counter 1 never exceeds 20 ms across a 1 hr soak | UART max-watermark print |
| G6 | Telemetry seq numbers are gap-free across 20 DFX swaps (SW0↔SW1↔SW2↔SW3) | frame capture |
| G7 | Selecting SW0 (PDM) moves `pcm`/`energy`/`mic_*`/`doa_deg` off `-3` and all block-4 sonar/ADC keys onto `-3` within one frame | frame capture |

---

## 7. Revision log

| Rev | Date | Change |
|---|---|---|
| schema 1 | 27 Jul 2026 | Initial freeze. 75 keys. Supersedes `docs/ethernet-spec.md` §7.3.1 (10 keys); §7.3's header table and §7.4 checksum remain in force. |
