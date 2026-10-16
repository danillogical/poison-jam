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
  {"wait_screen": {"screen": "menu", "timeout": 60}}        wait for a named screen (title, menu, hud, dialogue, talk_icon)
  {"key_until": {"vk": "RETURN", "until": "menu", "every": 6, "max": 10}}
                                                            press until a named screen appears
  {"key": {"vk": "RETURN", "hold": 2.0, "snap": "n"}}       name from the table, or a number;
                                                            snap captures the frame while held
  {"key_down": "3"} / {"key_up": "3"}                       hold a key across steps; the guest
                                                            keeps it until key_up
  {"hold_keys": {"vks": ["3", "Z"], "hold": 2.0}}           hold several keys at once
  {"sleep": 5}
  {"wait_change": {"timeout": 30, "min_mean_delta": 8}}     records, never fails
  {"wait_stable": {"timeout": 30, "seconds": 3, "max_mean_delta": 2}}
  {"touch": "path"} / {"remove": "path"}                    create / delete a file (arms RECOMP_FRAME_TRACE)
  {"peek": {"name": "n", "va": "0xADDR", "size": 4, "expect": 0}}
                                                            read one guest value into peeks.jsonl
  {"peek": {"name": "n", "chain": ["0x22FCE0", "0x148", "0xCA4"], "type": "f32", "count": 3}}
                                                            follow a pointer chain (see resolve_chain), then
                                                            read `count` u32 or f32 values at the end
  {"ram_snap": "name"} / {"ram_snap": {"name": "n", "va": "0xADDR", "length": 0x1000}}
                                                            write a guest RAM range to ram/<name>.bin
  {"snap": "name"}
  {"bench": {"name": "garage", "seconds": 60, "poll_ms": 2}}
                                                            no input: poll the host's s_present_serial every poll_ms
                                                            (0.5-50, default 2) for `seconds`, then write the frame-time
                                                            distribution of that window only to bench.json, keyed by name
                                                            (see bench_intervals / bench_stats; a poll that sees the
                                                            serial jump by n > 1 is counted in aliased_polls)
  {"log_pos": "name"} / {"log_pos": {"name": "n", "objs": ["0x2C", "0x2E"]}}
                                                            append position, heading yaw and camera yaw of
                                                            guest objects (default 0x2C, the player) to positions.jsonl
  {"goto": {"target": [x, z], "radius": 25, "timeout": 120, "pulse": 0.4, "player_obj": "0x2C",
            "until": [{"off": "0x300", "eq": 1}], "snap": "name",
            "jump_when_stuck": false, "jump_hold": 0.3, "jump_follow": 1.0}}
                                                            closed-loop walk: each pulse reads the player's position and
                                                            the camera from guest memory, holds the camera-relative
                                                            numpad keys toward the target (steer_keys) for `pulse`
                                                            seconds, and repeats until within `radius` (horizontal), or
                                                            every `until` u32 at player_ch+off holds, or the timeout
                                                            (recorded, never fails the script).  One line per iteration
                                                            goes to goto.jsonl; five pulses that move under 5 units total
                                                            log `stuck` and try one sidestep pulse, alternating sides.
                                                            Stuck detection arms once the player has moved over 1 unit
                                                            from the start or 8 pulses have passed.  With
                                                            `jump_when_stuck`, stuck events cycle jump / sidestep left /
                                                            jump / sidestep right: a jump holds the steer keys plus Z
                                                            (VK 0x5A, the A button) for `jump_hold` seconds, then the
                                                            steer keys alone for `jump_follow` seconds.

`peek` and `ram_snap` read guest memory through ReadProcessMemory and change no guest state;
they are the only way to observe a guest value in a run, because this driver produces no dump.
Every press (`key`, `key_until`, `hold_keys`, `key_down`/`key_up`) also appends a record to
`presses.jsonl` (a `goto` pulse appends one record per key hold): the host's own view of what happened, read from the executable's `s_key_down`,
`s_present_serial` and `s_flips` globals and from the window's focus state. It answers whether a
press reached the guest at all, which a frame cannot.

Output goes to `logs/runs/<YYYYMMDD-HHMMSS-mmm>-keyscript-<label>/`, the same archive tree as
`run-jsrf.py`: frames, save root, jsrf_run.log, keyscript.log, result.json and a metadata.json
(exe hash, project/toolkit revision and dirty state, env switches, exploratory profile, log hash)
so `check-stop-chain.py` accepts the run as a citation.  The save root is private to the run, so
the default save images are never touched.  No RECOMP_PAD_* switch is set: the pad is driven
through the emulated USB path, not injected.

Usage:  python -X utf8 scripts/key-script-run.py <label> <seconds> <script.json> [KEY=VALUE ...]

Adopt a finished run from the old `logs/keyscript-<label>/` layout (copies it into the archive
tree; revisions are supplied by the caller and recorded as unmeasured):
        python -X utf8 scripts/key-script-run.py --adopt logs/keyscript-<label> \\
            --project-rev <sha> --toolkit-rev <sha> [--toolkit-dirty] [--project-dirty] [--exe-sha256 <hex>]

Windows only (window enumeration and PostMessage use user32).
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "Release"
EXE = BUILD / "jsrf_recomp.exe"
TOOLKIT = ROOT.parent / "xboxrecomp"
# Build outputs copied into the run directory; the collector is not used by this driver.
KEY_ARTIFACTS = ("jsrf_recomp.exe", "jsrf_recomp.pdb", "jsrf_recomp.map", "build-source.json")

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
# The most of each band that may be white on the title: text, not a filled screen. Measured on the
# title 0.19 (press) and 0.43 (wordmark); the white Smilebit card 0.61-0.68 and 1.00; an attract-city
# frame up to 0.77 on the wordmark band.
TITLE_MAX_FILL = 0.40
MARK_MAX_FILL = 0.70
# The main menu's highlighted entry (NEW GAME): orange text on a dark bar. Measured over 502 frames of
# the archived e2e runs, colour alone is NOT enough: the attract-mode city reaches 601 orange samples in
# this band against a 600 threshold, and it was accepted as the menu by the V3 run (2026-10-13). The real
# menu scores 1623. So the colour gate stays, and a structural witness decides.
MENU_BAND = (215, 265, 225, 440)
MENU_MIN_ORANGE = 600
# The structural witness: orange glyph pixels that have dark menu-bar pixels within 3 rows above AND
# below them. Measured over those 502 frames the separation is clean and wide -- 257 on all 17 real-menu
# frames (both back ends, every resolution-1 frame), and at most 66 on everything else (attract city,
# title, in-game, boot logos). 130 sits in that gap, so this is a structural test, not a tuned threshold.
MENU_LINE_NEW = (228, 258, 225, 440)     # the NEW GAME line: the orange glyphs
MENU_GLYPH_MIN = (200, 110, 210, 70)     # r >, g >, g <, b < : the highlight's glyph colour bounds
MENU_BAR_DARK = 115                      # max channel of a menu-bar (dark teal) pixel
MENU_BAR_GAP = 3                         # rows above/below that must contain bar pixels
MENU_MIN_BAR_GLYPHS = 130
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

# Guest RAM is 64 MiB (xboxrecomp/src/kernel/xbox_memory_layout.h XBOX_TOTAL_RAM), reachable
# both as canonical RAM at its own VA and through the contiguous window at 0x80000000.
GUEST_RAM_SIZE = 0x04000000
# Page size for the read fallback and for ram_snap alignment.
GUEST_PAGE_SIZE = 0x1000
# ReadProcessMemory chunk size; a failed chunk is retried page by page.
READ_CHUNK = 0x100000


BENCH_NAME_RE = r"[A-Za-z_][A-Za-z0-9_-]*"
BENCH_STALL_S = 0.1


def bench_intervals(samples):
    """Frame intervals from (t_seconds, present_serial) polls: (intervals_s, aliased_polls).

    An interval is the time between consecutive polls at which the serial increased, divided by
    the increase (the u32 serial may wrap).  A poll that saw an increase n > 1 yields n equal
    intervals and counts once in aliased_polls.  The first increase only sets the reference time:
    the window's first poll lands at an arbitrary phase of a frame, so it is not an interval end.
    """
    intervals, aliased, ref_t, prev = [], 0, None, None
    for t, serial in samples:
        if prev is not None:
            n = (serial - prev) & 0xFFFFFFFF
            if n:
                if ref_t is not None:
                    intervals.extend([(t - ref_t) / n] * n)
                    if n > 1:
                        aliased += 1
                ref_t = t
        prev = serial
    return intervals, aliased


def bench_poll_stats(samples):
    """Poll bookkeeping for (t, serial) samples: polls, median period, polls with an increase, presents."""
    gaps = sorted(b[0] - a[0] for a, b in zip(samples, samples[1:]))
    incr = sum(1 for a, b in zip(samples, samples[1:]) if (b[1] - a[1]) & 0xFFFFFFFF)
    presents = (samples[-1][1] - samples[0][1]) & 0xFFFFFFFF if samples else 0
    return {"polls": len(samples),
            "median_poll_period_ms": _median(gaps) * 1000.0 if gaps else None,
            "increment_polls": incr, "presents": presents}


def _median(values):
    v = sorted(values)
    if not v:
        return None
    mid = len(v) // 2
    return v[mid] if len(v) % 2 else (v[mid - 1] + v[mid]) / 2.0


def _percentile(sorted_values, q):
    """Linear-interpolated q-th percentile (0..100) of an ascending list."""
    if not sorted_values:
        return None
    pos = (len(sorted_values) - 1) * q / 100.0
    lo = int(math.floor(pos))
    hi = min(lo + 1, len(sorted_values) - 1)
    return sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * (pos - lo)


def bench_stats(intervals, duration, presents, aliased_polls=0, poll=None):
    """The bench.json measurements: intervals in seconds in, frame times in ms out.

    `poll` is the bench_poll_stats dict (optional); its fields are merged in and
    aliased_fraction is aliased_polls over polls that saw an increase.
    """
    ms = sorted(i * 1000.0 for i in intervals)
    poll = poll or {}
    incr = poll.get("increment_polls")
    out = {
        "duration_s": duration,
        "polls": poll.get("polls"),
        "median_poll_period_ms": poll.get("median_poll_period_ms"),
        "presents": presents,
        "presents_per_s": presents / duration if duration > 0 else None,
        "frame_ms_p50": _percentile(ms, 50),
        "frame_ms_p90": _percentile(ms, 90),
        "frame_ms_p99": _percentile(ms, 99),
        "frame_ms_max": ms[-1] if ms else None,
        "frame_ms_mean": sum(ms) / len(ms) if ms else None,
        "intervals": len(ms),
        "aliased_polls": aliased_polls,
        "increment_polls": incr,
        "aliased_fraction": aliased_polls / incr if incr else None,
        "stalls_over_100ms": sum(1 for i in intervals if i > BENCH_STALL_S),
    }
    return out


def bench_device(log_text):
    """The GPU named in the run log by the Vulkan backend, or None (the D3D11 path logs none)."""
    m = re.search(r"\[GPUVK\] device: (.+?) \(type", log_text or "")
    return m.group(1) if m else None


class ScriptError(Exception):
    pass


# Guest object table: obj(id) is the u32 at [[OBJ_TABLE_PTR] + OBJ_TABLE_BASE + 4*id].
OBJ_TABLE_PTR = 0x0022FCE0
OBJ_TABLE_BASE = 0x98
OFF_POS = 0xCA4          # 3 x f32 (x, y, z), y up
OFF_CAMERA = 0x334       # camera forward, 3 x f32; only x and z are used
OFF_HEADING = 0x34C      # (sin yaw, 0, cos yaw)
PLAYER_OBJ = 0x2C
VK_FORWARD, VK_BACK, VK_LEFT, VK_RIGHT = 0x68, 0x62, 0x64, 0x66   # numpad 8 / 2 / 4 / 6
# Sector k (45 degrees each, positive = right of the camera forward) -> keys held.
STEER_KEYS = {
    0: (VK_FORWARD,), 1: (VK_FORWARD, VK_RIGHT), 2: (VK_RIGHT,), 3: (VK_BACK, VK_RIGHT),
    4: (VK_BACK,), -4: (VK_BACK,), -3: (VK_BACK, VK_LEFT), -2: (VK_LEFT,), -1: (VK_FORWARD, VK_LEFT),
}
STUCK_PULSES = 5
STUCK_MIN_MOVE = 5.0
STUCK_ARM_MOVE = 1.0       # horizontal distance from the goto start that arms stuck detection
STUCK_ARM_PULSES = 8       # or this many pulses, whichever comes first
VK_JUMP = 0x5A             # Z on the emulated keyboard pad: the A button


def goto_stop_reason(dist, radius, until_results):
    """Why a goto should stop now: "radius", "until" (a non-empty list, all true), or None."""
    if dist <= radius:
        return "radius"
    if isinstance(until_results, list) and until_results and all(until_results):
        return "until"
    return None


def stuck_action(n):
    """Action for the n-th stuck event (0-based) when jumping is enabled."""
    if n % 2 == 0:
        return "jump"
    return "side_left" if n % 4 == 1 else "side_right"


def resolve_chain(read_u32, chain):
    """Follow a pointer chain and return the final address without reading it.

    The address starts at chain[0]; each later element does addr = read_u32(addr) + elem.
    A null pointer partway through raises ScriptError."""
    if not chain:
        raise ScriptError("empty pointer chain")
    addr = chain[0]
    for elem in chain[1:]:
        ptr = read_u32(addr)
        if ptr == 0:
            raise ScriptError("null pointer at 0x%08X in chain" % addr)
        addr = ptr + elem
    return addr


def wrap_angle(a):
    """Wrap radians into (-pi, pi]."""
    a = math.fmod(a, 2 * math.pi)
    if a > math.pi:
        a -= 2 * math.pi
    elif a <= -math.pi:
        a += 2 * math.pi
    return a


def relative_angle(dx, dz, fwd_x, fwd_z):
    """Radians in (-pi, pi] from the camera forward to the bearing (dx, dz); positive = right."""
    return wrap_angle(math.atan2(dx, dz) - math.atan2(fwd_x, fwd_z))


def steer_keys(dx, dz, fwd_x, fwd_z):
    """VK codes that walk toward (dx, dz) given the camera forward (fwd_x, fwd_z).

    The left stick is camera-relative, so the bearing is taken relative to the camera yaw
    (atan2(x, z)) and rounded to one of eight 45 degree sectors.  A zero-length (dx, dz) gives ()."""
    if dx == 0 and dz == 0:
        return ()
    sector = round(math.degrees(relative_angle(dx, dz, fwd_x, fwd_z)) / 45.0)
    return STEER_KEYS[int(sector)]


def object_chain(obj_id, off=0):
    """The pointer chain that ends at (guest object `obj_id`) + off."""
    return [OBJ_TABLE_PTR, OBJ_TABLE_BASE + 4 * obj_id, off]


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


def _parse_va(value, kind):
    """A guest VA from an int or a string parsed with int(x, 0)."""
    if isinstance(value, bool):
        raise ScriptError("%s: bad va %r" % (kind, value))
    if isinstance(value, int):
        va = value
    elif isinstance(value, str):
        try:
            va = int(value, 0)
        except ValueError:
            raise ScriptError("%s: bad va %r" % (kind, value)) from None
    else:
        raise ScriptError("%s: bad va %r" % (kind, value))
    if va < 0:
        raise ScriptError("%s: va must not be negative" % kind)
    return va


def _check_guest_range(va, length, kind, floor=0x1000):
    """Reject a range that is not inside canonical RAM or the contiguous window."""
    for lo, hi in ((floor, GUEST_RAM_SIZE), (0x80000000, 0x80000000 + GUEST_RAM_SIZE)):
        if va >= lo and va + length <= hi:
            return
    raise ScriptError("%s: 0x%08X+%d is outside guest RAM" % (kind, va, length))


def _parse_obj_ids(kind, values):
    """A non-empty list of guest object ids (ints or int(x, 0) strings, 0..255)."""
    if not isinstance(values, list) or not values:
        raise ScriptError("%s: objs must be a non-empty list of object ids" % kind)
    ids = []
    for v in values:
        i = _parse_va(v, kind)
        if i > 0xFF:
            raise ScriptError("%s: object id %r out of range" % (kind, v))
        ids.append(i)
    return ids


def read_host_pages(handle, address, length):
    """Read `length` bytes at host `address`; return (bytes, guest page offsets that failed).

    Observation only: ReadProcessMemory, never a write. A chunk that fails to read is retried
    page by page, so one unmapped page does not hide its readable neighbours. Unreadable pages
    are zero-filled and reported -- they are never presented as data."""
    import ctypes
    import ctypes.wintypes as wt

    k32 = ctypes.windll.kernel32
    k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                      ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    k32.ReadProcessMemory.restype = wt.BOOL
    buf = bytearray(length)
    failed = []

    def read_at(off, size):
        got = ctypes.c_size_t(0)
        dst = (ctypes.c_char * size).from_buffer(buf, off)
        ok = k32.ReadProcessMemory(handle, ctypes.c_void_p(address + off), dst, size,
                                   ctypes.byref(got))
        return bool(ok) and got.value == size

    off = 0
    while off < length:
        size = min(READ_CHUNK, length - off)
        if not read_at(off, size):
            for page in range(off, off + size, GUEST_PAGE_SIZE):
                psize = min(GUEST_PAGE_SIZE, length - page)
                if not read_at(page, psize):
                    failed.append(page)
        off += size
    return bytes(buf), failed


class GuestMemory:
    """Read guest memory from the live game process. Observation only: never writes.

    Guest VA `v` lives at host address `v + g_xbox_mem_offset`, where the offset is the
    runtime value of the executable's own `g_xbox_mem_offset` global (every mapped window,
    including the contiguous one, is reachable through it)."""

    def __init__(self, pid, map_path):
        self.pid = pid
        self.map_path = Path(map_path)
        self.handle = None
        self.offset = None
        self._base = None
        self._symbols = {}

    def _open(self):
        """Open the process and cache its module base. Idempotent."""
        if self.handle is not None:
            return
        import ctypes
        import ctypes.wintypes as wt

        k32 = ctypes.windll.kernel32
        k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
        k32.OpenProcess.restype = wt.HANDLE
        # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
        handle = k32.OpenProcess(0x0410, False, self.pid)
        if not handle:
            raise ScriptError("cannot read guest memory: OpenProcess winerror=%d"
                              % ctypes.get_last_error())
        self.handle = handle
        k32.K32EnumProcessModulesEx.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD,
                                                ctypes.POINTER(wt.DWORD), wt.DWORD]
        arr = (ctypes.c_void_p * 64)()
        needed = wt.DWORD(0)
        if not k32.K32EnumProcessModulesEx(handle, arr, ctypes.sizeof(arr),
                                           ctypes.byref(needed), 3):
            raise ScriptError("cannot read guest memory: EnumProcessModules winerror=%d"
                              % ctypes.get_last_error())
        self._base = int(arr[0])

    def host_address(self, symbol):
        """The host address of a symbol in the executable, from the map file."""
        if symbol in self._symbols:
            return self._symbols[symbol]
        self._open()
        here = str(Path(__file__).resolve().parent)
        if here not in sys.path:
            sys.path.insert(0, here)
        import jsrf_dump
        try:
            addr, preferred = jsrf_dump.map_symbol(str(self.map_path), symbol)
        except Exception as e:                      # CaptureError or a missing symbol
            raise ScriptError("host symbol %s: %s" % (symbol, e))
        value = self._base + (addr - preferred)
        self._symbols[symbol] = value
        return value

    def host_read(self, symbol, size, offset=0):
        """A little-endian unsigned integer read from a host symbol. Observation only."""
        import ctypes
        import ctypes.wintypes as wt

        self._open()
        k32 = ctypes.windll.kernel32
        k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                          ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        buf = (ctypes.c_ubyte * size)()
        n = ctypes.c_size_t(0)
        if not k32.ReadProcessMemory(self.handle, ctypes.c_void_p(self.host_address(symbol) + offset),
                                     buf, size, ctypes.byref(n)):
            raise ScriptError("host symbol %s unreadable" % symbol)
        return int.from_bytes(bytes(buf), "little")

    def _resolve(self):
        import ctypes
        import ctypes.wintypes as wt

        self._open()
        k32 = ctypes.windll.kernel32
        handle = self.handle
        here = str(Path(__file__).resolve().parent)
        if here not in sys.path:
            sys.path.insert(0, here)
        import jsrf_dump
        addr, preferred = jsrf_dump.map_symbol(str(self.map_path), "g_xbox_mem_offset")
        slot = self._base + (addr - preferred)
        raw = ctypes.c_int64()
        if not k32.ReadProcessMemory(handle, ctypes.c_void_p(slot), ctypes.byref(raw), 8, None):
            raise ScriptError("cannot read guest memory: g_xbox_mem_offset unreadable")
        if raw.value == 0:
            raise ScriptError("g_xbox_mem_offset is 0: guest memory not mapped yet")
        self.offset = raw.value

    def read(self, va, length):
        """(bytes, unreadable guest page VAs) at guest VA `va`."""
        if self.offset is None:
            self._resolve()
        data, failed = read_host_pages(self.handle, self.offset + va, length)
        return data, [va + off for off in failed]

    def close(self):
        if self.handle:
            import ctypes
            ctypes.windll.kernel32.CloseHandle(self.handle)
            self.handle = None


def parse_script(data):
    """Validate a decoded script and return a list of (kind, options) tuples."""
    if not isinstance(data, list) or not data:
        raise ScriptError("script must be a non-empty JSON list of steps")
    steps = []
    peek_names, snap_names = set(), set()
    bench_names = set()
    held = set()          # vks a key_down opened and no key_up has closed yet
    for i, raw in enumerate(data):
        if not isinstance(raw, dict) or len(raw) != 1:
            raise ScriptError("step %d: must be an object with exactly one key" % i)
        (kind, body), = raw.items()
        if kind == "wait_title":
            o = _opts(kind, body, {"timeout": 300})
            o["timeout"] = _num(kind, "timeout", o["timeout"], 1)
        elif kind == "key":
            o = _opts(kind, body, {"vk": None, "hold": 2.0, "snap": None})
            if o["vk"] is None:
                raise ScriptError("key: vk is required")
            o["vk"] = parse_vk(o["vk"])
            o["hold"] = _num(kind, "hold", o["hold"], 0)
            if o["vk"] in held:
                raise ScriptError("key: vk 0x%02X is held by key_down" % o["vk"])
            if o["snap"] is not None and not re.fullmatch(r"[A-Za-z0-9_.-]+", str(o["snap"])):
                raise ScriptError("key: snap must match [A-Za-z0-9_.-]+")
        elif kind == "hold_keys":
            o = _opts(kind, body, {"vks": None, "hold": 1.0})
            if not isinstance(o["vks"], list) or not 2 <= len(o["vks"]) <= 4:
                raise ScriptError("hold_keys: vks must be a list of 2 to 4 keys")
            o["vks"] = [parse_vk(v) for v in o["vks"]]
            if len(set(o["vks"])) != len(o["vks"]):
                raise ScriptError("hold_keys: vks must be distinct")
            for vk in o["vks"]:
                if vk in held:
                    raise ScriptError("hold_keys: vk 0x%02X is held by key_down" % vk)
            o["hold"] = _num(kind, "hold", o["hold"], 0)
        elif kind == "key_down":
            o = _opts(kind, body if isinstance(body, dict) else {"vk": body}, {"vk": None})
            if o["vk"] is None:
                raise ScriptError("key_down: vk is required")
            o["vk"] = parse_vk(o["vk"])
            if o["vk"] in held:
                raise ScriptError("key_down: vk 0x%02X already held" % o["vk"])
            held.add(o["vk"])
        elif kind == "key_up":
            o = _opts(kind, body if isinstance(body, dict) else {"vk": body}, {"vk": None})
            if o["vk"] is None:
                raise ScriptError("key_up: vk is required")
            o["vk"] = parse_vk(o["vk"])
            if o["vk"] not in held:
                raise ScriptError("key_up: vk 0x%02X is not held" % o["vk"])
            held.discard(o["vk"])
        elif kind == "sleep":
            o = {"seconds": _num(kind, "seconds", body, 0)}
        elif kind == "bench":
            o = _opts(kind, body, {"name": None, "seconds": None, "poll_ms": 2})
            if not isinstance(o["name"], str) or not re.fullmatch(BENCH_NAME_RE, o["name"]):
                raise ScriptError("bench: name must be a simple identifier")
            if o["name"] in bench_names:
                raise ScriptError("bench: duplicate name %r" % o["name"])
            bench_names.add(o["name"])
            if o["seconds"] is None:
                raise ScriptError("bench: seconds is required")
            o["seconds"] = _num(kind, "seconds", o["seconds"], 0)
            if o["seconds"] <= 0:
                raise ScriptError("bench: seconds must be > 0")
            o["poll_ms"] = _num(kind, "poll_ms", o["poll_ms"], 0.5)
            if o["poll_ms"] > 50:
                raise ScriptError("bench: poll_ms must be between 0.5 and 50")
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
            o = _opts(kind, body, {"vk": None, "hold": 2.0, "until": None, "every": 6.0,
                                   "max": 10, "snap": None, "snap_after": 0})
            if o["vk"] is None or o["until"] not in SCREENS:
                raise ScriptError("key_until: vk and until (one of %s) are required" % ", ".join(sorted(SCREENS)))
            o["vk"] = parse_vk(o["vk"])
            o["hold"] = _num(kind, "hold", o["hold"], 0)
            o["every"] = _num(kind, "every", o["every"], 0)
            o["max"] = int(_num(kind, "max", o["max"], 1))
            o["snap_after"] = _num(kind, "snap_after", o["snap_after"], 0)
            if o["vk"] in held:
                raise ScriptError("key_until: vk 0x%02X is held by key_down" % o["vk"])
            if o["snap"] is not None and not re.fullmatch(r"[A-Za-z0-9_.-]+", str(o["snap"])):
                raise ScriptError("key_until: snap must match [A-Za-z0-9_.-]+")
        elif kind == "wait_screen":
            o = _opts(kind, body, {"screen": None, "timeout": 60})
            if o["screen"] not in SCREENS:
                raise ScriptError("wait_screen: screen must be one of %s" % ", ".join(sorted(SCREENS)))
            o["timeout"] = _num(kind, "timeout", o["timeout"], 1)
        elif kind in ("touch", "remove"):
            if not isinstance(body, str) or not body:
                raise ScriptError("%s: a file path is required" % kind)
            o = {"path": body}
        elif kind == "snap":
            if not isinstance(body, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+", body):
                raise ScriptError("snap: name must be a simple string")
            o = {"name": body}
        elif kind == "peek":
            o = _opts(kind, body, {"name": None, "va": None, "size": 4, "expect": None,
                                   "chain": None, "type": "u32", "count": 1})
            given = body or {}
            if not isinstance(o["name"], str) or not re.fullmatch(r"[A-Za-z0-9_.-]+", o["name"]):
                raise ScriptError("peek: name must be a simple string")
            if o["chain"] is not None:
                if o["va"] is not None or "size" in given:
                    raise ScriptError("peek: a chain peek takes type/count, not va or size")
                if not isinstance(o["chain"], list) or not o["chain"]:
                    raise ScriptError("peek: chain must be a non-empty list")
                o["chain"] = [_parse_va(e, "peek") for e in o["chain"]]
                if o["type"] not in ("u32", "f32"):
                    raise ScriptError("peek: type must be u32 or f32")
                if isinstance(o["count"], bool) or not isinstance(o["count"], int) \
                        or not 1 <= o["count"] <= 64:
                    raise ScriptError("peek: count must be an int from 1 to 64")
                _check_guest_range(o["chain"][0], 4 if len(o["chain"]) > 1 else 4 * o["count"], "peek")
                if o["expect"] is not None and (o["type"] != "u32" or o["count"] != 1):
                    raise ScriptError("peek: expect applies only to a single u32")
            else:
                if "type" in given or "count" in given:
                    raise ScriptError("peek: type/count need a chain")
                if o["va"] is None:
                    raise ScriptError("peek: va or chain is required")
                o["va"] = _parse_va(o["va"], "peek")
                if o["size"] not in (1, 2, 4):
                    raise ScriptError("peek: size must be 1, 2 or 4")
                _check_guest_range(o["va"], o["size"], "peek")
            if o["expect"] is not None:
                if isinstance(o["expect"], bool) or not isinstance(o["expect"], int) or o["expect"] < 0:
                    raise ScriptError("peek: expect must be a non-negative int")
        elif kind == "log_pos":
            if isinstance(body, str):
                body = {"name": body}
            o = _opts(kind, body, {"name": None, "objs": [PLAYER_OBJ]})
            if not isinstance(o["name"], str) or not re.fullmatch(r"[A-Za-z0-9_.-]+", o["name"]):
                raise ScriptError("log_pos: name must be a simple string")
            o["objs"] = _parse_obj_ids(kind, o["objs"])
        elif kind == "goto":
            o = _opts(kind, body, {"target": None, "radius": 25, "timeout": 120, "pulse": 0.4,
                                   "player_obj": PLAYER_OBJ, "until": [], "snap": None,
                                   "jump_when_stuck": False, "jump_hold": 0.3, "jump_follow": 1.0})
            t = o["target"]
            if (not isinstance(t, list) or len(t) != 2
                    or any(isinstance(v, bool) or not isinstance(v, (int, float))
                           or not math.isfinite(v) for v in t)):
                raise ScriptError("goto: target must be [x, z] numbers")
            o["target"] = [float(t[0]), float(t[1])]
            o["radius"] = _num(kind, "radius", o["radius"], 0)
            o["timeout"] = _num(kind, "timeout", o["timeout"], 1)
            o["pulse"] = _num(kind, "pulse", o["pulse"], 0.05)
            o["player_obj"] = _parse_obj_ids(kind, [o["player_obj"]])[0]
            if not isinstance(o["jump_when_stuck"], bool):
                raise ScriptError("goto: jump_when_stuck must be true or false")
            o["jump_hold"] = _num(kind, "jump_hold", o["jump_hold"], 0.05)
            o["jump_follow"] = _num(kind, "jump_follow", o["jump_follow"], 0)
            if not isinstance(o["until"], list):
                raise ScriptError("goto: until must be a list")
            conds = []
            for c in o["until"]:
                if not isinstance(c, dict) or set(c) != {"off", "eq"}:
                    raise ScriptError("goto: each until entry needs exactly off and eq")
                if isinstance(c["eq"], bool) or not isinstance(c["eq"], int) or c["eq"] < 0:
                    raise ScriptError("goto: until eq must be a non-negative int")
                conds.append({"off": _parse_va(c["off"], "goto"), "eq": c["eq"]})
            o["until"] = conds
            if o["snap"] is not None and not re.fullmatch(r"[A-Za-z0-9_.-]+", str(o["snap"])):
                raise ScriptError("goto: snap must match [A-Za-z0-9_.-]+")
        elif kind == "ram_snap":
            if isinstance(body, str):
                o = {"name": body, "va": 0, "length": GUEST_RAM_SIZE}
            else:
                o = _opts(kind, body, {"name": None, "va": 0, "length": GUEST_RAM_SIZE})
                o["va"] = _parse_va(o["va"], "ram_snap")
            if not isinstance(o["name"], str) or not re.fullmatch(r"[A-Za-z0-9_.-]+", o["name"]):
                raise ScriptError("ram_snap: name must be a simple string")
            if o["va"] % 0x1000:
                raise ScriptError("ram_snap: va must be page-aligned")
            if not isinstance(o["length"], int) or o["length"] <= 0 or o["length"] % 0x1000:
                raise ScriptError("ram_snap: length must be a positive multiple of 0x1000")
            if o["length"] > GUEST_RAM_SIZE:
                raise ScriptError("ram_snap: length must not exceed 0x%X" % GUEST_RAM_SIZE)
            _check_guest_range(o["va"], o["length"], "ram_snap", floor=0x0)
        else:
            raise ScriptError("step %d: unknown step %r" % (i, kind))
        if kind == "peek":
            if o["name"] in peek_names:
                raise ScriptError("peek: duplicate name %r" % o["name"])
            peek_names.add(o["name"])
        if kind == "ram_snap":
            if o["name"] in snap_names:
                raise ScriptError("ram_snap: duplicate name %r" % o["name"])
            snap_names.add(o["name"])
        steps.append((kind, o))
    if held:
        raise ScriptError("key_down without key_up: 0x%02X" % sorted(held)[0])
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


def band_samples(a, band_rect=TITLE_BAND):
    """Pixels title_white_count samples in a band (every 2nd row and column), at the frame size of `a`."""
    h, w = a.shape[0], a.shape[1]
    top, bottom, left, right = band_rect
    rw, rh = TITLE_REF_SIZE
    t, b = int(round(top * h / rh)), int(round(bottom * h / rh))
    l, r = int(round(left * w / rw)), int(round(right * w / rw))
    return len(range(t, b, 2)) * len(range(l, r, 2))


def is_title(a):
    """True when the press-start text and the wordmark are white text, not a white screen.

    Both bands must reach their 640x480 thresholds, scaled by the frame's area, and neither may be
    mostly white: the white Smilebit card fills 61-68% of the press band and 100% of the wordmark band,
    where the title fills at most 19% and 43%, so without the upper bound `wait_title` fired on the
    logos and the script ran ahead of the game."""
    rw, rh = TITLE_REF_SIZE
    scale = (a.shape[0] * a.shape[1]) / (rw * rh)
    press, mark = title_white_count(a), title_white_count(a, MARK_BAND)
    return (TITLE_MIN_WHITE * scale <= press <= TITLE_MAX_FILL * band_samples(a)
            and MARK_MIN_WHITE * scale <= mark <= MARK_MAX_FILL * band_samples(a, MARK_BAND))


def menu_orange_count(a):
    """Orange pixels (the highlighted menu entry) in the menu band, scaled to the frame size of `a`."""
    b = _band(a, MENU_BAND)
    return int(((b[:, :, 0] > 160) & (b[:, :, 1] > 100) & (b[:, :, 2] < 90)).sum())


def menu_bar_glyphs(a):
    """Orange highlight glyph pixels that sit ON a dark menu bar: bar pixels within MENU_BAR_GAP rows
    above and below.

    This is the structural half of `is_menu`. The NEW GAME entry is orange text drawn on one of the
    menu's dark bars, so its glyph pixels have dark neighbours above and below them. Coloured city
    geometry that merely falls in the band does not: it is textured, so it has no such vertical pairing.
    The rows above/below are searched at the full frame width, so an upscaled frame only lengthens the
    glyphs; it does not add or remove the pairing.
    """
    import numpy as np

    b = _band(a, MENU_LINE_NEW)
    r, g, bl = b[:, :, 0], b[:, :, 1], b[:, :, 2]
    gr, gg, gb, gm = MENU_GLYPH_MIN
    glyph = (r > gr) & (g > gg) & (g < gb) & (bl < gm)
    if not glyph.any():
        return 0
    dark = a.max(axis=2) < MENU_BAR_DARK
    h, w = a.shape[0], a.shape[1]
    y0 = int(MENU_LINE_NEW[0] * h / TITLE_REF_SIZE[1])
    x0 = int(MENU_LINE_NEW[2] * w / TITLE_REF_SIZE[0])
    # The search distance scales with the frame: at 2x the glyph strokes are two pixels tall, so a fixed
    # three-row window would land inside the stroke instead of on the bar around it.
    gap = max(MENU_BAR_GAP, MENU_BAR_GAP * h // TITLE_REF_SIZE[1])
    n = 0
    for yy, xx in zip(*np.where(glyph)):
        gy, gx = y0 + int(yy), x0 + int(xx)
        if (dark[max(0, gy - gap):gy, gx].any()
                and dark[gy + 1:gy + 1 + gap, gx].any()):
            n += 1
    return n


def is_menu(a):
    """True on the main menu (NEW GAME / LOAD GAME / VS MODE).

    Colour alone is not enough: the attract-mode city reaches 601 orange samples in the menu band
    against a 600 threshold, so the V3 run accepted the city as the menu and its script ran on the wrong
    screens. The colour gate therefore stays as a cheap filter and the structural witness above decides.
    """
    rw, rh = TITLE_REF_SIZE
    if menu_orange_count(a) < MENU_MIN_ORANGE * (a.shape[0] * a.shape[1]) / (rw * rh):
        return False
    return menu_bar_glyphs(a) >= MENU_MIN_BAR_GLYPHS


# HUD bands, calibrated on every frame of run
# 20261011-223618-932-keyscript-culling-skate-fixed (160 frames): the spray-can icon
# reads 368-371 red pixels on gameplay frames and 0 on dialogue pages, fades and menus;
# the health bar reads 1640 yellow pixels on gameplay frames and 0 elsewhere.
ICON_BAND = (64, 108, 462, 492)
BAR_BAND = (42, 58, 398, 516)
ICON_MIN_RED = 250
BAR_MIN_YELLOW = 800
# Dialogue draws a white speech balloon in the upper middle. Cutscene pages also letterbox the
# frame; in-game NPC talks do not, so the balloon alone is the test. Gameplay never exceeds 744
# balloon pixels (its "?" balloon icon is 122); a talk measures 23,000-29,000.
BALLOON_BAND = (50, 120, 100, 540)
BALLOON_WHITE_LEVEL = 220
BALLOON_MIN_WHITE = 10000
# The "talk available" balloon: a white bubble with a blue face, up and right of the player's
# head. Calibrated over 3784 archived Packet 001 frames: 3516 count < 10, 18 count 10-39, 250
# count >= 40. See is_talk_icon for the tally that makes this threshold decisive.
TALK_ICON_BAND = (120, 230, 330, 430)
TALK_ICON_MIN_BLUE = 40
# The band also catches the blue Dogenzaka Hill location banner (blue text on a blue field).
# A real talk indicator is a blue face inside a WHITE balloon, so it carries 583-857 white
# pixels around the blue blob while every banner frame carries none.
TALK_ICON_MIN_BUBBLE_WHITE = 150


def _band(a, band_rect):
    """The band of `a` (HxWx3) named by (top, bottom, left, right) in 640x480 reference coordinates."""
    h, w = a.shape[0], a.shape[1]
    top, bottom, left, right = band_rect
    rw, rh = TITLE_REF_SIZE
    return a[int(top * h / rh):int(bottom * h / rh), int(left * w / rw):int(right * w / rw), :3]


def hud_icon_red_count(a):
    """Red pixels of the HUD spray-can icon."""
    b = _band(a, ICON_BAND)
    return int(((b[:, :, 0] > 170) & (b[:, :, 1] < 70) & (b[:, :, 2] < 70)).sum())


def hud_bar_yellow_count(a):
    """Yellow pixels of the HUD health bar."""
    b = _band(a, BAR_BAND)
    return int(((b[:, :, 0] > 180) & (b[:, :, 1] > 120) & (b[:, :, 2] < 80)).sum())


def is_hud(a):
    """True on an in-game HUD frame: the spray-can icon or the health bar is drawn.

    The OR keeps the detector working if the icon changes colour once cans are held. A talk
    still draws a partial health bar (830 yellow pixels measured), so a speech balloon wins:
    without this, `key_until ... hud` would return on the first frame of the dialogue it is
    meant to wait out."""
    if is_dialogue(a):
        return False
    rw, rh = TITLE_REF_SIZE
    scale = (a.shape[0] * a.shape[1]) / (rw * rh)
    return (hud_icon_red_count(a) >= ICON_MIN_RED * scale
            or hud_bar_yellow_count(a) >= BAR_MIN_YELLOW * scale)


def dialogue_balloon_white_count(a):
    """White pixels of the speech balloon band."""
    b = _band(a, BALLOON_BAND)
    return int((b > BALLOON_WHITE_LEVEL).all(axis=2).sum())


def is_dialogue(a):
    """True when a speech balloon is on screen, letterboxed or not.

    Cutscene dialogue letterboxes the frame, but an in-game NPC talk does not. Measured on
    run 20261012-020638-074-keyscript-p001-e1: Corn's TE003 talk balloon covers 27,901 white pixels
    at black fractions of only 0.066/0.007, while the letterboxed intro pages reach 0.998.
    Requiring the letterbox therefore missed every in-game talk, so `key_until ... dialogue`
    could never fire on one.

    The spray-can icon is a free second test: it is drawn on every gameplay frame (374 px
    measured) and never during a talk, so requiring its absence rejects a bright outdoor
    scene that happens to fill the balloon band. Across 3357 frames of this packet's runs it
    suppressed no talk, and the largest balloon on a non-dialogue frame was 1076 against the
    10000 threshold."""
    return (dialogue_balloon_white_count(a) >= BALLOON_MIN_WHITE
            and hud_icon_red_count(a) == 0)


SCREENS = {"title": is_title, "menu": is_menu, "hud": is_hud, "dialogue": is_dialogue}
SCREENS["talk_icon"] = None  # assigned below, after its helper is defined


def talk_icon_blue_count(a):
    """Blue pixels of the "talk available" balloon above and right of the player's head."""
    b = _band(a, TALK_ICON_BAND)
    return int(((b[:, :, 2] > 150) & (b[:, :, 0] < 90)
                & (b[:, :, 1] > 60) & (b[:, :, 1] < 160)).sum())


def is_talk_icon(a):
    """True when the game is showing its "talk available" balloon.

    Measured over 3784 archived Packet 001 frames with this detector: 3516 count < 10,
    18 count 10-39 (boot logos and partial fades), 250 count >= 40. That threshold is
    decisive rather than merely separating classes: across every `key_until` R press from
    E2 onward, an icon at press time talked 4/4 times and never failed, while 38 of the 45
    presses without one opened no talk -- including every E16/E17/E18 attempt, whose
    `post-00-fwd1` frame had shown the icon about a second earlier, before the player rolled
    out of range. The talk needs proximity; this icon is how the game states it.

    The band also catches the blue Dogenzaka Hill location banner, which is blue text on a
    blue field. What separates them is the bubble: a real talk indicator is a blue face inside
    a WHITE balloon, so it carries hundreds of white pixels immediately around the blue blob
    (583-857 measured over the six archived icons), while every banner frame carries none.
    Height alone did not separate them -- the banner's text measures 44 px tall mid-fade,
    under any cap that still admits the 37-50 px real icon."""
    if not is_hud(a):
        return False
    rw, rh = TITLE_REF_SIZE
    scale = (a.shape[0] * a.shape[1]) / (rw * rh)
    n = talk_icon_blue_count(a)
    if n < TALK_ICON_MIN_BLUE * scale:
        return False
    b = _band(a, TALK_ICON_BAND)
    import numpy as _np
    blue = ((b[:, :, 2] > 150) & (b[:, :, 0] < 90)
            & (b[:, :, 1] > 60) & (b[:, :, 1] < 160))
    ys, xs = _np.nonzero(blue)
    if not len(xs):
        return False
    pad = 3
    y0, y1 = max(0, ys.min() - pad), min(b.shape[0], ys.max() + pad + 1)
    x0, x1 = max(0, xs.min() - pad), min(b.shape[1], xs.max() + pad + 1)
    white = (b[y0:y1, x0:x1].min(axis=2) > BALLOON_WHITE_LEVEL)
    return int(white.sum()) >= TALK_ICON_MIN_BUBBLE_WHITE * scale


SCREENS["talk_icon"] = is_talk_icon


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

    def is_foreground(self, hwnd):
        """True when `hwnd` is the foreground window.

        WM_KILLFOCUS clears every held key, so a press can be posted and still lost. Recording
        focus at release time is what tells a delivery failure from a proximity failure."""
        try:
            return self.user32.GetForegroundWindow() == hwnd
        except Exception:
            return None


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

    def __init__(self, out, proc, live, window, deadline, t0, log, guest=None):
        self.out, self.proc, self.live, self.win = out, proc, live, window
        self.deadline, self.t0, self.log = deadline, t0, log
        self.guest = guest
        self.hwnd = None
        self.frame = None
        self.frame_id = 0            # increments whenever a new frame is read
        self._sig = None
        self.timeline_seq = 0
        self._tl_next = 0.0
        self._tl_mean = None
        self.held = {}               # vk -> open press record, for key_down/key_up

    def release_held(self):
        """Post KEYUP for every key a key_down left held. Called before the process dies, so a
        script that ends mid-hold cannot leave a key stuck in the guest or the host."""
        released = []
        for vk, rec in list(self.held.items()):
            try:
                if self.hwnd is not None:
                    self.win.key(self.hwnd, vk, False)
                    self.log("KEYUP vk=0x%02X (released by cleanup)" % vk)
            finally:
                rec["t_up_s"] = round(self.elapsed(), 3)
                rec["released_by"] = "cleanup"
                self.press_release(rec, [vk])
                self.held.pop(vk, None)
                released.append(vk)
        return released

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

    def press_probe(self, vk):
        """Host-side state read just before a KEYDOWN: present serial, flips, key arrival."""
        out = {"presents_down": None, "flips_down": None}
        try:
            out["presents_down"] = self.guest.host_read("s_present_serial", 4)
            out["flips_down"] = self.guest.host_read("s_flips", 4)
        except (ScriptError, OSError, AttributeError) as e:
            out["telemetry_error"] = str(e)
        return out

    def press_release(self, rec, vks):
        """Complete a press record just before KEYUP: what the guest did, and whether the
        window still had focus. WM_KILLFOCUS clears every held key, so a press can be lost
        while its KEYDOWN was genuinely posted; this is what distinguishes the two."""
        try:
            rec["presents_up"] = self.guest.host_read("s_present_serial", 4)
            rec["flips_up"] = self.guest.host_read("s_flips", 4)
            rec["key_seen"] = [self.guest.host_read("s_key_down", 1, vk) for vk in vks] \
                if len(vks) > 1 else self.guest.host_read("s_key_down", 1, vks[0])
            rec["foreground"] = self.win.is_foreground(self.hwnd)
        except (ScriptError, OSError, AttributeError) as e:
            rec["telemetry_error"] = str(e)
        self._append_jsonl("presses.jsonl", rec)
        return rec

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
            rec = {"step": "key", "vk": o["vk"], "t_down_s": round(self.elapsed(), 3)}
            rec.update(self.press_probe(o["vk"]))
            self.win.key(self.hwnd, o["vk"], True)
            self.log("KEYDOWN vk=0x%02X" % o["vk"])
            try:
                self.pause(o["hold"])
                # A talk that only shows while the key is held (Corn's TE003 talk) is gone by
                # the time a following `snap` runs, so let the press capture it from inside.
                if o.get("snap"):
                    self.snap(o["snap"])
            finally:
                self.win.key(self.hwnd, o["vk"], False)
                self.log("KEYUP vk=0x%02X" % o["vk"])
            rec["t_up_s"] = round(self.elapsed(), 3)
            self.press_release(rec, [o["vk"]])
            return {"sent": True, "vk": o["vk"], "hold": o["hold"], "snap": o.get("snap"),
                    "press": rec}
        if kind == "hold_keys":
            self.wait_for(30, lambda: self.hwnd is not None)
            if self.hwnd is None:
                return {"sent": False, "error": "no window"}
            rec = {"step": "hold_keys", "vks": list(o["vks"]),
                   "t_down_s": round(self.elapsed(), 3)}
            rec.update(self.press_probe(o["vks"][0]))
            for vk in o["vks"]:
                self.win.key(self.hwnd, vk, True)
                self.log("KEYDOWN vk=0x%02X" % vk)
            try:
                self.pause(o["hold"])
                if o.get("snap"):
                    self.snap(o["snap"])
            finally:
                for vk in reversed(o["vks"]):
                    self.win.key(self.hwnd, vk, False)
                    self.log("KEYUP vk=0x%02X" % vk)
            rec["t_up_s"] = round(self.elapsed(), 3)
            self.press_release(rec, list(o["vks"]))
            return {"sent": True, "vks": o["vks"], "hold": o["hold"], "press": rec}
        if kind == "key_down":
            # Hold a key across steps. The talk balloon appears only while R is held, and
            # whether the guest still has the key at the moment of interest is exactly what
            # a press-and-release step cannot express.
            self.wait_for(30, lambda: self.hwnd is not None)
            if self.hwnd is None:
                return {"sent": False, "error": "no window"}
            vk = o["vk"] if isinstance(o["vk"], int) else parse_vk(o["vk"])
            rec = {"step": "key_down", "vk": vk, "t_down_s": round(self.elapsed(), 3)}
            rec.update(self.press_probe(vk))
            self.win.key(self.hwnd, vk, True)
            self.log("KEYDOWN vk=0x%02X (held)" % vk)
            self.held[vk] = rec
            return {"sent": True, "vk": vk}
        if kind == "key_up":
            self.wait_for(30, lambda: self.hwnd is not None)
            if self.hwnd is None:
                return {"sent": False, "error": "no window"}
            vk = o["vk"] if isinstance(o["vk"], int) else parse_vk(o["vk"])
            self.win.key(self.hwnd, vk, False)
            self.log("KEYUP vk=0x%02X" % vk)
            rec = self.held.pop(vk, {"step": "key_up", "vk": vk})
            rec["t_up_s"] = round(self.elapsed(), 3)
            self.press_release(rec, [vk])
            return {"sent": True, "vk": vk, "press": rec}
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
            self.wait_for(30, lambda: self.hwnd is not None)
            if self.hwnd is None:
                return {"satisfied": False, "presses": 0, "error": "no window"}
            presses = []
            for n in range(1, o["max"] + 1):
                # Test while the key is held, then again just after release. A talk that
                # only shows during the press (Corn's TE003 talk) is missed entirely by a
                # release-then-test loop, while a page that persists is caught either way.
                rec = {"step": "key_until", "vk": o["vk"], "press": n,
                       "until": o["until"], "t_down_s": round(self.elapsed(), 3)}
                rec.update(self.press_probe(o["vk"]))
                self.win.key(self.hwnd, o["vk"], True)
                self.log("KEYDOWN vk=0x%02X" % o["vk"])
                hit = False
                try:
                    hit = self.wait_for(o["hold"], lambda: self.frame is not None and test(self.frame))
                    if hit and o.get("snap"):
                        # The balloon appears before its typewriter finishes and the talk ends
                        # on its own, so a snap taken at detection catches a partial line.
                        if o.get("snap_after"):
                            self.pause(o["snap_after"])
                        self.snap(o["snap"])
                finally:
                    self.win.key(self.hwnd, o["vk"], False)
                    self.log("KEYUP vk=0x%02X" % o["vk"])
                rec["t_up_s"] = round(self.elapsed(), 3)
                rec["hit"] = hit
                self.press_release(rec, [o["vk"]])
                presses.append(rec)
                if hit or self.wait_for(o["every"], lambda: self.frame is not None and test(self.frame)):
                    return {"satisfied": True, "presses": n, "presses_log": presses}
            return {"satisfied": False, "presses": o["max"], "presses_log": presses}
        if kind == "wait_screen":
            test = SCREENS[o["screen"]]
            ok = self.wait_for(o["timeout"], lambda: self.frame is not None and test(self.frame))
            return {"satisfied": ok}
        if kind == "touch":
            Path(o["path"]).parent.mkdir(parents=True, exist_ok=True)
            Path(o["path"]).write_bytes(b"")
            return {"touched": o["path"]}
        if kind == "remove":
            try:
                Path(o["path"]).unlink()
                return {"removed": o["path"]}
            except FileNotFoundError:
                return {"removed": None}
        if kind == "snap":
            self.tick()
            return {"file": self.snap(o["name"])}
        if kind == "bench":
            return self._bench(o)
        if kind == "peek":
            return self._peek(o)
        if kind == "ram_snap":
            return self._ram_snap(o)
        if kind == "log_pos":
            return self._log_pos(o)
        if kind == "goto":
            return self._goto(o)
        raise ScriptError("unknown step %r" % kind)

    def _bench(self, o):
        """Poll s_present_serial for o["seconds"], with no input, and write the window to bench.json."""
        t_start = self.elapsed()
        period = o["poll_ms"] / 1000.0
        samples = []
        t_end_pc = time.perf_counter() + o["seconds"]
        next_pc = time.perf_counter()
        while self.alive() and not self.expired():
            now = time.perf_counter()
            if now >= t_end_pc:
                break
            samples.append((now, self.guest.host_read("s_present_serial", 4)))
            next_pc += period
            wait = next_pc - time.perf_counter()
            if wait > 0:
                time.sleep(wait)
            else:
                next_pc = time.perf_counter()      # fell behind: do not burst to catch up
        t_end = self.elapsed()
        intervals, aliased = bench_intervals(samples)
        poll = bench_poll_stats(samples)
        duration = samples[-1][0] - samples[0][0] if len(samples) > 1 else 0.0
        ctx = getattr(self, "bench_context", {})
        log_path = self.out / "jsrf_run.log"
        log_text = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else ""
        rec = {"name": o["name"], "t_start_s": round(t_start, 3), "t_end_s": round(t_end, 3),
               "requested_seconds": o["seconds"], "poll_ms": o["poll_ms"]}
        rec.update(bench_stats(intervals, duration, poll["presents"], aliased, poll))
        rec.update({"backend": ctx.get("backend"), "exe_sha256": ctx.get("exe_sha256"),
                    "device": bench_device(log_text)})
        path = self.out / "bench.json"
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            data = {}
        data[o["name"]] = rec
        path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        f = lambda v, fmt: "-" if v is None else fmt % v
        self.log("BENCH %s backend=%s window=%.1fs polls=%d poll=%sms presents=%d (%s/s) "
                 "frame_ms p50=%s p90=%s p99=%s max=%s aliased=%d/%s stalls>100ms=%d"
                 % (o["name"], rec["backend"], duration, rec["polls"],
                    f(rec["median_poll_period_ms"], "%.2f"), rec["presents"],
                    f(rec["presents_per_s"], "%.2f"), f(rec["frame_ms_p50"], "%.1f"),
                    f(rec["frame_ms_p90"], "%.1f"), f(rec["frame_ms_p99"], "%.1f"),
                    f(rec["frame_ms_max"], "%.1f"), aliased, rec["increment_polls"],
                    rec["stalls_over_100ms"]))
        return {"bench": o["name"], "presents": rec["presents"],
                "presents_per_s": rec["presents_per_s"]}

    def read_u32(self, va):
        """One u32 of guest memory; ScriptError if it is outside RAM or unreadable."""
        return self.read_words(va, 1, "<I")[0]

    def read_words(self, va, count, fmt="<I"):
        """`count` little-endian 4-byte values (fmt is the struct code) at guest `va`."""
        if self.guest is None:
            raise ScriptError("no guest memory reader")
        _check_guest_range(va, 4 * count, "guest read")
        data, unreadable = self.guest.read(va, 4 * count)
        if unreadable:
            raise ScriptError("guest read 0x%08X+%d unreadable" % (va, 4 * count))
        return struct.unpack("<%d%s" % (count, fmt[-1]), data)

    def object_address(self, obj_id, off=0):
        """Guest address of (object `obj_id`) + off, re-read through the object table."""
        return resolve_chain(self.read_u32, object_chain(obj_id, off))

    def object_state(self, obj_id):
        """(char pointer, position xyz, camera forward xyz, heading xyz) of a guest object."""
        ch = self.object_address(obj_id)
        pos = self.read_words(ch + OFF_POS, 3, "<f")
        cam = self.read_words(ch + OFF_CAMERA, 3, "<f")
        head = self.read_words(ch + OFF_HEADING, 3, "<f")
        if not all(math.isfinite(v) for v in pos + cam + head):
            raise ScriptError("object 0x%02X has non-finite position/camera" % obj_id)
        return ch, pos, cam, head

    def press_keys(self, step, vks, seconds, extra=None):
        """Hold `vks` for `seconds` (the hold_keys mechanism, for any number of keys) and append
        the press record; returns the record."""
        rec = {"step": step, "vks": list(vks), "t_down_s": round(self.elapsed(), 3)}
        rec.update(extra or {})
        rec.update(self.press_probe(vks[0]))
        for vk in vks:
            self.win.key(self.hwnd, vk, True)
            self.log("KEYDOWN vk=0x%02X" % vk)
        try:
            self.pause(seconds)
        finally:
            for vk in reversed(vks):
                self.win.key(self.hwnd, vk, False)
                self.log("KEYUP vk=0x%02X" % vk)
        rec["t_up_s"] = round(self.elapsed(), 3)
        self.press_release(rec, list(vks))
        return rec

    def _goto(self, o):
        """Walk the player toward o["target"] (x, z) by closed-loop steering; see the docstring."""
        self.wait_for(30, lambda: self.hwnd is not None)
        if self.hwnd is None:
            return {"satisfied": False, "iterations": 0, "stuck": 0, "jumps": 0,
                    "error": "no window"}
        tx, tz = o["target"]
        start = time.time()
        iterations = stuck = jumps = 0
        side = 1
        origin = None              # horizontal position at the first successful read
        armed = False              # stuck detection waits out the game ignoring early pulses
        hist = []                  # horizontal positions at the last iterations
        reason, last = "timeout", None
        while self.alive() and not self.expired():
            try:
                ch, pos, cam, _ = self.object_state(o["player_obj"])
                untils = [self.read_u32(ch + c["off"]) for c in o["until"]]
            except ScriptError as e:
                self._append_jsonl("goto.jsonl", {"t_s": round(self.elapsed(), 2), "error": str(e)})
                if time.time() - start >= o["timeout"]:
                    break
                self.pause(o["pulse"])
                continue
            iterations += 1
            if origin is None:
                origin = (pos[0], pos[2])
            if not armed and (math.hypot(pos[0] - origin[0], pos[2] - origin[1]) > STUCK_ARM_MOVE
                              or iterations > STUCK_ARM_PULSES):
                armed = True
            dx, dz = tx - pos[0], tz - pos[2]
            dist = math.hypot(dx, dz)
            cam_yaw = math.atan2(cam[0], cam[2])
            rel = relative_angle(dx, dz, cam[0], cam[2])
            last = {"position": [round(v, 2) for v in pos], "distance": round(dist, 2),
                    "until_values": untils}
            rec = {"t_s": round(self.elapsed(), 2), "pos": [round(v, 2) for v in pos],
                   "cam_yaw_deg": round(math.degrees(cam_yaw), 1),
                   "rel_deg": round(math.degrees(rel), 1), "dist": round(dist, 2)}
            reason = goto_stop_reason(
                dist, o["radius"], [v == c["eq"] for v, c in zip(untils, o["until"])]) or "timeout"
            if reason != "timeout":
                rec.update({"keys": [], "stop": reason})
                self._append_jsonl("goto.jsonl", rec)
                break
            if time.time() - start >= o["timeout"]:
                rec.update({"keys": [], "stop": "timeout"})
                self._append_jsonl("goto.jsonl", rec)
                break
            hist = (hist + [(pos[0], pos[2])])[-(STUCK_PULSES + 1):]
            moved = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(hist, hist[1:]))
            jump = False
            if armed and len(hist) == STUCK_PULSES + 1 and moved < STUCK_MIN_MOVE:
                action = stuck_action(stuck) if o["jump_when_stuck"] else "sidestep"
                stuck += 1
                if action == "jump":
                    jump = True
                    jumps += 1
                    keys = steer_keys(dx, dz, cam[0], cam[2])
                    rec.update({"stuck": True, "stuck_n": stuck, "moved": round(moved, 2),
                                "action": "jump", "keys": list(keys)})
                    self.log("goto stuck #%d (moved %.2f over %d pulses); jump"
                             % (stuck, moved, STUCK_PULSES))
                else:
                    if action == "side_left":
                        side = -1
                    elif action == "side_right":
                        side = 1
                    bearing = math.atan2(dx, dz) + side * math.pi / 2
                    keys = steer_keys(math.sin(bearing), math.cos(bearing), cam[0], cam[2])
                    rec.update({"stuck": True, "stuck_n": stuck, "moved": round(moved, 2),
                                "sidestep": side, "action": action, "keys": list(keys)})
                    self.log("goto stuck #%d (moved %.2f over %d pulses); sidestep %+d"
                             % (stuck, moved, STUCK_PULSES, side))
                    if action == "sidestep":
                        side = -side
                hist = []
            else:
                keys = steer_keys(dx, dz, cam[0], cam[2])
                rec["keys"] = list(keys)
            self._append_jsonl("goto.jsonl", rec)
            if jump:
                self.press_keys("goto", list(keys) + [VK_JUMP], o["jump_hold"],
                                {"dist": rec["dist"], "action": "jump"})
                if o["jump_follow"] > 0:
                    if keys:
                        self.press_keys("goto", keys, o["jump_follow"],
                                        {"dist": rec["dist"], "action": "jump_follow"})
                    else:
                        self.pause(o["jump_follow"])
            elif keys:
                self.press_keys("goto", keys, o["pulse"], {"dist": rec["dist"]})
            else:
                self.pause(o["pulse"])
        # The state the loop ended on, read once more so the result reflects the last pulse.
        try:
            ch, pos, _, _ = self.object_state(o["player_obj"])
            untils = [self.read_u32(ch + c["off"]) for c in o["until"]]
            last = {"position": [round(v, 2) for v in pos],
                    "distance": round(math.hypot(tx - pos[0], tz - pos[2]), 2),
                    "until_values": untils}
            if reason == "timeout":
                reason = goto_stop_reason(
                    last["distance"], o["radius"],
                    [v == c["eq"] for v, c in zip(untils, o["until"])]) or "timeout"
        except ScriptError:
            pass
        snap = self.snap(o["snap"]) if o.get("snap") else None
        res = {"satisfied": reason != "timeout", "reason": reason, "iterations": iterations,
               "stuck": stuck, "jumps": jumps, "final": last, "snap": snap}
        self.log("goto %s iterations=%d stuck=%d jumps=%d final=%s"
                 % (reason, iterations, stuck, jumps, last))
        return res

    def _log_pos(self, o):
        """Append position, heading yaw and camera yaw of each requested object to positions.jsonl."""
        rec = {"name": o["name"], "t_s": round(self.elapsed(), 2), "objs": {}}
        for obj in o["objs"]:
            try:
                ch, pos, cam, head = self.object_state(obj)
                rec["objs"]["0x%02X" % obj] = {
                    "ch": "0x%08X" % ch, "pos": [round(v, 3) for v in pos],
                    "heading_yaw_deg": round(math.degrees(math.atan2(head[0], head[2])), 2),
                    "camera_yaw_deg": round(math.degrees(math.atan2(cam[0], cam[2])), 2)}
            except ScriptError as e:
                rec["objs"]["0x%02X" % obj] = {"error": str(e)}
        self.log("LOGPOS %s %s" % (o["name"], json.dumps(rec["objs"])))
        self._append_jsonl("positions.jsonl", rec)
        return rec

    def _peek(self, o):
        """Read one guest value and append a lossless record to peeks.jsonl."""
        if o.get("chain") is not None:
            return self._peek_chain(o)
        rec = {"name": o["name"], "va": "0x%08X" % o["va"], "size": o["size"],
               "expect": o["expect"], "t_s": round(self.elapsed(), 2)}
        try:
            if self.guest is None:
                raise ScriptError("no guest memory reader")
            data, unreadable = self.guest.read(o["va"], o["size"])
            value = int.from_bytes(data, "little")
            rec.update({"value": value, "value_hex": "0x%X" % value,
                        "unreadable": bool(unreadable),
                        "matches": (None if o["expect"] is None
                                    else (not unreadable and value == o["expect"]))})
        except (ScriptError, OSError) as e:
            rec.update({"error": str(e), "matches": False})
        self.log("PEEK %s va=%s size=%d value=%s expect=%s matches=%s"
                 % (o["name"], rec["va"], o["size"], rec.get("value_hex", "-"),
                    o["expect"], rec.get("matches")))
        self._append_jsonl("peeks.jsonl", rec)
        return rec

    def _peek_chain(self, o):
        """Follow o["chain"], read `count` u32/f32 values there, and append the record."""
        rec = {"name": o["name"], "chain": ["0x%X" % e for e in o["chain"]], "type": o["type"],
               "count": o["count"], "expect": o["expect"], "t_s": round(self.elapsed(), 2)}
        try:
            va = resolve_chain(self.read_u32, o["chain"])
            rec["va"] = "0x%08X" % va
            vals = self.read_words(va, o["count"], "<f" if o["type"] == "f32" else "<I")
            rec["values"] = list(vals)
            if o["type"] == "u32":
                rec["values_hex"] = ["0x%X" % v for v in vals]
            if o["count"] == 1:
                rec["value"] = vals[0]
            rec["matches"] = None if o["expect"] is None else vals[0] == o["expect"]
        except ScriptError as e:
            rec.update({"error": str(e), "matches": False})
        self.log("PEEK %s chain=%s va=%s values=%s matches=%s"
                 % (o["name"], rec["chain"], rec.get("va", "-"), rec.get("values", "-"),
                    rec.get("matches")))
        self._append_jsonl("peeks.jsonl", rec)
        return rec

    def _ram_snap(self, o):
        """Write a guest RAM range to ram/<name>.bin plus a sidecar, and report its hash."""
        import hashlib

        rec = {"name": o["name"], "va": "0x%08X" % o["va"], "length": o["length"],
               "t_s": round(self.elapsed(), 2)}
        if self.guest is None:
            rec.update({"error": "no guest memory reader"})
            self._append_jsonl("ram-snaps.jsonl", rec)
            return rec
        t0 = time.time()
        data, unreadable = self.guest.read(o["va"], o["length"])
        (self.out / "ram").mkdir(exist_ok=True)
        (self.out / "ram" / (o["name"] + ".bin")).write_bytes(data)
        rec.update({"sha256": hashlib.sha256(data).hexdigest(),
                    "unreadable_pages": ["0x%08X" % v for v in unreadable],
                    "read_s": round(time.time() - t0, 2), "file": o["name"] + ".bin"})
        (self.out / "ram" / (o["name"] + ".json")).write_text(
            json.dumps(rec, indent=2) + "\n", encoding="utf-8")
        self.log("RAM %s va=%s length=%d unreadable=%d" % (o["name"], rec["va"], o["length"],
                                                          len(unreadable)))
        self._append_jsonl("ram-snaps.jsonl", rec)
        out = dict(rec)
        out["unreadable_pages"] = len(unreadable)
        return out

    def _append_jsonl(self, name, rec):
        """Append one JSON line. The file is the lossless record of the deciding event."""
        with open(self.out / name, "a", encoding="utf-8") as f:
            f.write(json.dumps(rec) + "\n")
            f.flush()


def load_run_jsrf():
    """Import scripts/run-jsrf.py (hyphenated name) so its archive helpers are reused, not copied."""
    here = str(Path(__file__).resolve().parent)
    if here not in sys.path:
        sys.path.insert(0, here)
    spec = importlib.util.spec_from_file_location("run_jsrf", Path(here) / "run-jsrf.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def run_stamp(moment):
    """Run-directory timestamp, formatted exactly as run-jsrf.py names its runs."""
    return moment.strftime("%Y%m%d-%H%M%S-") + "%03d" % (moment.microsecond // 1000)


def file_sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def adopt(argv):
    """Archive a finished old-layout run folder under logs/runs/, recording what was not measured."""
    ap = argparse.ArgumentParser(prog="key-script-run.py --adopt")
    ap.add_argument("--adopt", required=True, type=Path, metavar="OLD_DIR")
    ap.add_argument("--project-rev", required=True)
    ap.add_argument("--toolkit-rev", required=True)
    ap.add_argument("--project-dirty", action="store_true")
    ap.add_argument("--toolkit-dirty", action="store_true")
    ap.add_argument("--exe-sha256", default=None, help="only if the executable's hash is actually known")
    args = ap.parse_args(argv)
    for rev in (args.project_rev, args.toolkit_rev):
        if not re.fullmatch(r"[0-9a-fA-F]{7,40}", rev):
            ap.error("revision must be a hex commit id: %r" % rev)
    if args.exe_sha256 is not None and not re.fullmatch(r"[0-9a-fA-F]{64}", args.exe_sha256):
        ap.error("--exe-sha256 must be 64 hex digits")
    old = args.adopt.resolve()
    for name in ("jsrf_run.log", "result.json"):
        if not (old / name).is_file():
            print("cannot adopt %s: missing %s" % (old, name), file=sys.stderr)
            return 2
    if not old.name.startswith("keyscript-"):
        print("expected an old-layout folder named keyscript-<label>: %s" % old, file=sys.stderr)
        return 2
    label = old.name[len("keyscript-"):]
    result = json.loads((old / "result.json").read_text(encoding="utf-8-sig"))
    mtimes = [p.stat().st_mtime for p in old.rglob("*") if p.is_file()]
    started = datetime.fromtimestamp(min(mtimes))
    dest = ROOT / "logs" / "runs" / ("%s-keyscript-%s" % (run_stamp(started), label))
    if dest.exists():
        print("already archived: %s" % dest, file=sys.stderr)
        return 2
    shutil.copytree(old, dest)  # copy, not move: the old folder is left untouched

    run_jsrf = load_run_jsrf()
    overrides = dict(kv.split("=", 1) for kv in result.get("overrides", []))
    env = dict(DEFAULT_ENV)
    env.update(overrides)
    settings = run_jsrf.environment_settings(env)
    unmeasured = [
        "repository_identities.*.revision and dirty state: supplied by the caller after the run",
        "repository_identities.*.patch_sha256 and status_sha256: no patch or status was archived",
        "xbe_sha256 and artifact hashes: not recorded at run time",
        "settings: reconstructed from the driver's DEFAULT_ENV plus result.json overrides, not read from the launched process",
    ]
    if args.exe_sha256 is None:
        unmeasured.insert(0, "exe_sha256: executable was not hashed or archived")
    metadata = {
        "adopted": True,
        "adoption_note": ("Archived after the run finished from %s. Repository revisions and dirty state "
                          "were supplied by the operator, not measured by the driver; fields listed in "
                          "'unmeasured' are null or reconstructed." % old.name),
        "adopted_utc": datetime.now(timezone.utc).isoformat(),
        "adopted_from": str(old),
        "unmeasured": unmeasured,
        "started_local_derived": started.isoformat(),
        "started_utc": None,
        "cwd": str(ROOT),
        "seconds": result.get("seconds"),
        "probe": "",
        "profile_source": "keyscript-adopted",
        "project_archived": False,
        "configuration": "Release",
        "xbe_sha256": None,
        "exe_sha256": args.exe_sha256.lower() if args.exe_sha256 else None,
        "settings": settings,
        "keyscript_profile": {"requested_profile": "exploratory", "classification": "exploratory",
                              "note": "declared, not classifier-verified: settings are reconstructed"},
        "repository_identities": {
            "project": {"revision": args.project_rev, "dirty": args.project_dirty,
                        "patch_sha256": None, "status_sha256": None, "supplied_after_run": True},
            "toolkit": {"revision": args.toolkit_rev, "dirty": args.toolkit_dirty,
                        "patch_sha256": None, "status_sha256": None, "supplied_after_run": True},
        },
        "key_script": {"path": result.get("script"), "sha256": None},
        "run_log_sha256": file_sha256(dest / "jsrf_run.log"),
    }
    (dest / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print("Adopted: %s" % dest)
    return 0


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
    argv = sys.argv[1:] if argv is None else argv
    if "--adopt" in argv:
        return adopt(argv)
    args = parse_args(argv)
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

    run_jsrf = load_run_jsrf()
    env = dict(os.environ)
    inherited_settings = run_jsrf.environment_settings(env)
    try:
        run_jsrf.validate_launch_profile("exploratory", "", inherited_settings)
    except run_jsrf.ProfileError as e:
        print("refusing exploratory launch: %s" % e, file=sys.stderr)
        return 2

    started = datetime.now()
    out = ROOT / "logs" / "runs" / ("%s-keyscript-%s" % (run_stamp(started), args.label))
    frames = out / "frames"
    out.mkdir(parents=True, exist_ok=False)
    (out / "timeline").mkdir()
    frames.mkdir()
    (out / "save").mkdir()
    for name in KEY_ARTIFACTS:
        if (BUILD / name).is_file():
            shutil.copy2(BUILD / name, out / name)
    run_exe = out / "jsrf_recomp.exe"
    (out / "toolkit.patch").write_bytes(run_jsrf.run_git(TOOLKIT, "diff", "--binary", "HEAD", binary=True))
    (out / "toolkit-status.txt").write_text(run_jsrf.run_git(TOOLKIT, "status", "--porcelain"), encoding="utf-8")
    project_archived = run_jsrf.archive_project_state(out)
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

    env.update(DEFAULT_ENV)
    env.update({"JSRF_LOG_PATH": str(logpath), "RECOMP_FB_DUMP": str(live)})
    for kv in args.overrides:
        k, v = kv.split("=", 1)
        env[k] = v
    effective_settings = run_jsrf.environment_settings(env)
    try:
        profile_record = run_jsrf.make_profile_record("exploratory", inherited_settings, effective_settings, "")
    except run_jsrf.ProfileError as e:
        print("cannot classify run: %s" % e, file=sys.stderr)
        return 2
    command = [str(run_exe), "--save-root=" + str(out / "save")]
    project_identity = None
    if project_archived:
        project_identity = run_jsrf.repository_identity(ROOT, out / "project.patch", out / "project-status.txt")
        project_identity["dirty"] = bool((out / "project-status.txt").read_text(encoding="utf-8").strip())
    toolkit_identity = run_jsrf.repository_identity(TOOLKIT, out / "toolkit.patch", out / "toolkit-status.txt")
    toolkit_identity["dirty"] = bool((out / "toolkit-status.txt").read_text(encoding="utf-8").strip())
    artifact_sha256 = {n: file_sha256(out / n) for n in KEY_ARTIFACTS if (out / n).is_file()}
    # Not stored as `run_profile`: the strict archive validator in jsrf_run_profile.py requires a
    # collector command and artifacts this driver does not have, and would class the run UNKNOWN.
    # Without it the run classifies by its settings (legacy path), which is what exploratory means here.
    metadata = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "cwd": str(ROOT),
        "seconds": args.seconds,
        "probe": "",
        "profile_source": "keyscript",
        "expected_checkpoints": [],
        "project_archived": project_archived,
        "configuration": "Release",
        "xbe_path": str(ROOT / "game" / "default.xbe"),
        "xbe_sha256": file_sha256(ROOT / "game" / "default.xbe"),
        "exe_sha256": artifact_sha256["jsrf_recomp.exe"],
        "pdb_sha256": artifact_sha256.get("jsrf_recomp.pdb"),
        "map_sha256": artifact_sha256.get("jsrf_recomp.map"),
        "build_source_sha256": artifact_sha256.get("build-source.json"),
        "artifact_sha256": artifact_sha256,
        "settings": effective_settings,
        "keyscript_profile": profile_record,
        "disk_gate": {"skipped": True, "reason": "key-script driver does not run the disk gate"},
        "command": command,
        "repository_identities": {"project": project_identity, "toolkit": toolkit_identity},
        "save_root": {"archive_root": str(out.resolve()),
                      "expected_resolved_path": str((out / "save").resolve()),
                      "observed_resolved_path": None, "observed_path_layer_root": None,
                      "disposable": True, "verified": False},
        "key_script": {"path": str(args.script), "sha256": file_sha256(args.script)},
        "sources": run_jsrf.source_entries(),
    }
    (out / "metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    # The source present at launch, as run-jsrf archives it: check-stop-chain reads the manifest from it.
    if subprocess.run([sys.executable, str(ROOT / "scripts" / "archive-run-source.py"), str(out)]).returncode != 0:
        print("Source archiving failed.", file=sys.stderr)
        return 2

    log("=== start label=%s seconds=%d script=%s ===" % (args.label, args.seconds, args.script))
    stdout = open(out / "run.out", "w")
    proc = subprocess.Popen(command, cwd=str(ROOT),
                            env=env, stdout=stdout, stderr=subprocess.STDOUT)
    t0 = time.time()
    log("game pid=%d started" % proc.pid)
    # Always open the handle: press telemetry reads host symbols (presents, flips, the key
    # array) on every run, not just runs with explicit peeks. It stays observation only.
    guest = GuestMemory(proc.pid, out / "jsrf_recomp.map")
    run = Run(out, proc, live, Window(), args.seconds, t0, log, guest)
    run.bench_context = {"backend": env.get("RECOMP_GPU_BACKEND") or "cpu",
                         "exe_sha256": artifact_sha256["jsrf_recomp.exe"]}

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
        # A script that ended mid-hold must not leave a key stuck; release before the kill.
        try:
            released = run.release_held()
            if released:
                log("released held keys: %s" % ", ".join("0x%02X" % v for v in released))
        except Exception as e:
            log("release_held failed: %s" % e)
        if run.alive():
            proc.kill()
            killed = True
        try:
            proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            pass
        if guest is not None:
            guest.close()
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
        "outcome": "keyscript_complete" if script_ok and len(results) == len(steps)
                   and not any(r.get("skipped") for r in results) else "keyscript_incomplete",
        "duration_seconds": round(run.elapsed(), 1),
    }
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    log_text = logpath.read_text(encoding="utf-8", errors="replace") if logpath.is_file() else ""
    metadata["run_log_sha256"] = file_sha256(logpath) if logpath.is_file() else None
    seen, layer, verified = run_jsrf.save_root_observation(log_text, metadata["save_root"]["expected_resolved_path"])
    metadata["save_root"].update(observed_resolved_path=seen, observed_path_layer_root=layer, verified=verified)
    (out / "metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
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
