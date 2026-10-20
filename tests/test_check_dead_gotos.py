"""Controls for `scripts/check-dead-gotos.py`.

The label validator cannot keep a `goto` whose label is not in the emitting body.
It used to write `(void)0; /* goto loc_X - dead code ... */`, so a taken jump fell
through silently; it now writes `RECOMP_JUMP_TRAP(site, target)`, which is fatal.
The checker fails on any surviving `(void)0` form, and on any trap whose target
has a symbol of its own, because that jump should have been a tail call.
`sub_00169000` (`jmp 0x168230` deleted, so the callee's `ret 8` never ran) is the
motivating case.

What is pinned here, with the spellings the generator really writes:

1. the retired `(void)0` form fails by itself;
2. each way a trap target can be callable (recovered, generated body, stub, own
   dispatch symbol, manifest entry) fails by itself, so dropping one rule fails a
   named case;
3. the three no-symbol shapes (alias folded into an owner that starts first, alias
   whose owner starts after it and so aborts, plain interior) are counted and never
   fail; in particular `folded` and `fatal` are told apart through the
   `recomp_alias_<VA>` dispatch the toolkit emits, which an earlier regex missed;
4. the macro's own `#define` is not a site;
5. the current generated tree is clean.
"""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / "scripts" / "check-dead-gotos.py"

_spec = importlib.util.spec_from_file_location("check_dead_gotos", CHECKER)
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)


def trap(site: int, target: int) -> str:
    return (f"    RECOMP_JUMP_TRAP(0x{site:08X}u, 0x{target:08X}u); "
            f"/* goto loc_{target:08X}: no label in this function, "
            f"no symbol at the target */\n")


def silent(target: int) -> str:
    return (f"    (void)0; /* goto loc_{target:08X} - dead code, "
            f"label not in function */\n")


def func(start: int, *jumps: str) -> str:
    return f"void sub_{start:08X}(void)\n{{\n" + "".join(jumps) + "}\n\n"


def own_dispatch(addr: int) -> str:
    return f"    {{ 0x{addr:08X}u, (recomp_func_t)sub_{addr:08X} }},\n"


def folded_alias(addr: int, owner: int, i: int = 0) -> str:
    """Wrapper and table entry for an alias whose owner starts first."""
    return (f"static void recomp_alias_{addr:08X}(void) "
            f"{{ recomp_alias_observe({i}u); sub_{owner:08X}(); }}\n"
            f"    {{ 0x{addr:08X}u, (recomp_func_t)recomp_alias_{addr:08X} }},\n")


def fatal_alias(addr: int, owner: int, i: int = 0) -> str:
    """Wrapper and table entry for an alias whose owner starts after it."""
    return (f"static void recomp_alias_{addr:08X}(void) "
            f"{{ recomp_alias_observe({i}u); recomp_icall_fail_log(0x{addr:08X}u); "
            f"abort(); }} /* owner 0x{owner:08X} starts after this alias */\n"
            f"    {{ 0x{addr:08X}u, (recomp_func_t)recomp_alias_{addr:08X} }},\n")


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

    def fresh(self):
        self.setUp()

    def test_the_retired_void0_spelling_fails_by_itself(self):
        rc, out, err = self.counts(gen=func(0x1000, silent(0x5000)))
        self.assertEqual(rc, 1, out + err)
        self.assertIn("silent (void)0 deletions: 1", out)
        self.assertIn("recomp_0000.c:3: sub_00001000 -> 0x00005000 [silent]", out)
        self.assertIn("FAIL", err)

    def test_a_silent_deletion_fails_even_to_an_interior_target(self):
        # Nothing callable is lost, but a taken jump still falls through.
        rc, out, err = self.counts(gen=func(0x1000, silent(0x5000)),
                                   dispatch=own_dispatch(0x1000))
        self.assertEqual(rc, 1, out + err)
        self.assertIn("fatal traps: 0", out)

    def test_each_callable_trap_target_fails_by_itself(self):
        site = func(0x1000, trap(0x1005, 0x2000))
        cases = {
            "generated": dict(gen=site + func(0x2000)),
            "stub": dict(gen=site, stubs="void sub_00002000(void) {}\n"),
            "dispatch": dict(gen=site, dispatch=own_dispatch(0x2000)),
            "recovered": dict(gen=site, recovered="void sub_00002000(void) {\n}\n"),
        }
        for category, kw in cases.items():
            with self.subTest(category):
                self.fresh()
                rc, out, err = self.counts(**kw)
                self.assertEqual(rc, 1, out + err)
                self.assertIn(f"      {category:<10} 1\n", out)
                self.assertIn("callable target: 1", out)
                self.assertIn("silent (void)0 deletions: 0", out)
                self.assertIn("FAIL", err)

    def test_a_manifest_entry_is_callable(self):
        rc, out, err = self.counts(gen=func(0x1000, trap(0x1005, 0x2000)),
                                   manifest=[0x2000])
        self.assertEqual(rc, 1, out + err)
        self.assertIn("      recovered  1\n", out)

    def test_a_recovered_case_label_under_its_own_symbol_is_callable(self):
        rc, out, err = self.counts(
            gen=func(0x1000, trap(0x1005, 0x2000)),
            recovered="    case 0x00002000u: return sub_00002000;\n")
        self.assertEqual(rc, 1, out + err)
        self.assertIn("      dispatch   1\n", out)

    def test_folded_fatal_and_interior_targets_do_not_fail(self):
        rc, out, err = self.counts(
            gen=func(0x1000, trap(0x1005, 0x4000), trap(0x100A, 0x4100),
                     trap(0x100F, 0x5000)) + func(0x3000),
            dispatch=folded_alias(0x4000, 0x3000, 0)
            + fatal_alias(0x4100, 0x4200, 1))
        self.assertEqual(rc, 0, out + err)
        self.assertIn("callable target: 0", out)
        self.assertIn("      folded     1\n", out)
        self.assertIn("      fatal      1\n", out)
        self.assertIn("      interior   1\n", out)
        self.assertIn("no symbol at target: 3", out)
        self.assertEqual(err, "")

    def test_an_alias_wrapper_is_not_a_callable_symbol(self):
        # recomp_alias_X is dispatched at the alias VA but is not sub_X.
        rc, out, err = self.counts(
            gen=func(0x1000, trap(0x1005, 0x4000)),
            dispatch=folded_alias(0x4000, 0x3000))
        self.assertEqual(rc, 0, out + err)
        self.assertIn("      dispatch   0\n", out)

    def test_counts_are_by_category_across_a_mixed_tree(self):
        rc, out, err = self.counts(
            gen=(func(0x1000, trap(0x1005, 0x2000), trap(0x100A, 0x2000),
                      trap(0x100F, 0x6000))      # 2 generated, 1 interior
                 + func(0x2000)
                 + func(0x3000, trap(0x3005, 0x4000), silent(0x4000))),
            dispatch=folded_alias(0x4000, 0x3000),
            recovered="void sub_00007000(void) {\n"
                      + trap(0x7005, 0x7000) + "}\n")
        self.assertEqual(rc, 1, out + err)
        self.assertIn("6 untranslatable jump(s)", out)
        self.assertIn("silent (void)0 deletions: 1", out)
        self.assertIn("fatal traps: 5", out)
        self.assertIn("callable target: 3", out)
        self.assertIn("      generated  2\n", out)
        self.assertIn("      recovered  1\n", out)
        self.assertIn("      folded     1\n", out)
        self.assertIn("      interior   1\n", out)

    def test_the_define_alone_is_not_a_site(self):
        gen = ("#define RECOMP_JUMP_TRAP(0x00001005u, 0x00002000u) x\n"
               "void sub_00001000(void)\n{\n    (void)0;\n}\n")
        rc, out, err = self.counts(gen=gen)
        self.assertEqual(rc, 0, out + err)
        self.assertIn("0 untranslatable jump(s)", out)

    def test_the_report_names_the_function_and_target(self):
        rc, out, err = self.counts(gen=func(0x1000, trap(0x1005, 0x2000))
                                   + func(0x2000))
        self.assertIn("recomp_0000.c:3: sub_00001000 -> 0x00002000 [generated]", out)

    def test_a_missing_tree_is_an_error_not_a_pass(self):
        rc, out, err = run(self.root, self.root / "manifest.json")
        self.assertEqual(rc, 2)
        self.assertIn("generate the tree first", err)

    @unittest.skipUnless((ROOT / "src/recomp/gen").is_dir()
                         and (ROOT / "src/recomp/recovered").is_dir(),
                         "generated tree absent")
    def test_the_current_generated_tree_is_clean(self):
        rc, out, err = run(ROOT / "src/recomp", ROOT / "config/recovered-functions.json")
        self.assertEqual(rc, 0, out + err)
        self.assertIn("silent (void)0 deletions: 0", out)
        self.assertIn("callable target: 0", out)
        # Not vacuous: the scan read the real files.
        files = int(re.search(r"in (\d+) file", out).group(1))
        self.assertGreaterEqual(files, 2, out)


if __name__ == "__main__":
    unittest.main()
