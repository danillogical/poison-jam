"""Tests for scripts/key-script-run.py: the new detectors, step parsing, and the page reader.

Run directly: python -X utf8 tests/test_key_script_run.py
"""
from __future__ import annotations

import ctypes
import importlib.util
import sys
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

    def test_black_frame_is_neither(self):
        a = frame((0, 0, 0))
        self.assertFalse(ksr.is_hud(a))
        self.assertFalse(ksr.is_dialogue(a))

    def test_screens_include_the_new_detectors(self):
        self.assertEqual(set(ksr.SCREENS), {"title", "menu", "hud", "dialogue"})


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
