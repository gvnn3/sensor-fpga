#!/usr/bin/env python3
"""TOS-style tricorder display for the sensor-fpga telemetry stream.

Listens for the 75-key UDP broadcast frames (port 5001, 1 Hz) described
in docs/telemetry-spec.md and renders them in a Star Trek: The Original
Series tricorder skin: moire scan discs, jewel lamps, and three mission
pages selected by the GEO / MET / BIO buttons.

Stdlib only (tkinter + socket + threading).  Run on any machine on the
same LAN as the board:

    python3 tools/tricorder_ui.py [--port 5001]

Every received frame (telemetry, event, dfx) is also logged raw to
tricorder-logs/telemetry.log, rotated at 1 MiB with at most 32 files
kept.  --log-dir moves it; --no-log turns it off.
"""

import argparse
import logging
import logging.handlers
import math
import os
import socket
import threading
import time

try:
    import tkinter as tk
except ImportError:  # UI needs python3-tk; decode logic works without
    tk = None

# --- telemetry decoding (spec section 1.4 / 1.5) -----------------------

# Signed (Type S) keys use the -1000001..-1000003 sentinels; everything
# else numeric uses -1..-3.  `link` carries -2 only; `rm` -1 is "swap".
SIGNED_KEYS = {
    "temp_cc", "acc_x_mg", "acc_y_mg", "acc_z_mg", "acc_temp_cc",
    "xadc_temp_cc", "bme_temp_cc", "scd_temp_cc", "max_temp_cc",
    "pcm", "doa_deg", "afe_gain_db", "sonar_snr_db",
}
NO_SENTINEL = {"seq", "uptime", "schema", "ip", "sens_mask", "probe_s",
               "i2c", "swaps", "swap_us", "rm"}

ABSENT, ERROR, NA = "ABSENT", "ERROR", "N/A"


def decode(key, raw):
    """Return (value:int, status:str|None). status None means live data."""
    try:
        v = int(raw)
    except ValueError:
        return 0, ERROR
    if key in NO_SENTINEL:
        return v, None
    if key == "link":
        return v, (ERROR if v == -2 else None)
    if key in SIGNED_KEYS:
        if v == -1000001:
            return v, ABSENT
        if v == -1000002:
            return v, ERROR
        if v == -1000003:
            return v, NA
        return v, None
    if v == -1:
        return v, ABSENT
    if v == -2:
        return v, ERROR
    if v == -3:
        return v, NA
    return v, None


RM_NAMES = {-1: "SWAP", 0: "PDM", 1: "SONAR", 2: "HEL-B", 3: "HEL-A"}


def fmt_ip(v):
    if v == 0:
        return "NO LEASE"
    return ".".join(str((v >> s) & 0xFF) for s in (24, 16, 8, 0))


# --- UDP listener thread ----------------------------------------------

LOG_MAX_BYTES = 1024 * 1024   # rotate at 1 MiB
LOG_KEEP = 32                 # active file + 31 backups


def make_frame_logger(log_dir):
    """Rotating logger for raw frames: telemetry.log, .1 .. .31."""
    os.makedirs(log_dir, exist_ok=True)
    lg = logging.getLogger("tricorder.frames")
    lg.setLevel(logging.INFO)
    lg.propagate = False
    h = logging.handlers.RotatingFileHandler(
        os.path.join(log_dir, "telemetry.log"),
        maxBytes=LOG_MAX_BYTES, backupCount=LOG_KEEP - 1)
    h.setFormatter(logging.Formatter("%(message)s"))
    lg.addHandler(h)
    return lg


class Telemetry:
    def __init__(self, port, log=None):
        self.lock = threading.Lock()
        self.frame = {}
        self.dfx = {}          # last type=dfx swap-event frame
        self.last_rx = 0.0
        self.frames = 0
        self.log = log
        t = threading.Thread(target=self._rx, args=(port,), daemon=True)
        t.start()

    def _rx(self, port):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(("", port))
        while True:
            data, addr = s.recvfrom(4096)
            text = data.decode("ascii", "replace")
            if self.log:
                stamp = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime())
                self.log.info("# %s.%03d %s %d bytes\n%s",
                              stamp, int(time.time() * 1000) % 1000,
                              addr[0], len(data), text.rstrip("\n"))
            kv = {}
            for line in text.split("\n"):
                if "=" in line:
                    k, _, v = line.partition("=")
                    kv[k] = v
            with self.lock:
                # Aperiodic frames (type=event / type=dfx) must not
                # displace the telemetry snapshot the display renders.
                if kv.get("type") == "dfx":
                    self.dfx = kv
                elif "type" not in kv:
                    self.frame = kv
                    self.last_rx = time.time()
                self.frames += 1

    def snapshot(self):
        with self.lock:
            return dict(self.frame), dict(self.dfx), self.last_rx, self.frames


# --- look and feel -----------------------------------------------------

CASE = "#1a1a1d"        # tricorder shell
CASE_EDGE = "#3a3a40"   # brushed trim
SCREEN_BG = "#050608"
AMBER = "#ffb000"
CYAN = "#7fd4ff"
GREEN = "#39d353"
RED = "#ff4b3e"
BLUE = "#4f7bff"
DIM = "#4a4f55"
FONT = ("Courier", 11, "bold")
FONT_SM = ("Courier", 9, "bold")
FONT_LG = ("Courier", 13, "bold")

# Wavelength -> approximate display color for the AS7343 bars.
SPECTRAL = [
    ("as_415_cnt", "415", "#7b2fbe"), ("as_445_cnt", "445", "#2f43be"),
    ("as_480_cnt", "480", "#2f9dbe"), ("as_515_cnt", "515", "#2fbe6a"),
    ("as_555_cnt", "555", "#8abe2f"), ("as_590_cnt", "590", "#bcbe2f"),
    ("as_630_cnt", "630", "#be7a2f"), ("as_680_cnt", "680", "#be3b2f"),
    ("as_clear_cnt", "CLR", "#c8c8c8"), ("as_nir_cnt", "NIR", "#8a4a4a"),
]

# (label, key, scale divisor, unit) rows per page.
PAGE_MET = [
    ("AMB TEMP", "temp_cc", 100, "\N{DEGREE SIGN}C"),
    ("BME TEMP", "bme_temp_cc", 100, "\N{DEGREE SIGN}C"),
    ("HUMIDITY", "bme_rh_pct", 1, "%"),
    ("PRESSURE", "bme_pres_pa", 100, "hPa"),
    ("GAS RES ", "bme_gas_ohm", 1, "\N{GREEK CAPITAL LETTER OMEGA}"),
    ("CO2     ", "co2_ppm", 1, "ppm"),
    ("SCD TEMP", "scd_temp_cc", 100, "\N{DEGREE SIGN}C"),
    ("SCD RH  ", "scd_rh_pct", 1, "%"),
    ("LIGHT   ", "light_lux", 1, "lx"),
    ("FLICKER ", "as_flicker_hz", 1, "Hz"),
]
PAGE_BIO = [
    ("PULSE   ", "hr_bpm", 1, "bpm"),
    ("SP-O2   ", "spo2_pct", 1, "%"),
    ("PPG RED ", "ppg_red_cnt", 1, ""),
    ("PPG IR  ", "ppg_ir_cnt", 1, ""),
    ("PPG GRN ", "ppg_grn_cnt", 1, ""),
    ("SKN TEMP", "max_temp_cc", 100, "\N{DEGREE SIGN}C"),
    ("MIC SPL ", "mic_spl_db", 1, "dB"),
    ("BEARING ", "doa_deg", 1, "\N{DEGREE SIGN}"),
    ("PCM     ", "pcm", 1, ""),
    ("ENERGY  ", "energy", 1, ""),
]
PAGE_GEO = [
    ("ACCEL X ", "acc_x_mg", 1000, "g"),
    ("ACCEL Y ", "acc_y_mg", 1000, "g"),
    ("ACCEL Z ", "acc_z_mg", 1000, "g"),
    ("ACC MAG ", "acc_mag_mg", 1000, "g"),
    ("SONAR   ", "sonar_range_mm", 10, "cm"),
    ("SNR     ", "sonar_snr_db", 1, "dB"),
    ("SIPM    ", "sipm_cps", 1, "cps"),
    ("ADC RMS ", "adc_rms_code", 1, ""),
    ("CAM LUM ", "cam_lum_pct", 1, "%"),
    ("MOTION  ", "cam_motion_pct", 1, "%"),
]
PAGES = {"GEO": PAGE_GEO, "MET": PAGE_MET, "BIO": PAGE_BIO}


class TricorderUI:
    def __init__(self, root, tele):
        self.tele = tele
        self.page = "MET"
        self.phase = 0.0
        root.title("TR-590 MK VII TRICORDER")
        root.configure(bg="black")
        self.cv = tk.Canvas(root, width=460, height=780,
                            bg="black", highlightthickness=0)
        self.cv.pack()
        self.cv.bind("<Button-1>", self.click)
        self.tick(root)

    # -- input --

    def click(self, ev):
        for name, (x0, y0, x1, y1) in self.buttons.items():
            if x0 <= ev.x <= x1 and y0 <= ev.y <= y1:
                self.page = name

    # -- drawing helpers --

    def txt(self, x, y, s, color=AMBER, font=FONT, anchor="w"):
        self.cv.create_text(x, y, text=s, fill=color, font=font,
                            anchor=anchor)

    def moire(self, cx, cy, r, speed):
        """Rotating radial-line disc, the TOS scan-head look."""
        self.cv.create_oval(cx - r, cy - r, cx + r, cy + r,
                            fill="#0c0c10", outline=CASE_EDGE, width=2)
        a0 = self.phase * speed
        for i in range(12):
            a = a0 + i * math.pi / 6
            self.cv.create_line(cx + 0.25 * r * math.cos(a),
                                cy + 0.25 * r * math.sin(a),
                                cx + 0.92 * r * math.cos(a),
                                cy + 0.92 * r * math.sin(a),
                                fill="#2c3e50", width=3)
        self.cv.create_oval(cx - 4, cy - 4, cx + 4, cy + 4,
                            fill=CASE_EDGE, outline="")

    def jewel(self, cx, cy, color, lit, label):
        fill = color if lit else "#26262b"
        self.cv.create_oval(cx - 11, cy - 11, cx + 11, cy + 11,
                            fill=fill, outline=CASE_EDGE, width=2)
        if lit:  # glint
            self.cv.create_oval(cx - 4, cy - 6, cx + 1, cy - 1,
                                fill="white", outline="")
        self.txt(cx, cy + 22, label, DIM, FONT_SM, "center")

    def value_text(self, key, frame, scale, unit):
        if key not in frame:
            return "-----", DIM
        v, st = decode(key, frame[key])
        if st == ABSENT:
            return "-----", DIM
        if st == ERROR:
            return "ERR", RED
        if st == NA:
            return "N/A", BLUE
        if scale == 1:
            return f"{v}{unit}", GREEN
        return f"{v / scale:.2f}{unit}", GREEN

    # -- main loop --

    def tick(self, root):
        frame, dfx, last_rx, nframes = self.tele.snapshot()
        age = time.time() - last_rx if last_rx else 1e9
        live = age < 3.0
        self.phase += 0.12 if live else 0.02
        self.cv.delete("all")
        cv = self.cv

        # shell
        cv.create_rectangle(8, 8, 452, 772, fill=CASE,
                            outline=CASE_EDGE, width=4)
        cv.create_rectangle(8, 8, 452, 150, fill="#141416",
                            outline=CASE_EDGE, width=4)

        # scan head: moire discs + jewels
        self.moire(80, 78, 52, 1.0)
        self.moire(380, 78, 52, -0.7)
        link_v, link_st = decode("link", frame.get("link", "-2")) \
            if frame else (0, ERROR)
        blink = int(self.phase * 4) % 2 == 0
        self.jewel(180, 68, GREEN, live and blink, "SCAN")
        self.jewel(230, 68, AMBER,
                   bool(frame) and link_st is None and link_v == 1, "LINK")
        self.jewel(280, 68, RED, (not live) and blink, "LOST")

        # screen bezel
        cv.create_rectangle(30, 170, 430, 620, fill=SCREEN_BG,
                            outline="#555a60", width=3)

        # header line
        if frame:
            seq = frame.get("seq", "?")
            up = int(frame.get("uptime", "0") or 0)
            ip = fmt_ip(decode("ip", frame.get("ip", "0"))[0])
            hdr = f"SEQ {seq}  UP {up // 3600:02}:{up % 3600 // 60:02}:{up % 60:02}"
            self.txt(45, 192, hdr, CYAN)
            self.txt(415, 192, ip, CYAN, FONT_SM, "e")
        else:
            self.txt(230, 192, "SEARCHING FOR CARRIER...", RED,
                     FONT, "center")
        cv.create_line(40, 206, 420, 206, fill="#2a2f36")

        # sensor rows for the active page
        rows = PAGES[self.page]
        y = 230
        for label, key, scale, unit in rows:
            self.txt(50, y, label, AMBER)
            val, color = self.value_text(key, frame, scale, unit)
            self.txt(300, y, val, color, FONT_LG, "e")
            # signal bar for live values
            if color == GREEN:
                w = int(60 + 40 * math.sin(self.phase + y))
                cv.create_rectangle(315, y - 5, 315 + max(8, w), y + 5,
                                    fill="#123a1c", outline=GREEN)
            y += 32

        # spectral strip (always shown, TOS rainbow)
        cv.create_line(40, 556, 420, 556, fill="#2a2f36")
        self.txt(45, 570, "SPECTRAL", AMBER, FONT_SM)
        x = 120
        for key, lab, color in SPECTRAL:
            h, fill = 4, "#1b1b20"
            if key in frame:
                v, st = decode(key, frame[key])
                if st is None:
                    h = max(4, min(38, int(v / 65535 * 38)))
                    fill = color
            cv.create_rectangle(x, 600 - h, x + 20, 600,
                                fill=fill, outline="#333")
            self.txt(x + 10, 610, lab, DIM, ("Courier", 7), "center")
            x += 30

        # status footer inside screen
        if frame:
            rm = RM_NAMES.get(decode("rm", frame.get("rm", "0"))[0],
                              "RSVD")
            i2c = frame.get("i2c", "?")
            swaps = frame.get("swaps", "?")
            # flash_us arrives only on type=dfx swap frames: last known
            try:
                flash = f"{int(dfx['flash_us']) / 1e6:.2f}s"
            except (KeyError, ValueError):
                flash = "--"
            self.txt(45, 640, f"RM:{rm}  I2C:{i2c}  SWAPS:{swaps}"
                     f"  FLASH:{flash}", DIM, FONT_SM)
        self.txt(415, 640, f"FRAMES:{nframes}", DIM, FONT_SM, "e")

        # GEO / MET / BIO buttons
        self.buttons = {}
        x = 60
        for name in ("GEO", "MET", "BIO"):
            active = name == self.page
            box = (x, 670, x + 100, 720)
            cv.create_rectangle(*box, fill="#2c2c31" if active else "#18181c",
                                outline=AMBER if active else CASE_EDGE,
                                width=3)
            self.txt(x + 50, 695, name, AMBER if active else DIM,
                     FONT_LG, "center")
            self.buttons[name] = box
            x += 120

        self.txt(230, 748, "TR-590 MK VII \N{BULLET} STARFLEET TRICORDER",
                 DIM, FONT_SM, "center")

        root.after(50, lambda: self.tick(root))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=5001,
                    help="UDP telemetry port (default 5001)")
    ap.add_argument("--log-dir", default="tricorder-logs",
                    help="frame log directory (default ./tricorder-logs; "
                         "1 MiB rotation, 32 files kept)")
    ap.add_argument("--no-log", action="store_true",
                    help="disable frame logging")
    args = ap.parse_args()
    if tk is None:
        raise SystemExit("tkinter missing - install with: "
                         "sudo apt install python3-tk")
    log = None if args.no_log else make_frame_logger(args.log_dir)
    root = tk.Tk()
    TricorderUI(root, Telemetry(args.port, log))
    root.mainloop()


if __name__ == "__main__":
    main()
