"""Tests for scripts/a2h-oom-slice.py.

Required by `A2h-oom-causal-slice-r1`, which specifies fixtures for: a good invocation;
duplicate/conflicting calls; missing/truncated events; wrong thread; wrong size;
width/signedness; and a mid-instruction disassembly start that is REJECTED rather than
decoded as plausible garbage.

The truncation case is the important one: this project has repeatedly accepted a partial
parse as if it were complete (a byte-width scan that missed 182 writers; a disassembly from a
mid-instruction boundary that capstone rendered as plausible `.byte` garbage). The parser must
therefore FAIL on a truncated invocation rather than silently undercount.

Run:  python -X utf8 -m unittest scripts.test_a2h_oom_slice
"""
from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location("a2h_oom_slice",
                                               ROOT / "scripts" / "a2h-oom-slice.py")
a2h = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(a2h)

FAIL_SIZE = a2h.FAIL_SIZE


def ordinal_line(ret: str, esp: str = "0x00F7FCF0") -> str:
    return f"[KERNEL] #1: ordinal 184 (slot 10) esp={esp} ret={ret}"


def alloc_line(size: int, base: str = "00000000", typ: str = "0x801000") -> str:
    return (f"  [KERNEL] NtAllocateVirtualMemory: base=0x{base} "
            f"size={size} type={typ}")


def good_log(entries=None) -> str:
    """A minimal well-formed log: N invocations then an OOM and an ICALL."""
    if entries is None:
        entries = [("0x00149E50", 2097200, "0x00F7FBD4"),
                   ("0x00149E50", FAIL_SIZE, "0x00F7FCF0")]
    lines = ["[BOOT] start"]
    for ret, size, esp in entries:
        lines.append(ordinal_line(ret, esp))
        lines.append("  [KERNEL] → returned 0x00000000")
        lines.append(alloc_line(size))
    lines.append(f"xbox_HeapAlloc: out of memory (requested {FAIL_SIZE}, "
                 f"used 12715008/50855936)")
    lines.append("  [KERNEL] → returned 0xC0000017")
    lines.append("[ICALL] invalid target 0x00000000 tid=65356 esp=00F7FD00 "
                 "return=0014982E")
    return "\n".join(lines) + "\n"


class Fixture:
    def __init__(self, text: str):
        self.text = text

    def __enter__(self) -> Path:
        self._dir = tempfile.TemporaryDirectory()
        self.path = Path(self._dir.name) / "jsrf_run.log"
        self.path.write_text(self.text, encoding="utf-8")
        return self.path

    def __exit__(self, *exc):
        self._dir.cleanup()
        return False


class ParseTests(unittest.TestCase):
    def test_good_invocation_binds_site_size_and_esp(self):
        with Fixture(good_log()) as p:
            r = a2h.parse_log(p)
            self.assertEqual(r["invocation_count"], 2)
            self.assertEqual(r["failing_indices"], [1])
            self.assertEqual(len(r["oom_events"]), 1)
            self.assertEqual(r["oom_events"][0]["requested"], FAIL_SIZE)
            self.assertEqual(len(r["icall_events"]), 1)
            self.assertEqual(r["icall_events"][0]["return"], "0014982E")

    def test_pairs_the_ret_with_the_following_alloc(self):
        """The ret and the size must come from the SAME invocation, in order."""
        entries = [("0xAAAAAAAA", 111, "0x1"), ("0xBBBBBBBB", FAIL_SIZE, "0x2")]
        with Fixture(good_log(entries)) as p:
            r = a2h.parse_log(p)
            self.assertEqual(r["invocations"][0]["ret"], "0xAAAAAAAA")
            self.assertEqual(r["invocations"][0]["size"], 111)
            self.assertEqual(r["invocations"][1]["ret"], "0xBBBBBBBB")
            self.assertEqual(r["invocations"][1]["size"], FAIL_SIZE)

    def test_duplicate_calls_at_one_site_are_both_reported(self):
        entries = [("0x00149E50", 2097200, "0x00F7FBD4"),
                   ("0x00149E50", FAIL_SIZE, "0x00F7FCF0")]
        with Fixture(good_log(entries)) as p:
            s = a2h.summarise(a2h.parse_log(p))
            self.assertEqual(s["same_site_count"], 2)
            self.assertEqual(s["failing_call_site"], "0x00149E50")
            self.assertEqual(len(s["other_invocations_at_this_site"]), 1)
            self.assertEqual(s["other_invocations_at_this_site"][0]["size"], 2097200)

    def test_different_frames_detected_from_esp(self):
        entries = [("0x00149E50", 2097200, "0x00F7FBD4"),
                   ("0x00149E50", FAIL_SIZE, "0x00F7FCF0")]
        with Fixture(good_log(entries)) as p:
            s = a2h.summarise(a2h.parse_log(p))
            self.assertTrue(s["different_frames"])
            self.assertEqual(len(s["distinct_esps_at_site"]), 2)

    def test_same_frame_is_not_reported_as_different(self):
        entries = [("0x00149E50", 2097200, "0x00F7FCF0"),
                   ("0x00149E50", FAIL_SIZE, "0x00F7FCF0")]
        with Fixture(good_log(entries)) as p:
            s = a2h.summarise(a2h.parse_log(p))
            self.assertFalse(s["different_frames"])

    def test_wrong_thread_is_preserved_verbatim(self):
        """The ICALL thread must be reported as logged, not normalised away."""
        text = good_log().replace("tid=65356", "tid=2948")
        with Fixture(text) as p:
            r = a2h.parse_log(p)
            self.assertEqual(r["icall_events"][0]["tid"], 2948)

    def test_wrong_size_does_not_match_the_failing_set(self):
        with Fixture(good_log([("0x00149E50", 12345, "0x1")])) as p:
            r = a2h.parse_log(p)
            self.assertEqual(r["failing_indices"], [])

    def test_large_unsigned_size_is_not_sign_flipped(self):
        """598869040 is a positive uint32; it must not be read as negative."""
        with Fixture(good_log([("0x00149E50", FAIL_SIZE, "0x1")])) as p:
            r = a2h.parse_log(p)
            self.assertEqual(r["invocations"][0]["size"], 598869040)
            self.assertGreater(r["invocations"][0]["size"], 0)


class FailureTests(unittest.TestCase):
    def test_truncated_invocation_is_fatal_not_skipped(self):
        """An ordinal-184 line with no following alloc line must FAIL.

        Silently skipping it would undercount the population -- the exact class of error
        this project keeps making.
        """
        text = ordinal_line("0x00149E50") + "\n  [KERNEL] → returned 0x00000000\n"
        with Fixture(text) as p:
            with self.assertRaises(a2h.LogError):
                a2h.parse_log(p)

    def test_missing_log_is_fatal(self):
        with Fixture(good_log()) as p:
            with self.assertRaises(a2h.LogError):
                a2h.parse_log(p.parent / "nope.log")

    def test_empty_log_is_fatal(self):
        with Fixture("   \n") as p:
            with self.assertRaises(a2h.LogError):
                a2h.parse_log(p)

    def test_log_with_no_invocations_is_fatal(self):
        with Fixture("[BOOT] start\nnothing here\n") as p:
            with self.assertRaises(a2h.LogError):
                a2h.parse_log(p)

    def test_undecodable_bytes_are_fatal(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "jsrf_run.log"
            p.write_bytes(b"\xff\xfe\x00bad")
            with self.assertRaises(a2h.LogError):
                a2h.parse_log(p)


class DeterminismTests(unittest.TestCase):
    def test_repeat_parse_is_byte_identical(self):
        with Fixture(good_log()) as p:
            a = json.dumps(a2h.parse_log(p), indent=2)
            b = json.dumps(a2h.parse_log(p), indent=2)
            self.assertEqual(a, b)

    def test_log_hash_is_recorded_and_stable(self):
        with Fixture(good_log()) as p:
            h1 = a2h.parse_log(p)["log_sha256"]
            h2 = a2h.parse_log(p)["log_sha256"]
            self.assertEqual(h1, h2)
            self.assertEqual(len(h1), 64)

    def test_changed_content_changes_the_hash(self):
        with Fixture(good_log()) as p1:
            h1 = a2h.parse_log(p1)["log_sha256"]
        with Fixture(good_log() + "extra\n") as p2:
            h2 = a2h.parse_log(p2)["log_sha256"]
        self.assertNotEqual(h1, h2)


class ComparisonTests(unittest.TestCase):
    def test_identical_chains_compare_same(self):
        with Fixture(good_log()) as p1, Fixture(good_log()) as p2:
            c = a2h.compare(a2h.parse_log(p1), a2h.parse_log(p2))
            self.assertTrue(c["oom_tuple_same"])
            self.assertTrue(c["icall_tuple_same"])
            self.assertTrue(c["failing_alloc_same"])
            self.assertTrue(c["invocation_count"]["same"])

    def test_byte_offset_difference_does_not_report_a_semantic_difference(self):
        """Two logs of the same chain differ in byte offsets by construction.

        The comparison must be SEMANTIC: an earlier version compared whole dicts including
        the byte `offset`, and reported a spurious difference between two runs that were
        semantically identical.
        """
        a = good_log()
        # same semantic content, different leading padding -> different offsets
        b = "[PAD] padding line\n" + good_log()
        with Fixture(a) as p1, Fixture(b) as p2:
            c = a2h.compare(a2h.parse_log(p1), a2h.parse_log(p2))
            self.assertTrue(c["failing_alloc_same"],
                            "semantically identical chains must compare the same")
            self.assertIn("compared_fields", c["failing_alloc_semantic"])
            self.assertNotIn("offset", c["failing_alloc_semantic"]["compared_fields"])

    def test_different_sizes_are_detected(self):
        a = good_log()
        b = good_log([("0x00149E50", 2097200, "0x00F7FBD4"),
                      ("0x00149E50", 999, "0x00F7FCF0")])
        with Fixture(a) as p1, Fixture(b) as p2:
            c = a2h.compare(a2h.parse_log(p1), a2h.parse_log(p2))
            self.assertFalse(c["failing_alloc_same"])

    def test_duplicate_conflicting_calls_are_visible(self):
        """Two DIFFERENT sizes from the same site must both appear, not be collapsed."""
        entries = [("0x00149E50", 111, "0x1"), ("0x00149E50", 222, "0x2")]
        with Fixture(good_log(entries)) as p:
            s = a2h.summarise(a2h.parse_log(p))
            self.assertIn("0x00149E50", s["sites_reached_more_than_once"])
            sizes = [x["size"] for x in s["invocations_by_site"]["0x00149E50"]]
            self.assertEqual(sizes, [111, 222])

    def test_site_grouping_is_present_even_without_a_failing_size(self):
        """A caller must be able to inspect per-site invocations without a failure first."""
        entries = [("0x00149E50", 111, "0x1"), ("0x00149E50", 222, "0x2")]
        with Fixture(good_log(entries)) as p:
            s = a2h.summarise(a2h.parse_log(p))
            self.assertEqual(s["failing_indices"], [])
            self.assertIn("invocations_by_site", s)
            self.assertIn("0x00149E50", s["invocations_by_site"])

    def test_single_visit_site_is_not_listed_as_repeated(self):
        entries = [("0x00149E50", FAIL_SIZE, "0x1"), ("0x001484AC", 528384, "0x2")]
        with Fixture(good_log(entries)) as p:
            s = a2h.summarise(a2h.parse_log(p))
            self.assertEqual(s["sites_reached_more_than_once"], [])


if __name__ == "__main__":
    unittest.main()
