# Tricorder Test Mode — User Manual

Test mode turns the Nexys A7-100T's 16 slide switches and 8-digit
seven-segment display into a self-contained instrument panel for the
tricorder. Each switch selects one internal function and the display
shows that function's readout — a short tag on the left, its value
right-aligned. This is **test mode**: no external sensor harness is
required or used. Some readouts are live values from the running RTOS
tasks; the rest are static placeholders that confirm the selection path
works end to end.

Verified on: Digilent Nexys A7-100T, branch `gnn-a7-tricorder`.

## Quick start

1. Boot the board as usual (see `docs/demo.md`); the demo rotation and
   console output keep running — test mode rides alongside them.
2. Slide **all switches down (off)**. The display shows the idle banner.
3. Slide **exactly one** of SW0–SW14 up. The display shows that
   function's tag and value.
4. Slide it back down before selecting another. Two or more switches up
   is an error (see below).

## The switch table

Switches are labeled SW0 (rightmost) to SW15 (leftmost) on the board
silkscreen.

| Switch | Function | Tag | Readout shows | Live? |
|---|---|---|---|---|
| SW0 | PDM microphone personality | `PdM` | `run` when the PDM personality is loaded (flipping SW0 loads it) | Placeholder |
| SW1 | Sonar personality | `Snr` | Burst/peak | Placeholder |
| SW2 | Hello-A DFX module | `HELA` | Counter | Placeholder |
| SW3 | Hello-B DFX module | `HELB` | Counter | Placeholder |
| SW4 | On-board ADT7420 temperature | `tEnP` | Temperature in °C to 0.1° resolution from the sensors task | **Live** |
| SW5 | External I2C bus scan | `I2C` | Device count from the last JA bus walk (0 with no harness) | **Live** |
| SW6 | DDR2 memory status | `ddr` | On marker | Placeholder |
| SW7 | DFX swap counter | `SUAP` | Count of completed personality swaps — watch it climb every 8 s | **Live** |
| SW8 | QSPI flash status | `FLSH` | On marker (future: JEDEC ID) | Placeholder |
| SW9 | Uptime | `UP` | Seconds since boot, from the RTOS tick count | **Live** |
| SW10 | Last PDM PCM sample | `PCM` | Static code until the PDM RM is loaded | Placeholder |
| SW11 | Last PDM energy window | `EnG` | Static code | Placeholder |
| SW12 | Last sonar peak | `PEA` | Static code | Placeholder |
| SW13 | Last swap time | `tSUP` | Microseconds for the most recent swap | **Live** (when available) |
| SW14 | Build/version ID | `uEr` | Static build identifier | Static |
| SW15 | — reserved — | | Ignored; leave it off. It never affects the display | |

Placeholder readouts show the function's code plus an `On` marker —
they prove the switch, the register path, and the display work, and
will become live values as the corresponding hardware personalities are
wired into test mode.

## One switch at a time

SW0–SW14 are **mutually exclusive**. The selection logic enforces this:

- **No switches on** — the display shows the idle banner. This is the
  normal resting state.
- **Exactly one on** — that function's readout.
- **Two or more on** — the display shows `Err SEL`. Nothing is broken;
  the panel is refusing an ambiguous selection. **To recover, slide
  switches down until only one (or none) remains up.** The readout
  returns immediately.

SW15 does not count toward this rule — it is reserved and ignored in
any position.

## Reading the display

The eight digits are numbered right to left: **digit 0 is the
rightmost** digit (AN0), digit 7 the leftmost (AN7). Layouts put the
function tag on the left and the value right-aligned, so the least
significant digit of any number is always the rightmost digit — read
values the way you'd read a number.

The character set is limited to what seven segments can draw
(`0-9 A b C c d E F G H h I J L n o P q r S t U u y -` and blank), so
tags are short and some letters look approximate — `S` and `5` are the
same glyph, `W` is drawn as `U` (so the swap tags read `SUAP` and
`tSUP`), and `M`-like sounds are spelled around (e.g. `PdM` renders
with an `n`-style pattern).

## What test mode does *not* do

Selecting a function only changes the **display readout**. It does not
start a measurement, trigger a DFX swap, or drive any external pin — in
test mode no external devices are wired, and the demo's own rotation
and console traffic continue regardless of switch positions. Think of
it as a read-only gauge cluster over the running system.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Display shows `Err SEL` | More than one of SW0–SW14 is up; slide all but one down |
| Display stuck on idle banner with a switch up | The switch up is SW15 (leftmost — reserved); use SW0–SW14 |
| Live value never changes (e.g. `UP` frozen) | The RTOS is halted or never booted — check the console per `docs/demo.md`; the display scanner keeps the last written value |
| Readout shows a code + `On` instead of a value | Expected — that function is a placeholder in test mode (see the Live? column) |
| SW5 `I2C` shows 0 | Normal without the external sensor harness on JA |

## Sounds

The board plays two effect sounds through the on-board mono audio jack
(`sounds/` holds the source MP3s; `tools/mp3_to_mem.py` bakes them into
the FPGA image at 8 kHz):

| Sound | File | When it plays |
|---|---|---|
| Scan | `tos_tricorder_scan.mp3` (first ~4 s) | Each time a function is switched ON (for SW0-SW3 this is also the personality load sound) |
| Alert | `tos_tricoder_alert.mp3` | On entering `Err SEL` (more than one switch up), and whenever a personality load fails its signature check |

Plug headphones or a speaker into the audio jack (J8). The scan
sound also plays once at boot as a power-on chirp. The sound plays
once per trigger; flipping the same switch off and on again replays it.

## Telemetry over Ethernet

Once a second the board broadcasts one UDP datagram on port 5001 with a
reading for **every sensor in the tricorder plan** — the four that are
wired today and the several dozen that are not. Nothing is hidden and
nothing is left out: an unwired sensor reports a value that says "I am
not here" rather than vanishing from the frame.

### Listening

Put a computer on the same LAN as the board and run:

```bash
nc -ulp 5001
```

(Some `nc` builds want `nc -ul 5001`; either is fine — the frame goes to
the broadcast address, so no board IP is needed.) A frame arrives every
second, starting the moment the Ethernet link comes up — the board sends
its heartbeat frames even before its DHCP lease lands.

To confirm you are seeing whole frames:

```bash
nc -ul 5001 | awk -F= 'NF==2{n++} /^seq=/{if(n)print "keys:",n; n=1}'
```

That prints `keys: 75` for every frame, always. The frame is
**fixed-shape**: the same 75 keys, in the same order, every second,
forever. There is no sparse mode and no optional key, so a consumer can
parse by position or by name and never has to handle a missing field.

### What a frame looks like

Plain ASCII, one `key=value` per line, `\n`-terminated, no spaces, all
values decimal signed 32-bit integers (no floats — the MicroBlaze has no
FPU in this build). Scale lives in the key's suffix: `_cc` is
centi-degrees Celsius, `_mv` millivolts, `_mg` milli-g, `_pct` whole
percent, and so on.

A real frame from the demo build — Hello-A personality loaded, DHCP lease
held, no external harness attached:

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
...
sonar_bursts_cnt=-3
```

So: `temp_cc=3128` is 31.28 °C, `swap_us=1042` means the last personality
swap took 1.042 ms, and `ip=3232235902` is 0xC0A8017E — 192.168.1.126.
`sens_mask=16385` is bit 0 + bit 14: the on-board ADT7420 plus the
soldered-down PDM microphone. The ADXL362 and XADC keys read as
sentinels in this build (no accelerometer SPI core in the BD; the DDR
controller owns the XADC primitive). Everything that reads negative here
is a **sentinel**, explained below.

You may also see the occasional extra frame on the same port whose first
line is `type=event`. Those are the aperiodic switch-change events, not
telemetry — skip any frame that does not start with `seq=`.

A third frame type, `type=dfx`, is emitted once per personality swap: it
names the personality switched from and to (`from_name=A to_name=PDM`),
the size in bytes of each partial bitstream, and how long the
reconfiguration took (`swap_us=1040` is 1.04 ms; `flash_us` is the QSPI
slot read that preceded it, 0 when staged from DDR). `sig=mismatch` means
the load failed its signature check — you will also hear the alert
sound.

### The sentinel convention — why almost everything reads −1 today

Only four sensors are actually on the board (ADT7420 temperature, ADXL362
accelerometer, ADMP421 microphone, XADC die monitor). Everything else in
the key table is planned but unwired. Rather than omit those keys, the
board sends a value that **cannot be a real reading**, so you can always
tell "no data" from data — and, more usefully, tell *why* there is no
data:

| Value | Condition | What it means |
|---|---|---|
| `-1` | **ABSENT** | Not wired, not detected, or never probed. There is no data and none is expected. |
| `-2` | **ERROR** | The device *is* there, but the last read failed, was NAKed, or hit the 2 ms bus timeout. Data is expected but missing right now. |
| `-3` | **N/A** | The value comes from the reconfigurable partition and the currently loaded personality does not provide it (this includes "a swap is in progress"). The question is not meaningful in this configuration. |

That covers keys that can never legitimately be negative — counts, ppm,
lux, pascals, percentages, millivolts. Thirteen keys **can** legitimately
be negative (temperatures, accelerations, PCM samples, bearings, dB
figures), and for those `-1` is a perfectly good reading: `temp_cc=-1` is
−0.01 °C. Those keys use the same three reason codes shifted a million
below anything the hardware can produce:

| Value | Condition |
|---|---|
| `-1000001` | ABSENT |
| `-1000002` | ERROR |
| `-1000003` | N/A |

The rule for both sets is the same — **the last digit is the reason
code**: 1 absent, 2 error, 3 not applicable. The signed keys are marked
**†** in the tables below; they are `temp_cc`, `pcm`, `acc_x_mg`,
`acc_y_mg`, `acc_z_mg`, `acc_temp_cc`, `xadc_temp_cc`, `doa_deg`,
`bme_temp_cc`, `scd_temp_cc`, `max_temp_cc`, `afe_gain_db` and
`sonar_snr_db`.

Two things are deliberately *not* sentinels:

- `rm=-1` keeps its original meaning, "a personality swap is in
  progress". It is the one key exempt from the convention.
- Every real reading is clamped into its declared range before it is
  printed, so a misbehaving driver can never produce a number that looks
  like a sentinel by accident.

Reading the example frame above with this in hand: `pcm=-1000003` and
`energy=-3` because Hello-A is loaded and it has no microphone path;
`xadc_vccint_mv=-1` because the memory controller currently owns the XADC
so there is no software path to it; and every `bme_*`, `co2_*`, `as_*`,
`ppg_*`, `adc_*`, `cam_*`, `sipm_*` and `sonar_*` key is `-1` or `-3`
because that hardware is not attached. Flip **SW0** to load the PDM
personality and `pcm`, `energy` and the `mic_*` keys leave `-3` within one
frame; flip **SW1** for sonar and the `sonar_*`, `adc_*` and `afe_*` keys
do the same while the microphone keys go to `-3`.

### The keys

Seventy-five keys in four blocks, emitted in exactly this order.

**Block 0 — the original ten** (unchanged names and meanings; existing
consumers keep working)

| Key | Units | Meaning |
|---|---|---|
| `seq` | count | Frame counter, +1 per frame, wraps at 2³² |
| `uptime` | seconds | Seconds since boot |
| `temp_cc` † | 0.01 °C | On-board ADT7420 temperature (3128 = 31.28 °C) |
| `i2c` | count | Devices answering on the external JA I2C bus |
| `rm` | enum | Loaded personality: 0 PDM, 1 SONAR, 2 Hello-B, 3 Hello-A, −1 swapping |
| `swaps` | count | Completed DFX swaps since boot |
| `swap_us` | µs | Duration of the most recent swap (0 = none yet) |
| `pcm` † | signed sample | Last PDM PCM sample, −32768…32767 |
| `energy` | count | Energy of the last PDM window |
| `peak` | count | Last sonar echo peak |

**Block 1 — system** (always live; these keys never carry a sentinel,
except `link`)

| Key | Units | Meaning |
|---|---|---|
| `schema` | — | Payload schema version — `1`. Refuse to parse a value you do not recognise |
| `ip` | — | DHCP lease as a big-endian IPv4 integer; `0` = no lease yet |
| `link` | — | 0 link down, 1 link up, `-2` the PHY is not answering on MDIO |
| `sens_mask` | bitmap | Bit *i* set = sensor *i* detected (bit order is `enum sensor_id` in `sw/tricorder-rtos/src/sensors.h`) |
| `probe_s` | seconds | Age of the presence information — seconds since the last complete discovery sweep |

**Block 2 — on-board devices** (physically present on the Nexys A7-100T;
no harness needed)

| Key | Units | Meaning |
|---|---|---|
| `acc_x_mg` † | milli-g | ADXL362 acceleration, X axis (1004 = 1.004 g) |
| `acc_y_mg` † | milli-g | ADXL362 acceleration, Y axis |
| `acc_z_mg` † | milli-g | ADXL362 acceleration, Z axis |
| `acc_mag_mg` | milli-g | Vector magnitude of the three axes |
| `acc_temp_cc` † | 0.01 °C | ADXL362 die temperature |
| `xadc_temp_cc` † | 0.01 °C | FPGA die temperature |
| `xadc_vccint_mv` | mV | FPGA core supply |
| `xadc_vccaux_mv` | mV | FPGA auxiliary supply |
| `xadc_vbram_mv` | mV | Block-RAM supply |
| `mic_n_cnt` | count | Active PDM microphone channels (0–7) |
| `mic_spl_db` | dB SPL | A-weighted sound-pressure estimate |
| `doa_deg` † | degrees | Bearing of the dominant sound source, −180…180 (needs the mic array) |

The four `xadc_*` keys read absent (`-1000001` / `-1`) in the current
build: the DDR2 memory controller instantiates the XADC primitive, so
software has no path to it until the block design is changed. The
`mic_*` and `doa_deg` keys need the PDM personality loaded *and* the
six-microphone array attached.

**Block 3 — the JA I2C sensor harness** — five STEMMA QT breakouts plus
the camera's control port, daisy-chained into JA1/JA2. **None of this is
wired today**, so every key here reads `-1` (or `-1000001` for the †
keys) in every frame. Plug the harness in and they go live with no
software change.

| Key | Units | Meaning |
|---|---|---|
| `bme_temp_cc` † | 0.01 °C | BME688 temperature |
| `bme_rh_pct` | % RH | BME688 relative humidity |
| `bme_pres_pa` | Pa | BME688 barometric pressure |
| `bme_gas_ohm` | Ω | BME688 gas-sensor resistance (VOC proxy — lower is dirtier air) |
| `co2_ppm` | ppm | SCD-41 CO₂ concentration |
| `scd_temp_cc` † | 0.01 °C | SCD-41 temperature |
| `scd_rh_pct` | % RH | SCD-41 relative humidity |
| `light_lux` | lux | VEML7700 ambient light, gain/integration corrected |
| `light_raw` | count | VEML7700 raw counts (keeps sub-lux resolution) |
| `as_415_cnt` | count | AS7341 violet channel, 415 nm |
| `as_445_cnt` | count | AS7341 indigo, 445 nm |
| `as_480_cnt` | count | AS7341 blue, 480 nm |
| `as_515_cnt` | count | AS7341 cyan, 515 nm |
| `as_555_cnt` | count | AS7341 green, 555 nm |
| `as_590_cnt` | count | AS7341 yellow, 590 nm |
| `as_630_cnt` | count | AS7341 orange, 630 nm |
| `as_680_cnt` | count | AS7341 red, 680 nm |
| `as_clear_cnt` | count | AS7341 clear (unfiltered) channel |
| `as_nir_cnt` | count | AS7341 near-infrared channel |
| `as_flicker_hz` | Hz | Mains flicker detected in the light: 100, 120, or 0 for none |
| `ppg_red_cnt` | count | MAX30105 red LED photodiode counts |
| `ppg_ir_cnt` | count | MAX30105 infrared counts |
| `ppg_grn_cnt` | count | MAX30105 green counts |
| `hr_bpm` | bpm | Heart rate derived from the IR channel |
| `spo2_pct` | % | Blood oxygen saturation from the red/IR ratio |
| `max_temp_cc` † | 0.01 °C | MAX30105 die temperature |

`hr_bpm` and `spo2_pct` are the one place the `-1` / `-2` distinction
earns its keep: `-1` means the pulse sensor is not on the bus at all,
while `-2` means it is there and healthy but has seen no valid pulse for
ten seconds — i.e. nobody's finger is on it.

**Block 4 — instruments behind the DFX partition** — these come from
whichever personality is loaded. With Hello-A or Hello-B they all read
`-3` / `-1000003` (not applicable); with the right personality loaded but
no analog front end attached they read `-1` / `-1000001` (absent). None of
this hardware is wired today.

| Key | Units | Meaning | Needs |
|---|---|---|---|
| `adc_rate_ksps` | kSPS | AD9226 sample rate the personality commanded | ADC |
| `adc_min_code` | LSB code | Minimum sample in the last window, 0–4095 | ADC |
| `adc_max_code` | LSB code | Maximum sample in the last window | ADC |
| `adc_rms_code` | LSB code | RMS about mid-scale in the last window | ADC |
| `adc_rail_cnt` | count | Samples pinned at 0x000/0xFFF — the floating-input signature | ADC |
| `dac_mode` | enum | AD9708 output: 0 idle, 1 tone, 2 sweep, 3 sonar burst | DAC |
| `dac_freq_hz` | Hz | Commanded output frequency | DAC |
| `dac_ampl_pct` | % | Commanded full-scale amplitude | DAC |
| `cam_pid` | — | OV7670 product/version ID; 30323 (0x7673) means the camera answered | — |
| `cam_frames_cnt` | count | Frames captured into DDR2 since the personality loaded | camera |
| `cam_fps_hz` | Hz | Measured frame rate | camera |
| `cam_lum_pct` | % | Mean frame luminance | camera |
| `cam_motion_pct` | % | Frame-to-frame difference metric | camera |
| `sipm_cps` | counts/s | SiPM photon counts in the last one-second window | SiPM |
| `sipm_tot_cnt` | count | Total SiPM counts since the personality loaded | SiPM |
| `sipm_thresh_mv` | mV | Commanded comparator threshold | SiPM |
| `afe_gain_db` † | dB | AD8332 ultrasound gain commanded, −5…+48 | AFE |
| `afe_hilo` | enum | AD8332 gain range: 0 low, 1 high | AFE |
| `sonar_range_mm` | mm | Pulse-echo range from the 40 kHz pair | sonar |
| `sonar_tof_us` | µs | Echo time of flight | sonar |
| `sonar_snr_db` † | dB | Echo peak over the noise floor | sonar |
| `sonar_bursts_cnt` | count | Bursts fired since the personality loaded | sonar |

`cam_pid` is the odd one out: the camera is identified over the always-on
I2C bus rather than through the partition, so it reports `-1` (absent)
rather than `-3` no matter which personality is loaded — plug the camera
in and it reads 30323 immediately, while `cam_fps_hz` stays `-3` until the
camera personality is swapped in.

The full frozen schema — exact ranges, discovery rules, and byte-level
framing — is `docs/telemetry-spec.md`.

### Telemetry troubleshooting

| Symptom | Cause / fix |
|---|---|
| `nc` shows nothing | Listener is not on the board's LAN segment, or a firewall is dropping broadcast UDP; check `link=1` on the console and that the board's Ethernet LEDs are lit |
| Frames arrive but `ip=0` | No DHCP lease yet — the board broadcasts anyway; it retries a DISCOVER every 3 s |
| `link=-2` | The Ethernet PHY never answered on MDIO — a hardware fault, not a cable problem |
| Every value is `-1` or `-3` | Expected with no sensor harness attached (see above); check `temp_cc` and `uptime` are moving to confirm the board is alive |
| `temp_cc=-1000002` | The ADT7420 is on the bus but its last read failed; it recovers on its own within about 10 s once the bus is healthy |
| A frame has fewer than 75 lines | A frame beginning `type=event` is a switch event, not telemetry. A short *telemetry* frame is a bug — report it |

## Standalone operation

With jumper **JP1 set to QSPI**, the board is fully standalone: at
power-on the FPGA configures itself from flash and a built-in bootloader
loads the tricorder software — the boot chirp and `trICordr` banner
appear with no computer attached. (With JP1 on JTAG, use
`hw/scripts/run_rtos_dfx.xsdb` or configure `build/dfx/full_a.bit` —
the bootloader does the rest either way.)
