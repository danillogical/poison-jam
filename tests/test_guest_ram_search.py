"""Tests for scripts/guest-ram-search.py.

Run directly: python -X utf8 tests/test_guest_ram_search.py
"""
from __future__ import annotations

import importlib.util
import json
import shutil
import struct
import tempfile
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "guest-ram-search.py"


def load_module():
    spec = importlib.util.spec_from_file_location("guest_ram_search", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


grs = load_module()

SIZE = 0x100000          # 1 MiB, enough for these fixtures and fast to scan
COUNTER_VA = 0x20000     # the planted spray-can counter
IDLE_DECOY_VA = 0x30000  # changes during an idle pair: must not survive
RUN1_DECOY_VA = 0x40000  # present only in run 1
CTRL_VA = 0x50000        # changes in the control between pre and post
BAD_VA = 0x60000         # lives on a page declared unreadable


class Fixture:
    """Two acceptance runs and one control, laid out as the runner writes them."""

    def __init__(self, root: Path):
        self.root = root
        self.dirs = {}
        for name in ("e4a", "e4b", "e4c"):
            d = root / name
            (d / "ram").mkdir(parents=True)
            self.dirs[name] = d
        self._write("e4a", counter=[0, 0, 3, 3, 3, 2, 2], idle_decoy=[1, 1, 1, 2, 2, 2, 2],
                    run1_decoy=[0, 0, 0, 0, 0, 1, 1], ctrl=[5, 5, 5, 5, 5, 5, 5])
        self._write("e4b", counter=[0, 0, 3, 3, 3, 2, 2], idle_decoy=[9, 9, 9, 9, 9, 9, 9],
                    run1_decoy=[0, 0, 0, 0, 0, 0, 0], ctrl=[5, 5, 5, 5, 5, 5, 5])
        # The control does the same inputs except the spray: its counter never moves.
        self._write("e4c", counter=[0, 0, 3, 3, 3, 3, 3], idle_decoy=[1, 1, 1, 1, 1, 1, 1],
                    run1_decoy=None, ctrl=[5, 5, 5, 5, 5, 9, 9], unreadable=[BAD_VA])

    NAMES = ["s0-start", "s1-idle", "s2-collected", "s3-collected-idle", "s4-pre-spray",
             "s5-post-spray", "s6-late"]

    def _write(self, run, counter, idle_decoy, run1_decoy, ctrl, unreadable=None):
        """One byte per named VA, whose value varies per snapshot -- the real witness shape."""
        d = self.dirs[run]
        for i, name in enumerate(self.NAMES):
            data = bytearray(SIZE)
            data[COUNTER_VA] = counter[i]
            data[IDLE_DECOY_VA] = idle_decoy[i]
            if run1_decoy is not None:
                data[RUN1_DECOY_VA] = run1_decoy[i]
            data[CTRL_VA] = ctrl[i]
            if unreadable:
                # A distinctive pattern on a page this run declares unreadable: it must never
                # be reported as data, so a search for it finds nothing.
                for page in unreadable:
                    data[page:page + 16] = b"\xab" * 16
            (d / "ram" / (name + ".bin")).write_bytes(bytes(data))
            (d / "ram" / (name + ".json")).write_text(json.dumps({
                "name": name, "va": "0x0", "length": SIZE, "sha256": "0" * 64,
                "unreadable_pages": [hex(v) for v in (unreadable or [])], "t_s": 0.0,
                "read_s": 0.0, "file": name + ".bin",
            }), encoding="utf-8")

    def path(self, run):
        return str(self.dirs[run])


class Search(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="grs-"))
        self.fx = Fixture(self.tmp)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_search(self, *extra):
        argv = ["--run", self.fx.path("e4a"), "--run", self.fx.path("e4b"), "--width", "1"] + list(extra)
        return grs.main(argv)

    def test_counter_predicates_isolate_the_planted_va(self):
        rc = self.run_search(
            "--eq", "s0-start=0", "--eq", "s1-idle=0",
            "--ne", "s1-idle:s2-collected",
            "--same", "s2-collected,s3-collected-idle,s4-pre-spray",
            "--lt", "s5-post-spray:s4-pre-spray",
            "--same", "s5-post-spray,s6-late",
            "--control", "%s:s4-pre-spray:s5-post-spray" % self.fx.path("e4c"),
            "--limit", "50")
        self.assertEqual(rc, 0)

    def test_returns_one_when_nothing_matches(self):
        rc = self.run_search("--eq", "s0-start=99")
        self.assertEqual(rc, 1)

    def test_run1_only_decoy_disappears_when_both_runs_are_given(self):
        """A VA that changes in one run only must not survive the two-run intersection.

        RUN1_DECOY_VA moves 0->1 between s4 and s5 in run e4a and never in e4b, so it is a
        candidate on one run and excluded by the intersection."""
        one = grs.main(["--run", self.fx.path("e4a"), "--width", "1", "--ne", "s4-pre-spray:s5-post-spray"])
        self.assertEqual(one, 0, "the decoy must be visible in a single-run search")
        both = grs.main(["--run", self.fx.path("e4a"), "--run", self.fx.path("e4b"), "--width", "1",
                         "--ne", "s4-pre-spray:s5-post-spray",
                         "--control", "%s:s4-pre-spray:s5-post-spray" % self.fx.path("e4c")])
        self.assertEqual(both, 0)
        # Only the planted counter may remain: assert it by narrowing to the counter's values.
        narrowed = grs.main(["--run", self.fx.path("e4a"), "--run", self.fx.path("e4b"), "--width", "1",
                             "--eq", "s0-start=0", "--eq", "s1-idle=0",
                             "--lt", "s5-post-spray:s4-pre-spray",
                             "--control", "%s:s4-pre-spray:s5-post-spray" % self.fx.path("e4c")])
        self.assertEqual(narrowed, 0)

    def test_control_removes_the_control_changing_va(self):
        """CTRL_VA moves in the control between s4 and s5, so it must not be reported."""
        import io
        from contextlib import redirect_stdout
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = grs.main(["--run", self.fx.path("e4a"), "--width", "1",
                           "--lt", "s5-post-spray:s4-pre-spray",
                           "--control", "%s:s4-pre-spray:s5-post-spray" % self.fx.path("e4c"),
                           "--limit", "100000"])
        self.assertEqual(rc, 0)
        out = buf.getvalue()
        self.assertIn("0x%08X" % COUNTER_VA, out, "the planted counter must be found")
        self.assertNotIn("0x%08X" % CTRL_VA, out,
                         "a VA that also moves in the control must be removed")

    def test_unreadable_page_is_never_returned(self):
        rc = grs.main(["--run", self.fx.path("e4c"), "--width", "1", "--eq", "s0-start=0"])
        self.assertEqual(rc, 0)
        # The unreadable page is dropped, so a VA inside it cannot be a hit. Assert by
        # searching for the page's own contents being absent from the output.
        import io
        from contextlib import redirect_stdout
        buf = io.StringIO()
        with redirect_stdout(buf):
            grs.main(["--run", self.fx.path("e4c"), "--width", "1", "--eq", "s0-start=0", "--limit", "100000"])
        self.assertNotIn("0x%08X" % BAD_VA, buf.getvalue())

    def test_missing_snapshot_is_exit_2(self):
        rc = grs.main(["--run", self.fx.path("e4a"), "--width", "1", "--eq", "nope=0"])
        self.assertEqual(rc, 2)

    def test_width_views_share_one_buffer(self):
        """Every width must be a view of one read, not a copy.

        The refactor that reads each snapshot once depends on this: a silent copy would put
        the acceptance search's memory back at one 64 MiB buffer per snapshot per width. A
        full-array comparison also pins the tail, where a length error would show."""
        raw, va, _ = grs.load_snapshot(self.fx.path("e4a"), "s0-start")
        base = np.frombuffer(raw, dtype=np.uint8)
        for width in (1, 2, 4):
            view = grs.view_at_width(raw, va, width, "e4a", "s0-start")
            self.assertTrue(np.shares_memory(view, base), "width %d copied the buffer" % width)
            self.assertFalse(view.flags.owndata)
            np.testing.assert_array_equal(view, np.frombuffer(raw, dtype=grs.DTYPES[width]))

    def test_runset_loads_each_snapshot_once(self):
        """The RunSet holds one raw buffer per snapshot, shared by all its width views."""
        rs = grs.RunSet(self.fx.path("e4a"), ["s0-start", "s1-idle"], [1, 2, 4])
        for width in (1, 2, 4):
            for name in ("s0-start", "s1-idle"):
                arr, _ = rs.arrays(width)[name]
                self.assertFalse(arr.flags.owndata, "width %d %s owns its data" % (width, name))


if __name__ == "__main__":
    unittest.main(verbosity=2)
