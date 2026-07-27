#!/usr/bin/env python3
"""Regenerate hw/rtl/audio_rom.mem from the committed sounds/*.mp3.

Layout (8 kHz, 8-bit offset-binary mono):
  [0     .. 32767]  tos_tricorder_scan.mp3, first 4.096 s
  [32768 .. 49151]  tos_tricoder_alert.mp3 (1.224 s, zero-padded to 16384)

Needs the `miniaudio` package (self-contained, no ffmpeg):
  python3 -m venv .venv && .venv/bin/pip install miniaudio
  .venv/bin/python tools/mp3_to_mem.py
"""

import os
import miniaudio

SCAN_LEN = 32768
ALERT_PAD = 16384
OUT = os.path.join(os.path.dirname(__file__), "..", "hw", "rtl", "audio_rom.mem")


def decode(path, limit):
    dec = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.SIGNED16,
                                nchannels=1, sample_rate=8000)
    s16 = list(dec.samples)[:limit]
    return [((x >> 8) + 128) & 0xFF for x in s16]


def main():
    here = os.path.join(os.path.dirname(__file__), "..")
    scan = decode(os.path.join(here, "sounds", "tos_tricorder_scan.mp3"), SCAN_LEN)
    scan += [128] * (SCAN_LEN - len(scan))
    alert = decode(os.path.join(here, "sounds", "tos_tricoder_alert.mp3"), ALERT_PAD)
    alert_used = len(alert)
    alert += [128] * (ALERT_PAD - len(alert))

    with open(OUT, "w") as f:
        for b in scan + alert:
            f.write(f"{b:02x}\n")
    print(f"audio_rom.mem: {SCAN_LEN} scan + {alert_used} alert samples "
          f"({(SCAN_LEN + ALERT_PAD) // 1024} KiB)")


if __name__ == "__main__":
    main()
