"""Tests for scripts/text-stability.py: flicker vs page turn classification and the run reader.

Run directly: python -X utf8 tests/test_text_stability.py
"""
from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "text-stability.py"

try:
    from PIL import Image
except ImportError:  # the system python on the Mac has numpy but not Pillow
    Image = None


def load_module():
    """Load the hyphenated script as a module, the way the other tests load hyphenated scripts."""
    spec = importlib.util.spec_from_file_location("text_stability", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


ts = load_module()


def crop(ink=()):
    """A tiny white region with black 'glyph' pixels at the given (row, col) positions."""
    a = np.full((8, 20, 3), 255, dtype=np.uint8)
    for r, c in ink:
        a[r, c] = 0
    return a


def glyph(col):
    """Ten ink pixels in one column: enough to clear the default min-pixels."""
    return [(r, col) for r in range(8)] + [(0, col + 1), (1, col + 1)]


A = crop(glyph(2))
B = crop(glyph(2) + glyph(6))     # a glyph added
B2 = crop(glyph(2) + glyph(10))   # a different glyph added
C = crop(glyph(2) + glyph(6) + glyph(10))


class ClassifyTests(unittest.TestCase):
    def test_flicker_is_a_b_a(self):
        flickers, turns = ts.classify_triplets([A, A, B, A, A])
        self.assertEqual(flickers, [2])
        self.assertEqual(turns, 0)

    def test_page_turn_is_a_a_b_b(self):
        flickers, turns = ts.classify_triplets([A, A, B, B])
        self.assertEqual(flickers, [])
        self.assertEqual(turns, 1)

    def test_progressive_reveal_is_two_changes_not_flicker(self):
        flickers, turns = ts.classify_triplets([A, B, C])
        self.assertEqual(flickers, [])
        self.assertEqual(turns, 2)

    def test_flicker_between_a_page_turn_and_steady_state(self):
        flickers, turns = ts.classify_triplets([A, A, B2, A, C, C])
        self.assertEqual(flickers, [2])
        self.assertEqual(turns, 1)  # A to C, after the flicker has resolved

    def test_noise_below_threshold_is_ignored(self):
        noisy = A.astype(np.int16) + 10          # every pixel moves, but under --thresh
        noisy = np.clip(noisy, 0, 255).astype(np.uint8)
        self.assertFalse(ts.regions_differ(A, noisy))
        self.assertEqual(ts.classify_triplets([A, noisy, A, noisy]), ([], 0))

    def test_a_few_stray_pixels_are_ignored(self):
        stray = crop(glyph(2) + [(7, 15), (7, 16)])
        self.assertFalse(ts.regions_differ(A, stray))
        self.assertEqual(ts.classify_triplets([A, stray, A]), ([], 0))

    def test_threshold_and_min_pixels_are_tunable(self):
        stray = crop(glyph(2) + [(7, 15), (7, 16)])
        self.assertTrue(ts.regions_differ(A, stray, min_pixels=2))
        faint = A.astype(np.int16) + 10
        faint = np.clip(faint, 0, 255).astype(np.uint8)
        self.assertTrue(ts.regions_differ(A, faint, thresh=5))

    def test_short_runs_have_nothing_to_classify(self):
        self.assertEqual(ts.classify_triplets([]), ([], 0))
        self.assertEqual(ts.classify_triplets([A]), ([], 0))
        self.assertEqual(ts.classify_triplets([A, B]), ([], 1))

    def test_alternation_is_all_flicker_with_no_page_turns(self):
        flickers, turns = ts.classify_triplets([A, B, A, B, A])
        self.assertEqual(flickers, [1, 2, 3])
        self.assertEqual(turns, 0)

    def test_split_runs_breaks_on_non_dialogue(self):
        self.assertEqual(ts.split_runs([True, True, False, True, False, False, True, True, True]),
                         [[0, 1], [3], [6, 7, 8]])
        self.assertEqual(ts.split_runs([False, False]), [])

    def test_a_cut_away_does_not_make_a_flicker(self):
        # A, (not dialogue), B, (not dialogue), A: every dialogue run is one frame long.
        crops = [A, None, B, None, A]
        names = ["a", "x", "b", "y", "c"]
        runs = ts.split_runs([True, False, True, False, True])
        m = ts.measure_region(crops, names, runs)
        self.assertEqual((m["dialogue_frames"], m["flicker_frames"], m["page_turns"]), (3, 0, 0))

    def test_measure_region_reports_rate_and_names(self):
        crops = [A, A, B, A, A]
        names = ["w_00001.bmp", "w_00002.bmp", "w_00003.bmp", "w_00004.bmp", "w_00005.bmp"]
        m = ts.measure_region(crops, names, [[0, 1, 2, 3, 4]])
        self.assertEqual(m["flickers"], ["w_00003.bmp"])
        self.assertEqual(m["dialogue_frames"], 5)
        self.assertAlmostEqual(m["flicker_rate"], 0.2)

    def test_parse_region(self):
        self.assertEqual(ts.parse_region("1,2,30,40"), (1, 2, 30, 40))
        for bad in ("1,2,3", "a,b,c,d", "10,10,5,20", "0,0,700,10"):
            with self.assertRaises(Exception):
                ts.parse_region(bad)


def dialogue_frame(text_ink=(), name_ink=()):
    """A 640x480 frame with a white balloon and letterbox, plus ink in the default regions."""
    a = np.full((480, 640, 3), 40, dtype=np.uint8)
    a[0:34, :] = 0
    a[450:480, :] = 0
    a[50:120, 100:540] = 255
    for r, c in text_ink:
        a[r, c] = 0
    for r, c in name_ink:
        a[r, c] = 255
    return a


def ink_block(top, left):
    return [(top + r, left + c) for r in range(4) for c in range(4)]


@unittest.skipIf(Image is None, "Pillow not installed")
class RunTests(unittest.TestCase):
    def make_run(self, frames):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        run = Path(tmp.name)
        (run / "frames").mkdir()
        for i, a in enumerate(frames):
            Image.fromarray(a).save(run / "frames" / ("w_%05d.bmp" % i))
        return run

    def test_flicker_in_text_is_found_and_name_is_clean(self):
        good = dialogue_frame(ink_block(70, 120) + ink_block(70, 200), ink_block(420, 300))
        bad = dialogue_frame(ink_block(70, 120), ink_block(420, 300))     # a glyph dropped
        run = self.make_run([good, good, bad, good, good])
        report, code = ts.run(run, ts.DEFAULT_REGIONS, ts.PIXEL_THRESH, ts.MIN_PIXELS)
        self.assertEqual(code, 0)
        self.assertEqual(report["dialogue_frames"], 5)
        text = report["regions"]["balloon_text"]
        self.assertEqual(text["flickers"], ["w_00002.bmp"])
        self.assertEqual(text["page_turns"], 0)
        name = report["regions"]["name_label"]
        self.assertEqual((name["flicker_frames"], name["page_turns"]), (0, 0))

    def test_page_turn_is_not_flicker(self):
        one = dialogue_frame(ink_block(70, 120))
        two = dialogue_frame(ink_block(90, 300))
        run = self.make_run([one, one, two, two])
        report, code = ts.run(run, ts.DEFAULT_REGIONS, ts.PIXEL_THRESH, ts.MIN_PIXELS)
        self.assertEqual(code, 0)
        text = report["regions"]["balloon_text"]
        self.assertEqual((text["flicker_frames"], text["page_turns"]), (0, 1))

    def test_no_dialogue_frames_exits_1(self):
        plain = np.full((480, 640, 3), 40, dtype=np.uint8)
        run = self.make_run([plain, plain])
        out = Path(tempfile.mkdtemp()) / "out.json"
        self.assertEqual(ts.main([str(run), "--json", str(out)]), 1)
        self.assertEqual(json.loads(out.read_text())["dialogue_frames"], 0)

    def test_bad_input_exits_2(self):
        self.assertEqual(ts.main([str(ROOT / "no-such-run")]), 2)
        empty = Path(tempfile.mkdtemp())
        (empty / "frames").mkdir()
        self.assertEqual(ts.main([str(empty)]), 2)
        run = self.make_run([dialogue_frame()])
        self.assertEqual(ts.main([str(run), "--region", "5,5,2,2"]), 2)

    def test_custom_region_and_json(self):
        good = dialogue_frame(ink_block(100, 110))
        bad = dialogue_frame()
        run = self.make_run([good, bad, good])
        out = Path(tempfile.mkdtemp()) / "out.json"
        self.assertEqual(ts.main([str(run), "--region", "100,90,200,115", "--json", str(out)]), 0)
        data = json.loads(out.read_text())
        self.assertEqual(data["regions"]["region1"]["flickers"], ["w_00001.bmp"])


if __name__ == "__main__":
    unittest.main()
