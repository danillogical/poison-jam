#!/usr/bin/env python3
"""Decides that the BUMPENVMAP CPU fixture really builds and runs OFF Windows.

This test is not a simulation and not a text check.  It takes the source list
of the SHIPPED ``add_executable(jsrf_bumpenv_q_cpu_test ...)`` registration out
of ``CMakeLists.txt``, compiles that fixture with a real POSIX C compiler,
links it, and runs the produced binary once per case, requiring exit 0 and the
``PASS bumpenv_q_cpu[<case>]`` line for each of the five cases.  A compile or
link failure is a FAILURE of this test with the compiler's own output, never a
skip.

On Windows the compiler is the WSL GCC (``wsl -e bash -lc 'command -v gcc'``):
a real Linux build, not a simulation.  What it decides is the FIXTURE's
portability, not CMake's: this file parses the shipped source list and
registration out of ``CMakeLists.txt``, supplies the ``platform`` translation
unit itself (see ``PLATFORM_SOURCE`` below), and invokes the compiler directly.
It does NOT configure or build a non-Windows CMake target graph, so it cannot
detect a broken ``if(NOT WIN32)`` branch in CMake -- that branch is verified by
source inspection only.  No Mac run is claimed by this file; only the host this
test runs on is actually compiled.

The only skip is "no POSIX compiler exists at all", which is reported as a
skip naming exactly what is missing; a skip is never a pass.
"""

from __future__ import annotations

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
GAME = HERE.parent
TOOLKIT = (GAME / ".." / "xboxrecomp").resolve()
CMAKE_LISTS = GAME / "CMakeLists.txt"

TARGET = "jsrf_bumpenv_q_cpu_test"
FIXTURE = "tests/test_bumpenv_q_cpu.c"
CASES = ["bump_zero_matrix", "bump_displacement", "bump_positive_delta",
         "bump_lum", "project2d_control"]

# The `platform` dependency in CMake is a library NAME, not a file.  On POSIX
# that library is exactly this translation unit (src/platform/CMakeLists.txt:
# `add_library(platform STATIC win32_compat.c)`), so the link below adds it as
# a source.  Windows gets these primitives from the CRT, which is why the CMake
# link line is inside `if(NOT WIN32)`.
PLATFORM_SOURCE = TOOLKIT / "src" / "platform" / "win32_compat.c"

# Run inside the POSIX shell: convert the host paths, compile the fixture with
# the same flags and include dirs the CMake target uses, compile the remaining
# sources, link, then run every case.  `wslpath` exists only under WSL; on a
# native POSIX host `wp` is the identity.
COMPILE_SCRIPT = r"""
set -u
CC_BIN=$1
G=$2
T=$3
D=$4
shift 4

wp() {
    if command -v wslpath >/dev/null 2>&1; then
        wslpath -a "$1"
    else
        printf '%s' "$1"
    fi
}

G=$(wp "$G")
T=$(wp "$T")
D=$(wp "$D")

INC="-I$T/src -I$T/src/kernel -I$T/src/nv2a -I$T/templates/runtime -I$G/tests/compat"

echo "== compile fixture $G/tests/test_bumpenv_q_cpu.c"
# shellcheck disable=SC2086
$CC_BIN -std=gnu11 -fno-asm -O2 -c "$G/tests/test_bumpenv_q_cpu.c" \
    -o "$D/fixture.o" $INC \
    -include "$G/tests/compat/jsrf_test_compat.h" || exit 10

objs="$D/fixture.o"
i=0
for src in "$@"; do
    src=$(wp "$src")
    echo "== compile $src"
    # shellcheck disable=SC2086
    $CC_BIN -std=gnu11 -O2 -c "$src" -o "$D/obj$i.o" $INC || exit 11
    objs="$objs $D/obj$i.o"
    i=$((i + 1))
done

echo "== link"
# shellcheck disable=SC2086
$CC_BIN $objs -o "$D/fixture" -lpthread -lm -ldl || exit 12

rc=0
while IFS= read -r c; do
    c=${c%$'\r'}
    [ -n "$c" ] || continue
    echo "== run $c"
    if ! "$D/fixture" --case "$c"; then
        echo "CASE FAILED (nonzero exit): $c"
        rc=1
    fi
done < "$D/cases"
exit $rc
"""


def _paren_span(text: str, start: int) -> tuple[int, int]:
    """Return (open, close) of the parenthesised call beginning at `start`."""
    open_at = text.index("(", start)
    depth = 0
    for i in range(open_at, len(text)):
        ch = text[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return open_at, i
    raise AssertionError("unbalanced parentheses in CMakeLists.txt")


def _strip_comments(text: str) -> str:
    return "\n".join(line.split("#", 1)[0] for line in text.splitlines())


class ShippedRegistrationTest(unittest.TestCase):
    """The CMake registration under test: target, sources and five cases."""

    @classmethod
    def setUpClass(cls):
        if not CMAKE_LISTS.is_file():
            raise AssertionError(f"no CMakeLists.txt at {CMAKE_LISTS}")
        cls.text = CMAKE_LISTS.read_text(encoding="utf-8")

    def test_target_and_sources_are_registered(self):
        head = f"add_executable({TARGET}"
        at = self.text.find(head)
        self.assertNotEqual(
            at, -1,
            f"CMakeLists.txt no longer registers add_executable({TARGET} ...); "
            f"the off-Windows fixture build has no registration to decide")
        open_at, close_at = _paren_span(self.text, at)
        args = _strip_comments(self.text[open_at + 1:close_at]).split()
        self.assertTrue(args, f"add_executable({TARGET} ...) has no sources")
        self.assertEqual(args[0], TARGET)
        sources = args[1:]
        self.assertIn(FIXTURE, sources,
                      f"{TARGET} does not list {FIXTURE} among its sources")
        for src in sources:
            if "${" not in src:
                continue
            self.assertTrue(
                src.startswith("${XBOXRECOMP_DIR}"),
                f"{TARGET} source {src!r} uses a CMake variable this test does "
                f"not know how to resolve to a file")
        self.assertGreaterEqual(len(sources), 2,
                                f"{TARGET} should compile the fixture plus the "
                                f"toolkit sources it needs")
        # Only used by the compile test below; recorded here so a missing
        # toolkit fails as a registration problem, not as a compile mystery.
        self.assertTrue(TOOLKIT.is_dir(),
                        f"the toolkit checkout is missing: {TOOLKIT}")

    def test_five_cases_are_registered(self):
        found = set()
        for m in re.finditer(r"add_test\(\s*NAME\s+jsrf_bumpenv_q_cpu_([A-Za-z0-9_]+)",
                             self.text):
            if "${" not in m.group(1):
                found.add(m.group(1))
        # The shipped registration spells the five cases in a foreach loop, so
        # collect the loop variable's list wherever its body adds the test.
        for m in re.finditer(r"foreach\(\s*(\w+)\s+([^)]*)\)", self.text):
            var, body_start = m.group(1), m.end()
            end = self.text.find("endforeach", body_start)
            if end == -1:
                continue
            body = self.text[body_start:end]
            if f"add_test(NAME jsrf_bumpenv_q_cpu_${{{var}}}" not in body:
                continue
            found.update(_strip_comments(m.group(2)).split())
        missing = [c for c in CASES if c not in found]
        extra = sorted(found - set(CASES))
        self.assertEqual(
            missing, [],
            f"CMakeLists.txt does not register the CTest case(s) {missing} for "
            f"{TARGET}; registered: {sorted(found)}")
        self.assertEqual(
            extra, [],
            f"CMakeLists.txt registers unexpected jsrf_bumpenv_q_cpu_* case(s) "
            f"{extra}; the shipped set is {CASES}")


class OffWindowsBuildTest(unittest.TestCase):
    """A real POSIX compile, link and run of the shipped fixture sources."""

    def _posix_compiler(self):
        """Return (runner, compiler, shell_args) for a real POSIX build."""
        if os.name == "nt":
            exe = shutil.which("wsl")
            if exe is None:
                self.skipTest(
                    "no POSIX C compiler: `wsl` is not on PATH, so "
                    "`wsl -e bash -lc 'command -v gcc'` cannot run")
            p = subprocess.run([exe, "-e", "bash", "-lc", "command -v gcc"],
                               capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
            found = (p.stdout or "").strip()
            if p.returncode != 0 or not found:
                self.skipTest(
                    "no POSIX C compiler: `wsl -e bash -lc 'command -v gcc'` "
                    f"found nothing (exit {p.returncode}, stderr "
                    f"{(p.stderr or '').strip()!r}); WSL or GCC is not installed")
            return exe, found, ["-e", "bash", "-c"]

        cc = os.environ.get("CC") or "cc"
        parts = cc.split()
        resolved = shutil.which(parts[0])
        if resolved is None:
            self.skipTest(
                f"no POSIX C compiler: neither $CC ({os.environ.get('CC')!r}) nor "
                f"`cc` is on PATH")
        shell = shutil.which("sh") or "/bin/sh"
        # Keep any flags $CC carries ("cc -m32"): the script expands $CC_BIN
        # unquoted, exactly as a Makefile would.
        compiler = " ".join([resolved, *parts[1:]])
        return shell, compiler, ["-c"]

    def _sources(self):
        text = CMAKE_LISTS.read_text(encoding="utf-8")
        at = text.find(f"add_executable({TARGET}")
        self.assertNotEqual(at, -1, f"{TARGET} is not registered in CMakeLists.txt")
        open_at, close_at = _paren_span(text, at)
        args = _strip_comments(text[open_at + 1:close_at]).split()[1:]
        out = []
        for src in args:
            if src == FIXTURE:
                continue
            resolved = src.replace("${XBOXRECOMP_DIR}", str(TOOLKIT))
            self.assertNotIn("${", resolved,
                             f"cannot resolve the CMake source {src!r} to a file")
            out.append(resolved)
        return out

    def test_compiles_links_and_passes_every_case(self):
        self.assertTrue(PLATFORM_SOURCE.is_file(),
                        f"the platform shim is missing: {PLATFORM_SOURCE} -- the "
                        f"CMake `platform` target compiles exactly this file on "
                        f"POSIX, so the off-Windows link cannot be reproduced")
        self.assertTrue((TOOLKIT / "src" / "platform").is_dir(),
                        f"no toolkit platform directory: {TOOLKIT / 'src' / 'platform'}")
        for needed in (GAME / "tests" / "compat" / "jsrf_test_compat.h",
                       TOOLKIT / "src" / "kernel",
                       TOOLKIT / "templates" / "runtime"):
            self.assertTrue(needed.exists(),
                            f"the fixture's build inputs are incomplete: {needed} "
                            f"is missing")

        exe, cc, shell_args = self._posix_compiler()
        sources = self._sources()
        self.assertTrue(sources, "the CMake target lists no toolkit sources")

        with tempfile.TemporaryDirectory() as tmp:
            # The case names come from this file's CASES list, so the run half
            # cannot drift from what the registration test asserts.
            (pathlib.Path(tmp) / "cases").write_text(
                "\n".join(CASES) + "\n", encoding="utf-8", newline="\n")
            cmd = [exe, *shell_args, COMPILE_SCRIPT, "bumpenv-portability",
                   cc, str(GAME), str(TOOLKIT), str(tmp),
                   *sources, str(PLATFORM_SOURCE)]
            p = subprocess.run(cmd, capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
            out = (p.stdout or "") + (p.stderr or "")
            sys.stdout.write(out)

            self.assertEqual(
                p.returncode, 0,
                f"the off-Windows build/run of {TARGET} failed (exit "
                f"{p.returncode}); compiler output:\n{out}")
            for case in CASES:
                self.assertIn(
                    f"PASS bumpenv_q_cpu[{case}]", out,
                    f"case {case} did not report PASS; output:\n{out}")
                self.assertNotIn(
                    f"CASE FAILED (nonzero exit): {case}", out,
                    f"case {case} exited nonzero; output:\n{out}")


if __name__ == "__main__":
    # A skip is never a pass: CTest is told SKIP_RETURN_CODE 77 for this test,
    # so a host with no POSIX compiler reports 77 (skipped) instead of the 0
    # unittest gives an all-skipped run.
    program = unittest.main(verbosity=2, exit=False)
    result = program.result
    failed = (len(result.failures) + len(result.errors)
              + len(getattr(result, "unexpectedSuccesses", [])))
    if failed:
        sys.exit(1)
    if result.skipped:
        print(f"SKIP: {len(result.skipped)} test(s) skipped -- no POSIX C "
              f"compiler to build the off-Windows fixture with", file=sys.stderr)
        sys.exit(77)
    sys.exit(0)
