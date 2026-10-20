"""The dispatch table must not run a function that does not contain the alias.

An abutting `tail_jump_alias` fold names the entry that starts where the alias
ends. That owner's body never contains the alias, so dispatching the alias to it
runs a different function: JSRF 0x000C42E0 (slot 7 of a .data handler table) ran
the constructor sub_000C44D0. The toolkit now keeps such an alias bodiless and
gives it a wrapper that fails like an unresolved call (`recomp_icall_fail_log`,
`abort`); the project recovered 0x000C42E0 and 0x0011B680 as their own functions.

Two decisions are pinned here, over the generated dispatch unit:

1. 0x000C42E0 and 0x0011B680 dispatch to their own `sub_<VA>`, which is defined,
   and are not aliases at all. 0x000C44D0 stays a separate entry.
2. Every alias recorded with an owner that starts after it has a wrapper that
   aborts and never reaches the owner. This compiles each wrapper line exactly as
   the generator wrote it, against stand-ins that print what they were asked to
   do, and runs it. A wrapper whose owner starts first (the control) must still
   call that owner and return.

Set JSRF_RECOMP_DIR to check another generated tree (the pre-fix backup, for
instance); it must hold gen/ and recovered/.
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECOMP = Path(os.environ.get("JSRF_RECOMP_DIR") or ROOT / "src/recomp")
GEN = RECOMP / "gen"
RECOVERED = RECOMP / "recovered"

# Recovered as their own functions after being folded onto a later owner.
RESTORED = (0x000C42E0, 0x0011B680)
STILL_DISTINCT = {0x000C42E0: 0x000C44D0}

MAP_RE = re.compile(r"\{\s*0x([0-9A-Fa-f]{8})u\s*,\s*0x([0-9A-Fa-f]{8})u\s*\}")
WRAPPER_RE = re.compile(
    r"^static void recomp_alias_([0-9A-Fa-f]{8})\(void\)(.*)$", re.M)
ENTRY_RE = re.compile(
    r"\{\s*0x([0-9A-Fa-f]{8})u\s*,\s*\(recomp_func_t\)\s*(\w+)\s*\}")
# `/* 0x00100041..0x0010005A are internal labels in sub_000FFFD0, not dispatchable
# entries. */`: the generator keeps the wrapper but leaves the slot out.
UNDISPATCHED_RE = re.compile(
    r"/\*\s*0x([0-9A-Fa-f]{8})\.\.0x([0-9A-Fa-f]{8})\b[^*]*"
    r"not dispatchable", re.S)
CALLEE_RE = re.compile(r"observe\(\d+u\);\s*(\w+)\(\);\s*\}")


def _dispatch():
    return (GEN / "recomp_dispatch.c").read_text(encoding="utf-8")


def _alias_map(text):
    """{alias VA: owner VA} from g_recomp_alias_icall_map."""
    start = text.index("g_recomp_alias_icall_map")
    end = text.index("};", start)
    return {int(a, 16): int(o, 16) for a, o in MAP_RE.findall(text[start:end])}


def _entries(text):
    return {int(a, 16): sym for a, sym in ENTRY_RE.findall(text)}


def _defined(name):
    pat = re.compile(rf"^void {name}\(void\)", re.M)
    return any(pat.search(p.read_text(encoding="utf-8", errors="replace"))
               for d in (GEN, RECOVERED) for p in sorted(d.glob("*.c")))


def _compiler():
    for c in (shutil.which("clang"), shutil.which("gcc"),
              r"C:\Program Files\LLVM\bin\clang.exe"):
        if c and Path(c).exists():
            return c
    raise AssertionError("no C compiler found (clang, gcc or "
                         r"C:\Program Files\LLVM\bin\clang.exe); this test "
                         "cannot run, which is a failure, not a skip")


class RestoredEntriesTest(unittest.TestCase):

    def test_restored_functions_dispatch_to_their_own_symbol(self):
        text = _dispatch()
        entries = _entries(text)
        aliases = _alias_map(text)
        for va in RESTORED:
            with self.subTest(va=f"0x{va:08X}"):
                self.assertEqual(entries.get(va), f"sub_{va:08X}")
                self.assertNotIn(va, aliases)
                self.assertNotIn(f"recomp_alias_{va:08X}", text)
                self.assertTrue(_defined(f"sub_{va:08X}"),
                                f"sub_{va:08X} is not defined")

    def test_the_former_owner_is_still_its_own_entry(self):
        entries = _entries(_dispatch())
        for va, owner in STILL_DISTINCT.items():
            self.assertEqual(entries.get(owner), f"sub_{owner:08X}")
            self.assertNotEqual(entries.get(va), entries.get(owner))


class AfterOwnerAliasTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        text = _dispatch()
        cls.entries = _entries(text)
        cls.aliases = _alias_map(text)
        cls.wrappers = {int(a, 16): f"static void recomp_alias_{a}(void){rest}"
                        for a, rest in WRAPPER_RE.findall(text)}
        cls.undispatched = [(int(a, 16), int(b, 16))
                            for a, b in UNDISPATCHED_RE.findall(text)]
        # The project removes the wrapper and the table entry of an alias that is an
        # interior label (generated-patches), leaving its row in the map.
        cls.removed = sorted(a for a in cls.aliases if a not in cls.wrappers)
        cls.late = sorted(a for a, o in cls.aliases.items()
                          if o > a and a in cls.wrappers)
        cls.early = sorted(a for a, o in cls.aliases.items()
                           if o < a and a in cls.wrappers)

    def test_the_tree_has_both_kinds_to_decide(self):
        # A vacuous pass (no aliases read) must not look like success.
        self.assertTrue(self.late, "no after-owner aliases found")
        self.assertTrue(self.early, "no earlier-owner aliases found")

    def test_an_alias_without_a_wrapper_has_no_table_slot(self):
        """Removed together, so nothing dispatches to a wrapper that is gone."""
        for va in self.removed:
            self.assertNotIn(va, self.entries, f"0x{va:08X}")

    def test_every_alias_has_a_wrapper_in_its_table_slot(self):
        """Slots do not move; only a listed internal label has none."""
        for va in self.late + self.early:
            self.assertIn(va, self.wrappers, f"0x{va:08X}")
            if any(lo <= va <= hi for lo, hi in self.undispatched):
                self.assertNotIn(va, self.entries, f"0x{va:08X}")
                continue
            self.assertEqual(self.entries.get(va), f"recomp_alias_{va:08X}")

    def test_each_after_owner_wrapper_aborts_and_never_calls_the_owner(self):
        self._run_all(self.late, expect_abort=True)

    def test_earlier_owner_wrappers_still_call_the_owner(self):
        """Control: the harness can tell a calling wrapper from a fatal one."""
        self._run_all(self.early[:3], expect_abort=False)

    def _run_all(self, vas, expect_abort):
        cc = _compiler()
        callees = set()
        for va in vas:
            m = CALLEE_RE.search(self.wrappers[va])
            if m:
                callees.add(m.group(1))
        # abort() is trapped and turned into a plain exit so the OS crash reporter
        # does not make every fatal wrapper take seconds.
        src = ["#include <stdint.h>", "#include <stdio.h>", "#include <stdlib.h>",
               "#include <signal.h>",
               "#ifdef _WIN32", "#include <process.h>", "#else",
               "#include <unistd.h>", "#endif",
               "static void on_abort(int s) { (void)s; fflush(stdout); _exit(134); }",
               "static void recomp_alias_observe(uint32_t i) { (void)i; }",
               "void recomp_icall_fail_log(uint32_t va)"
               ' { printf("FAIL_LOG %08X\\n", va); fflush(stdout); }']
        for name in sorted(callees):
            src.append(f'void {name}(void) {{ printf("OWNER {name}\\n"); '
                       f"fflush(stdout); }}")
        src += [self.wrappers[va] for va in vas]
        src.append("static void (*const fns[])(void) = {"
                   + ", ".join(f"recomp_alias_{va:08X}" for va in vas) + "};")
        src.append('int main(int c, char **v) { (void)c; signal(SIGABRT, on_abort); '
                   'fns[atoi(v[1])](); '
                   'puts("RETURNED"); return 0; }')
        with tempfile.TemporaryDirectory() as tmp:
            c = Path(tmp) / "t.c"
            c.write_text("\n".join(src) + "\n")
            exe = Path(tmp) / "t.exe"
            r = subprocess.run([cc, str(c), "-o", str(exe), "-w"],
                               capture_output=True, text=True)
            if r.returncode:
                raise AssertionError("compile failed:\n" + r.stderr[-3000:])
            for i, va in enumerate(vas):
                with self.subTest(va=f"0x{va:08X}"):
                    r = subprocess.run([str(exe), str(i)],
                                       capture_output=True, text=True)
                    if expect_abort:
                        self.assertNotEqual(r.returncode, 0, r.stdout)
                        self.assertIn(f"FAIL_LOG {va:08X}", r.stdout)
                        self.assertNotIn("OWNER", r.stdout)
                        self.assertNotIn("RETURNED", r.stdout)
                    else:
                        self.assertEqual(r.returncode, 0, r.stdout)
                        self.assertIn("OWNER ", r.stdout)
                        self.assertIn("RETURNED", r.stdout)
                        self.assertNotIn("FAIL_LOG", r.stdout)


if __name__ == "__main__":
    unittest.main()
