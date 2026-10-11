"""Run the recomp and feed its emulated keyboard from a JSON script, recording frames as evidence.

The runtime shows its framebuffer in a window of class `XboxRecompFramebuffer`; with
`RECOMP_USB=1 RECOMP_KEYBOARD=1` a WM_KEYDOWN/WM_KEYUP posted to that window becomes a button
on the guest's emulated pad.  This driver launches the exe directly (the run collector stalls
once a D3D11 device exists), follows the window's frame dump, and executes the steps of a script
(see `config/key-scripts/`) decided by frame content, saving a PNG before and after every step
plus a sparse timeline of frames whose mean moved.

Why a key press is *held*: the guest samples the pad about once per game frame, and at the title
that is one frame per 1-2 s, so a brief tap usually falls between two samples and is never seen
(1 of ~40 taps registered).  One KEYDOWN, a hold longer than several frame periods, then one
KEYUP guarantees a sample lands inside the press.  No key-repeat is sent during the hold.

Script steps (JSON list, run in order):
  {"wait_title": {"timeout": 300}}                          fails the script on timeout
  {"wait_screen": {"screen": "menu", "timeout": 60}}        wait for a named screen (title, menu)
  {"key_until": {"vk": "RETURN", "until": "menu", "every": 6, "max": 10}}
                                                            press until a named screen appears
  {"key": {"vk": "RETURN", "hold": 2.0}}                    name from the table, or a number
  {"sleep": 5}
  {"wait_change": {"timeout": 30, "min_mean_delta": 8}}     records, never fails
  {"wait_stable": {"timeout": 30, "seconds": 3, "max_mean_delta": 2}}
  {"snap": "name"}

Output goes to `logs/keyscript-<label>/` (frames, save root, jsrf_run.log, result.json).  The
save root is private to the run, so the default save images are never touched.  No RECOMP_PAD_*
switch is set: the pad is driven through the emulated USB path, not injected.

Usage:  python -X utf8 scripts/key-script-run.py <label> <seconds> <script.json> [KEY=VALUE ...]

Windows only (window enumeration and PostMessage use user32).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "Release" / "jsrf_recomp.exe"

WM_KEYDOWN, WM_KEYUP = 0x0100, 0x0101
WINDOW_CLASS = b"XboxRecompFramebuffer"

VK_NAMES = {
    "BACK": 0x08, "RETURN": 0x0D, "ESCAPE": 0x1B, "SPACE": 0x20,
    "LEFT": 0x25, "UP": 0x26, "RIGHT": 0x27, "DOWN": 0x28,
}
VK_NAMES.update({chr(c): c for c in range(ord("A"), ord("Z") + 1)})
VK_NAMES.update({chr(c): c for c in range(ord("0"), ord("9") + 1)})

# Defaults for the launcher; KEY=VALUE arguments override them.
DEFAULT_ENV = {
    "RECOMP_APU_TRAP": "1",
    "RECOMP_PB_EXEC": "1",
    "RECOMP_FB_WINDOW": "1",
    "RECOMP_FB_PRESENT_DUMP_EVERY": "10",
    "JSRF_COLLECTED": "1",
    "RECOMP_GPU_BACKEND": "d3d11",
    "RECOMP_TRACE_BUDGET": "0",
    "RECOMP_USB": "1",
    "RECOMP_KEYBOARD": "1",
    "RECOMP_INPUT_DIAG": "1",
    "RECOMP_KEY_TRACE": "1",
    "RECOMP_USB_STATS": "1",
    "RECOMP_FB_WINDOW_DUMP_EVERY": "30",
}

# Title detector: near-white pixels in the "PLEASE PRESS START TO BEGIN" band of a 640x480 frame
# (rows 220..256, columns 185..460, every 2nd pixel).  Counts >= 300 on the title, peak ~527.
TITLE_BAND = (220, 256, 185, 460)
TITLE_REF_SIZE = (640, 480)
TITLE_MIN_WHITE = 300
# The JETSETRADIOFUTURE wordmark: 2835 white samples on every measured title frame, under 600 on the
# boot logos, so the press band alone cannot mistake a logo's white text for the title.
MARK_BAND = (300, 372, 135, 505)
MARK_MIN_WHITE = 2500
# The main menu's highlighted entry (NEW GAME): orange text in this band, 1623 samples on the menu, 0 on
# the title, its attract demo and the boot logos.
MENU_BAND = (215, 265, 225, 440)
MENU_MIN_ORANGE = 600
TITLE_WHITE_LEVEL = 200
POLL_SECONDS = 0.25
# Sparse timeline: sample every TIMELINE_SECONDS, keep when the mean moved more than the delta.
TIMELINE_SECONDS = 2.0
KEEP_MEAN_DELTA = 3.0

LOG_COUNTS = (
    ("key_down", re.compile(r"\[KEY\] down")),
    ("input_nonzero", re.compile(r"\[INPUT\].*?buttons=(?:0[xX])?0*[0-9a-fA-F]*[1-9a-fA-F][0-9a-fA-F]*")),
    ("icall", re.compile(r"\[ICALL\]")),
    ("fatal", re.compile(r"\[FATAL")),
    ("still_rejecting", re.compile(r"still rejecting")),
)
FBPRESENT_RE = re.compile(r"\[FBPRESENT\] t=\S+ presents=\d+.*")


class ScriptError(Exception):
    pass


def parse_vk(value):
    """Return a virtual-key code from a name in VK_NAMES or a number (int or '0x0D')."""
    if isinstance(value, bool):
        raise ScriptError("bad vk %r" % (value,))
    if isinstance(value, int):
        code = value
    elif isinstance(value, str):
        name = value.strip().upper()
        if name.startswith("VK_"):
            name = name[3:]
        if name in VK_NAMES:
            return VK_NAMES[name]
        try:
            code = int(name, 0)
        except ValueError:
            raise ScriptError("unknown vk %r" % (value,)) from None
    else:
        raise ScriptError("bad vk %r" % (value,))
    if not 1 <= code <= 0xFE:
        raise ScriptError("vk out of range: %r" % (value,))
    return code


def _opts(kind, body, defaults):
    """Merge a step's option dict over its defaults, rejecting unknown keys."""
    if body is None:
        body = {}
    if not isinstance(body, dict):
        raise ScriptError("%s: options must be an object" % kind)
    unknown = set(body) - set(defaults)
    if unknown:
        raise ScriptError("%s: unknown option(s) %s" % (kind, ", ".join(sorted(unknown))))
    merged = dict(defaults)
    merged.update(body)
    return merged


def _num(kind, key, value, minimum=0.0):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or value < minimum:
        raise ScriptError("%s: %s must be a number >= %s" % (kind, key, minimum))
    return float(value)


def parse_script(data):
    """Validate a decoded script and return a list of (kind, options) tuples."""
    if not isinstance(data, list) or not data:
        raise ScriptError("script must be a non-empty JSON list of steps")
    steps = []
    for i, raw in enumerate(data):
        if not isinstance(raw, dict) or len(raw) != 1:
            raise ScriptError("step %d: must be an object with exactly one key" % i)
        (kind, body), = raw.items()
        if kind == "wait_title":
            o = _opts(kind, body, {"timeout": 300})
            o["timeout"] = _num(kind, "timeout", o["timeout"], 1)
        elif kind == "key":
            o = _opts(kind, body, {"vk": None, "hold": 2.0})
            if o["vk"] is None:
                raise ScriptError("key: vk is required")
            o["vk"] = parse_vk(o["vk"])
            o["hold"] = _num(kind, "hold", o["hold"], 0)
        elif kind == "sleep":
            o = {"seconds": _num(kind, "seconds", body, 0)}
        elif kind == "wait_change":
            o = _opts(kind, body, {"timeout": 30, "min_mean_delta": 8})
            o["timeout"] = _num(kind, "timeout", o["timeout"], 1)
            o["min_mean_delta"] = _num(kind, "min_mean_delta", o["min_mean_delta"], 0)
        elif kind == "wait_stable":
            o = _opts(kind, body, {"timeout": 30, "seconds": 3, "max_mean_delta": 2})
            o["timeout"] = _num(kind, "timeout", o["timeout"], 1)
            o["seconds"] = _num(kind, "seconds", o["seconds"], 0)
            o["max_mean_delta"] = _num(kind, "max_mean_delta", o["max_mean_delta"], 0)
        elif kind == "key_until":
            o = _opts(kind, body, {"vk": None, "hold": 2.0, "until": None, "every": 6.0, "max": 10})
            if o["vk"] is None or o["until"] not in SCREENS:
                raise ScriptError("key_until: vk and until (one of %s) are required" % ", ".join(sorted(SCREENS)))
            o["vk"] = parse_vk(o["vk"])
            o["hold"] = _num(kind, "hold", o["hold"], 0)
            o["every"] = _num(kind, "every", o["every"], 0)
            o["max"] = int(_num(kind, "max", o["max"], 1))
        elif kind == "wait_screen":
            o = _opts(kind, body, {"screen": None, "timeout": 60})
            if o["screen"] not in SCREENS:
                raise ScriptError("wait_screen: screen must be one of %s" % ", ".join(sorted(SCREENS)))
            o["timeout"] = _num(kind, "timeout", o["timeout"], 1)
        elif kind == "snap":
            if not isinstance(body, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+", body):
                raise ScriptError("snap: name must be a simple string")
            o = {"name": body}
        else:
            raise ScriptError("step %d: unknown step %r" % (i, kind))
        steps.append((kind, o))
    return steps


def title_white_count(a, band_rect=TITLE_BAND):
    """Count near-white pixels in a band (default the press-start band), scaled to the frame size of `a` (HxWx3)."""
    h, w = a.shape[0], a.shape[1]
    top, bottom, left, right = band_rect
    rw, rh = TITLE_REF_SIZE
    t, b = int(round(top * h / rh)), int(round(bottom * h / rh))
    l, r = int(round(left * w / rw)), int(round(right * w / rw))
    band = a[t:b:2, l:r:2, :3]
    lvl = TITLE_WHITE_LEVEL
    return int(((band[:, :, 0] > lvl) & (band[:, :, 1] > lvl) & (band[:, :, 2] > lvl)).sum())


def is_title(a):
    """True when both the press-start band and the wordmark band reach their 640x480 thresholds, scaled by the frame's area."""
    rw, rh = TITLE_REF_SIZE
    scale = (a.shape[0] * a.shape[1]) / (rw * rh)
    return (title_white_count(a) >= TITLE_MIN_WHITE * scale
            and title_white_count(a, MARK_BAND) >= MARK_MIN_WHITE * scale)


def menu_orange_count(a):
    """Orange pixels (the highlighted menu entry) in the menu band, scaled to the frame size of `a`."""
    h, w = a.shape[0], a.shape[1]
    top, bottom, left, right = MENU_BAND
    rw, rh = TITLE_REF_SIZE
    band = a[int(top * h / rh):int(bottom * h / rh), int(left * w / rw):int(right * w / rw), :3]
    return int(((band[:, :, 0] > 160) & (band[:, :, 1] > 100) & (band[:, :, 2] < 90)).sum())


def is_menu(a):
    """True on the main menu (NEW GAME / LOAD GAME / VS MODE)."""
    rw, rh = TITLE_REF_SIZE
    return menu_orange_count(a) >= MENU_MIN_ORANGE * (a.shape[0] * a.shape[1]) / (rw * rh)


SCREENS = {"title": is_title, "menu": is_menu}


def mean_delta(a, b):
    """Mean absolute per-channel difference of two frames; infinite when the sizes differ."""
    import numpy as np

    if a is None or b is None or a.shape != b.shape:
        return float("inf")
    return float(np.abs(a.astype(np.int16) - b.astype(np.int16)).mean())


def load_frame(path):
    """Read an image file into an HxWx3 uint8 array, or None if it is missing or half-written."""
    from PIL import Image
    import numpy as np

    try:
        with Image.open(path) as im:
            return np.asarray(im.convert("RGB")).copy()
    except Exception:
        return None


def save_png(a, path):
    from PIL import Image

    Image.fromarray(a).save(path)


class Window:
    """Finds the framebuffer window by class name and posts key messages to it."""

    def __init__(self):
        import ctypes
        import ctypes.wintypes as wt

        self.ctypes, self.wt = ctypes, wt
        self.user32 = ctypes.windll.user32
        self.user32.GetClassNameA.argtypes = [wt.HWND, ctypes.c_char_p, ctypes.c_int]
        self.user32.PostMessageA.argtypes = [wt.HWND, ctypes.c_uint, wt.WPARAM, wt.LPARAM]
        self.enum_proc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)

    def find(self):
        found = []

        def cb(h, _l):
            buf = self.ctypes.create_string_buffer(256)
            self.user32.GetClassNameA(h, buf, 256)
            if buf.value == WINDOW_CLASS:
                found.append(h)
            return True

        self.user32.EnumWindows(self.enum_proc(cb), 0)
        return found[0] if found else None

    def key(self, hwnd, vk, down):
        self.user32.PostMessageA(hwnd, WM_KEYDOWN if down else WM_KEYUP, vk, 0)


def game_running():
    """True when a jsrf_recomp.exe process already exists."""
    r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq jsrf_recomp.exe", "/NH"],
                       capture_output=True, text=True)
    return "jsrf_recomp.exe" in r.stdout


def count_log(path):
    """Return ({name: count}, last [FBPRESENT] line) for the run log."""
    counts = {name: 0 for name, _ in LOG_COUNTS}
    last = None
    try:
        text = Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return counts, last
    for line in text.splitlines():
        for name, rx in LOG_COUNTS:
            if rx.search(line):
                counts[name] += 1
        m = FBPRESENT_RE.search(line)
        if m:
            last = m.group(0).strip()
    return counts, last


class Run:
    """The launched game, its frame dump, and the step executor."""

    def __init__(self, out, proc, live, window, deadline, t0, log):
        self.out, self.proc, self.live, self.win = out, proc, live, window
        self.deadline, self.t0, self.log = deadline, t0, log
        self.hwnd = None
        self.frame = None
        self.frame_id = 0            # increments whenever a new frame is read
        self._sig = None
        self.timeline_seq = 0
        self._tl_next = 0.0
        self._tl_mean = None

    def elapsed(self):
        return time.time() - self.t0

    def alive(self):
        return self.proc.poll() is None

    def expired(self):
        return self.elapsed() >= self.deadline

    def tick(self):
        """Poll window and frame dump once, keep the timeline, and sleep one poll period."""
        if self.hwnd is None:
            self.hwnd = self.win.find()
            if self.hwnd:
                self.log("window found hwnd=%d" % self.hwnd)
        try:
            st = self.live.stat()
            sig = (st.st_mtime_ns, st.st_size)
        except OSError:
            sig = None
        if sig is not None and sig != self._sig:
            a = load_frame(self.live)
            if a is not None:
                self._sig = sig
                self.frame = a
                self.frame_id += 1
        el = self.elapsed()
        if self.frame is not None and el >= self._tl_next:
            self._tl_next = el + TIMELINE_SECONDS
            mean = float(self.frame.mean())
            if self._tl_mean is None or abs(mean - self._tl_mean) > KEEP_MEAN_DELTA:
                self._tl_mean = mean
                name = "%05d_%06.1fs_m%03d.png" % (self.timeline_seq, el, int(mean))
                save_png(self.frame, self.out / "timeline" / name)
                self.timeline_seq += 1
        time.sleep(POLL_SECONDS)

    def pause(self, seconds):
        end = time.time() + seconds
        while time.time() < end and self.alive() and not self.expired():
            self.tick()

    def snap(self, name):
        if self.frame is None:
            self.log("snap %s: no frame yet" % name)
            return None
        path = self.out / (name + ".png")
        save_png(self.frame, path)
        return path.name

    def wait_for(self, timeout, pred):
        """Tick until pred() is true, the timeout passes, or the run ends; return satisfied."""
        end = time.time() + timeout
        while self.alive() and not self.expired():
            if pred():
                return True
            if time.time() >= end:
                return False
            self.tick()
        return False

    def run_step(self, kind, o):
        """Execute one step and return its result dict (without timing)."""
        if kind == "wait_title":
            self.tick()
            seen = {"peak": 0}

            def pred():
                if self.frame is None:
                    return False
                seen["peak"] = max(seen["peak"], title_white_count(self.frame))
                return is_title(self.frame)

            ok = self.wait_for(o["timeout"], pred)
            return {"satisfied": ok, "peak_white": seen["peak"]}
        if kind == "key":
            self.wait_for(30, lambda: self.hwnd is not None)
            if self.hwnd is None:
                return {"sent": False, "error": "no window"}
            self.win.key(self.hwnd, o["vk"], True)
            self.log("KEYDOWN vk=0x%02X" % o["vk"])
            try:
                self.pause(o["hold"])
            finally:
                self.win.key(self.hwnd, o["vk"], False)
                self.log("KEYUP vk=0x%02X" % o["vk"])
            return {"sent": True, "vk": o["vk"], "hold": o["hold"]}
        if kind == "sleep":
            self.pause(o["seconds"])
            return {}
        if kind == "wait_change":
            self.tick()
            base = self.frame
            best = {"max": 0.0}

            def pred():
                nonlocal base
                if base is None:
                    base = self.frame
                    return False
                d = mean_delta(self.frame, base)
                if d != float("inf"):
                    best["max"] = max(best["max"], d)
                return d > o["min_mean_delta"]

            ok = self.wait_for(o["timeout"], pred)
            return {"satisfied": ok, "changed": ok, "max_mean_delta": round(best["max"], 2)}
        if kind == "wait_stable":
            self.tick()
            state = {"prev": self.frame, "id": self.frame_id, "since": time.time()}

            def pred():
                if self.frame_id != state["id"]:
                    d = mean_delta(self.frame, state["prev"])
                    state["prev"], state["id"] = self.frame, self.frame_id
                    if d > o["max_mean_delta"]:
                        state["since"] = time.time()
                return (state["prev"] is not None
                        and time.time() - state["since"] >= o["seconds"])

            ok = self.wait_for(o["timeout"], pred)
            return {"satisfied": ok}
        if kind == "key_until":
            test = SCREENS[o["until"]]
            for n in range(1, o["max"] + 1):
                self.run_step("key", {"vk": o["vk"], "hold": o["hold"]})
                if self.wait_for(o["every"], lambda: self.frame is not None and test(self.frame)):
                    return {"satisfied": True, "presses": n}
            return {"satisfied": False, "presses": o["max"]}
        if kind == "wait_screen":
            test = SCREENS[o["screen"]]
            ok = self.wait_for(o["timeout"], lambda: self.frame is not None and test(self.frame))
            return {"satisfied": ok}
        if kind == "snap":
            self.tick()
            return {"file": self.snap(o["name"])}
        raise ScriptError("unknown step %r" % kind)


def parse_args(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("label")
    ap.add_argument("seconds", type=int, help="run deadline; the game is killed then")
    ap.add_argument("script", type=Path, help="JSON key script (see config/key-scripts/)")
    ap.add_argument("overrides", nargs="*", metavar="KEY=VALUE",
                    help="extra environment overrides for the game")
    args = ap.parse_args(argv)
    for kv in args.overrides:
        if "=" not in kv or kv.startswith("="):
            ap.error("override must be KEY=VALUE: %r" % kv)
    return args


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        steps = parse_script(json.loads(args.script.read_text(encoding="utf-8")))
    except (OSError, ValueError, ScriptError) as e:
        print("bad script %s: %s" % (args.script, e), file=sys.stderr)
        return 2
    if not EXE.is_file():
        print("missing %s" % EXE, file=sys.stderr)
        return 2
    if game_running():
        print("another jsrf_recomp.exe is running; stop it first", file=sys.stderr)
        return 2

    out = ROOT / "logs" / ("keyscript-%s" % args.label)
    frames = out / "frames"
    (out / "timeline").mkdir(parents=True, exist_ok=True)
    frames.mkdir(parents=True, exist_ok=True)
    (out / "save").mkdir(exist_ok=True)
    # Trailing underscore keeps the window dump's literal name distinct from the executor's
    # own "%s%05d.bmp" dumps; the runtime writes the prefix verbatim.
    live = frames / "w_"
    logpath = out / "jsrf_run.log"
    keylog = out / "keyscript.log"

    def log(msg):
        line = "%s %s" % (time.strftime("%H:%M:%S"), msg)
        print(line, flush=True)
        with open(keylog, "a", encoding="utf-8") as f:
            f.write(line + "\n")

    env = dict(os.environ)
    env.update(DEFAULT_ENV)
    env.update({"JSRF_LOG_PATH": str(logpath), "RECOMP_FB_DUMP": str(live)})
    for kv in args.overrides:
        k, v = kv.split("=", 1)
        env[k] = v

    log("=== start label=%s seconds=%d script=%s ===" % (args.label, args.seconds, args.script))
    stdout = open(out / "run.out", "w")
    proc = subprocess.Popen([str(EXE), "--save-root=" + str(out / "save")], cwd=str(ROOT),
                            env=env, stdout=stdout, stderr=subprocess.STDOUT)
    t0 = time.time()
    log("game pid=%d started" % proc.pid)
    run = Run(out, proc, live, Window(), args.seconds, t0, log)

    results = []
    script_ok = True
    killed = False
    try:
        for n, (kind, o) in enumerate(steps):
            if not run.alive() or run.expired():
                results.append({"n": n, "step": kind, "skipped": True})
                script_ok = False
                continue
            tag = "%02d_%s" % (n, kind)
            start = run.elapsed()
            run.tick()
            log("step %d %s %s" % (n, kind, json.dumps(o)))
            before = run.snap(tag + "_before")
            res = run.run_step(kind, o)
            run.tick()
            after = run.snap(tag + "_after")
            end = run.elapsed()
            res.update({"n": n, "step": kind, "options": o, "start_s": round(start, 1),
                        "end_s": round(end, 1), "before": before, "after": after})
            results.append(res)
            log("step %d done t=%.1fs %s" % (n, end, json.dumps(
                {k: v for k, v in res.items() if k in ("satisfied", "sent", "max_mean_delta")})))
            if kind == "wait_title" and not res["satisfied"]:
                log("FAIL: title not seen")
                script_ok = False
                break
        # Keep recording until the deadline or the game exits.
        while run.alive() and not run.expired() and script_ok:
            run.tick()
    finally:
        if run.alive():
            proc.kill()
            killed = True
        try:
            proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            pass
        stdout.close()

    counts, last_present = count_log(logpath)
    result = {
        "label": args.label,
        "script": str(args.script),
        "seconds": args.seconds,
        "overrides": args.overrides,
        "script_ok": script_ok and len(results) == len(steps)
                     and not any(r.get("skipped") for r in results),
        "steps": results,
        "process": {"killed_at_deadline_or_failure": killed, "exit_code": proc.returncode,
                    "ran_s": round(run.elapsed(), 1)},
        "log_counts": counts,
        "last_fbpresent": last_present,
        "timeline_frames": run.timeline_seq,
    }
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    log("=== end script_ok=%s ran=%.1fs killed=%s exit=%s ===" % (
        result["script_ok"], run.elapsed(), killed, proc.returncode))
    for r in results:
        log("  %s %-12s %6s-%6ss %s" % (
            r["n"], r["step"], r.get("start_s", "-"), r.get("end_s", "-"),
            "skipped" if r.get("skipped") else
            ("satisfied=%s" % r["satisfied"] if "satisfied" in r else "ok")))
    log("counts %s" % json.dumps(counts))
    log("last %s" % last_present)
    log("result %s" % (out / "result.json"))
    return 0 if result["script_ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
