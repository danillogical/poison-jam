"""Tests for scripts/key-script-run.py: the new detectors, step parsing, and the page reader.

Run directly: python -X utf8 tests/test_key_script_run.py
"""
from __future__ import annotations

import ctypes
import importlib.util
import json
import math
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


MENU_ORANGE_RGB = (255, 157, 0)   # the real NEW GAME highlight colour (measured on the archived menu)


def menu_bar_frame(orange_glyphs=0, city=False):
    """A synthetic menu-like frame: orange pixels in the NEW GAME line box.

    Two shapes, both measured from the archived runs (see MENU_LINE_NEW's comment):

    - `orange_glyphs=N`: the real menu's shape -- a ~30 px dark entry bar whose orange glyph band is ~3 px
      tall at its centre, so every glyph pixel has dark bar colour within a few rows above AND below.
    - `city=True`: the attract-mode city's shape -- orange pixels with bright scene colour around them and
      no dark bar, which is what made the colour-only detector accept the city."""
    a = frame((70, 95, 60))
    top, bottom, left, right = ksr.MENU_LINE_NEW
    mid = (top + bottom) // 2
    if city:
        # Bright textured colour through the box, orange pixels at the lower edge with bright green above
        # and below -- no dark bar, exactly the measured city frame (which scored 601 in this band, just
        # over the 600 gate, with only 7 of those pixels having dark bar colour around them).
        a[top:bottom, left:right] = (115, 121, 82)
        a[mid:mid + 6, left:right] = (131, 178, 90)
        for y in range(mid + 8, bottom, 3):
            a[y:y + 2, left:right] = (205, 145, 49)
        return a
    a[top:bottom, left:right] = (8, 40, 41)           # the dark entry bar
    # The real glyph band spans rows 234..252 of the 228..258 box (measured: 1397 glyph pixels over ~19
    # rows) as letter strokes with dark bar between them, so most glyph pixels have bar colour within a
    # few rows both above and below. Model that: 3-px-tall orange strokes with 2-px dark gaps, which is
    # what makes the vertical pairing hold for the whole glyph band rather than only at its edges.
    gtop, gbot = top + 6, bottom - 6
    for y in range(gtop, gbot, 5):
        for i in range(orange_glyphs):
            x = left + 6 + i * 4
            a[y:min(y + 3, gbot), x:x + 2] = MENU_ORANGE_RGB
    return a


class MenuDetector(unittest.TestCase):
    """The menu detector must not accept colour alone.

    Defect it pins (V3 run 20261013-185536-693-keyscript-e2e-vk-v3): `is_menu` was
    `menu_orange_count >= 600`, and the attract-mode city reached 601, so `key_until ... menu`
    was satisfied on the city and the whole script then ran against the wrong screens."""

    def test_menu_detected_on_glyph_lines(self):
        """Orange glyph strokes on the NEW GAME bar, plus a LOAD GAME line, is the menu."""
        self.assertTrue(ksr.is_menu(menu_bar_frame(orange_glyphs=60)))

    def test_city_colour_is_not_the_menu(self):
        """Orange pixels reaching the colour threshold with bright scene colour around them are NOT the
        menu. This is the V3 defect exactly: the attract-mode city scored 601 against a 600 threshold."""
        a = menu_bar_frame(city=True)
        self.assertGreaterEqual(ksr.menu_orange_count(a), ksr.MENU_MIN_ORANGE)
        self.assertEqual(ksr.menu_bar_glyphs(a), 0)
        self.assertFalse(ksr.is_menu(a))

    def test_orange_glyphs_without_a_bar_are_not_the_menu(self):
        """Orange glyph shapes on bright scene colour, with no dark bar around them, are not the menu:
        the witness needs the bar, not just orange strokes."""
        a = frame((180, 150, 60))                     # bright city-ish colour everywhere
        top, bottom, left, _ = ksr.MENU_LINE_NEW
        mid = (top + bottom) // 2
        for i in range(60):
            x = left + 6 + i * 4
            a[mid - 1:mid + 2, x:x + 2] = MENU_ORANGE_RGB
        self.assertGreaterEqual(ksr.menu_orange_count(a), ksr.MENU_MIN_ORANGE)
        self.assertFalse(ksr.is_menu(a))

    def test_no_orange_is_not_the_menu(self):
        """No orange at all is never the menu (the title screen and the in-game pause)."""
        self.assertFalse(ksr.is_menu(frame((40, 40, 40))))

    def test_menu_detection_survives_upscaling(self):
        """A 2x nearest upscale of the menu is still the menu, and of the colour-only frame still is
        not. The bar-gap search scales with the frame, so longer glyph runs do not defeat it."""
        pos = menu_bar_frame(orange_glyphs=60)
        neg = menu_bar_frame(city=True)
        up = lambda a: np.repeat(np.repeat(a, 2, axis=0), 2, axis=1)
        self.assertTrue(ksr.is_menu(up(pos)))
        self.assertFalse(ksr.is_menu(up(neg)))

    def test_menu_detection_survives_brightness(self):
        """The detector must not depend on a particular exposure: a dimmed menu is still the menu, and a
        brightened colour-only frame still is not."""
        pos = menu_bar_frame(orange_glyphs=60)
        neg = menu_bar_frame(city=True)
        dim = np.clip(pos.astype(np.float32) * 0.85, 0, 255).astype(np.uint8)
        bright = np.clip(neg.astype(np.float32) * 1.2, 0, 255).astype(np.uint8)
        self.assertTrue(ksr.is_menu(dim))
        self.assertFalse(ksr.is_menu(bright))


def title_frame(press_fill, mark_fill, size=(640, 480), bg=(10, 10, 10)):
    """A dark frame whose press-start and wordmark bands hold white column stripes.

    The stripe width is chosen on the detector's sampled grid (every 2nd row and column), so the sampled
    white fraction of each band is `press_fill` / `mark_fill` to within one sampled column."""
    w, h = size
    a = np.zeros((h, w, 3), dtype=np.uint8) + np.array(bg, dtype=np.uint8)
    for band, fill in ((ksr.TITLE_BAND, press_fill), (ksr.MARK_BAND, mark_fill)):
        top, bottom, left, right = band
        t, b = int(round(top * h / 480)), int(round(bottom * h / 480))
        l, r = int(round(left * w / 640)), int(round(right * w / 640))
        k = int(round(fill * len(range(l, r, 2))))
        a[t:b, l:l + 2 * k] = (255, 255, 255)
    return a


class TitleDetector(unittest.TestCase):
    """`is_title` must want white text in both bands, not a white fill.

    Defect it pins: the white Smilebit logo card satisfied the lower bounds, so `wait_title` fired on the
    logos and the end-to-end script desynchronised from the game."""

    def test_all_white_frame_is_not_the_title(self):
        self.assertFalse(ksr.is_title(frame((255, 255, 255))))

    def test_all_black_frame_is_not_the_title(self):
        self.assertFalse(ksr.is_title(frame((0, 0, 0))))

    def test_white_text_on_dark_is_the_title(self):
        a = title_frame(0.20, 0.43)
        self.assertTrue(ksr.is_title(a))

    def test_fixture_hits_the_target_fractions(self):
        """Guard the fixture itself: the sampled fractions are the ones the tests claim."""
        a = title_frame(0.20, 0.43)
        self.assertAlmostEqual(ksr.title_white_count(a) / ksr.band_samples(a), 0.20, delta=0.02)
        self.assertAlmostEqual(ksr.title_white_count(a, ksr.MARK_BAND) / ksr.band_samples(a, ksr.MARK_BAND),
                               0.43, delta=0.02)

    def test_title_survives_scaling(self):
        self.assertTrue(ksr.is_title(title_frame(0.20, 0.43, size=(1280, 960))))

    def test_all_white_frame_is_not_the_title_at_2x(self):
        self.assertFalse(ksr.is_title(np.full((960, 1280, 3), 255, dtype=np.uint8)))

    def test_one_band_only_is_not_the_title(self):
        self.assertFalse(ksr.is_title(title_frame(0.20, 0.0)))     # press band only
        self.assertFalse(ksr.is_title(title_frame(0.0, 0.43)))     # wordmark band only

    def test_too_little_white_is_not_the_title(self):
        self.assertFalse(ksr.is_title(title_frame(0.05, 0.43)))    # ~124 sampled < 300
        self.assertFalse(ksr.is_title(title_frame(0.20, 0.10)))    # ~666 sampled < 2500

    def test_press_fill_above_its_upper_bound_is_not_the_title(self):
        self.assertTrue(ksr.is_title(title_frame(0.38, 0.43)))
        self.assertFalse(ksr.is_title(title_frame(0.43, 0.43)))

    def test_wordmark_fill_above_its_upper_bound_is_not_the_title(self):
        self.assertTrue(ksr.is_title(title_frame(0.20, 0.68)))
        self.assertFalse(ksr.is_title(title_frame(0.20, 0.73)))

    def test_upper_bounds_hold_at_2x(self):
        self.assertFalse(ksr.is_title(title_frame(0.43, 0.43, size=(1280, 960))))
        self.assertFalse(ksr.is_title(title_frame(0.20, 0.73, size=(1280, 960))))


def orange_blobs_frame(bar_above, bar_below, size_scale=1):
    """A bright frame with 60 orange glyph strokes in the NEW GAME line, and optionally dark bar rows
    directly above and/or below each stroke row (so the pairing can be broken on one side)."""
    a = frame((180, 150, 60))
    top, bottom, left, _ = ksr.MENU_LINE_NEW
    for y in range(top + 6, bottom - 6, 10):      # 10 apart so one row's bar is not its neighbour's
        for i in range(60):
            x = left + 6 + i * 4
            a[y:y + 3, x:x + 2] = MENU_ORANGE_RGB
        if bar_above:
            a[y - 2:y, :] = (8, 40, 41)
        if bar_below:
            a[y + 3:y + 5, :] = (8, 40, 41)
    if size_scale > 1:
        a = np.repeat(np.repeat(a, size_scale, axis=0), size_scale, axis=1)
    return a


class MenuDetectorGates(unittest.TestCase):
    """The two gates of `is_menu`, each exercised alone (the cases above cover the main shapes)."""

    def test_fixture_pairs_bars_when_asked(self):
        a = orange_blobs_frame(True, True)
        self.assertGreaterEqual(ksr.menu_orange_count(a), ksr.MENU_MIN_ORANGE)
        self.assertTrue(ksr.is_menu(a))

    def test_dark_bar_on_one_side_only_is_not_the_menu(self):
        for above, below in ((True, False), (False, True)):
            a = orange_blobs_frame(above, below)
            self.assertGreaterEqual(ksr.menu_orange_count(a), ksr.MENU_MIN_ORANGE)
            self.assertFalse(ksr.is_menu(a), "bar above=%s below=%s" % (above, below))

    def test_orange_blobs_with_no_bar_are_not_the_menu_at_2x(self):
        a = orange_blobs_frame(False, False, size_scale=2)
        self.assertGreaterEqual(ksr.menu_orange_count(a), ksr.MENU_MIN_ORANGE * 4)
        self.assertFalse(ksr.is_menu(a))

    def test_bar_pairing_survives_2x(self):
        self.assertTrue(ksr.is_menu(orange_blobs_frame(True, True, size_scale=2)))

    def test_colour_gate_is_required(self):
        """Enough barred glyph pixels but fewer than 600 orange in the band is not the menu."""
        a = menu_bar_frame(orange_glyphs=14)
        self.assertGreaterEqual(ksr.menu_bar_glyphs(a), ksr.MENU_MIN_BAR_GLYPHS)
        self.assertLess(ksr.menu_orange_count(a), ksr.MENU_MIN_ORANGE)
        self.assertFalse(ksr.is_menu(a))


class ArchivedMenuFrames(unittest.TestCase):
    """Verify the detector on the real archived frames, locally only.

    `logs/` is gitignored, so these frames exist on this machine and never in the repository: no retail
    screenshot is committed. The class skips when the archive is absent, which keeps the suite green on a
    fresh clone while still running the real-frame check where the evidence lives.

    The two negatives are the exact frames the colour-only detector accepted, which is the defect this
    work fixes (V3 run 20261013-185536-693-keyscript-e2e-vk-v3)."""

    V3 = ROOT / "logs/runs/20261013-185536-693-keyscript-e2e-vk-v3"
    D3D11 = ROOT / "logs/runs/20261013-010916-545-keyscript-e2e-f2"
    POSITIVES = ("paused.png", "57_key_after.png")
    NEGATIVES = ("01_key_until_after.png", "main-menu.png")

    @staticmethod
    def _load(path):
        from PIL import Image
        with Image.open(path) as im:
            return np.asarray(im.convert("RGB")).copy()

    def _frames(self, root, names):
        return [(n, root / n) for n in names]

    @unittest.skipUnless((ROOT / "logs/runs/20261013-185536-693-keyscript-e2e-vk-v3/paused.png").exists()
                         and (ROOT / "logs/runs/20261013-010916-545-keyscript-e2e-f2/main-menu.png").exists(),
                         "archived e2e frames are local-only (logs/ is gitignored)")
    def test_real_frames_classify(self):
        """The actual menus are the menu and the attract-city frames are not, at 1x and 2x."""
        for name, path in (self._frames(self.V3, self.POSITIVES)
                           + self._frames(self.D3D11, ("main-menu.png",))):
            a = self._load(path)
            self.assertTrue(ksr.is_menu(a), "%s must be the main menu" % name)
            self.assertTrue(ksr.is_menu(np.repeat(np.repeat(a, 2, axis=0), 2, axis=1)),
                            "%s must still be the menu at 2x" % name)
        for name, path in (self._frames(self.V3, self.NEGATIVES)
                           + self._frames(self.D3D11, ("paused.png",))):
            a = self._load(path)
            self.assertFalse(ksr.is_menu(a), "%s is not the main menu" % name)


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


F, B, L, R = 0x68, 0x62, 0x64, 0x66
SECTOR_KEYS = {0: (F,), 1: (F, R), 2: (R,), 3: (B, R), 4: (B,),
               -4: (B,), -3: (B, L), -2: (L,), -1: (F, L)}


def yaw_vec(deg, scale=1.0):
    """The (x, z) vector whose bearing atan2(x, z) is `deg`."""
    r = math.radians(deg)
    return (scale * math.sin(r), scale * math.cos(r))


class ResolveChain(unittest.TestCase):
    def test_follows_pointers_and_adds_offsets(self):
        mem = {0x1000: 0x5000, 0x5010: 0x9000}
        reads = []

        def rd(a):
            reads.append(a)
            return mem[a]
        self.assertEqual(ksr.resolve_chain(rd, [0x1000, 0x10, 0xCA4]), 0x9000 + 0xCA4)
        self.assertEqual(reads, [0x1000, 0x5010])

    def test_single_element_chain_does_no_read(self):
        def rd(a):
            raise AssertionError("must not read")
        self.assertEqual(ksr.resolve_chain(rd, [0x22FCE0]), 0x22FCE0)

    def test_null_pointer_partway_raises(self):
        mem = {0x1000: 0x5000, 0x5010: 0}
        with self.assertRaises(ksr.ScriptError):
            ksr.resolve_chain(lambda a: mem[a], [0x1000, 0x10, 0x20, 0x30])

    def test_null_at_first_read_raises(self):
        with self.assertRaises(ksr.ScriptError):
            ksr.resolve_chain(lambda a: 0, [0x1000, 0x10])


class SteerKeys(unittest.TestCase):
    def check_sectors(self, cam_deg):
        fwd = yaw_vec(cam_deg)
        for k, keys in SECTOR_KEYS.items():
            for jitter in (-10, 0, 10):
                tx, tz = yaw_vec(cam_deg + 45 * k + jitter, 7.0)
                self.assertEqual(ksr.steer_keys(tx, tz, *fwd), keys,
                                 "cam=%s sector=%s jitter=%s" % (cam_deg, k, jitter))

    def test_all_sectors_camera_forward_z(self):
        self.check_sectors(0)

    def test_all_sectors_camera_off_axis(self):
        self.check_sectors(137)

    def test_positive_bearing_is_right(self):
        self.assertEqual(ksr.steer_keys(1, 0, 0, 1), (R,))
        self.assertEqual(ksr.steer_keys(-1, 0, 0, 1), (L,))
        self.assertEqual(ksr.steer_keys(0, 1, 0, 1), (F,))
        self.assertEqual(ksr.steer_keys(0, -1, 0, 1), (B,))

    def test_wraparound_camera_170_target_minus_170_is_forward(self):
        fwd = yaw_vec(170)
        tx, tz = yaw_vec(-170)
        self.assertEqual(ksr.steer_keys(tx, tz, *fwd), (F,))

    def test_wraparound_the_other_way_is_forward(self):
        fwd = yaw_vec(-170)
        tx, tz = yaw_vec(170)
        self.assertEqual(ksr.steer_keys(tx, tz, *fwd), (F,))

    def test_zero_length_returns_empty(self):
        self.assertEqual(ksr.steer_keys(0, 0, 0, 1), ())
        self.assertEqual(ksr.steer_keys(0.0, 0.0, 0.6, -0.8), ())

    def test_scale_invariance(self):
        for cam in (0, 137, -60):
            for rel in range(-180, 180, 20):
                if abs(abs(rel) % 45 - 22.5) < 3:
                    continue
                base = ksr.steer_keys(*yaw_vec(cam + rel), *yaw_vec(cam))
                for s in (0.001, 3.0, 1e4):
                    self.assertEqual(ksr.steer_keys(*yaw_vec(cam + rel, s), *yaw_vec(cam)), base)
                    self.assertEqual(ksr.steer_keys(*yaw_vec(cam + rel), *yaw_vec(cam, s)), base)

    def test_rotation_invariance(self):
        for rel in (0, 30, 60, 100, 130, 170, -30, -80, -110, -150):
            base = ksr.steer_keys(*yaw_vec(rel), *yaw_vec(0))
            for rot in (17, 90, 137, 200, -75):
                self.assertEqual(ksr.steer_keys(*yaw_vec(rel + rot), *yaw_vec(rot)), base,
                                 "rel=%s rot=%s" % (rel, rot))


class ParseChainPeekGotoLogPos(unittest.TestCase):
    def parse(self, steps):
        return ksr.parse_script(steps)

    def peek(self, **kw):
        body = {"name": "n"}
        body.update(kw)
        return [{"peek": body}]

    def test_chain_peek_accepted(self):
        steps = self.parse([{"peek": {"name": "n", "chain": ["0x22FCE0", "0x148", "0xCA4"],
                                      "type": "f32", "count": 3}}])
        o = steps[0][1]
        self.assertEqual(steps[0][0], "peek")
        self.assertEqual(o["chain"], [0x22FCE0, 0x148, 0xCA4])
        self.assertEqual(o["type"], "f32")
        self.assertEqual(o["count"], 3)

    def test_old_va_size_form_still_accepted(self):
        steps = self.parse(self.peek(va="0x11000", size=4))
        self.assertEqual(steps[0][1]["va"], 0x11000)

    def test_chain_type_must_be_u32_or_f32(self):
        for bad in ("u16", "f64", "", 4):
            with self.assertRaises(ksr.ScriptError):
                self.parse(self.peek(chain=["0x22FCE0", "0x10"], type=bad))
        for good in ("u32", "f32"):
            self.parse(self.peek(chain=["0x22FCE0", "0x10"], type=good))

    def test_chain_count_range(self):
        for bad in (0, -1, 65, 1.5, "3"):
            with self.assertRaises(ksr.ScriptError):
                self.parse(self.peek(chain=["0x22FCE0", "0x10"], count=bad))
        for good in (1, 64):
            self.parse(self.peek(chain=["0x22FCE0", "0x10"], count=good))

    def test_chain_with_va_or_size_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse(self.peek(chain=["0x22FCE0", "0x10"], va="0x11000"))
        with self.assertRaises(ksr.ScriptError):
            self.parse(self.peek(chain=["0x22FCE0", "0x10"], size=4))

    def test_type_or_count_without_chain_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse(self.peek(va="0x11000", type="f32"))
        with self.assertRaises(ksr.ScriptError):
            self.parse(self.peek(va="0x11000", count=2))

    def test_duplicate_chain_peek_name_rejected(self):
        with self.assertRaises(ksr.ScriptError) as cm:
            self.parse(self.peek(chain=["0x22FCE0", "0x10"]) + self.peek(va="0x11000"))
        self.assertIn("duplicate", str(cm.exception))

    GOTO = {"target": [10, -20.5], "radius": 25, "timeout": 120, "pulse": 0.4,
            "player_obj": "0x2C", "until": [{"off": "0x300", "eq": 1}], "snap": "n"}

    def goto(self, **kw):
        body = dict(self.GOTO)
        body.update(kw)
        return self.parse([{"goto": body}])

    def test_goto_full_form_accepted(self):
        steps = self.goto()
        self.assertEqual(steps[0][0], "goto")
        o = steps[0][1]
        self.assertEqual(o["target"], [10.0, -20.5])
        self.assertEqual(o["radius"], 25)
        self.assertEqual(o["until"], [{"off": 0x300, "eq": 1}])
        self.assertEqual(o["snap"], "n")

    def test_goto_defaults(self):
        o = self.parse([{"goto": {"target": [1, 2]}}])[0][1]
        self.assertEqual((o["radius"], o["timeout"], o["pulse"]), (25, 120, 0.4))
        self.assertEqual(o["until"], [])

    def test_goto_target_required_and_two_numbers(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"goto": {"radius": 5}}])
        for bad in ([1], [1, 2, 3], "1,2", [1, "2"], [None, 2], [True, 2], 5):
            with self.assertRaises(ksr.ScriptError):
                self.goto(target=bad)

    def test_goto_numeric_options_validated(self):
        for key in ("radius", "timeout", "pulse"):
            with self.assertRaises(ksr.ScriptError):
                self.goto(**{key: "fast"})
        o = self.goto(radius=3.5, timeout=60, pulse=0.2)[0][1]
        self.assertEqual((o["radius"], o["timeout"], o["pulse"]), (3.5, 60, 0.2))

    def test_goto_unknown_option_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.goto(speed=3)

    def test_log_pos_string_and_object_forms(self):
        a = self.parse([{"log_pos": "name"}])
        self.assertEqual(a[0][0], "log_pos")
        self.assertEqual(a[0][1]["name"], "name")
        b = self.parse([{"log_pos": {"name": "n", "objs": ["0x2C", "0x2E"]}}])
        self.assertEqual(b[0][1]["objs"], [0x2C, 0x2E])

    def test_log_pos_unknown_option_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([{"log_pos": {"name": "n", "bogus": 1}}])


class GotoStopReason(unittest.TestCase):
    def test_radius_is_inclusive(self):
        self.assertEqual(ksr.goto_stop_reason(25, 25, []), "radius")
        self.assertEqual(ksr.goto_stop_reason(24.9, 25, []), "radius")
        self.assertIsNone(ksr.goto_stop_reason(25.1, 25, []))

    def test_radius_wins_over_until(self):
        self.assertEqual(ksr.goto_stop_reason(1, 25, [True, True]), "radius")
        self.assertEqual(ksr.goto_stop_reason(25, 25, [True]), "radius")

    def test_until_all_truthy_stops(self):
        self.assertEqual(ksr.goto_stop_reason(100, 25, [True]), "until")
        self.assertEqual(ksr.goto_stop_reason(100, 25, [True, 1, True]), "until")

    def test_until_partial_does_not_stop(self):
        self.assertIsNone(ksr.goto_stop_reason(100, 25, [True, False]))
        self.assertIsNone(ksr.goto_stop_reason(100, 25, [False]))
        self.assertIsNone(ksr.goto_stop_reason(100, 25, [True, 0]))

    def test_empty_until_never_stops(self):
        self.assertIsNone(ksr.goto_stop_reason(100, 25, []))


class StuckAction(unittest.TestCase):
    def test_first_ten_events(self):
        want = ["jump", "side_left", "jump", "side_right", "jump",
                "side_left", "jump", "side_right", "jump", "side_left"]
        self.assertEqual([ksr.stuck_action(n) for n in range(10)], want)

    def test_large_n(self):
        self.assertEqual(ksr.stuck_action(1000), "jump")
        self.assertEqual(ksr.stuck_action(1001), "side_left")
        self.assertEqual(ksr.stuck_action(1003), "side_right")
        self.assertEqual(ksr.stuck_action(10 ** 9 + 1), "side_left")
        self.assertEqual(ksr.stuck_action(10 ** 9 + 3), "side_right")


class GotoJumpOptions(unittest.TestCase):
    def goto(self, **kw):
        body = {"target": [0, 0]}
        body.update(kw)
        return ksr.parse_script([{"goto": body}])[0][1]

    def test_vk_jump_is_z(self):
        self.assertEqual(ksr.VK_JUMP, 0x5A)

    def test_defaults(self):
        o = self.goto()
        self.assertIs(o["jump_when_stuck"], False)
        self.assertEqual(o["jump_hold"], 0.3)
        self.assertEqual(o["jump_follow"], 1.0)
        self.assertEqual(o["radius"], 25)
        self.assertEqual(o["timeout"], 120)
        self.assertEqual(o["pulse"], 0.4)

    def test_jump_when_stuck_must_be_bool(self):
        self.assertIs(self.goto(jump_when_stuck=True)["jump_when_stuck"], True)
        self.assertIs(self.goto(jump_when_stuck=False)["jump_when_stuck"], False)
        for bad in (1, 0, "yes", None):
            with self.assertRaises(ksr.ScriptError):
                self.goto(jump_when_stuck=bad)

    def test_jump_hold_bounds(self):
        self.assertEqual(self.goto(jump_hold=0.05)["jump_hold"], 0.05)
        self.assertEqual(self.goto(jump_hold=2)["jump_hold"], 2.0)
        for bad in (0.01, -1, "0.3", True):
            with self.assertRaises(ksr.ScriptError):
                self.goto(jump_hold=bad)

    def test_jump_follow_bounds(self):
        self.assertEqual(self.goto(jump_follow=0)["jump_follow"], 0.0)
        self.assertEqual(self.goto(jump_follow=2.5)["jump_follow"], 2.5)
        for bad in (-0.1, -1, "1", True):
            with self.assertRaises(ksr.ScriptError):
                self.goto(jump_follow=bad)

    def test_unknown_option_still_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.goto(jump_everywhere=True)


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


def steady_samples(frame_s, poll_s, seconds, start_serial=100):
    """Polls every poll_s of a counter that increments every frame_s (increments land between polls)."""
    out, t = [], 0.0
    while t <= seconds + 1e-9:
        out.append((t, start_serial + int(math.floor(t / frame_s + 1e-9))))
        t += poll_s
    return out


def stream_from_intervals(intervals, lead=0.0105, start_serial=0):
    """One poll per present at the exact present times, after a lead-in poll with no increase."""
    samples, t, s = [(0.0, start_serial)], lead, start_serial
    samples.append((t, s + 1))
    s += 1
    for dt in intervals:
        t += dt
        s += 1
        samples.append((t, s))
    return samples


class BenchIntervals(unittest.TestCase):
    def test_constant_60fps_polled_at_2ms(self):
        samples = steady_samples(1 / 60.0, 0.002, 2.0)
        intervals, aliased = ksr.bench_intervals(samples)
        self.assertEqual(aliased, 0)
        self.assertGreater(len(intervals), 100)
        stats = ksr.bench_stats(intervals, 2.0, 120, aliased, ksr.bench_poll_stats(samples))
        self.assertAlmostEqual(stats["frame_ms_p50"], 16.67, delta=2.1)   # one poll of quantisation
        self.assertAlmostEqual(stats["frame_ms_mean"], 16.67, delta=0.3)
        self.assertEqual(stats["aliased_fraction"], 0.0)
        self.assertEqual(stats["stalls_over_100ms"], 0)

    def test_constant_stream_exact(self):
        # Increments land exactly on polls, so the intervals are exact.
        samples = stream_from_intervals([0.016] * 10)
        intervals, aliased = ksr.bench_intervals(samples)
        self.assertEqual(len(intervals), 10)
        for i in intervals:
            self.assertAlmostEqual(i, 0.016, places=9)
        self.assertEqual(aliased, 0)

    def test_first_increase_gives_no_interval(self):
        samples = [(0.0, 5), (0.002, 5), (0.004, 6)]
        self.assertEqual(ksr.bench_intervals(samples), ([], 0))
        samples.append((0.020, 7))
        intervals, aliased = ksr.bench_intervals(samples)
        self.assertEqual(len(intervals), 1)
        self.assertAlmostEqual(intervals[0], 0.016)

    def test_first_increase_aliased_is_not_counted(self):
        # The reference-setting increase yields nothing, aliased or not.
        intervals, aliased = ksr.bench_intervals([(0.0, 5), (0.01, 8)])
        self.assertEqual((intervals, aliased), ([], 0))

    def test_polls_without_increase_yield_nothing(self):
        samples = [(0.0, 1), (0.01, 2), (0.012, 2), (0.014, 2), (0.016, 2), (0.03, 3)]
        intervals, aliased = ksr.bench_intervals(samples)
        self.assertEqual(len(intervals), 1)
        self.assertAlmostEqual(intervals[0], 0.02)     # spans the idle polls, not 0.002
        self.assertEqual(aliased, 0)

    def test_interval_is_between_increases_not_between_polls(self):
        samples = [(0.0, 0), (0.001, 1), (0.003, 1), (0.005, 1), (0.017, 2), (0.019, 2)]
        intervals, _ = ksr.bench_intervals(samples)
        self.assertEqual(len(intervals), 1)
        self.assertAlmostEqual(intervals[0], 0.016)

    def test_jittered_stream_exact_percentiles(self):
        known = [0.010, 0.012, 0.014, 0.016, 0.018, 0.020, 0.022, 0.024, 0.026, 0.028, 0.030]
        intervals, aliased = ksr.bench_intervals(stream_from_intervals(known))
        self.assertEqual(aliased, 0)
        self.assertEqual(len(intervals), len(known))
        for got, want in zip(intervals, known):
            self.assertAlmostEqual(got, want, places=9)
        stats = ksr.bench_stats(intervals, 1.0, 12)
        self.assertAlmostEqual(stats["frame_ms_p50"], 20.0, places=6)
        self.assertAlmostEqual(stats["frame_ms_p90"], 28.0, places=6)   # position 9 of 0..10
        self.assertAlmostEqual(stats["frame_ms_p99"], 29.8, places=6)    # 28 + 0.9 * 2
        self.assertAlmostEqual(stats["frame_ms_max"], 30.0, places=6)
        self.assertAlmostEqual(stats["frame_ms_mean"], 20.0, places=6)

    def test_percentile_interpolates(self):
        stats = ksr.bench_stats([0.010, 0.020, 0.030, 0.040], 1.0, 5)
        self.assertAlmostEqual(stats["frame_ms_p50"], 25.0, places=6)
        self.assertAlmostEqual(stats["frame_ms_p90"], 37.0, places=6)    # position 2.7
        self.assertAlmostEqual(stats["frame_ms_p99"], 39.7, places=6)    # position 2.97

    def test_percentiles_are_over_intervals_not_polls(self):
        # Polls every 2 ms; presents at 10, 30, 60 and 150 ms -> intervals 20, 30, 90 ms.
        present_at = [10, 30, 60, 150]
        samples = [(ms / 1000.0, sum(1 for p in present_at if p <= ms)) for ms in range(0, 160, 2)]
        intervals, aliased = ksr.bench_intervals(samples)
        self.assertEqual(aliased, 0)
        self.assertEqual([round(i * 1000) for i in intervals], [20, 30, 90])
        poll = ksr.bench_poll_stats(samples)
        self.assertEqual(poll["polls"], 80)
        stats = ksr.bench_stats(intervals, 0.16, poll["presents"], aliased, poll)
        self.assertAlmostEqual(stats["frame_ms_p50"], 30.0, places=6)    # raw poll gaps would say 2
        self.assertAlmostEqual(stats["frame_ms_max"], 90.0, places=6)
        self.assertAlmostEqual(stats["frame_ms_mean"], 140.0 / 3, places=6)

    def test_stall(self):
        known = [0.016] * 5 + [0.250] + [0.016] * 5
        intervals, aliased = ksr.bench_intervals(stream_from_intervals(known))
        stats = ksr.bench_stats(intervals, 1.0, 12, aliased)
        self.assertAlmostEqual(stats["frame_ms_max"], 250.0, places=6)
        self.assertEqual(stats["stalls_over_100ms"], 1)

    def test_stall_threshold_is_strictly_over_100ms(self):
        stats = ksr.bench_stats([0.100, 0.101, 0.05], 1.0, 4)
        self.assertEqual(stats["stalls_over_100ms"], 1)

    def test_aliasing_gives_n_equal_intervals(self):
        samples = [(0.0, 10), (0.001, 11), (0.001 + 0.048, 14)]
        intervals, aliased = ksr.bench_intervals(samples)
        self.assertEqual(aliased, 1)
        self.assertEqual(len(intervals), 3)
        for i in intervals:
            self.assertAlmostEqual(i, 0.016, places=9)

    def test_aliased_fraction(self):
        samples = [(0.0, 0), (0.01, 1), (0.02, 2), (0.05, 5), (0.06, 6)]
        intervals, aliased = ksr.bench_intervals(samples)
        poll = ksr.bench_poll_stats(samples)
        self.assertEqual(aliased, 1)
        self.assertEqual(poll["increment_polls"], 4)
        stats = ksr.bench_stats(intervals, 0.06, poll["presents"], aliased, poll)
        self.assertAlmostEqual(stats["aliased_fraction"], 0.25)

    def test_wrap_around(self):
        samples = [(0.0, 0xFFFFFFFE), (0.010, 0xFFFFFFFF), (0.042, 0x00000001), (0.058, 0x00000002)]
        intervals, aliased = ksr.bench_intervals(samples)
        self.assertEqual(aliased, 1)
        self.assertEqual(len(intervals), 3)       # +2 across the wrap -> two 16 ms, then one 16 ms
        for i in intervals:
            self.assertAlmostEqual(i, 0.016, places=9)

    def test_wrap_presents(self):
        samples = [(0.0, 0xFFFFFFFF), (0.01, 0x00000001)]
        self.assertEqual(ksr.bench_poll_stats(samples)["presents"], 2)
        self.assertEqual(ksr.bench_intervals(samples), ([], 0))

    def test_empty_and_no_increment_windows(self):
        for samples in ([], [(0.0, 7)], [(0.0, 7), (0.002, 7), (0.004, 7)]):
            intervals, aliased = ksr.bench_intervals(samples)
            self.assertEqual((intervals, aliased), ([], 0))
            poll = ksr.bench_poll_stats(samples)
            self.assertEqual(poll["presents"], 0)
            self.assertEqual(poll["polls"], len(samples))
            self.assertEqual(poll["increment_polls"], 0)
            stats = ksr.bench_stats(intervals, 1.0, poll["presents"], aliased, poll)
            for k in ("frame_ms_p50", "frame_ms_p90", "frame_ms_p99", "frame_ms_max", "frame_ms_mean"):
                self.assertIsNone(stats[k], k)
            self.assertEqual(stats["stalls_over_100ms"], 0)
            self.assertEqual(stats["presents_per_s"], 0)

    def test_stats_empty_without_poll(self):
        stats = ksr.bench_stats([], 5.0, 0)
        self.assertIsNone(stats["frame_ms_p50"])
        self.assertIsNone(stats["frame_ms_max"])

    def test_presents_per_s(self):
        stats = ksr.bench_stats([0.016] * 3, 2.0, 120)
        self.assertAlmostEqual(stats["presents_per_s"], 60.0)

    def test_poll_stats_fields(self):
        samples = [(0.0, 1), (0.002, 1), (0.004, 2), (0.010, 2), (0.012, 4)]
        poll = ksr.bench_poll_stats(samples)
        self.assertEqual(poll["polls"], 5)
        self.assertEqual(poll["increment_polls"], 2)
        self.assertEqual(poll["presents"], 3)
        # gaps: 2, 2, 6, 2 ms -> median 2 ms
        self.assertAlmostEqual(poll["median_poll_period_ms"], 2.0, places=6)


class BenchStepParse(unittest.TestCase):
    def parse(self, steps):
        return ksr.parse_script(steps)

    def bench(self, **kw):
        return {"bench": kw}

    def test_accepts_full_form(self):
        steps = self.parse([self.bench(name="garage", seconds=60, poll_ms=2)])
        self.assertEqual(steps[0][0], "bench")
        self.assertEqual(steps[0][1]["name"], "garage")
        self.assertEqual(steps[0][1]["seconds"], 60)
        self.assertEqual(steps[0][1]["poll_ms"], 2)

    def test_poll_ms_defaults_to_2(self):
        steps = self.parse([self.bench(name="g", seconds=1)])
        self.assertEqual(steps[0][1]["poll_ms"], 2)

    def test_poll_ms_bounds(self):
        for ok in (0.5, 50, 10):
            self.parse([self.bench(name="g", seconds=1, poll_ms=ok)])
        for bad in (0.49, 0.1, 0, 50.01, 100, -1, "2", None, True):
            with self.assertRaises(ksr.ScriptError, msg=repr(bad)):
                self.parse([self.bench(name="g", seconds=1, poll_ms=bad)])

    def test_name_required_and_pattern(self):
        for ok in ("a", "_x", "garage-2", "A_b-9"):
            self.parse([self.bench(name=ok, seconds=1)])
        for bad in ("", "1abc", "-a", "a b", "a.b", "a/b", "é", 5, None):
            with self.assertRaises(ksr.ScriptError, msg=repr(bad)):
                self.parse([self.bench(name=bad, seconds=1)])
        with self.assertRaises(ksr.ScriptError):
            self.parse([self.bench(seconds=1)])

    def test_seconds_required_and_positive(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([self.bench(name="g")])
        for bad in (0, -1, "5", None, True):
            with self.assertRaises(ksr.ScriptError, msg=repr(bad)):
                self.parse([self.bench(name="g", seconds=bad)])
        self.parse([self.bench(name="g", seconds=0.5)])

    def test_duplicate_name_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([self.bench(name="g", seconds=1), self.bench(name="g", seconds=2)])
        self.parse([self.bench(name="g", seconds=1), self.bench(name="h", seconds=2)])

    def test_unknown_option_rejected(self):
        with self.assertRaises(ksr.ScriptError):
            self.parse([self.bench(name="g", seconds=1, extra=1)])
        with self.assertRaises(ksr.ScriptError):
            self.parse([self.bench(name="g", seconds=1, snap="x")])


if __name__ == "__main__":
    unittest.main(verbosity=2)
