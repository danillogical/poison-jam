"""Tests for `scripts/gen-nv2a-method-inventory.py`'s fail-closed guards.

The generator WRITES TWO TRACKED FILES: this repo's
`docs/jsrf-nv2a-method-inventory.md` and the toolkit's
`src/nv2a/nv2a_method_table.c`. The table is an ADMISSION set -- the NV2A
submission walk rejects the whole stream on a method it lacks (ledger L39) --
so writing a wrong table silently re-breaks the title.

It parses argv by hand, and that combination already caused one incident:
`--help` fell through both hand-rolled filters, so the run-name list took its
default of ONE old run and both files were rewritten with ~86 methods dropped
-- a destructive no-op that printed `wrote ...` as though it had succeeded. A
second destructive path was measured later: `--budget=0` stopped the walk on
its first word and wrote a THREE-method table over the real toolkit file.

So the load-bearing tests here are the ones that encode those failures:

  * `test_help_writes_nothing` / `test_help_works_without_the_decoder`;
  * `test_unknown_option_fails_closed`;
  * `test_nonpositive_budget_fails_closed` -- the measured `--budget=0` case;
  * `test_removal_is_refused_by_default` and
    `test_allow_removals_permits_a_removal` -- the guard that makes the whole
    class fail closed rather than depending on the caller getting argv right;
  * `test_successful_generation_is_isolated` -- a REAL end-to-end run with both
    outputs redirected, proving the isolation flags actually work (the earlier
    version of this suite only proved the guard paths, so it passed even when
    `--table-out` was ignored).

Every test asserts on FILE STATE, not on the presence of a `wrote` line: the
incident printed `wrote ...` while destroying the table, so output text is not
evidence about writes.

Run:  python -X utf8 -m unittest tests.test_gen_nv2a_method_inventory_guard
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "gen-nv2a-method-inventory.py"
TOOLKIT_TABLE = Path(r"C:\Users\logic\Repos\xboxrecomp\src\nv2a\nv2a_method_table.c")
DOC = ROOT / "docs" / "jsrf-nv2a-method-inventory.md"

# The three runs the committed table was generated from (TR section 22).
GENERATING_RUNS = [
    "20260922-110235-244-spanfix-1185b0",
    "20260930-230206-594-f4-frames-after-horizon-fix",
    "20261006-203929-286-title005-ceiling",
]
HAVE_ARCHIVES = all((ROOT / "logs" / "runs" / r / "jsrf_run.log").is_file()
                    for r in GENERATING_RUNS)


def _sha(path: Path) -> str | None:
    if not path.is_file():
        return None
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _methods_in(text: str) -> int:
    """How many method entries a generated table text declares."""
    return len(re.findall(r"0x[0-9A-F]{4}u,", text))


def _script_supports_output_overrides() -> bool:
    """Whether the script under test can redirect BOTH of its writes.

    This preflight is not decoration. The script writes two TRACKED files at
    hardcoded paths, and a version without `--table-out`/`--doc-out` ignores the
    flags and writes the real ones. Running this suite against such a version
    would therefore DESTROY the repository artifacts -- which is exactly what
    happened once while building these tests, when a stash was used to produce a
    RED baseline. So every invocation is gated on the script proving it can be
    redirected; a reverted script makes the suite skip rather than wreck the tree.
    """
    src = SCRIPT.read_text(encoding="utf-8", errors="replace")
    return "table-out" in src and "doc-out" in src


HAVE_OVERRIDES = _script_supports_output_overrides()


@unittest.skipUnless(HAVE_OVERRIDES,
                     "the script under test cannot redirect both of its writes; "
                     "running these tests would overwrite the real artifacts")
class GuardTests(unittest.TestCase):
    """Argument and guard behaviour. These never reach the decoder."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="nv2a-guard-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.table = self.tmp / "table.c"
        self.doc = self.tmp / "doc.md"
        # A destination that EXISTS and holds a larger set, so the removal guard
        # is armed. Seeded from the real committed table when available.
        if TOOLKIT_TABLE.is_file():
            shutil.copy2(TOOLKIT_TABLE, self.table)

    def run_gen(self, *argv: str):
        """Run the REAL script with BOTH outputs redirected into the temp dir.

        Redirecting both is what makes these tests safe: a guard that only
        redirected one output would still write the other into the repository.
        """
        return subprocess.run(
            [sys.executable, "-X", "utf8", str(SCRIPT),
             "--table-out=%s" % self.table, "--doc-out=%s" % self.doc, *argv],
            capture_output=True, text=True, timeout=300)

    def assert_nothing_written(self, proc):
        """The temp outputs must be byte-identical to how they started.

        Asserting on file state rather than on the absence of a `wrote` line is
        deliberate: the recorded incident printed `wrote ...` while it was
        destroying the table.
        """
        self.assertNotEqual(proc.returncode, 0,
                            "this invocation must fail closed")
        if TOOLKIT_TABLE.is_file():
            self.assertEqual(_sha(self.table), _sha(TOOLKIT_TABLE),
                             "the table destination was modified by a refused run")
        self.assertFalse(self.doc.exists(),
                         "the document was written by a refused run")

    def test_help_writes_nothing(self):
        """The recorded incident: `--help` must not regenerate anything."""
        proc = self.run_gen("--help")
        self.assertEqual(proc.returncode, 0)
        self.assertIn("usage:", proc.stdout)
        self.assertFalse(self.doc.exists())
        if TOOLKIT_TABLE.is_file():
            self.assertEqual(_sha(self.table), _sha(TOOLKIT_TABLE))

    def test_short_help_writes_nothing(self):
        proc = self.run_gen("-h")
        self.assertEqual(proc.returncode, 0)
        self.assertIn("usage:", proc.stdout)
        self.assertFalse(self.doc.exists())

    def test_help_works_without_the_decoder(self):
        """A usage request must not die on an unrelated import error.

        The decoder import used to sit ABOVE the guards, so `--help` raised
        ModuleNotFoundError instead of printing usage. That is what made running
        the tool bare look like the only way to ask it anything.
        """
        proc = self.run_gen("--help")
        self.assertNotIn("ModuleNotFoundError", proc.stderr)
        self.assertNotIn("Traceback", proc.stderr)
        self.assertIn("usage:", proc.stdout)

    def test_unknown_option_fails_closed(self):
        proc = self.run_gen("--puts=0x80001000")
        self.assert_nothing_written(proc)
        self.assertIn("unrecognised option", proc.stderr)

    def test_case_typo_fails_closed(self):
        """`--PUT=` is not `--put=`, and must not become a silent default."""
        proc = self.run_gen("--PUT=0x80001000")
        self.assert_nothing_written(proc)

    def test_known_option_without_value_fails_closed(self):
        proc = self.run_gen("--table-out")
        self.assert_nothing_written(proc)
        self.assertIn("missing a value", proc.stderr)

    def test_empty_table_out_fails_closed(self):
        """`--table-out=` must not silently fall back to the real toolkit path.

        An empty value used to lose to the env/default via `or`, so a caller who
        meant to redirect the write would overwrite the real table instead.
        """
        proc = subprocess.run(
            [sys.executable, "-X", "utf8", str(SCRIPT), "--table-out="],
            capture_output=True, text=True, timeout=300)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("needs a path", proc.stderr)

    def test_nonpositive_budget_fails_closed(self):
        """The measured `--budget=0` destruction: a 3-method table over the real file."""
        for bad in ("0", "-1", "-5"):
            with self.subTest(budget=bad):
                proc = self.run_gen(GENERATING_RUNS[0], "--budget=%s" % bad)
                self.assert_nothing_written(proc)
                self.assertIn("must be positive", proc.stderr)

    def test_non_numeric_values_fail_closed(self):
        for arg in ("--budget=abc", "--get=nothex", "--put=zzz"):
            with self.subTest(arg=arg):
                proc = self.run_gen(GENERATING_RUNS[0], arg)
                self.assert_nothing_written(proc)

    def test_removal_is_refused_by_default(self):
        """A table that drops methods must not be written without being asked.

        This is the guard that makes the whole class fail closed: it does not
        depend on the caller spelling argv correctly. Generating from ONE old run
        is a strict subset of the committed three-run table.
        """
        if not TOOLKIT_TABLE.is_file():
            self.skipTest("no toolkit table to compare against")
        proc = self.run_gen(GENERATING_RUNS[0])
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("REMOVES methods", proc.stderr)
        self.assertEqual(_sha(self.table), _sha(TOOLKIT_TABLE),
                         "a refused removal still modified the table")

    def test_allow_removals_permits_a_removal(self):
        """The escape hatch must be invocable as a bare flag.

        It is parsed into `flags`, not `opts`; reading only `opts` made the
        documented hatch impossible to use.
        """
        if not TOOLKIT_TABLE.is_file():
            self.skipTest("no toolkit table to compare against")
        before = _sha(self.table)
        proc = self.run_gen(GENERATING_RUNS[0], "--allow-removals")
        self.assertEqual(proc.returncode, 0,
                         "the escape hatch must work: %s" % proc.stderr[-400:])
        self.assertNotEqual(_sha(self.table), before,
                            "--allow-removals must actually write")
        self.assertLess(_methods_in(self.table.read_text(encoding="utf-8")),
                        _methods_in(TOOLKIT_TABLE.read_text(encoding="utf-8")))

    def test_usage_names_the_no_argument_default_trap(self):
        """The usage text must keep naming the trap, not hide it."""
        proc = self.run_gen("--help")
        self.assertIn("DROP methods", proc.stdout)
        self.assertIn("name every run whose ring matters", proc.stdout)
        self.assertIn("--allow-removals", proc.stdout)


class WitnessManifestTests(unittest.TestCase):
    """The runtime-witness manifest must not be able to admit an unjustified method.

    This is a provenance gate, not a formatting gate. The manifest is the ONLY
    input whose entries are taken on trust -- every other method in the table is
    derived from a decode -- so a manifest that can name one method while quoting
    a witness for another would let the table admit a method no witness justified.
    That was a real defect: an entry declaring `method 0x1734` but quoting the
    `0x0BB0` record, with `log_sha256 = "not-a-sha"` and no `log_line`, was
    accepted, and `0x1734` was admitted. Each vector below is now refused, and
    both artifacts must be left untouched when it is.
    """

    VALID = {
        "class": "0x97",
        "method": "0x0BB0",
        "run": "20261006-213505-255-title005-admit3",
        "log_sha256": "840e307802874c25f8bbe34ea1f97a3d918d3ccf98e2ad6e35e921e5a7743538",
        "log_line": 81481,
        "witness": "[PFIFO] admit-unknown class=97 method=0BB0 param=00000000 at=0002FF9C",
    }

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="nv2a-witness-"))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.table = self.tmp / "table.c"
        self.doc = self.tmp / "doc.md"
        self.manifest = self.tmp / "witness.json"
        if TOOLKIT_TABLE.is_file():
            shutil.copy2(TOOLKIT_TABLE, self.table)

    def run_with(self, entry, extra=()):
        """Generate from one run with a one-entry manifest, both outputs redirected.

        `--allow-removals` is passed by the acceptance CONTROL only: generating
        from one old run is a strict subset of the committed three-run table, so
        the removal guard would otherwise fire first and the control would be
        testing the removal guard instead of the witness gate.
        """
        self.manifest.write_text(json.dumps({"entries": [entry]}), encoding="utf-8")
        return subprocess.run(
            [sys.executable, "-X", "utf8", str(SCRIPT),
             "--table-out=%s" % self.table, "--doc-out=%s" % self.doc,
             "--witness=%s" % self.manifest, *extra, GENERATING_RUNS[0]],
            capture_output=True, text=True, timeout=300)

    def assert_refused(self, proc, needle):
        self.assertNotEqual(proc.returncode, 0, "this manifest must be refused")
        self.assertIn(needle, proc.stderr)
        if TOOLKIT_TABLE.is_file():
            self.assertEqual(_sha(self.table), _sha(TOOLKIT_TABLE),
                             "a refused manifest still modified the table")

    def test_valid_entry_is_accepted(self):
        """The control: a genuine entry must still work, or the gate is vacuous.

        The witness is accepted only if the generation SUCCEEDS, so the assertion
        is that the manifest is not what stopped it. `--allow-removals` isolates
        the witness gate from the removal guard, which would otherwise fire for
        an unrelated reason (one run is a subset of the committed three-run set).
        """
        proc = self.run_with(dict(self.VALID), extra=("--allow-removals",))
        self.assertEqual(proc.returncode, 0,
                         "a valid witness must be accepted: %s" % proc.stderr[-400:])
        # It must have actually admitted the method, not merely exited 0.
        text = self.table.read_text(encoding="utf-8")
        self.assertIn("0x0BB0u,", text)

    def test_declared_method_must_match_the_witness(self):
        """The reported defect: name one method, quote a record for another."""
        bad = dict(self.VALID, method="0x1734")
        self.assert_refused(self.run_with(bad), "contradicts itself")

    def test_declared_class_must_match_the_witness(self):
        bad = dict(self.VALID, **{"class": "0x39"})
        self.assert_refused(self.run_with(bad), "contradicts itself")

    def test_witness_must_be_an_admit_unknown_record(self):
        bad = dict(self.VALID, witness="trust me, I saw it")
        self.assert_refused(self.run_with(bad), "not a [PFIFO] admit-unknown record")

    def test_hash_must_be_sha256_shaped(self):
        bad = dict(self.VALID, log_sha256="not-a-sha")
        self.assert_refused(self.run_with(bad), "64 hex characters")

    def test_stale_hash_is_refused_when_the_archive_is_present(self):
        """A present log whose hash disagrees means the witness is stale or edited."""
        if not (ROOT / "logs" / "runs" / self.VALID["run"] / "jsrf_run.log").is_file():
            self.skipTest("the witness archive is not present")
        bad = dict(self.VALID, log_sha256="0" * 64)
        self.assert_refused(self.run_with(bad), "stale or edited")

    def test_fabricated_witness_text_is_refused(self):
        """Right hash, but the quoted record is not in the log."""
        if not (ROOT / "logs" / "runs" / self.VALID["run"] / "jsrf_run.log").is_file():
            self.skipTest("the witness archive is not present")
        bad = dict(self.VALID, witness=self.VALID["witness"].replace("param=00000000",
                                                                    "param=DEADBEEF"))
        self.assert_refused(self.run_with(bad), "does not appear")

    def test_missing_provenance_fields_are_refused(self):
        for field in ("run", "log_sha256", "witness"):
            with self.subTest(missing=field):
                bad = dict(self.VALID)
                del bad[field]
                self.assert_refused(self.run_with(bad), "no auditable provenance")


@unittest.skipUnless(HAVE_ARCHIVES, "generating archives are not present")
@unittest.skipUnless(HAVE_OVERRIDES,
                     "the script under test cannot redirect both of its writes")
class EndToEndIsolationTests(unittest.TestCase):
    """A real generation with BOTH outputs redirected.

    The earlier version of this suite copied only the generator, so a valid
    invocation died at the decoder import and the override was never exercised;
    its `--table-out` test passed even when the flag was ignored, because it
    never required the target to exist.
    """

    def test_successful_generation_is_isolated(self):
        tmp = Path(tempfile.mkdtemp(prefix="nv2a-e2e-"))
        self.addCleanup(shutil.rmtree, tmp, ignore_errors=True)
        table = tmp / "table.c"
        doc = tmp / "doc.md"
        real_table_before = _sha(TOOLKIT_TABLE)
        real_doc_before = _sha(DOC)

        proc = subprocess.run(
            [sys.executable, "-X", "utf8", str(SCRIPT),
             "--table-out=%s" % table, "--doc-out=%s" % doc, *GENERATING_RUNS],
            capture_output=True, text=True, timeout=600)
        self.assertEqual(proc.returncode, 0,
                         "generation failed: %s" % proc.stderr[-600:])

        # It must have PRODUCED both artifacts at the redirected paths...
        self.assertTrue(table.is_file(), "--table-out was ignored")
        self.assertTrue(doc.is_file(), "--doc-out was ignored")
        text = table.read_text(encoding="utf-8")
        self.assertGreater(_methods_in(text), 300,
                           "the generated table looks empty")
        self.assertIn("Generated by", text)

        # ...and left BOTH real repository artifacts untouched.
        self.assertEqual(_sha(TOOLKIT_TABLE), real_table_before,
                         "the real toolkit table was modified")
        self.assertEqual(_sha(DOC), real_doc_before,
                         "the real inventory document was modified")

    def test_generated_table_reproduces_the_committed_one(self):
        """The documented invocation must still reproduce the committed table.

        This is the regression that matters for the next job: the guards must not
        change what a correct invocation produces.
        """
        if not TOOLKIT_TABLE.is_file():
            self.skipTest("no toolkit table to compare against")
        tmp = Path(tempfile.mkdtemp(prefix="nv2a-repro-"))
        self.addCleanup(shutil.rmtree, tmp, ignore_errors=True)
        table = tmp / "table.c"
        proc = subprocess.run(
            [sys.executable, "-X", "utf8", str(SCRIPT),
             "--table-out=%s" % table, "--doc-out=%s" % (tmp / "doc.md"),
             *GENERATING_RUNS],
            capture_output=True, text=True, timeout=600)
        self.assertEqual(proc.returncode, 0, proc.stderr[-600:])
        # Compare semantically: the toolkit file is checked out with CRLF on
        # Windows, so a byte comparison would fail for a line-ending reason.
        def methods(text):
            return re.findall(r"0x([0-9A-F]{4})u,", text)
        self.assertEqual(methods(table.read_text(encoding="utf-8")),
                         methods(TOOLKIT_TABLE.read_text(encoding="utf-8")),
                         "the regenerated table differs from the committed one")


if __name__ == "__main__":
    unittest.main()
