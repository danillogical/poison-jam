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
XBE_WORD = 0x852C518B


def spray_block(r_vk=True, hold=0.5, control=False):
    """The steps between the pre- and post-spray peeks."""
    if control:
        block = [{"step": "sleep", "n": 90, "options": {"seconds": hold}}]
    else:
        block = [{"step": "key", "n": 90,
                  "options": {"vk": r_vk and 0x33 or 0x5A, "hold": hold, "snap": None}}]
    return block


def make_run(root: Path, name, *, cans, tag=None, control=False, spray_present=True,
             satisfied=True, script_ok=True, snaps=SNAPS, r_vk=0x33, hold=0.5):
    """A run laid out as key-script-run.py writes it: VAs as ints in result.json, as strings
    in peeks.jsonl, and a peek step carrying the value it read."""
    d = root / name
    d.mkdir(parents=True)
    steps = [
        {"step": "peek", "n": 0, "value": XBE_WORD,
         "options": {"name": "ctl-xbe-text", "va": 0x00011000, "size": 4, "expect": XBE_WORD}},
        {"step": "snap", "n": 1, "options": {"name": "gameplay-start"}},
        {"step": "peek", "n": 2, "options": {"name": "cans-collected", "expect": cans[0]}},
        {"step": "snap", "n": 3, "options": {"name": "cans-collected"}},
        {"step": "peek", "n": 4, "options": {"name": "cans-pre-spray", "expect": cans[1]}},
        {"step": "snap", "n": 5, "options": {"name": "pre-spray"}},
        *spray_block(r_vk=r_vk, hold=hold, control=control),
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
    peeks = [{"name": "ctl-xbe-text", "va": "0x00011000", "size": 4, "expect": XBE_WORD,
              "value": XBE_WORD, "matches": True, "unreadable": False},
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
    (d / "metadata.json").write_text(json.dumps({
        "exe_sha256": "e" * 64, "xbe_sha256": "a" * 64,
        "save_root": {"verified": True, "disposable": True}}), encoding="utf-8")
    for n in snaps:
        (d / (n + ".png")).write_bytes(b"\x89PNG\r\n\x1a\n")
    return d


def edit_json(path, fn):
    data = json.loads(path.read_text(encoding="utf-8"))
    fn(data)
    path.write_text(json.dumps(data), encoding="utf-8")


def edit_peeks(d, name, **changes):
    lines = [json.loads(x) for x in (d / "peeks.jsonl").read_text(encoding="utf-8").splitlines()
             if x.strip()]
    for r in lines:
        if r["name"] == name:
            r.update(changes)
    (d / "peeks.jsonl").write_text("".join(json.dumps(r) + "\n" for r in lines), encoding="utf-8")


def insert_after_block_action(d, step):
    """Insert `step` right after the spray-block action (the step numbered 90)."""
    def fn(r):
        i = next(i for i, s in enumerate(r["steps"]) if s.get("n") == 90)
        r["steps"].insert(i + 1, step)
    edit_json(d / "result.json", fn)


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
        edit_json(c / "result.json", lambda r: r["steps"].insert(
            1, {"step": "sleep", "n": 999, "options": {"seconds": 1}}))
        fails, _ = self.decide(a, c)
        self.assertTrue(any("differ outside the spray block" in f for f in fails), fails)

    def test_unsatisfied_key_until_fails(self):
        a = make_run(self.tmp, "a6", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c6", cans=(3, 3, 3, 3), control=True)
        for d in (a, c):
            edit_json(d / "result.json", lambda r: r["steps"].insert(
                1, {"step": "key_until", "n": 998, "satisfied": False,
                    "options": {"vk": 0x33, "until": "dialogue"}}))
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
        edit_peeks(a, "cans-collected", matches=False)
        c = make_run(self.tmp, "c10", cans=(3, 3, 3, 3), control=True)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("did not match" in f for f in fails), fails)

    def test_unreadable_peek_fails(self):
        a = make_run(self.tmp, "a11", cans=(3, 3, 2, 2))
        edit_peeks(a, "cans-collected", unreadable=True)
        c = make_run(self.tmp, "c11", cans=(3, 3, 3, 3), control=True)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("unreadable page" in f for f in fails), fails)

    def test_missing_build_identity_is_rejected(self):
        """Two runs that both omit the identity fields must not compare as equal.

        `None == None` is True, so removing `exe_sha256` and `xbe_sha256` from both metadata
        files used to pass the pair as comparable -- a control from an unknown build."""
        a = make_run(self.tmp, "a14", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c14", cans=(3, 3, 3, 3), control=True)
        for d in (a, c):
            m = json.loads((d / "metadata.json").read_text(encoding="utf-8"))
            m.pop("exe_sha256", None)
            m.pop("xbe_sha256", None)
            (d / "metadata.json").write_text(json.dumps(m), encoding="utf-8")
        fails, _ = self.decide(a, c)
        self.assertTrue(any("exe_sha256 is missing or malformed" in f for f in fails), fails)
        self.assertTrue(any("xbe_sha256 is missing or malformed" in f for f in fails), fails)

    def test_malformed_build_identity_is_rejected(self):
        """A short or non-hex value is not an identity."""
        a = make_run(self.tmp, "a15", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c15", cans=(3, 3, 3, 3), control=True)
        for d in (a, c):
            m = json.loads((d / "metadata.json").read_text(encoding="utf-8"))
            m["exe_sha256"] = "e" * 40          # not 64 hex chars
            m["xbe_sha256"] = "not-a-hash"
            (d / "metadata.json").write_text(json.dumps(m), encoding="utf-8")
        fails, _ = self.decide(a, c)
        self.assertTrue(any("exe_sha256 is missing or malformed" in f for f in fails), fails)
        self.assertTrue(any("xbe_sha256 is missing or malformed" in f for f in fails), fails)

    def test_pair_from_different_builds_fails(self):
        a = make_run(self.tmp, "a16", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c16", cans=(3, 3, 3, 3), control=True)
        m = json.loads((c / "metadata.json").read_text(encoding="utf-8"))
        m["exe_sha256"] = "f" * 64
        (c / "metadata.json").write_text(json.dumps(m), encoding="utf-8")
        fails, _ = self.decide(a, c)
        self.assertTrue(any("executables differ" in f for f in fails), fails)

    def test_missing_result_is_exit_2(self):
        rc = cgr.main([str(self.tmp / "nope"), str(self.tmp / "nope2")])
        self.assertEqual(rc, 2)

    # F1: the mapping control is validated by value, not by the record's presence.
    def test_xbe_control_value_zero_fails(self):
        a = make_run(self.tmp, "a12", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c12", cans=(3, 3, 3, 3), control=True)
        edit_peeks(a, "ctl-xbe-text", value=0, matches=None)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("is not the XBE control 0x852C518B" in f for f in fails), fails)

    def test_xbe_control_wrong_va_fails(self):
        a = make_run(self.tmp, "a13", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c13", cans=(3, 3, 3, 3), control=True)
        edit_peeks(a, "ctl-xbe-text", va="0x00012000")
        edit_json(a / "result.json", lambda r: r["steps"][0]["options"].update(va=0x00012000))
        fails, _ = self.decide(a, c)
        self.assertTrue(any("not 0x00011000 size 4" in f for f in fails), fails)

    # F2: every non-action step must match on its options, inside and outside the block.
    def test_block_sleep_durations_differ_fails(self):
        a = make_run(self.tmp, "a14", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c14", cans=(3, 3, 3, 3), control=True)
        insert_after_block_action(a, {"step": "sleep", "n": 91, "options": {"seconds": 1}})
        insert_after_block_action(c, {"step": "sleep", "n": 91, "options": {"seconds": 30}})
        fails, _ = self.decide(a, c)
        self.assertTrue(any("spray block step 91 differs" in f for f in fails), fails)

    def test_block_non_action_key_differs_fails(self):
        a = make_run(self.tmp, "a15", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c15", cans=(3, 3, 3, 3), control=True)
        insert_after_block_action(a, {"step": "key", "n": 91,
                                      "options": {"vk": 0x5A, "hold": 1.0, "snap": None}})
        insert_after_block_action(c, {"step": "key", "n": 91,
                                      "options": {"vk": 0x58, "hold": 1.0, "snap": None}})
        fails, _ = self.decide(a, c)
        self.assertTrue(any("spray block step" in f and "differs" in f for f in fails), fails)

    def test_non_witness_peek_after_block_differs_fails(self):
        a = make_run(self.tmp, "a16", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c16", cans=(3, 3, 3, 3), control=True)
        for d, want in ((a, 1), (c, 2)):
            edit_json(d / "result.json", lambda r, want=want: r["steps"].append(
                {"step": "peek", "n": 57,
                 "options": {"name": "hud-late", "va": 0x2000, "size": 4, "expect": want}}))
        fails, _ = self.decide(a, c)
        self.assertTrue(any("differ outside the spray block" in f for f in fails), fails)

    def test_action_duration_mismatch_fails(self):
        a = make_run(self.tmp, "a17", cans=(3, 3, 2, 2), hold=0.5)
        c = make_run(self.tmp, "c17", cans=(3, 3, 3, 3), control=True, hold=0.7)
        fails, _ = self.decide(a, c)
        self.assertTrue(any("no matching control sleep" in f for f in fails), fails)

    def test_pair_from_different_builds_fails(self):
        a = make_run(self.tmp, "a18", cans=(3, 3, 2, 2))
        c = make_run(self.tmp, "c18", cans=(3, 3, 3, 3), control=True)
        edit_json(c / "metadata.json", lambda m: m.update(exe_sha256="f" * 64))
        fails, _ = self.decide(a, c)
        self.assertTrue(any("executables differ" in f for f in fails), fails)


if __name__ == "__main__":
    unittest.main(verbosity=2)
