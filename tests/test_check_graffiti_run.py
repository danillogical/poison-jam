"""Tests for scripts/check-graffiti-run.py: the acceptance decision and its named failures.

Run directly: python -X utf8 tests/test_check_graffiti_run.py
"""
from __future__ import annotations

import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "check-graffiti-run.py"


def load_module():
    spec = importlib.util.spec_from_file_location("check_graffiti_run", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


cgr = load_module()

SNAPS = ("gameplay-start", "cans-collected", "pre-spray", "spray-during", "post-spray",
         "post-spray-late")


def spray_block(r_vk=True, hold=0.5, control=False):
    """The steps between the pre- and post-spray peeks."""
    if control:
        block = [{"step": "sleep", "n": 90, "options": {"seconds": hold}}]
    else:
        block = [{"step": "key", "n": 90, "options": {"vk": r_vk and 0x33 or 0x5A, "hold": hold}}]
    return block


def make_run(root: Path, name, *, cans, tag=None, control=False, spray_present=True,
             satisfied=True, script_ok=True, snaps=SNAPS, r_vk=0x33):
    d = root / name
    d.mkdir(parents=True)
    steps = [
        {"step": "peek", "n": 0, "options": {"name": "ctl-xbe-text", "va": "0x00011000",
                                             "size": 4, "expect": 0x852C518B}},
        {"step": "snap", "n": 1, "options": {"name": "gameplay-start"}},
        {"step": "peek", "n": 2, "options": {"name": "cans-collected", "expect": cans[0]}},
        {"step": "snap", "n": 3, "options": {"name": "cans-collected"}},
        {"step": "peek", "n": 4, "options": {"name": "cans-pre-spray", "expect": cans[1]}},
        {"step": "snap", "n": 5, "options": {"name": "pre-spray"}},
        *spray_block(r_vk=r_vk, control=control),
        {"step": "snap", "n": 6, "options": {"name": "spray-during"}},
        {"step": "snap", "n": 7, "options": {"name": "post-spray"}},
        {"step": "peek", "n": 8, "options": {"name": "cans-post-spray", "expect": cans[2]}},
        {"step": "peek", "n": 9, "options": {"name": "cans-late", "expect": cans[3]}},
        {"step": "snap", "n": 10, "options": {"name": "post-spray-late"}},
    ]
    if tag is not None:
        steps.insert(5, {"step": "peek", "n": 55, "options": {"name": "tag-pre-spray",
                                                             "expect": tag[0]}})
        steps.append({"step": "peek", "n": 56, "options": {"name": "tag-post-spray",
                                                          "expect": tag[1]}})
    for s in steps:
        if "satisfied" in s:
            s["satisfied"] = satisfied
    (d / "result.json").write_text(json.dumps({"label": name, "script_ok": script_ok,
                                               "steps": steps}), encoding="utf-8")
    peeks = [{"name": "ctl-xbe-text", "value": 0x852C518B, "matches": True, "unreadable": False},
             {"name": "cans-collected", "value": cans[0], "matches": True, "unreadable": False},
             {"name": "cans-pre-spray", "value": cans[1], "matches": True, "unreadable": False},
             {"name": "cans-post-spray", "value": cans[2], "matches": True, "unreadable": False},
             {"name": "cans-late", "value": cans[3], "matches": True, "unreadable": False}]
    if tag is not None:
        peeks.append({"name": "tag-pre-spray", "value": tag[0], "matches": True,
                      "unreadable": False})
        peeks.append({"name": "tag-post-spray", "value": tag[1], "matches": True,
                      "unreadable": False})
    (d / "peeks.jsonl").write_text("".join(json.dumps(p) + "\n" for p in peeks), encoding="utf-8")
    for n in snaps:
        (d / (n + ".png")).write_bytes(b"\x89PNG\r\n\x1a\n")
    return d


class Decision(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="cgr-"))

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def decide(self, accept, control):
        return cgr.check(cgr.load(str(accept)), cgr.load(str(control)))

    def test_correct_pair_passes(self):
        a = make_run(self.tmp, "a", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c", cans=(3, 3, 3, 3), control=True)
        fails, _ = self.decide(a, c)
        self.assertEqual(fails, [])

    def test_control_with_r_press_left_in_fails(self):
        a = make_run(self.tmp, "a2", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c2", cans=(3, 3, 3, 3), control=False)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("no R press inside the spray block" in f or "control" in f
                            for f in fails), fails)

    def test_acceptance_without_decrement_fails(self):
        a = make_run(self.tmp, "a3", cans=(3, 3, 3, 3))
        c = make_run(self.tmp, "c3", cans=(3, 3, 3, 3), control=True)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("did not fall below" in f for f in fails), fails)

    def test_control_that_decrements_fails(self):
        a = make_run(self.tmp, "a4", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c4", cans=(3, 3, 2, 2), control=True)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("control: cans changed without a spray" in f for f in fails), fails)

    def test_mismatched_step_outside_spray_block_fails(self):
        a = make_run(self.tmp, "a5", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c5", cans=(3, 3, 3, 3), control=True)
        r = json.loads((c / "result.json").read_text(encoding="utf-8"))
        r["steps"].insert(0, {"step": "sleep", "n": 999, "options": {"seconds": 1}})
        (c / "result.json").write_text(json.dumps(r), encoding="utf-8")
        fails, _ = self.decide(a, c)
        self.assertTrue(any("differ outside the spray block" in f for f in fails), fails)

    def test_unsatisfied_key_until_fails(self):
        a = make_run(self.tmp, "a6", cans=(3, 3, 2, 2))
        a2 = json.loads((a / "result.json").read_text(encoding="utf-8"))
        a2["steps"].insert(1, {"step": "key_until", "n": 998, "satisfied": False,
                               "options": {"vk": 0x33, "until": "dialogue"}})
        (a / "result.json").write_text(json.dumps(a2), encoding="utf-8")
        c = make_run(self.tmp, "c6", cans=(3, 3, 3, 3), control=True)
        c2 = json.loads((c / "result.json").read_text(encoding="utf-8"))
        c2["steps"].insert(1, {"step": "key_until", "n": 998, "satisfied": False,
                               "options": {"vk": 0x33, "until": "dialogue"}})
        (c / "result.json").write_text(json.dumps(c2), encoding="utf-8")
        fails, _ = self.decide(a, c)
        self.assertTrue(any("not satisfied" in f for f in fails), fails)

    def test_tag_flag_checked_when_present(self):
        a = make_run(self.tmp, "a7", cans=(3, 3, 2, 2), tag=(0, 1))
        c = make_run(self.tmp, "c7", cans=(3, 3, 3, 3), tag=(0, 0), control=True)
        fails, _ = self.decide(a, c)
        self.assertEqual(fails, [])
        a2 = make_run(self.tmp, "a8", cans=(3, 3, 2, 2), tag=(0, 0))
        c2 = make_run(self.tmp, "c8", cans=(3, 3, 3, 3), tag=(0, 1), control=True)
        fails2, _ = self.decide(a2, c2)
        self.assertTrue(any("tag state did not change" in f for f in fails2), fails2)
        self.assertTrue(any("control: tag state changed" in f for f in fails2), fails2)

    def test_missing_snapshot_fails(self):
        a = make_run(self.tmp, "a9", cans=(3, 3, 2, 2), snaps=SNAPS[:-1])
        c = make_run(self.tmp, "c9", cans=(3, 3, 3, 3), control=True)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("missing snapshot post-spray-late.png" in f for f in fails), fails)

    def test_unmatched_peek_fails(self):
        a = make_run(self.tmp, "a10", cans=(3, 3, 2, 2))
        p = (a / "peeks.jsonl").read_text(encoding="utf-8").replace('"matches": true',
                                                                    '"matches": false', 1)
        (a / "peeks.jsonl").write_text(p, encoding="utf-8")
        c = make_run(self.tmp, "c10", cans=(3, 3, 3, 3), control=True)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("did not match" in f for f in fails), fails)

    def test_unreadable_peek_fails(self):
        a = make_run(self.tmp, "a11", cans=(3, 3, 2, 2))
        p = (a / "peeks.jsonl").read_text(encoding="utf-8").replace('"unreadable": false',
                                                                    '"unreadable": true', 1)
        (a / "peeks.jsonl").write_text(p, encoding="utf-8")
        c = make_run(self.tmp, "c11", cans=(3, 3, 3, 3), control=True)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("unreadable page" in f for f in fails), fails)

    def test_missing_result_is_exit_2(self):
        rc = cgr.main([str(self.tmp / "nope"), str(self.tmp / "nope2")])
        self.assertEqual(rc, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
