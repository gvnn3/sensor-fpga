---
name: run-sensor-fpga
description: Run, boot, drive and screenshot the sensor-fpga tricorder demo - the Nexys A7 FreeRTOS/DFX board (xsdb + UART) and the Tk telemetry GUI tools/tricorder_ui.py. Use when asked to run the demo, boot the board, start or screenshot the tricorder UI, grab a telemetry frame, or test a tricorder_ui.py change without hardware.
---

Two things to drive: the **board** (Nexys A7-100T on USB: xsdb programs
it, UART on `/dev/ttyUSB1` is the console, UDP broadcast port 5001 is
the telemetry) and the **GUI** (`tools/tricorder_ui.py`, a Tk window
that renders the telemetry). Both go through
`.claude/skills/run-sensor-fpga/driver.py`. The GUI runs without the
board: `driver.py fake` feeds it a canned frame.

All paths relative to repo root. Driver state lands in
`/tmp/tricorder-run/` (override with `--work` or `$TRICORDER_WORK`).

## Prerequisites

Already present on this machine; nothing was installed this session.

- Vivado/Vitis 2025.2 at `~/Vivado/2025.2` (xsdb). Board path only.
- Board on USB: `lsusb | grep 0403:6010`. User in `plugdev`, no sudo.
- `python3-tk`, `python3-pil`, `xwd`, `xwininfo` (x11-utils) for GUI + screenshots.
- A display. This box: Wayland session, `DISPLAY=:0` is XWayland; the Tk
  window maps there.

## Build

Board artifacts are committed under `build/`. If missing (~45 min):

```bash
make -C hw static && make -C hw dfx && make -C hw rtos
```

(Not run this session - artifacts were present. `driver.py boot` checks
for `build/dfx/full_a.bit` and the RTOS ELF and tells you if not.)

## Run (agent path)

### GUI only - no board needed

Use this for any change to `tools/tricorder_ui.py`.

```bash
D=.claude/skills/run-sensor-fpga/driver.py
python3 $D ui --port 5002                      # Tk window on :0, listens udp/5002
python3 $D fake --port 5002 --dfx --count 2    # 2 telemetry frames + 1 dfx swap frame
sleep 2
python3 $D shot --out /tmp/tricorder-run/fake.png
python3 $D kill-ui
```

Then **Read the PNG**. Expect: `SEQ 1001`, AMB TEMP 27.18 °C, CO2
467 ppm, SCD 23.07 °C / 52 %, footer `RM:HEL-A SWAPS:0 FLASH:0.28s
FRAMES:3`, SCAN + LINK lamps lit. `sample_frame.txt` in this directory
is a real 75-key frame captured from the board; edit it to test other
values/sentinels.

### Full demo - board + GUI

```bash
D=.claude/skills/run-sensor-fpga/driver.py
python3 $D console       # background cat of /dev/ttyUSB1 -> /tmp/tricorder-run/console.log
python3 $D boot          # xsdb: program full_a.bit, stage 4 partials in DDR2, load ELF, con (~12 s)
python3 $D wait-boot     # polls console.log for RTOS banner + "NET: link up" (exit 1 on MISMATCH/FAIL)
python3 $D telem         # one 75-key frame from udp/5001 to stdout (exit 1 if not 75 keys)
python3 $D ui            # GUI on udp/5001
sleep 3
python3 $D shot --out /tmp/tricorder-run/board.png
```

Expect in the PNG: `SEQ` small, UP a few seconds, AMB/CO2/SCD live,
`RM:HEL-A SWAPS:0 FLASH:--`. Console tail should show
`NET: telemetry seq=0 keys=75 len=1078`.

**DFX swaps** are switch-driven on the board (`dfx_task` in
`sw/tricorder-rtos/src/main.c` acts on `ui_live.req_rm`, which the ui
task sets from SW0-SW3), so `SWAPS:0` right after boot is correct. The
driver pokes the same field over xsdb:

```bash
python3 $D swap pdm      # one hop; waits for the type=dfx frame, asserts sig=ok
python3 $D rotate        # pdm -> sonar -> b -> a in one xsdb session
```

Output (measured, flash-resident partials):

```
hop from   to      swap_us  flash_us  sig
0   PDM    SONAR       932    252341  ok
1   SONAR  B           775    209746  ok
2   B      A           781    211405  ok
3/3 dfx frames in 8.8s - PASS
```

Exit 1 on a missing frame, `sig!=ok`, wrong `to`, or `MISMATCH` /
`DMA: error` in `console.log`. A hop to the personality already loaded
is skipped ("already on A"); `swap` to the current one prints
`nothing to do - PASS`. Console shows
`DFX: RM-PDM live in 1028 us (flash read 278211 us), sig=9D3A0001 OK`
per hop. Run `ui` *before* `swap` if you want the footer to show
`FLASH:0.28s` - the GUI only learns flash time from the aperiodic dfx
frame.

Cleanup:

```bash
python3 $D kill-ui; python3 $D kill-console
```

`kill-console` matters: only one reader may hold `/dev/ttyUSB1`, so the
user's own `cat` gets nothing while the driver's capture runs.

| command | does |
|---|---|
| `console` / `kill-console` | start/stop UART capture to `<work>/console.log` (115200 8N1) |
| `boot` | runs `hw/scripts/run_rtos_dfx.xsdb`; fails unless xsdb prints `RTOS_DFX: running` |
| `wait-boot [--timeout 90]` | wait for banner + link up after the *last* banner in console.log |
| `telem [--port] [--timeout]` | print one telemetry frame (skips `type=dfx`/`type=event`) |
| `swap pdm\|sonar\|a\|b [--gap] [--timeout]` | xsdb `stop; mwr &ui_live.req_rm; con`, wait for dfx frame, assert |
| `rotate [--order pdm,sonar,b,a]` | same, N hops in one xsdb session, `--gap` 3 s between pokes |
| `ui [--port]` | launch GUI on `$DISPLAY` (default :0), wait for window |
| `fake [--port] [--count] [--interval] [--dfx]` | send `sample_frame.txt` to 127.0.0.1 |
| `shot [--out png]` | `xwd -id <window>` -> PNG via Pillow |
| `kill-ui` | pkill tricorder_ui.py |

## Run (human path)

```bash
stty -F /dev/ttyUSB1 115200 raw -echo && cat /dev/ttyUSB1   # terminal 1
~/Vivado/2025.2/Vivado/bin/xsdb hw/scripts/run_rtos_dfx.xsdb  # terminal 2
python3 tools/tricorder_ui.py                                 # terminal 3, window opens
nc -ulp 5001                                                  # optional raw frames
```

`docs/demo.md` and `docs/manual.md` have the switch table and the
expected console transcript.

## Test

No automated tests for the GUI; the fake-frame path above is the test.
Firmware tests are hardware runs - see `docs/notebook.md`.

## Gotchas

- **Board is already running at power-on.** JP1 on QSPI boots the demo
  from flash, so `console.log` has traffic before `boot` runs and shows
  *two* RTOS banners (flash boot, then the JTAG-loaded ELF). `wait-boot`
  looks only after the last banner.
- **Point-to-point Ethernet, no DHCP server.** `NET: DHCP discover`
  repeats forever and the GUI says `NO LEASE`, `ip=0`. Not a fault.
  Telemetry is a broadcast, so `telem`/`nc -ulp 5001`/the GUI all get it anyway.
- **No screenshot tool on this box.** No grim/scrot/import/gnome-screenshot.
  `xwd -root` fails under XWayland with `BadMatch (X_GetImage)`; only
  `xwd -id <window>` works. Pillow has no XWD reader, so `driver.py shot`
  parses the XWD header itself (`bits_per_pixel` is header word 11,
  `bytes_per_line` word 12, colormap entries word 19, 12 bytes each; this
  visual is 32 bpp BGRX).
- **pgrep/pkill self-match.** Driver subprocesses run under a shell
  wrapper whose cmdline contains the pattern, so `pgrep -f 'cat /dev/ttyUSB1'`
  via `shell=True` always matched. Use argv lists with an anchored
  pattern (`^cat /dev/ttyUSB1`). Same for `pkill`.
- **The window size varies.** First launch mapped at 638x1189, later
  ones at 958x1189 (window manager). Don't hardcode coordinates.
- **`docs/demo.md` says JTAG staging takes "a couple of minutes".**
  Measured: 12 s for the whole xsdb script, twice.
- **`&ui_live` moves between builds** (0x80013F18 in notebook entry 39,
  0x80013FA0 now). `swap`/`rotate` read it from the ELF with `nm` every
  run; never hardcode it. The poke targets the first field, `req_rm`.
- **dfx frames ride net_task's loop.** The `type=dfx` UDP frame lands up
  to ~1 s after the swap, so `rotate` listens on a thread while xsdb runs
  and matches frames to hops by order. `--gap` below ~1.5 s risks
  out-of-order or merged hops.
- **Fake frames must omit `type=`.** The GUI keeps a separate snapshot
  for `type=dfx`/`type=event`; a frame with `type=` never updates the
  main page. `fake --dfx` sends a proper swap frame (sets `FLASH:0.28s`).

## Troubleshooting

- **`console already running` but no `cat` process**: old driver
  self-match bug (fixed). Check with `pgrep -af '^cat /dev/ttyUSB1'`.
- **`timeout waiting for RTOS banner + link up in console.log`**: capture
  wasn't started (`driver.py console` first) or someone else holds the
  tty. `fuser /dev/ttyUSB1`.
- **`boot FAILED (no 'RTOS_DFX: running' from xsdb)`**: board off / cable
  out, or stale `hw_server`. `lsusb | grep 0403:6010`, `pkill hw_server`,
  retry.
- **`no 'TR-590 MK VII TRICORDER' window on :0`**: UI not running, or
  wrong display. `driver.py --display :0 ui`.
- **`xsdb poked 0/1 (board off? stale hw_server?)`**: `puts "POKE n"`
  never printed. Same fix as boot failures.
- **`ui_live not in ... symtab`**: ELF missing or stripped; `make -C hw rtos`.
- **`no telemetry on udp/5001 in 5.0s`**: board not booted or NIC on a
  different link. `telem` binds all interfaces; the board broadcasts to
  255.255.255.255.
