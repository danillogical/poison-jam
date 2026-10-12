"""Tests for scripts/key-script-run.py: the new detectors, step parsing, and the page reader.

Run directly: python -X utf8 tests/test_key_script_run.py
"""
from __future__ import annotations

import ctypes
import importlib.util
import json
import sys
import tempfile
import time
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "key-script-run.py"


def load_module():
    """Load the hyphenated script as a module, the way the other tests load hyphenated scripts."""
    spec = importlib.util.spec_from_file_location("key_script_run", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


ksr = load_module()


def frame(fill=(0, 0, 0)):
    """A 480x640 RGB frame of one colour."""
    return np.zeros((480, 640, 3), dtype=np.uint8) + np.array(fill, dtype=np.uint8)


def hud_frame():
    """A frame with the spray-can icon band and the health-bar band painted."""
    a = frame((40, 40, 40))
    a[64:108, 462:492] = (220, 20, 20)      # the icon
    a[42:58, 398:516] = (230, 180, 30)      # the bar
    return a


def dialogue_frame():
    """A letterboxed frame with a white speech balloon."""
    a = frame((40, 40, 40))
    a[0:34, :] = (0, 0, 0)
    a[450:480, :] = (0, 0, 0)
    a[50:120, 100:540] = (255, 255, 255)
    return a


def talk_frame():
    """An in-game NPC talk: a speech balloon with no letterbox and the HUD still drawn.

    This is the shape measured for Gum's talk in run
    20261012-020638-074-keyscript-p001-e1: 27,901 balloon pixels at black fractions of
    0.066/0.007, with the health bar still partly drawn."""
    a = frame((40, 40, 40))
    a[42:58, 398:516] = (230, 180, 30)      # the partial bar a talk still shows
    a[50:120, 100:540] = (255, 255, 255)    # the balloon
    return a


def icon_blob(a, top, left):
    """A sparse blue face in a WHITE balloon, as the real talk indicator renders.

    The real balloon is a white bubble whose blue face covers 52-74 pixels in a 22-37 px
    wide, 37-50 px tall footprint. The white bubble is what separates it from the blue
    Dogenzaka Hill banner, which is blue text on a blue field with no white around it."""
    for r in range(0, 37, 4):
        for c in range(0, 22, 4):
            a[top + r, left + c] = (16, 101, 156)
    a[top - 6:top + 43, left - 8:left + 30] = np.where(
        a[top - 6:top + 43, left - 8:left + 30] == (16, 101, 156),
        a[top - 6:top + 43, left - 8:left + 30],
        np.array([255, 255, 255], dtype=np.uint8))
    return a


def balloon_icon_frame():
    """Gameplay with the "?" balloon icon: 122 white pixels, which is not a talk."""
    a = frame((40, 40, 40))
    a[64:108, 462:492] = (220, 20, 20)
    a[42:58, 398:516] = (230, 180, 30)
    a[90:110, 360:380] = (255, 255, 255)    # a small balloon icon, ~122 white pixels
    return a


class Detectors(unittest.TestCase):
    def test_hud_detected_on_icon_and_bar(self):
        a = hud_frame()
        self.assertTrue(ksr.is_hud(a))
        self.assertFalse(ksr.is_dialogue(a))

    def test_hud_detected_on_bar_alone(self):
        """The icon may change colour once cans are held; the bar alone must still read as HUD."""
        a = frame((40, 40, 40))
        a[42:58, 398:516] = (230, 180, 30)
        self.assertTrue(ksr.is_hud(a))

    def test_dialogue_detected_and_not_hud(self):
        a = dialogue_frame()
        self.assertTrue(ksr.is_dialogue(a))
        self.assertFalse(ksr.is_hud(a))

    def test_unletterboxed_talk_is_dialogue_and_not_hud(self):
        """An NPC talk has no letterbox and still draws the bar; the balloon must win.

        Requiring the letterbox here is the defect this pins: it made
        `key_until ... dialogue` unfireable on every in-game talk, and left `... hud`
        matching the talk's own first frame."""
        a = talk_frame()
        self.assertTrue(ksr.is_dialogue(a))
        self.assertFalse(ksr.is_hud(a))

    def test_balloon_icon_alone_is_not_a_talk(self):
        a = balloon_icon_frame()
        self.assertFalse(ksr.is_dialogue(a))
        self.assertTrue(ksr.is_hud(a))

    def test_bright_scene_with_the_hud_icon_is_not_a_talk(self):
        """A gameplay frame whose band happens to be white must not read as a talk.

        The spray-can icon is drawn on every gameplay frame and never during a talk, so its
        presence rejects a bright outdoor scene that fills the balloon band."""
        a = frame((255, 255, 255))
        a[64:108, 462:492] = (220, 20, 20)          # the icon, as on every gameplay frame
        self.assertGreaterEqual(ksr.dialogue_balloon_white_count(a), ksr.BALLOON_MIN_WHITE)
        self.assertFalse(ksr.is_dialogue(a))

    def test_black_frame_is_neither(self):
        a = frame((0, 0, 0))
        self.assertFalse(ksr.is_hud(a))
        self.assertFalse(ksr.is_dialogue(a))

    def test_screens_include_the_new_detectors(self):
        self.assertEqual(set(ksr.SCREENS), {"title", "menu", "hud", "dialogue", "talk_icon"})

    def test_talk_icon_needs_the_blue_balloon(self):
        """The talk indicator is a blue-faced balloon in its band, on a HUD frame.

        Its threshold is load-bearing: across the archived Packet 001 presses, an icon at
        press time talked 4/4 times and never failed, while 38 of 45 presses without one
        opened no talk. The measured balloon is 22-37 px wide and 37-38 px tall (52-74 blue
        pixels), so the patches here are sized to that."""
        a = hud_frame()
        self.assertFalse(ksr.is_talk_icon(a))
        icon_blob(a, 140, 360)                        # sparse face, as measured
        self.assertTrue(ksr.is_talk_icon(a))
        a[140:177, 360:382] = (40, 40, 40)
        a[140:143, 360:365] = (16, 101, 156)          # 3x5 = 15 px, below the threshold
        self.assertFalse(ksr.is_talk_icon(a))

    def test_talk_icon_is_false_on_a_dialogue_frame(self):
        """A talk covers the band, so the icon must not be claimed while a page is up."""
        a = talk_frame()
        a[140:160, 360:380] = (16, 101, 156)
        self.assertFalse(ksr.is_talk_icon(a))

    def test_talk_icon_rejects_the_blue_location_banner(self):
        """The band also catches the blue Dogenzaka Hill banner, which is not a talk.

        The banner is blue text on a blue field; a real talk indicator is a blue face inside
        a white balloon. Height alone did not separate them (the banner measures 44 px tall
        mid-fade, inside the real icon's range), so the white bubble is the test."""
        a = hud_frame()
        a[120:187, 360:386] = (16, 101, 156)          # blue on blue: no bubble
        self.assertGreaterEqual(ksr.talk_icon_blue_count(a), ksr.TALK_ICON_MIN_BLUE)
        self.assertFalse(ksr.is_talk_icon(a))
        a[120:187, 360:386] = (40, 40, 40)
        icon_blob(a, 140, 360)                        # blue face inside a white balloon
        self.assertTrue(ksr.is_talk_icon(a))


class ParseScript(unittest.TestCase):
    def parse(self, steps):
        return ksr.parse_script(steps)

    def test_peek_round_trip(self):
        steps = self.parse([{"peek": {"name": "a", "va": "0x11000", "size": 4, "expect": 1}}])
        self.assertEqual(steps[0][0], "peek")
        self.assertEqual(steps[0][1]["va"], 0x11000)
        self.assertEqual(steps[0][1]["size"], 4)

    def test_duplicate_peek_name_rejected(self):
        with self.assertRaises(ksr.ScriptError) as cm:
            self.parse([{"peek": {"name": "a", "va": "0x11000"}},
                        {"peek": {"name": "a", "va": "0x12000"}}])
        self.assertIn("duplicate", str(cm.exception))

    def test_peek_bad_size_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"peek": {"name": "a", "va": "0x11000", "size": 3}}])

    def test_peek_out_of_range_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"peek": {"name": "a", "va": "0x04000000", "size": 4}}])
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"peek": {"name": "a", "va": "0x00000800"}}])

    def test_peek_contiguous_window_accepted(self):
        steps = self.parse([{"peek": {"name": "a", "va": "0x80011000"}}])
        self.assertEqual(steps[0][1]["va"], 0x80011000)

    def test_ram_snap_shorthand_and_explicit(self):
        steps = self.parse([{"ram_snap": "s0"},
                            {"ram_snap": {"name": "s1", "va": "0x1000", "length": 0x2000}}])
        self.assertEqual(steps[0][1]["va"], 0)
        self.assertEqual(steps[0][1]["length"], ksr.GUEST_RAM_SIZE)
        self.assertEqual(steps[1][1]["length"], 0x2000)

    def test_duplicate_ram_snap_name_rejected(self):
        with self.assertRaises(ksr.ScriptError) as cm:
            self.parse([{"ram_snap": "s0"}, {"ram_snap": "s0"}])
        self.assertIn("duplicate", str(cm.exception))

    def test_ram_snap_unaligned_length_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"ram_snap": {"name": "s", "va": 0, "length": 0x1234}}])

    def test_ram_snap_unaligned_va_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"ram_snap": {"name": "s", "va": "0x1001", "length": 0x1000}}])

    def test_existing_steps_still_parse(self):
        steps = self.parse([{"wait_title": {"timeout": 300}}, {"sleep": 1}, {"snap": "x"},
                            {"key": {"vk": "Z", "hold": 1.0}},
                            {"key_until": {"vk": "RETURN", "until": "menu"}},
                            {"wait_screen": {"screen": "hud"}}])
        self.assertEqual([k for k, _ in steps],
                         ["wait_title", "sleep", "snap", "key", "key_until", "wait_screen"])

    def test_hold_keys_parses_and_validates(self):
        steps = self.parse([{"hold_keys": {"vks": ["3", "Z"], "hold": 2.0}}])
        self.assertEqual(steps[0][0], "hold_keys")
        self.assertEqual(steps[0][1]["vks"], [0x33, 0x5A])
        with self.assertRaises(ksr.ScriptError):      # one key is not a chord
            self.parse([{"hold_keys": {"vks": ["3"], "hold": 1.0}}])
        with self.assertRaises(ksr.ScriptError):      # repeats would send two KEYDOWNs
            self.parse([{"hold_keys": {"vks": ["3", "3"], "hold": 1.0}}])

    def test_key_down_and_key_up_parse_and_validate(self):
        """A key held across steps is what lets a talk be captured while R stays down."""
        steps = self.parse([{"key_down": "3"}, {"sleep": 1}, {"key_up": {"vk": "3"}}])
        self.assertEqual([k for k, _ in steps], ["key_down", "sleep", "key_up"])
        self.assertEqual(steps[0][1]["vk"], 0x33)
        self.assertEqual(steps[2][1]["vk"], 0x33)
        for bad, why in (
                ([{"key_down": "3"}, {"key_down": "3"}, {"key_up": "3"}], "already held"),
                ([{"key_up": "3"}], "is not held"),
                ([{"key_down": "3"}, {"key": {"vk": "3", "hold": 1}}], "held by key_down"),
                ([{"key_down": "3"}, {"key_until": {"vk": "3", "until": "hud"}}],
                 "held by key_down"),
                ([{"key_down": "3"}, {"hold_keys": {"vks": ["3", "Z"], "hold": 1}}],
                 "held by key_down"),
                ([{"key_down": "3"}], "without key_up"),
        ):
            with self.assertRaises(ksr.ScriptError, msg=why):
                self.parse(bad)
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"key_down": "3"}, {"key_up": "3"}, {"key_up": "3"}])

    def test_snap_options_parse_and_validate(self):
        """`snap`/`snap_after` let a press capture a talk that only shows while held."""
        steps = self.parse([{"key": {"vk": "3", "hold": 1.2, "snap": "talk-00"}},
                            {"key_until": {"vk": "3", "until": "dialogue", "hold": 2.0,
                                           "snap": "talk-01", "snap_after": 1.5}}])
        self.assertEqual(steps[0][1]["snap"], "talk-00")
        self.assertIsNone(steps[0][1].get("snap_after"))
        self.assertEqual(steps[1][1]["snap"], "talk-01")
        self.assertEqual(steps[1][1]["snap_after"], 1.5)
        self.assertEqual(self.parse([{"key": {"vk": "3"}}])[0][1]["snap"], None)
        with self.assertRaises(ksr.ScriptError):      # a path is not a snapshot name
            self.parse([{"key": {"vk": "3", "snap": "../escape"}}])
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"key_until": {"vk": "3", "until": "dialogue", "snap": "a/b"}}])


class PressTelemetry(unittest.TestCase):
    """presses.jsonl records the host's own view of a press, with fakes for the live pieces."""

    class FakeWin:
        def __init__(self):
            self.calls = []

        def key(self, hwnd, vk, down):
            self.calls.append((vk, down))

        def is_foreground(self, hwnd):
            return True

    class FakeProc:
        def poll(self):
            return None

    class FakeGuest:
        def __init__(self, values):
            self.values = values
            self.reads = []

        def host_read(self, symbol, size, offset=0):
            self.reads.append((symbol, size, offset))
            if symbol == "s_key_down":
                return self.values.get("key_seen", 0)
            return self.values.get(symbol, 0)

    def make_run(self, tmp, values):
        win = self.FakeWin()
        guest = self.FakeGuest(values)
        run = ksr.Run(tmp, self.FakeProc(), tmp / "live.bmp", win, 60.0, time.time(),
                      lambda *a, **k: None, guest)
        run.hwnd = 1
        return run, win, guest

    def test_key_down_then_key_up_writes_one_press_record(self):
        with tempfile.TemporaryDirectory() as d:
            tmp = Path(d)
            values = {"s_present_serial": 100, "s_flips": 7}
            run, win, guest = self.make_run(tmp, values)
            # The up-read must see the guest having advanced during the hold.
            guest.values = values
            run.run_step("key_down", {"vk": 0x33})
            values["s_present_serial"] = 105
            values["key_seen"] = 1
            run.run_step("key_up", {"vk": 0x33})
            lines = [json.loads(x) for x in (tmp / "presses.jsonl").read_text().splitlines()
                     if x.strip()]
            self.assertEqual(len(lines), 1)
            rec = lines[0]
            self.assertEqual(rec["vk"], 0x33)
            self.assertEqual(rec["key_seen"], 1)
            self.assertTrue(rec["foreground"])
            self.assertGreater(rec["presents_up"], rec["presents_down"])
            self.assertFalse(run.held)
            self.assertIn((0x33, True), win.calls)
            self.assertIn((0x33, False), win.calls)

    def test_release_held_posts_keyup_for_an_unclosed_press(self):
        with tempfile.TemporaryDirectory() as d:
            tmp = Path(d)
            run, win, guest = self.make_run(tmp, {"s_present_serial": 1, "s_flips": 1})
            run.run_step("key_down", {"vk": "3"})
            self.assertTrue(run.held)
            released = run.release_held()
            self.assertEqual(released, [0x33])
            self.assertFalse(run.held)
            self.assertIn((0x33, False), win.calls)
            rec = json.loads((tmp / "presses.jsonl").read_text().splitlines()[0])
            self.assertEqual(rec["released_by"], "cleanup")

    def test_telemetry_failure_does_not_stop_the_press(self):
        """A host read that fails must record the error and let the press proceed."""
        class Boom:
            def host_read(self, symbol, size, offset=0):
                raise ksr.ScriptError("no such symbol")

        with tempfile.TemporaryDirectory() as d:
            tmp = Path(d)
            win = self.FakeWin()
            run = ksr.Run(tmp, self.FakeProc(), tmp / "live.bmp", win, 60.0, time.time(),
                          lambda *a, **k: None, Boom())
            run.hwnd = 1
            res = run.run_step("key", {"vk": 0x5A, "hold": 0.0})
            self.assertTrue(res["sent"])
            self.assertIn("telemetry_error", res["press"])
            self.assertIn((0x5A, True), win.calls)


class ReadHostPages(unittest.TestCase):
    """The page reader against this test's own process, with a deliberately unmapped page."""

    def test_middle_page_unreadable_is_zero_filled_and_reported(self):
        k32 = ctypes.windll.kernel32
        k32.VirtualAlloc.restype = ctypes.c_void_p
        k32.VirtualAlloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint, ctypes.c_uint]
        k32.VirtualProtect.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint,
                                       ctypes.POINTER(ctypes.c_uint)]
        MEM_COMMIT_RESERVE, PAGE_READWRITE, PAGE_NOACCESS = 0x3000, 0x04, 0x01
        base = k32.VirtualAlloc(None, 3 * ksr.GUEST_PAGE_SIZE, MEM_COMMIT_RESERVE, PAGE_READWRITE)
        self.assertTrue(base, "VirtualAlloc failed")
        try:
            ctypes.memset(base, 0xAB, ksr.GUEST_PAGE_SIZE)
            ctypes.memset(base + ksr.GUEST_PAGE_SIZE, 0xCD, ksr.GUEST_PAGE_SIZE)
            ctypes.memset(base + 2 * ksr.GUEST_PAGE_SIZE, 0xEF, ksr.GUEST_PAGE_SIZE)
            old = ctypes.c_uint(0)
            self.assertTrue(k32.VirtualProtect(ctypes.c_void_p(base + ksr.GUEST_PAGE_SIZE),
                                               ksr.GUEST_PAGE_SIZE, PAGE_NOACCESS,
                                               ctypes.byref(old)))
            data, failed = ksr.read_host_pages(ctypes.windll.kernel32.GetCurrentProcess(),
                                               base, 3 * ksr.GUEST_PAGE_SIZE)
        finally:
            k32.VirtualFree.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint]
            k32.VirtualFree(ctypes.c_void_p(base), 0, 0x8000)

        self.assertEqual(len(data), 3 * ksr.GUEST_PAGE_SIZE)
        self.assertEqual(data[:ksr.GUEST_PAGE_SIZE], b"\xab" * ksr.GUEST_PAGE_SIZE)
        self.assertEqual(data[2 * ksr.GUEST_PAGE_SIZE:], b"\xef" * ksr.GUEST_PAGE_SIZE)
        self.assertEqual(set(data[ksr.GUEST_PAGE_SIZE:2 * ksr.GUEST_PAGE_SIZE]), {0})
        self.assertEqual(failed, [ksr.GUEST_PAGE_SIZE])


if __name__ == "__main__":
    unittest.main(verbosity=2)
