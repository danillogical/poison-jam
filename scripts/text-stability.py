"""Measure dialogue-text flicker in a key-script run's dumped frames.

In-game dialogue text sometimes shows a few glyphs missing or wrong for one frame and is right
again on the next. This tool counts those transient frames per text region so a fix has a
before/after number, and keeps them apart from lasting changes (a page turn, or the typewriter
reveal adding characters), which are not defects.

Frames come from `<run-dir>/frames/w_NNNNN.bmp`, the framebuffer-window dump. NNNNN counts
window dumps, so it orders frames in time; it is not the presented-frame number. The spacing is
RECOMP_FB_WINDOW_DUMP_EVERY window ticks, so only a run dumped with `RECOMP_FB_WINDOW_DUMP_EVERY=1`
compares truly adjacent presented frames. At a larger spacing a one-frame glitch is mostly
never sampled and the count is a lower bound. Only dialogue frames (key-script-run.is_dialogue)
are compared, and a non-dialogue frame between two of them ends the run, so A B A never spans
a cut away from the balloon.

A flicker frame is B in A B A: it differs from the previous frame, the next frame differs from
it, and previous and next match. A A B B is one page turn. A B C is two lasting changes.
Two frames "differ" in a region when at least --min-pixels pixels move by more than --thresh in
some channel, which ignores blend and compression noise.

Usage: python -X utf8 scripts/text-stability.py <run-dir> [--region x0,y0,x1,y1 ...] [--json out.json]
Regions are in 640x480 reference coordinates; with none given, the balloon text and the speaker
name label are measured.
Exit:  0 measured, 1 no dialogue frames found, 2 bad input.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]

# (x0, y0, x1, y1) in 640x480 reference pixels, measured on Gum's talk (run
# 20261012-203811-675-keyscript-gum-h) and Corn's opening talk (20261012-020638-074-keyscript-p001-e1).
# The balloon text stops short of the blinking A-button prompt at the balloon's lower right, and
# the name label sits in the bottom letterbox bar.
DEFAULT_REGIONS = {
    "balloon_text": (105, 56, 520, 122),
    "name_label": (200, 408, 440, 448),
}
PIXEL_THRESH = 32
MIN_PIXELS = 8
FRAME_RE = re.compile(r"^w_(\d+)\.bmp$")
# The present dumps (RECOMP_FB_PRESENT_DUMP_EVERY): written when the visible pixels change, so
# consecutive files are consecutive distinct presents, which is what a flicker is made of.
PRESENT_RE = re.compile(r"^w_p(\d+)\.bmp$")


def load_key_script_run():
    """Load the hyphenated key-script-run script for its dialogue detector and frame reader."""
    spec = importlib.util.spec_from_file_location("key_script_run", ROOT / "scripts" / "key-script-run.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def regions_differ(a, b, thresh=PIXEL_THRESH, min_pixels=MIN_PIXELS):
    """True when at least `min_pixels` pixels of two equal-shaped crops moved by more than `thresh`."""
    if a.shape != b.shape:
        return True
    moved = np.abs(a.astype(np.int16) - b.astype(np.int16)).max(axis=-1) > thresh
    return int(moved.sum()) >= min_pixels


def classify_triplets(seq, thresh=PIXEL_THRESH, min_pixels=MIN_PIXELS):
    """Classify one unbroken run of region crops.

    Returns (flicker_indices, page_turns). Index i is a flicker when seq[i] differs from both
    neighbours and the neighbours match each other (A B A). A page turn is a change between
    neighbours where neither frame is a flicker: A A B B has one, A B C has two, and A B A
    has none because both of its changes belong to the flicker."""
    n = len(seq)
    diff = [regions_differ(seq[i], seq[i + 1], thresh, min_pixels) for i in range(n - 1)]
    flickers = [i for i in range(1, n - 1)
                if diff[i - 1] and diff[i]
                and not regions_differ(seq[i - 1], seq[i + 1], thresh, min_pixels)]
    skip = set(flickers)
    turns = sum(1 for i, d in enumerate(diff) if d and i not in skip and i + 1 not in skip)
    return flickers, turns


def split_runs(is_dialogue_flags):
    """Index lists of consecutive True entries; any False entry ends a run."""
    runs, cur = [], []
    for i, flag in enumerate(is_dialogue_flags):
        if flag:
            cur.append(i)
        elif cur:
            runs.append(cur)
            cur = []
    if cur:
        runs.append(cur)
    return runs


def measure_region(crops, names, runs, thresh=PIXEL_THRESH, min_pixels=MIN_PIXELS):
    """Measure one region. `crops` and `names` are per frame; `runs` index into them."""
    flicker_names, turns, dialogue = [], 0, 0
    for run in runs:
        dialogue += len(run)
        flickers, t = classify_triplets([crops[i] for i in run], thresh, min_pixels)
        flicker_names += [names[run[k]] for k in flickers]
        turns += t
    return {
        "dialogue_frames": dialogue,
        "flicker_frames": len(flicker_names),
        "flicker_rate": (len(flicker_names) / dialogue) if dialogue else 0.0,
        "page_turns": turns,
        "flickers": flicker_names,
    }


def crop(ksr, frame, region):
    """The region (x0, y0, x1, y1 in 640x480 reference pixels) of a frame, scaled to its size."""
    x0, y0, x1, y1 = region
    return ksr._band(frame, (y0, y1, x0, x1))


def parse_region(text):
    try:
        x0, y0, x1, y1 = (int(v) for v in text.split(","))
    except ValueError:
        raise argparse.ArgumentTypeError("region must be x0,y0,x1,y1 integers: %r" % text)
    if not (0 <= x0 < x1 <= 640 and 0 <= y0 < y1 <= 480):
        raise argparse.ArgumentTypeError("region must satisfy 0<=x0<x1<=640 and 0<=y0<y1<=480: %r" % text)
    return (x0, y0, x1, y1)


def ordered_frames(frames_dir, pattern=FRAME_RE):
    """Frame paths in dump order (the numeric part of the file name)."""
    found = []
    for p in frames_dir.iterdir():
        m = pattern.match(p.name)
        if m:
            found.append((int(m.group(1)), p))
    return [p for _, p in sorted(found)]


def run(run_dir, regions, thresh, min_pixels, pattern=FRAME_RE):
    """Measure a run directory. Returns (report, exit_code); raises ValueError on bad input."""
    frames_dir = run_dir / "frames"
    if not frames_dir.is_dir():
        raise ValueError("no frames/ directory in %s" % run_dir)
    paths = ordered_frames(frames_dir, pattern)
    if not paths:
        raise ValueError("no %s frames in %s" % (pattern.pattern, frames_dir))
    ksr = load_key_script_run()

    names, flags, crops = [], [], {label: [] for label in regions}
    unreadable = []
    for p in paths:
        a = ksr.load_frame(p)
        if a is None:
            # An unreadable dump is a hole in the sequence, so it ends any dialogue run.
            unreadable.append(p.name)
            names.append(p.name)
            flags.append(False)
            for label in regions:
                crops[label].append(None)
            continue
        names.append(p.name)
        flags.append(bool(ksr.is_dialogue(a)))
        for label, region in regions.items():
            crops[label].append(crop(ksr, a, region) if flags[-1] else None)
    if len(unreadable) == len(paths):
        raise ValueError("no readable frames in %s" % frames_dir)

    runs = split_runs(flags)
    report = {
        "run_dir": str(run_dir),
        "frames_total": len(paths),
        "dialogue_frames": sum(flags),
        "dialogue_runs": len(runs),
        "unreadable_frames": unreadable,
        "pixel_thresh": thresh,
        "min_pixels": min_pixels,
        "regions": {},
    }
    for label, region in regions.items():
        m = measure_region(crops[label], names, runs, thresh, min_pixels)
        m["region"] = list(region)
        report["regions"][label] = m
    return report, (0 if report["dialogue_frames"] else 1)


def render(report):
    lines = ["%s: %d frames, %d dialogue in %d runs"
             % (report["run_dir"], report["frames_total"], report["dialogue_frames"], report["dialogue_runs"])]
    if report["unreadable_frames"]:
        lines.append("  unreadable: %s" % ", ".join(report["unreadable_frames"]))
    for label, m in report["regions"].items():
        lines.append("%s %s: dialogue %d, flicker %d (%.1f%%), page turns %d"
                     % (label, tuple(m["region"]), m["dialogue_frames"], m["flicker_frames"],
                        100.0 * m["flicker_rate"], m["page_turns"]))
        if m["flickers"]:
            lines.append("  flicker frames: %s" % ", ".join(m["flickers"]))
    return "\n".join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("run_dir", type=Path)
    ap.add_argument("--region", action="append", type=parse_region, metavar="x0,y0,x1,y1",
                    help="region in 640x480 reference pixels; repeatable; replaces the defaults")
    ap.add_argument("--thresh", type=int, default=PIXEL_THRESH,
                    help="per-channel pixel change that counts as moved (default %(default)s)")
    ap.add_argument("--min-pixels", type=int, default=MIN_PIXELS,
                    help="moved pixels that make two frames differ (default %(default)s)")
    ap.add_argument("--json", type=Path, metavar="out.json", help="also write the report as JSON")
    ap.add_argument("--source", choices=("window", "present"), default="window",
                    help="window dumps w_NNNNN.bmp (default) or present dumps w_pNNNN.bmp")
    try:
        args = ap.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if args.region:
        regions = {"region%d" % (i + 1): r for i, r in enumerate(args.region)}
    else:
        regions = dict(DEFAULT_REGIONS)
    try:
        report, code = run(args.run_dir, regions, args.thresh, args.min_pixels,
                           PRESENT_RE if args.source == "present" else FRAME_RE)
    except (ValueError, OSError) as e:
        print("text-stability: %s" % e, file=sys.stderr)
        return 2
    print(render(report))
    if args.json:
        args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if code:
        print("text-stability: no dialogue frames found", file=sys.stderr)
    return code


if __name__ == "__main__":
    sys.exit(main())
