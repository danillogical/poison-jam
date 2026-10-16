"""Tests for scripts/bench-summary.py: grouping, per-metric medians and spreads, and the CLI exit codes.

Run directly: python -X utf8 tests/test_bench_summary.py
"""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "bench-summary.py"


def load_module():
    """Load the hyphenated script as a module, the way the other tests load hyphenated scripts."""
    spec = importlib.util.spec_from_file_location("bench_summary", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


bs = load_module()


def rec(backend="vulkan", name="garage", pps=60.0, p50=16.7, p90=18.0, p99=25.0, **extra):
    r = {"backend": backend, "name": name, "presents_per_s": pps,
         "frame_ms_p50": p50, "frame_ms_p90": p90, "frame_ms_p99": p99}
    r.update(extra)
    return r


def write_run(root, label, records):
    """Make a run dir whose bench.json maps each record's name to the record."""
    d = Path(root) / label
    d.mkdir()
    (d / "bench.json").write_text(json.dumps({r["name"]: r for r in records}), encoding="utf-8")
    return str(d)


def run_cli(argv):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = bs.main(argv)
    return code, out.getvalue(), err.getvalue()


class Median(unittest.TestCase):
    def test_odd(self):
        self.assertEqual(bs.median([3, 1, 2]), 2)

    def test_even_averages_middle_two(self):
        self.assertEqual(bs.median([4, 1, 3, 2]), 2.5)
        self.assertEqual(bs.median([10, 20]), 15)

    def test_even_is_not_the_lower_middle(self):
        self.assertNotEqual(bs.median([1, 2, 3, 100]), 2)
        self.assertEqual(bs.median([1, 2, 3, 100]), 2.5)

    def test_single(self):
        self.assertEqual(bs.median([7]), 7)


class Grouping(unittest.TestCase):
    def test_mixed_backends_group_separately(self):
        a, b, c = rec("vulkan"), rec("d3d11"), rec("vulkan")
        groups = bs.group_by_backend([a, b, c])
        self.assertEqual(set(groups), {("vulkan", "garage"), ("d3d11", "garage")})
        self.assertEqual(groups[("vulkan", "garage")], [a, c])
        self.assertEqual(groups[("d3d11", "garage")], [b])

    def test_names_group_separately(self):
        groups = bs.group_by_backend([rec(name="garage"), rec(name="street")])
        self.assertEqual(set(groups), {("vulkan", "garage"), ("vulkan", "street")})


class Summarize(unittest.TestCase):
    def test_three_runs_median_and_spread(self):
        recs = [rec(pps=58, p50=17.0, p90=20.0, p99=40.0),
                rec(pps=60, p50=16.0, p90=18.0, p99=30.0),
                rec(pps=62, p50=18.0, p90=19.0, p99=50.0)]
        s = bs.summarize_group(recs)
        self.assertEqual(s["presents_per_s"], {"median": 60, "min": 58, "max": 62, "n": 3})
        self.assertEqual(s["frame_ms_p50"], {"median": 17.0, "min": 16.0, "max": 18.0, "n": 3})
        self.assertEqual(s["frame_ms_p90"], {"median": 19.0, "min": 18.0, "max": 20.0, "n": 3})
        self.assertEqual(s["frame_ms_p99"], {"median": 40.0, "min": 30.0, "max": 50.0, "n": 3})

    def test_even_count_median(self):
        s = bs.summarize_group([rec(p50=10.0), rec(p50=20.0), rec(p50=30.0), rec(p50=100.0)])
        self.assertEqual(s["frame_ms_p50"]["median"], 25.0)

    def test_metrics_cover_the_documented_set(self):
        self.assertEqual(set(bs.METRICS),
                         {"presents_per_s", "frame_ms_p50", "frame_ms_p90", "frame_ms_p99"})


class LoadRun(unittest.TestCase):
    def test_absent_bench_json_is_none(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(bs.load_run(tmp))

    def test_records_and_name_filter(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = write_run(tmp, "r1", [rec(name="garage"), rec(name="street")])
            self.assertEqual(sorted(r["name"] for r in bs.load_run(d)), ["garage", "street"])
            only = bs.load_run(d, "street")
            self.assertEqual([r["name"] for r in only], ["street"])


class Cli(unittest.TestCase):
    def test_three_runs_per_backend_exit_0(self):
        with tempfile.TemporaryDirectory() as tmp:
            dirs = [write_run(tmp, "v%d" % i, [rec("vulkan", pps=58 + 2 * i)]) for i in range(3)]
            dirs += [write_run(tmp, "d%d" % i, [rec("d3d11", pps=30 + i)]) for i in range(3)]
            code, out, err = run_cli(["--json"] + dirs)
            self.assertEqual(code, 0, err)
            data = json.loads(out)
            by = {b["backend"]: b for b in data["backends"]}
            self.assertEqual(set(by), {"vulkan", "d3d11"})
            self.assertEqual(by["vulkan"]["presents_per_s"]["median"], 60)
            self.assertEqual(by["vulkan"]["presents_per_s"]["min"], 58)
            self.assertEqual(by["vulkan"]["presents_per_s"]["max"], 62)
            self.assertEqual(by["d3d11"]["presents_per_s"]["median"], 31)
            self.assertEqual(data["missing"], [])

    def test_text_output_exit_0(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out, _ = run_cli([write_run(tmp, "a", [rec()])])
            self.assertEqual(code, 0)
            self.assertIn("vulkan", out)

    def test_missing_bench_json_exits_1(self):
        with tempfile.TemporaryDirectory() as tmp:
            good = write_run(tmp, "good", [rec()])
            bad = Path(tmp) / "nobench"
            bad.mkdir()
            code, _, _ = run_cli([good, str(bad)])
            self.assertEqual(code, 1)
            code, _, _ = run_cli([str(bad)])
            self.assertEqual(code, 1)

    def test_nonexistent_dir_exits_2(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, _, _ = run_cli([str(Path(tmp) / "does-not-exist")])
            self.assertEqual(code, 2)

    def test_malformed_json_exits_2(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp) / "broken"
            d.mkdir()
            (d / "bench.json").write_text("{not json", encoding="utf-8")
            code, _, _ = run_cli([str(d)])
            self.assertEqual(code, 2)

    def test_no_args_exits_2(self):
        code, _, _ = run_cli([])
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
