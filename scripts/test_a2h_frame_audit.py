#!/usr/bin/env python3
"""Tests for scripts/a2h-frame-audit.py.

These tests exist because the tool's FIRST version produced two wrong answers that both
looked plausible:

  1. it asserted the byte string 89442410 for `mov [esp+0x10], ebp` when the real bytes are
     896c2410 -- the verifier rejected it, which is the tool catching its author;
  2. `direct_calls_to` returned ZERO call sites for a target that demonstrably has thirteen,
     because it swept a 1.5 MB section linearly and desynchronised on the first data byte.

A silent wrong answer in either place would have changed a decision row, so both are now
pinned by tests. The counting test is the load-bearing one: the whole point of the tool is
that a naive "count the pushes immediately before the call" is WRONG at an intervening
zero-argument call, and it was wrong in a way that flipped a conclusion twice.

Run: python -X utf8 scripts/test_a2h_frame_audit.py
"""
from __future__ import annotations

import importlib
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import importlib.util

_spec = importlib.util.spec_from_file_location(
    "a2h_frame_audit", ROOT / "scripts" / "a2h-frame-audit.py")
audit = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(audit)


class TestByteVerification(unittest.TestCase):
    """The tool must reject a wrong byte string rather than return a plausible decode."""

    def test_wrong_bytes_are_rejected(self):
        with self.assertRaises(audit.AuditError):
            audit.verify(0x0017D20F, "89442410")   # the author's original mis-transcription

    def test_correct_bytes_accepted(self):
        text = audit.verify(0x0017D20F, "896c2410")
        self.assertIn("ebp", text)

    def test_misaligned_start_rejected(self):
        # 0x0014980F is mid-instruction; the verifier must refuse rather than guess.
        with self.assertRaises(audit.AuditError):
            audit.verify(0x0014980F, "45dc8945")

    def test_all_load_bearing_instructions_verify(self):
        for va, b, _label in (audit.CALLEE_PROLOGUE + audit.HELPER_FRAME
                              + [audit.READ_SITE, audit.PRODUCER]):
            with self.subTest(va="0x%08X" % va):
                self.assertTrue(audit.verify(va, b))

    def test_cmd_verify_passes(self):
        self.assertEqual(audit.cmd_verify(None), 0)


class TestCallSiteDiscovery(unittest.TestCase):
    """A linear sweep silently desynchronises; the byte-scan must not."""

    def test_finds_all_direct_calls(self):
        sites = audit.direct_calls_to(audit.CALLEE)
        self.assertEqual(len(sites), 13, "expected 13 direct call sites")

    def test_return_addresses_are_call_plus_five(self):
        for cva, ret in audit.direct_calls_to(audit.CALLEE):
            with self.subTest(call="0x%08X" % cva):
                self.assertEqual(ret, cva + 5)

    def test_known_bound_caller_present(self):
        rets = {ret for _cva, ret in audit.direct_calls_to(audit.CALLEE)}
        self.assertIn(0x0017C926, rets)

    def test_absent_target_yields_nothing(self):
        # A target nothing calls must return an empty list, not a bogus hit.
        self.assertEqual(audit.direct_calls_to(0x00000001), [])

    def test_matches_generated_source_return_addresses(self):
        """The XBE byte-scan and the recompiler's own call list must agree exactly."""
        gen = set()
        for f in sorted((ROOT / "src" / "recomp" / "gen").glob("*.c")):
            t = f.read_text(encoding="utf-8", errors="replace")
            for m in __import__("re").finditer(
                    r"PUSH32\(esp, 0x([0-9A-Fa-f]{8})u\); RECOMP_ABI_CALL\(0x001497DCu", t):
                gen.add(int(m.group(1), 16))
        rec = ROOT / "src" / "recomp" / "recovered" / "recovered.c"
        if rec.exists():
            t = rec.read_text(encoding="utf-8", errors="replace")
            for m in __import__("re").finditer(
                    r"PUSH32\(esp, 0x([0-9A-Fa-f]{8})u\); RECOMP_ABI_CALL\(0x001497DCu", t):
                gen.add(int(m.group(1), 16))
        tool = {ret for _cva, ret in audit.direct_calls_to(audit.CALLEE)}
        self.assertEqual(tool, gen)


class TestArgumentCounting(unittest.TestCase):
    """The load-bearing behaviour: an intervening zero-consumption call must NOT reset the count."""

    def setUp(self):
        self.deltas = audit.load_abi_deltas()
        self.exempt = audit.load_abi_exempt()

    def test_bound_caller_passes_three_arguments(self):
        n, detail, status = audit.arguments_at(0x0017C921, self.deltas, self.exempt)
        self.assertEqual(status, "ok")
        self.assertEqual(n, 3)

    def test_intervening_getter_consumes_nothing(self):
        """0x14a838 is `mov eax,[0x27dcd4]; ret` -- a plain ret consumes no arguments."""
        n, detail, status = audit.arguments_at(0x0017C921, self.deltas, self.exempt)
        calls = [d for d in detail if d[0] == "call"]
        self.assertTrue(calls, "expected an intervening call to be recorded")
        for _kind, _va, op in calls:
            self.assertIn("0x0014A838", op)
            self.assertIn("consumed=0", op)

    def test_naive_push_count_would_be_wrong(self):
        """Guard the specific error: counting pushes after the intervening call gives 1, not 3.

        This test pins WHY the tool exists. If someone replaces the stack walk with a naive
        count, this fails.
        """
        n, detail, status = audit.arguments_at(0x0017C921, self.deltas, self.exempt)
        pushes = [d for d in detail if d[0] == "push"]
        naive = 1  # only the push immediately before the call
        self.assertEqual(len(pushes), 3)
        self.assertNotEqual(naive, n)

    def test_every_call_site_resolves_or_reports_unknown(self):
        for cva, _ret in audit.direct_calls_to(audit.CALLEE):
            n, _detail, status = audit.arguments_at(cva, self.deltas, self.exempt)
            with self.subTest(call="0x%08X" % cva):
                if status == "ok":
                    self.assertIn(n, (1, 2, 3, 4))
                else:
                    self.assertTrue(status.startswith("unknown:"))

    def test_unknown_delta_fails_closed(self):
        """With an empty delta table the intervening call is unknown -> UNKNOWN, never a guess."""
        n, _detail, status = audit.arguments_at(0x0017C921, {}, self.exempt)
        self.assertIsNone(n)
        self.assertTrue(status.startswith("unknown:"))

    def test_abi_delta_table_is_nonempty(self):
        self.assertGreater(len(self.deltas), 4000)

    def test_callee_delta_is_sixteen(self):
        """ret 12 + the 4-byte return address = 16."""
        self.assertEqual(self.deltas.get(audit.CALLEE, [None])[0], 16)


class TestFrameArithmetic(unittest.TestCase):
    """The frame must come from the verified prologue, not from an assumption."""

    def test_helper_replaces_ebp_by_four_below_entry(self):
        f = audit.frame_for_entry(0x00F7FEA0)
        self.assertEqual(f["ebp"], 0x00F7FE9C)
        self.assertEqual(f["arg0"], 0x00F7FEA4)
        self.assertEqual(f["arg1"], 0x00F7FEA8)
        self.assertEqual(f["arg2"], 0x00F7FEAC)
        self.assertEqual(f["return_address"], 0x00F7FEA0)

    def test_slot_offsets_are_the_standard_argument_positions(self):
        """[ebp+8], [ebp+0xC], [ebp+0x10] alias arg0/arg1/arg2 -- that is what __SEH_prolog does."""
        f = audit.frame_for_entry(0x1000)
        self.assertEqual(f["arg0"] - f["ebp"], 8)
        self.assertEqual(f["arg1"] - f["ebp"], 0x0C)
        self.assertEqual(f["arg2"] - f["ebp"], 0x10)

    def test_two_logged_esps_agree_on_entry(self):
        """Independent cross-check: two esp readings inside one activation reduce to one E."""
        from_277 = 0x00F7FD00 + 0x1A0
        from_184 = 0x00F7FCF0 + 0x1B0
        self.assertEqual(from_277, from_184)
        self.assertEqual(from_277, 0x00F7FEA0)

    def test_cmd_frame_runs(self):
        class A:
            esp = "0x00F7FCF0"
            pushes = 0x1B0
        self.assertEqual(audit.cmd_frame(A), 0)


class TestNoCompetingWriter(unittest.TestCase):
    """The read at 0x00149800 must have no writer inside the callee, in EITHER spelling.

    AGENTS.md warns that one guest address can be spelled two ways -- a hex moffs load versus a
    signed-decimal ModRM disp32 -- for the same guest VA. A grep for one spelling under-counts,
    which already cost this project a site list covering 10 of 28 sites. So the no-writer
    conclusion is checked in both spellings, with a positive control proving the pattern works.
    """

    def _body(self):
        gen = ROOT / "src" / "recomp" / "gen"
        for f in sorted(gen.glob("*.c")):
            lines = f.read_text(encoding="utf-8", errors="replace").split("\n")
            for i, L in enumerate(lines):
                if L.startswith("void sub_001497DC(void)"):
                    depth = 0
                    for j in range(i, min(i + 4000, len(lines))):
                        depth += lines[j].count("{") - lines[j].count("}")
                        if j > i and depth == 0:
                            return lines[i:j + 1]
        return []

    def test_body_is_found(self):
        self.assertGreater(len(self._body()), 100, "callee body not located")

    def test_no_write_in_hex_spelling(self):
        writes = [L for L in self._body()
                  if re.search(r"MEM32\(ebp \+ 0x10\)\s*=[^=]", L)]
        self.assertEqual(writes, [], "unexpected writer: %s" % writes)

    def test_no_write_in_decimal_spelling(self):
        writes = [L for L in self._body()
                  if re.search(r"MEM32\(ebp \+ 16\)\s*=[^=]", L)]
        self.assertEqual(writes, [], "unexpected decimal-spelled writer: %s" % writes)

    def test_positive_control_reads_are_found(self):
        """The same pattern DOES find the known reads, so an empty write list means no writes
        rather than a broken regex."""
        reads = [L for L in self._body() if "ebp + 0x10" in L]
        self.assertGreaterEqual(len(reads), 5)


if __name__ == "__main__":
    unittest.main(verbosity=2)
