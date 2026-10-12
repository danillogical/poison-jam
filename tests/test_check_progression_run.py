"""Tests for scripts/check-progression-run.py: the progression decision and its named failures.

Run directly: python -X utf8 tests/test_check_progression_run.py
"""
from __future__ import annotations

import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "check-progression-run.py"


def load_module():
    spec = importlib.util.spec_from_file_location("check_progression_run", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


cpr = load_module()

XBE_WORD = 0x852C518B
A_VK = 0x5A
WITNESS_VA = 0x00123450
SNAPS = ("pre-action", "post-action", "witness-talk")


def make_run(root: Path, name, *, control=False, expect_second=5, observed_second=None,
             with_expect=True, action_left_in=False):
    """A lesson run: Gum's first talk, the action block, then the witness talk."""
    observed_second = expect_second if observed_second is None else observed_second
    d = root / name
    d.mkdir(parents=True)

    def witness(n, wname, want):
        return {"step": "peek", "n": n,
                "options": {"name": wname, "va": WITNESS_VA, "size": 4,
                            "expect": want if with_expect else None}}

    if control and not action_left_in:
        block = [{"step": "sleep", "n": 20, "options": {"seconds": 0.5}},
                 {"step": "sleep", "n": 21, "options": {"seconds": 2}}]
    else:
        block = [{"step": "key", "n": 20, "options": {"vk": A_VK, "hold": 0.5, "snap": None}},
                 {"step": "sleep", "n": 21, "options": {"seconds": 2}}]
    steps = [
        {"step": "peek", "n": 0, "value": XBE_WORD,
         "options": {"name": "ctl-xbe-text", "va": 0x00011000, "size": 4, "expect": XBE_WORD}},
        witness(1, "gum-first-talk", 2),
        {"step": "snap", "n": 2, "options": {"name": "pre-action"}},
        witness(3, "action-pre", 0),
        *block,
        witness(30, "action-post", 0),
        {"step": "snap", "n": 31, "options": {"name": "post-action"}},
        {"step": "snap", "n": 32, "options": {"name": "witness-talk"}},
        witness(33, "gum-second-talk", expect_second),
    ]
    (d / "result.json").write_text(json.dumps({"label": name, "script_ok": True,
                                               "steps": steps}), encoding="utf-8")
    values = {"gum-first-talk": 2, "action-pre": 0, "action-post": 0,
              "gum-second-talk": observed_second}
    expects = {"gum-first-talk": 2, "action-pre": 0, "action-post": 0,
               "gum-second-talk": expect_second}
    peeks = [{"name": "ctl-xbe-text", "va": "0x00011000", "size": 4, "expect": XBE_WORD,
              "value": XBE_WORD, "matches": True, "unreadable": False}]
    for wname, value in values.items():
        want = expects[wname] if with_expect else None
        peeks.append({"name": wname, "va": "0x%08X" % WITNESS_VA, "size": 4, "expect": want,
                      "value": value, "unreadable": False,
                      "matches": None if want is None else value == want})
    (d / "peeks.jsonl").write_text("".join(json.dumps(p) + "\n" for p in peeks), encoding="utf-8")
    (d / "metadata.json").write_text(json.dumps({
        "exe_sha256": "e" * 64, "xbe_sha256": "a" * 64,
        "save_root": {"verified": True, "disposable": True}}), encoding="utf-8")
    for n in SNAPS:
        (d / (n + ".png")).write_bytes(b"\x89PNG\r\n\x1a\n")
    return d


class Decision(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="cpr-"))

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def decide(self, a, c, witness=("gum-second-talk",)):
        return cpr.check(cpr.kp.load(a), cpr.kp.load(c), A_VK, witness)

    def test_correct_pair_passes(self):
        a = make_run(self.tmp, "a", expect_second=5)
        c = make_run(self.tmp, "c", control=True, expect_second=4)
        fails, summary = self.decide(a, c)
        self.assertEqual(fails, [])
        self.assertEqual(summary["witness"]["acceptance"]["gum-second-talk"]["value"], 5)

    def test_control_showing_progression_fails(self):
        a = make_run(self.tmp, "a2", expect_second=5)
        c = make_run(self.tmp, "c2", control=True, expect_second=4, observed_second=5)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("did not match" in f for f in fails), fails)

    def test_identical_expectations_cannot_discriminate(self):
        a = make_run(self.tmp, "a3", expect_second=5)
        c = make_run(self.tmp, "c3", control=True, expect_second=5)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("cannot discriminate" in f for f in fails), fails)

    def test_witness_without_expect_fails(self):
        a = make_run(self.tmp, "a4", expect_second=5, with_expect=False)
        c = make_run(self.tmp, "c4", control=True, expect_second=4)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("has no expect" in f for f in fails), fails)

    def test_control_with_action_left_in_fails(self):
        a = make_run(self.tmp, "a5", expect_second=5)
        c = make_run(self.tmp, "c5", control=True, expect_second=4, action_left_in=True)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("no matching control sleep" in f for f in fails), fails)

    def test_broken_mapping_control_fails(self):
        a = make_run(self.tmp, "a6", expect_second=5)
        c = make_run(self.tmp, "c6", control=True, expect_second=4)
        lines = [json.loads(x) for x in (a / "peeks.jsonl").read_text(encoding="utf-8").splitlines()]
        for r in lines:
            if r["name"] == "ctl-xbe-text":
                r.update(value=0, matches=None)
        (a / "peeks.jsonl").write_text("".join(json.dumps(r) + "\n" for r in lines),
                                       encoding="utf-8")
        fails, _ = self.decide(a, c)
        self.assertTrue(any("is not the XBE control" in f for f in fails), fails)


if __name__ == "__main__":
    unittest.main(verbosity=2)
