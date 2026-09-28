#!/usr/bin/env python3
"""Tests for scripts/a2h-null-slot-triage.py.

These exist because the tool's first versions produced FOUR wrong answers that all looked
plausible, and every one of them was the same error class this project keeps hitting -- reading
an absent or unattributed record as a measurement:

  1. `budget_declared` was searched for in the LOG TEXT, but the budget is an ENVIRONMENT VARIABLE.
     The Advisor corrected this: an env var is not log text, so its absence from the log says
     nothing. The tool now reads run METADATA.
  2. Even after that, the metadata lookup looked for a key CONTAINING `LOG_BUDGET` and returned
     None for a budget that WAS present, because the metadata stores env vars as
     `{"name": "...", "value": "..."}` records. Absent-record-read-as-negative, again.
  3. The baseline's ICALL line uses a DIFFERENT form from R1's. A parser knowing one form reported
     ZERO ICALL lines for the other run, which would silently mischaracterise a terminal event.
  4. `thread_check` returned `single_threaded: True` for an EMPTY list -- so a run with no parsed
     ICALL at all would be described as single-threaded. Absence of a witness read as a negative.

Each is now pinned by a test, plus positive controls proving the patterns actually match.

Run: python -X utf8 scripts/test_a2h_null_slot_triage.py
"""
from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

_spec = importlib.util.spec_from_file_location(
    "a2h_null_triage", ROOT / "scripts" / "a2h-null-slot-triage.py")
triage = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(triage)

R1 = ROOT / "logs" / "runs" / "20260927-160330-655-a4b2-gp-trap-trace"
BASELINE = ROOT / "logs" / "runs" / "20260927-130036-879-a4b2-nr-baseline"


def write_run(tmp: Path, log_lines, metadata=None):
    (tmp / "jsrf_run.log").write_text("\n".join(log_lines), encoding="utf-8")
    if metadata is not None:
        (tmp / "metadata.json").write_text(json.dumps(metadata), encoding="utf-8")
    return tmp


class TestIcallForms(unittest.TestCase):
    """A parser that knows ONE ICALL form silently reports zero for the other run."""

    def test_invalid_target_form_parses(self):
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), [
                "[ICALL] invalid target 0x00000000 tid=65356 esp=00F7FD00 return=0014982E"])
            r = triage.run(p)
        self.assertEqual(r["budget"]["icall_lines"], 1)
        self.assertEqual(r["threads"]["forms"], {"invalid_target": 1})

    def test_failed_to_resolve_form_parses(self):
        """The baseline's form, INCLUDING the space after `thread calls:`."""
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), [
                "[ICALL] Failed to resolve VA 0xFFFFFFFF (thread calls: 953, tid=57592, ms=425717953)"])
            r = triage.run(p)
        self.assertEqual(r["budget"]["icall_lines"], 1,
                         "the second ICALL form must parse, not silently report zero")
        self.assertEqual(r["threads"]["forms"], {"failed_to_resolve": 1})
        self.assertEqual(r["threads"]["tid_counts"], {57592: 1})

    def test_both_forms_in_one_log(self):
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), [
                "[ICALL] invalid target 0x00000000 tid=1 esp=00F7FD00 return=0014982E",
                "[ICALL] Failed to resolve VA 0xFFFFFFFF (thread calls: 953, tid=2, ms=1)"])
            r = triage.run(p)
        self.assertEqual(r["budget"]["icall_lines"], 2)
        self.assertEqual(r["threads"]["forms"], {"invalid_target": 1, "failed_to_resolve": 1})


class TestFailClosedOnAbsence(unittest.TestCase):
    """Absence of a witness must never read as a negative measurement."""

    def test_zero_icalls_is_unknown_not_single_threaded(self):
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), ["[KERNEL] #1: ordinal 1 (slot 1) esp=0x1 ret=0x2"])
            r = triage.run(p)
        self.assertIsNone(r["threads"]["single_threaded"],
                          "zero ICALL lines must NOT report single-threaded")
        self.assertTrue(r["threads"]["status"].startswith("UNKNOWN"))

    def test_real_runs_report_ok_status(self):
        for run in (R1, BASELINE):
            if not run.exists():
                self.skipTest("archive %s unavailable" % run.name)
            r = triage.run(run)
            with self.subTest(run=run.name):
                self.assertEqual(r["threads"]["status"], "ok")
                self.assertEqual(r["budget"]["icall_lines"], 1)


class TestBudgetGrounding(unittest.TestCase):
    """The budget is an env var: it must come from metadata, never from log text."""

    def test_metadata_name_value_record_is_read(self):
        """The real shape: a record whose `name` IS the env var."""
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), ["[KERNEL] #1: ordinal 1 (slot 1) esp=0x1 ret=0x2"],
                          metadata={"env": [{"name": "RECOMP_KERNEL_LOG_BUDGET",
                                             "value": "100000"}]})
            r = triage.run(p)
        self.assertEqual(r["budget"]["budget_declared"], 100000)
        self.assertEqual(r["budget"]["budget_source"], "metadata.json")

    def test_metadata_plain_key_is_read(self):
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), ["[KERNEL] #1: ordinal 1 (slot 1) esp=0x1 ret=0x2"],
                          metadata={"RECOMP_KERNEL_LOG_BUDGET": 500})
            r = triage.run(p)
        self.assertEqual(r["budget"]["budget_declared"], 500)

    def test_log_text_is_not_used_as_the_budget_source(self):
        """A budget-looking string in the LOG must not become the budget."""
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), [
                "[KERNEL] #1: ordinal 1 (slot 1) esp=0x1 ret=0x2",
                "RECOMP_KERNEL_LOG_BUDGET=999"])
            r = triage.run(p)
        self.assertIsNone(r["budget"]["budget_declared"],
                          "env vars are not log text; the log must not be the budget source")

    def test_real_runs_ground_the_budget_from_metadata(self):
        for run in (R1, BASELINE):
            if not run.exists():
                self.skipTest("archive %s unavailable" % run.name)
            r = triage.run(run)
            with self.subTest(run=run.name):
                self.assertEqual(r["budget"]["budget_source"], "metadata.json")
                self.assertEqual(r["budget"]["budget_declared"], 100000)
                self.assertGreater(r["budget"]["budget_headroom"], 0)


class TestPerThreadIndex(unittest.TestCase):
    """The counter is RECOMP_TLS, so a global contiguity test is the WRONG test."""

    def test_index_flagged_per_thread(self):
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), ["[KERNEL] #1: ordinal 1 (slot 1) esp=0x1 ret=0x2"])
            r = triage.run(p)
        self.assertTrue(r["budget"]["index_is_per_thread"])
        self.assertIn("RECOMP_TLS", r["budget"]["index_basis"])

    def test_interleaved_indices_are_reported_incomplete(self):
        """Indices that restart must not be called complete."""
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), [
                "[KERNEL] #1: ordinal 1 (slot 1) esp=0x1 ret=0x2",
                "[KERNEL] #2: ordinal 1 (slot 1) esp=0x1 ret=0x2",
                "[KERNEL] #1: ordinal 2 (slot 2) esp=0x1 ret=0x2",
            ])
            r = triage.run(p)
        self.assertFalse(r["budget"]["complete"])
        self.assertGreater(r["budget"]["call_index_reused"], 0)
        self.assertGreater(r["budget"]["call_index_decreases"], 0)

    def test_real_runs_show_the_per_thread_signature(self):
        if not R1.exists():
            self.skipTest("R1 archive unavailable")
        r = triage.run(R1)
        self.assertGreater(r["budget"]["call_index_reused"], 0,
                           "reused indices are the per-thread signature")
        self.assertGreater(r["budget"]["call_index_decreases"], 0)
        self.assertFalse(r["budget"]["complete"])


class TestArtifactIdentity(unittest.TestCase):
    """A count is only citable as an (artifact, query, value) triple."""

    def test_log_hash_is_recorded(self):
        if not R1.exists():
            self.skipTest("R1 archive unavailable")
        r = triage.run(R1)
        self.assertEqual(len(r["log_sha256"]), 64)
        self.assertGreater(r["log_bytes"], 0)

    def test_hash_changes_with_content(self):
        hashes = []
        for body in ("#1: ordinal 1", "#1: ordinal 2"):
            with tempfile.TemporaryDirectory() as td:
                p = write_run(Path(td), ["[KERNEL] %s (slot 1) esp=0x1 ret=0x2" % body])
                hashes.append(triage.run(p)["log_sha256"])
        self.assertNotEqual(hashes[0], hashes[1])


class TestOrderingAndStaticClaim(unittest.TestCase):
    def test_oom_before_icall_is_reported_as_observation(self):
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), [
                "[KERNEL] #1: ordinal 184 (slot 10) esp=0x1 ret=0x2",
                "xbox_HeapAlloc: out of memory (requested 598869040, used 1/2)",
                "[ICALL] invalid target 0x00000000 tid=1 esp=2 return=3"])
            r = triage.run(p)
        self.assertTrue(r["ordering"]["oom_before_icall"])
        self.assertEqual(r["ordering"]["oom_lines"], [2])

    def test_guest_check_is_labelled_static_not_log_derived(self):
        """The 'guest checks the result' fact comes from BYTES, not from this log."""
        with tempfile.TemporaryDirectory() as td:
            p = write_run(Path(td), ["[KERNEL] #1: ordinal 1 (slot 1) esp=0x1 ret=0x2"])
            r = triage.run(p)
        gc = r["ordering"]["guest_checks_result"]
        self.assertTrue(gc["value"])
        self.assertTrue(gc["evidence_class"].startswith("static"))


class TestSelfTest(unittest.TestCase):
    def test_embedded_self_test_passes(self):
        self.assertEqual(triage.self_test(), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
