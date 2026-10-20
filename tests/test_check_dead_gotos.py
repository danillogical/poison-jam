"""Controls for `scripts/check-dead-gotos.py`.

The checker fails when the label validator deleted a jump (`(void)0; /* goto
loc_X - dead code ... */`) whose target has a symbol of its own, because such a
jump is a tail call the lifter missed. `sub_00169000` is the motivating case: its
`jmp 0x168230` was deleted, so the callee's `ret 8` never ran.

What is pinned here:

1. each way a target can be callable (recovered, generated body, stub, own
   dispatch symbol, manifest entry) is detected, on its own, so that dropping
   one rule fails a named case;
2. the two no-symbol shapes (alias folded into its owner, plain interior) are
   counted but never fail the check, so it cannot be satisfied by deleting the
   rule that distinguishes them;
3. only the validator's exact comment is a deleted jump;
4. the current generated tree is clean.
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
CHECKER = ROOT / "scripts" / "check-dead-gotos.py"

_spec = importlib.util.spec_from_file_location("check_dead_gotos", CHECKER)
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)


def dead(target: int) -> str:
    return (f"    (void)0; /* goto loc_{target:08X} - dead code, "
            f"label not in function */\n")


def func(start: int, *jumps: int) -> str:
    return (f"void sub_{start:08X}(void)\n{{\n"
            + "".join(dead(t) for t in jumps) + "}\n\n")


def build(root: Path, *, gen="", dispatch="", stubs="", recovered="",
          manifest=()) -> Path:
    """Write a recomp tree and return the manifest path."""
    (root / "gen").mkdir(parents=True)
    (root / "recovered").mkdir()
    (root / "gen" / "recomp_0000.c").write_text(gen)
    (root / "gen" / "recomp_dispatch.c").write_text(dispatch)
    (root / "gen" / "recomp_stubs.c").write_text(stubs)
    (root / "recovered" / "recovered.c").write_text(recovered)
    path = root / "manifest.json"
    path.write_text(json.dumps([{"start": f"0x{a:08X}"} for a in manifest]))
    return path


def run(root: Path, manifest: Path):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        rc = checker.main(["--recomp-dir", str(root), "--manifest", str(manifest)])
    return rc, out.getvalue(), err.getvalue()


class CheckDeadGotosTest(unittest.TestCase):

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.root = Path(self._tmp.name) / "recomp"

    def counts(self, **kw):
        manifest = build(self.root, **kw)
        return run(self.root, manifest)

    def test_each_callable_shape_fails_by_itself(self):
        cases = {
            "generated": dict(gen=func(0x1000, 0x2000) + func(0x2000)),
            "stub": dict(gen=func(0x1000, 0x2000),
                         stubs="void sub_00002000(void) {}\n"),
            "dispatch": dict(
                gen=func(0x1000, 0x2000),
                dispatch="{ 0x00002000u, (recomp_func_t)sub_00002000 },\n"),
            "recovered": dict(
                gen=func(0x1000, 0x2000),
                recovered="void sub_00002000(void) {\n}\n"),
        }
        for category, kw in cases.items():
            with self.subTest(category):
                self.setUp()
                rc, out, err = self.counts(**kw)
                self.assertEqual(rc, 1, out + err)
                self.assertIn(f"    {category:<10} 1\n", out)
                self.assertIn("callable target: 1", out)
                self.assertIn("FAIL", err)

    def test_a_manifest_entry_is_callable(self):
        rc, out, err = self.counts(gen=func(0x1000, 0x2000), manifest=[0x2000])
        self.assertEqual(rc, 1, out + err)
        self.assertIn("    recovered  1\n", out)

    def test_a_recovered_case_label_under_its_own_symbol_is_callable(self):
        rc, out, err = self.counts(
            gen=func(0x1000, 0x2000),
            recovered="    case 0x00002000u: return sub_00002000;\n")
        self.assertEqual(rc, 1, out + err)
        self.assertIn("    dispatch   1\n", out)

    def test_a_folded_alias_and_an_interior_label_do_not_fail(self):
        # 0x4000 is dispatched under its owner's symbol: no symbol of its own.
        rc, out, err = self.counts(
            gen=func(0x1000, 0x4000, 0x5000) + func(0x3000),
            dispatch="{ 0x00004000u, (recomp_func_t)sub_00003000 },\n")
        self.assertEqual(rc, 0, out + err)
        self.assertIn("callable target: 0", out)
        self.assertIn("    folded     1\n", out)
        self.assertIn("    interior   1\n", out)
        self.assertIn("no symbol at target: 2", out)
        self.assertEqual(err, "")

    def test_counts_are_by_category_across_a_mixed_tree(self):
        rc, out, err = self.counts(
            gen=(func(0x1000, 0x2000, 0x2000, 0x6000)      # 2 generated, 1 interior
                 + func(0x2000)
                 + func(0x3000, 0x4000)),                    # 1 folded
            dispatch="{ 0x00004000u, (recomp_func_t)sub_00003000 },\n",
            recovered="void sub_00007000(void) {\n    (void)0; /* goto "
                      "loc_00007000 - dead code, label not in function */\n}\n")
        self.assertEqual(rc, 1, out + err)
        self.assertIn("5 deleted jump(s)", out)
        self.assertIn("callable target: 3", out)
        self.assertIn("    generated  2\n", out)
        self.assertIn("    recovered  1\n", out)
        self.assertIn("    folded     1\n", out)
        self.assertIn("    interior   1\n", out)

    def test_only_the_validators_comment_is_a_deleted_jump(self):
        gen = ("void sub_00001000(void)\n{\n"
               "    (void)0;\n"
               "    /* goto loc_00002000 */\n"
               "    goto loc_00002000;\n"
               "    sub_00002000(); return; /* tail jmp 0x00002000 */\n}\n\n"
               + func(0x2000))
        rc, out, err = self.counts(gen=gen)
        self.assertEqual(rc, 0, out + err)
        self.assertIn("0 deleted jump(s)", out)

    def test_the_report_names_the_function_and_target(self):
        rc, out, err = self.counts(gen=func(0x1000, 0x2000) + func(0x2000))
        self.assertIn("recomp_0000.c:3: sub_00001000 -> 0x00002000 [generated]", out)

    def test_a_missing_tree_is_an_error_not_a_pass(self):
        rc, out, err = run(self.root, self.root / "manifest.json")
        self.assertEqual(rc, 2)
        self.assertIn("generate the tree first", err)

    @unittest.skipUnless((ROOT / "src/recomp/gen").is_dir()
                         and (ROOT / "src/recomp/recovered").is_dir(),
                         "generated tree absent")
    def test_the_current_generated_tree_has_no_callable_dead_goto(self):
        rc, out, err = run(ROOT / "src/recomp", ROOT / "config/recovered-functions.json")
        self.assertEqual(rc, 0, out + err)
        self.assertIn("callable target: 0", out)
        # Not vacuous: the scan found the files and the interior jumps.
        self.assertNotIn("0 deleted jump(s)", out)


if __name__ == "__main__":
    unittest.main()
