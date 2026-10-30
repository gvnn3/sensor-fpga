#!/usr/bin/env python3
"""Agent driver for the sensor-fpga tricorder demo.

Two halves: the Nexys A7 board (xsdb + UART) and the Tk telemetry GUI
(tools/tricorder_ui.py).  Each subcommand is one thing an agent needs
to do; all state lives under --work (default: $TRICORDER_WORK or
/tmp/tricorder-run).

  board commands (need the Nexys A7 on USB)
    console       start background UART capture -> <work>/console.log
    boot          program bitstream, stage partials, boot RTOS via xsdb
    wait-boot     block until the RTOS banner shows in console.log
    telem         grab one UDP telemetry frame from the board -> stdout
    kill-console  release /dev/ttyUSB1
    swap RM       poke ui_live.req_rm over xsdb (RM = pdm|sonar|a|b), wait
                  for the type=dfx frame, assert sig=ok and to==RM
    rotate        pdm -> sonar -> b -> a in one xsdb session; table of
                  swap_us / flash_us per hop; exit 1 on any bad hop

  GUI commands (work with or without the board)
    ui [--port N] launch tools/tricorder_ui.py on DISPLAY (default :0)
    fake [--port N] [--dfx]
                  broadcast sample_frame.txt (and optionally a type=dfx
                  swap frame) to 127.0.0.1:N - drives the GUI headless
    shot [--out PNG]
                  screenshot the tricorder window (xwd -id, no root grab)
    kill-ui       stop every tricorder_ui.py

Stdlib + Pillow (for shot).  Run from the repo root.
"""

import argparse
import os
import pathlib
import re
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

REPO = pathlib.Path(__file__).resolve().parents[3]
HERE = pathlib.Path(__file__).resolve().parent
XSDB = pathlib.Path.home() / "Vivado/2025.2/Vivado/bin/xsdb"
UART = "/dev/ttyUSB1"
WIN_TITLE = "TR-590 MK VII TRICORDER"
ELF = REPO / "build/vitis-ws/tricorder_rtos/build/tricorder_rtos.elf"
RM_IDS = {"pdm": 0, "sonar": 1, "b": 2, "a": 3}      # ui_live.req_rm encoding
RM_NAMES = {v: k.upper() for k, v in RM_IDS.items()}


def work_dir(args):
    d = pathlib.Path(args.work)
    d.mkdir(parents=True, exist_ok=True)
    return d


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, text=True, capture_output=True, **kw)


# ---- board -----------------------------------------------------------

def cmd_console(args):
    w = work_dir(args)
    if subprocess.run(["pgrep", "-f", f"^cat {UART}"], capture_output=True).stdout.strip():
        print("console already running")
        return 0
    r = sh(f"stty -F {UART} 115200 raw -echo")
    if r.returncode:
        sys.exit(f"stty failed: {r.stderr.strip()} (board off? lsusb | grep 0403:6010)")
    log = w / "console.log"
    subprocess.Popen(f"exec cat {UART}", shell=True,
                     stdout=open(log, "ab"), stderr=subprocess.STDOUT,
                     start_new_session=True)
    print(f"console -> {log}")
    return 0


def cmd_kill_console(args):
    subprocess.run(["pkill", "-f", f"^cat {UART}"])
    print("console released")
    return 0


def cmd_boot(args):
    if not XSDB.exists():
        sys.exit(f"xsdb not found at {XSDB}")
    for f in ("build/dfx/full_a.bit",
              "build/vitis-ws/tricorder_rtos/build/tricorder_rtos.elf"):
        if not (REPO / f).exists():
            sys.exit(f"missing {f} - run make -C hw static/dfx/rtos first")
    t0 = time.time()
    r = subprocess.run([str(XSDB), "hw/scripts/run_rtos_dfx.xsdb"],
                       cwd=REPO, text=True, capture_output=True, timeout=600)
    tail = "\n".join(r.stdout.splitlines()[-3:])
    print(tail)
    if "RTOS_DFX: running" not in r.stdout:
        print(r.stderr, file=sys.stderr)
        sys.exit("boot FAILED (no 'RTOS_DFX: running' from xsdb)")
    print(f"boot ok in {time.time() - t0:.0f}s")
    return 0


def cmd_wait_boot(args):
    log = work_dir(args) / "console.log"
    deadline = time.time() + args.timeout
    while time.time() < deadline:
        txt = log.read_text(errors="replace") if log.exists() else ""
        # banner from the ELF we just loaded is the LAST one
        if "RTOS: FreeRTOS on MicroBlaze" in txt and "NET: link up" in txt.split(
                "RTOS: FreeRTOS on MicroBlaze")[-1]:
            tail = txt.split("RTOS: FreeRTOS on MicroBlaze")[-1]
            bad = re.findall(r".*(MISMATCH|FAIL).*", tail)
            print("booted; link up" + (f"; ERRORS: {bad}" if bad else ""))
            return 1 if bad else 0
        time.sleep(1)
    sys.exit("timeout waiting for RTOS banner + link up in console.log")


def recv_frame(port, timeout):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", port))
    s.settimeout(timeout)
    while True:
        data, _ = s.recvfrom(4096)
        txt = data.decode("ascii", "replace")
        if "type=" not in txt:           # skip event/dfx frames
            return txt


def cmd_telem(args):
    try:
        txt = recv_frame(args.port, args.timeout)
    except socket.timeout:
        sys.exit(f"no telemetry on udp/{args.port} in {args.timeout}s")
    print(txt.rstrip())
    n = sum(1 for l in txt.splitlines() if "=" in l)
    print(f"# {n} keys", file=sys.stderr)
    return 0 if n == 75 else 1


# ---- DFX swaps -------------------------------------------------------

def ui_live_addr():
    """&ui_live from the ELF symtab - it moves between builds (0x80013F18
    in entry 39, 0x80013FA0 now), so never hardcode it."""
    r = subprocess.run(["nm", str(ELF)], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == "ui_live":
            return int(parts[0], 16)
    sys.exit(f"ui_live not in {ELF} symtab (nm rc={r.returncode})")


class DfxListener(threading.Thread):
    """Collect type=dfx frames from the telemetry port while xsdb pokes."""

    def __init__(self, port):
        super().__init__(daemon=True)
        self.frames, self.stop = [], threading.Event()
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.s.bind(("", port))
        self.s.settimeout(0.5)

    def run(self):
        while not self.stop.is_set():
            try:
                data, _ = self.s.recvfrom(4096)
            except socket.timeout:
                continue
            kv = dict(l.partition("=")[::2] for l in data.decode("ascii", "replace").splitlines() if "=" in l)
            if kv.get("type") == "dfx":
                self.frames.append(kv)


def xsdb_poke(work, addr, ids, gap_s):
    """One xsdb session: stop; mwr &ui_live.req_rm id; con - per id."""
    tcl = work / "poke.tcl"
    lines = ["connect", 'targets -set -filter {name =~ "*MicroBlaze*#0*"}']
    for i, rm in enumerate(ids):
        if i:
            lines.append(f"after {int(gap_s * 1000)}")
        lines += ["stop", f"mwr 0x{addr:08X} {rm}", "con",
                  f'puts "POKE {rm}"']
    lines.append("disconnect")
    tcl.write_text("\n".join(lines) + "\n")
    r = subprocess.run([str(XSDB), str(tcl)], cwd=REPO, text=True,
                       capture_output=True, timeout=120)
    pokes = r.stdout.count("POKE ")
    if pokes != len(ids):
        print(r.stdout[-800:], r.stderr[-800:], file=sys.stderr)
        sys.exit(f"xsdb poked {pokes}/{len(ids)} (board off? stale hw_server?)")


def current_rm(port, timeout):
    try:
        f = recv_frame(port, timeout)
    except socket.timeout:
        sys.exit(f"no telemetry on udp/{port} - board not booted?")
    kv = dict(l.partition("=")[::2] for l in f.splitlines() if "=" in l)
    return int(kv.get("rm", -1))


def run_swaps(args, ids):
    w = work_dir(args)
    if not XSDB.exists():
        sys.exit(f"xsdb not found at {XSDB}")
    addr = ui_live_addr()
    cur = current_rm(args.port, 5)
    if ids and ids[0] == cur:
        print(f"already on {RM_NAMES.get(cur, cur)}; skipping first hop")
        ids = ids[1:]
    if not ids:
        print("nothing to do - PASS")
        return 0
    lst = DfxListener(args.port)
    lst.start()
    t0 = time.time()
    xsdb_poke(w, addr, ids, args.gap)
    # dfx frame rides net_task's 1 Hz loop; give the last hop time to land
    deadline = time.time() + args.timeout
    while len(lst.frames) < len(ids) and time.time() < deadline:
        time.sleep(0.2)
    lst.stop.set()
    print(f"{'hop':<4}{'from':<7}{'to':<7}{'swap_us':>8}{'flash_us':>10}  sig")
    ok = True
    for i, rm in enumerate(ids):
        if i >= len(lst.frames):
            print(f"{i:<4}{'-':<7}{RM_NAMES[rm]:<7}{'-':>8}{'-':>10}  MISSING")
            ok = False
            continue
        f = lst.frames[i]
        good = f.get("sig") == "ok" and f.get("to") == str(rm)
        ok &= good
        print(f"{i:<4}{f.get('from_name','?'):<7}{f.get('to_name','?'):<7}"
              f"{f.get('swap_us','?'):>8}{f.get('flash_us','?'):>10}  "
              f"{f.get('sig','?')}{'' if good else '  <-- BAD'}")
    log = w / "console.log"
    if log.exists():
        tail = log.read_text(errors="replace")[-20000:]
        bad = [l for l in tail.splitlines() if "MISMATCH" in l or "DMA: error" in l]
        if bad:
            print("console:", *bad[-3:], sep="\n  ")
            ok = False
    print(f"{len(lst.frames)}/{len(ids)} dfx frames in {time.time() - t0:.1f}s - "
          + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def cmd_swap(args):
    return run_swaps(args, [RM_IDS[args.rm]])


def cmd_rotate(args):
    return run_swaps(args, [RM_IDS[x] for x in args.order.split(",")])


# ---- GUI -------------------------------------------------------------

def cmd_ui(args):
    w = work_dir(args)
    env = dict(os.environ, DISPLAY=args.display)
    log = open(w / "ui.log", "ab")
    p = subprocess.Popen([sys.executable, "tools/tricorder_ui.py",
                          "--port", str(args.port), "--log-dir", str(w / "frames")],
                         cwd=REPO, env=env, stdout=log, stderr=subprocess.STDOUT,
                         start_new_session=True)
    for _ in range(50):
        time.sleep(0.2)
        if p.poll() is not None:
            sys.exit(f"ui exited {p.returncode}: {(w / 'ui.log').read_text()[-500:]}")
        if find_window(args.display):
            print(f"ui pid {p.pid} window up on {args.display} (udp/{args.port})")
            return 0
    sys.exit("ui started but no window appeared in 10s")


def cmd_kill_ui(args):
    subprocess.run(["pkill", "-f", "tools/tricorder_ui.py"])
    print("ui stopped")
    return 0


def cmd_fake(args):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    frame = (HERE / "sample_frame.txt").read_text()
    for i in range(args.count):
        f = re.sub(r"^seq=\d+", f"seq={1000 + i}", frame, flags=re.M)
        f = re.sub(r"^uptime=\d+", f"uptime={60 + i}", f, flags=re.M)
        s.sendto(f.encode(), ("127.0.0.1", args.port))
        if args.dfx and i == 0:
            dfx = ("type=dfx\ndfx=1\nfrom=3\nfrom_name=A\nto=0\nto_name=PDM\n"
                   "from_bytes=311452\nto_bytes=409856\nswap_us=1028\n"
                   "flash_us=278220\nsig=ok\nswaps=1\nuptime=60\n")
            s.sendto(dfx.encode(), ("127.0.0.1", args.port))
        if i + 1 < args.count:
            time.sleep(args.interval)
    print(f"sent {args.count} frame(s) to udp/{args.port}" + (" + dfx" if args.dfx else ""))
    return 0


def find_window(display):
    r = sh(f"DISPLAY={display} xwininfo -root -tree")
    m = re.search(r"(0x[0-9a-f]+) \"" + re.escape(WIN_TITLE), r.stdout)
    return m.group(1) if m else None


def xwd_to_png(xwd, png):
    from PIL import Image
    f = xwd.read_bytes()
    h = struct.unpack(">25I", f[:100])
    hs, w, ht, bpl, ncol = h[0], h[4], h[5], h[12], h[19]
    rmask = h[14]
    off = hs + ncol * 12
    im = Image.frombuffer("RGB", (w, ht), f[off:off + ht * bpl], "raw",
                          "BGRX" if rmask == 0xFF0000 else "RGBX", bpl, 1)
    im.save(png)
    return im


def cmd_shot(args):
    w = work_dir(args)
    wid = find_window(args.display)
    if not wid:
        sys.exit(f"no '{WIN_TITLE}' window on {args.display} - run `driver.py ui` first")
    xwd = w / "shot.xwd"
    r = sh(f"DISPLAY={args.display} xwd -id {wid} -silent -out {xwd}")
    if r.returncode:
        sys.exit(f"xwd failed: {r.stderr.strip()}")
    out = pathlib.Path(args.out) if args.out else w / f"shot-{int(time.time())}.png"
    im = xwd_to_png(xwd, out)
    print(f"{out} {im.size[0]}x{im.size[1]}")
    return 0


# ---- main ------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--work", default=os.environ.get("TRICORDER_WORK", "/tmp/tricorder-run"))
    ap.add_argument("--display", default=os.environ.get("DISPLAY", ":0"))
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("console").set_defaults(fn=cmd_console)
    sub.add_parser("kill-console").set_defaults(fn=cmd_kill_console)
    sub.add_parser("boot").set_defaults(fn=cmd_boot)
    p = sub.add_parser("wait-boot"); p.add_argument("--timeout", type=int, default=90)
    p.set_defaults(fn=cmd_wait_boot)
    p = sub.add_parser("telem"); p.add_argument("--port", type=int, default=5001)
    p.add_argument("--timeout", type=float, default=5); p.set_defaults(fn=cmd_telem)
    for name, fn in (("swap", cmd_swap), ("rotate", cmd_rotate)):
        p = sub.add_parser(name)
        if name == "swap":
            p.add_argument("rm", choices=sorted(RM_IDS))
        else:
            p.add_argument("--order", default="pdm,sonar,b,a")
        p.add_argument("--port", type=int, default=5001)
        p.add_argument("--gap", type=float, default=3.0, help="s between pokes")
        p.add_argument("--timeout", type=float, default=10, help="s to wait for last dfx frame")
        p.set_defaults(fn=fn)
    p = sub.add_parser("ui"); p.add_argument("--port", type=int, default=5001)
    p.set_defaults(fn=cmd_ui)
    sub.add_parser("kill-ui").set_defaults(fn=cmd_kill_ui)
    p = sub.add_parser("fake"); p.add_argument("--port", type=int, default=5001)
    p.add_argument("--count", type=int, default=1); p.add_argument("--interval", type=float, default=1)
    p.add_argument("--dfx", action="store_true"); p.set_defaults(fn=cmd_fake)
    p = sub.add_parser("shot"); p.add_argument("--out"); p.set_defaults(fn=cmd_shot)
    args = ap.parse_args()
    sys.exit(args.fn(args))


if __name__ == "__main__":
    main()
