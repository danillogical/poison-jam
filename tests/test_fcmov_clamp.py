"""The generated [0,1] clamp helpers must actually clamp (F4b).

sub_0014C870 is MSVC's min and sub_0014C850 its max; each ends in fcmove /
fcmovne. The lifter used to drop those as comments, so min(x, 1.0) returned 1.0
and max(x, 0) returned 0 and the SEGA fade's alpha was reset to 0 on every
update. This compiles the real bodies out of src/recomp/gen against the
toolkit's runtime header and executes them.

The negative control feeds the same harness the same bodies with the two
conditional-move lines turned back into the old comment. It must fail; a test
that cannot fail proves nothing.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLKIT = ROOT.parent / "xboxrecomp"


def _bodies():
    text = (ROOT / "src/recomp/gen/recomp_0003.c").read_text(encoding="utf-8")
    out = {}
    for name in ("sub_0014C850", "sub_0014C870"):
        m = re.search(r"\nvoid " + name + r"\(void\)\n\{.*?\n\}\n", text, re.S)
        if not m:
            raise AssertionError(name + " not found in recomp_0003.c")
        out[name] = m.group(0)
    return out


def _compiler():
    for c in (shutil.which("clang"), shutil.which("gcc"),
              r"C:\Program Files\LLVM\bin\clang.exe"):
        if c and Path(c).exists():
            return c
    raise unittest.SkipTest("no C compiler")


HARNESS = r'''
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
RECOMP_TLS int g_fp_cmp;
RECOMP_TLS uint16_t g_fp_cc;
RECOMP_TLS uint16_t g_fp_control_word;
RECOMP_TLS uint32_t g_fs_base, g_seh_ebp;
RECOMP_TLS int g_df;
ptrdiff_t g_xbox_mem_offset;
uint32_t g_xbox_code_lo, g_xbox_code_hi;
RECOMP_TLS volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE];
RECOMP_TLS volatile uint32_t g_icall_trace_idx;
RECOMP_TLS volatile uint64_t g_icall_count;
%BODIES%
static uint8_t ram[0x1000];
/* Call as the guest does: args at [esp+4], [esp+8]; result in st(0). */
static float call(void (*f)(void), float a, float b) {
    g_esp = 0x800; memcpy(ram + 0x804, &a, 4); memcpy(ram + 0x808, &b, 4);
    g_fp_top = 0; f();
    return (float)g_fp_stack[g_fp_top & 7];
}
int main(void) {
    g_xbox_mem_offset = (ptrdiff_t)ram;
    float x = 1.0f / 120.0f;
    float mn = call(sub_0014C870, x, 1.0f);       /* min(x, 1.0) */
    float mx = call(sub_0014C850, x, 0.0f);       /* max(x, 0.0) */
    float mn2 = call(sub_0014C870, 2.0f, 1.0f);
    float mx2 = call(sub_0014C850, -1.0f, 0.0f);
    printf("%.9g %.9g %.9g %.9g\n", mn, mx, mn2, mx2);
    return (mn == x) && (mx == x) && (mn2 == 1.0f) && (mx2 == 0.0f) ? 0 : 1;
}
'''


def _run(bodies):
    cc = _compiler()
    src = HARNESS.replace("%BODIES%", "\n".join(bodies.values()))
    with tempfile.TemporaryDirectory() as tmp:
        c = Path(tmp) / "t.c"
        c.write_text(src)
        exe = Path(tmp) / "t.exe"
        cmd = [cc, str(c), "-o", str(exe), "-w",
               "-I" + str(TOOLKIT / "templates/runtime")]
        if os.name != "nt":
            cmd.append("-lm")
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("compile failed:\n" + r.stderr[-3000:])
        return subprocess.run([str(exe)], capture_output=True, text=True)


class ClampTest(unittest.TestCase):
    def test_generated_clamp_helpers_clamp(self):
        bodies = _bodies()
        for name, body in bodies.items():
            self.assertNotIn("/* FPU:", body, name)
        r = _run(bodies)
        self.assertEqual(r.returncode, 0, r.stdout)

    def test_negative_control_old_lift_fails(self):
        bodies = _bodies()
        old = {k: re.sub(r"if \([^\n]*\) fp_top\(\) = fp_st1\(\); /\* (fcmovn?e) \*/",
                         r"/* FPU: \1 st(0), st(1) */", v)
               for k, v in bodies.items()}
        self.assertTrue(all("/* FPU: fcmov" in v for v in old.values()))
        r = _run(old)
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertEqual(r.stdout.split()[0], "1")  # min(1/120, 1.0) -> 1.0


if __name__ == "__main__":
    unittest.main()
