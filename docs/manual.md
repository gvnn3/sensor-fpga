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

## Standalone operation

With jumper **JP1 set to QSPI**, the board is fully standalone: at
power-on the FPGA configures itself from flash and a built-in bootloader
loads the tricorder software — the boot chirp and `trICordr` banner
appear with no computer attached. (With JP1 on JTAG, use
`hw/scripts/run_rtos_dfx.xsdb` or configure `build/dfx/full_a.bit` —
the bootloader does the rest either way.)
