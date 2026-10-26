"""The adjustor thunk sub_00169000 must reach its callee and inherit its `ret 8`.

The guest code is `sub dword ptr [esp+4], 4 ; jmp 0x168230`: adjust `this`, then
tail-call the target. A tail jump leaves through the callee's return, so the
callee's `ret 8` is the thunk's `ret 8`. The lifter deleted the jump instead
(`(void)0; /* goto loc_00168230 - dead code ... */`), so the thunk returned with
a bare `ret`: the guest stack stayed 8 bytes low and the popped ebx reached the
D3D vblank-callback setter as an ICALL target (TR 31.53).

This compiles the real body out of src/recomp/gen against the toolkit's runtime
header, with a stand-in for sub_00168230 that records what it was entered with
and then does what the real one's `ret 8` does. It checks the behaviour, not the
shape of the emitted line:

  * the callee is entered exactly once, with esp unchanged by the thunk;
  * the callee sees `this` decremented by 4 and the second argument untouched;
  * on return esp is 12 above its value at entry: the return address plus the
    callee's 8 bytes of arguments.

The negative control feeds the same harness the body with the tail call turned
back into the deleted jump. It must fail; a test that cannot fail proves nothing.
Set JSRF_GEN_DIR to run the same check against another generated tree (the
pre-fix backup, for instance).
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(ROOT / 'scripts'))
from toolkit_path import toolkit_path  # noqa: E402
TOOLKIT = toolkit_path()
GEN_DIR = Path(os.environ.get("JSRF_GEN_DIR") or ROOT / "src/recomp/gen")
THUNK = "sub_00169000"


def _body():
    for path in sorted(GEN_DIR.glob("recomp_*.c")):
        text = path.read_text(encoding="utf-8")
        m = re.search(r"\nvoid " + THUNK + r"\(void\)\n\{.*?\n\}\n", text, re.S)
        if m:
            return m.group(0)
    raise AssertionError(THUNK + " not found under " + str(GEN_DIR))


def _compiler():
    for c in (shutil.which("clang"), shutil.which("gcc"),
              r"C:\Program Files\LLVM\bin\clang.exe"):
        if c and Path(c).exists():
            return c
    # A skip is counted as a pass by CTest; a missing compiler must fail loudly.
    raise AssertionError("no C compiler found (clang, gcc or "
                         r"C:\Program Files\LLVM\bin\clang.exe); this test "
                         "cannot run, which is a failure, not a skip")


HARNESS = r'''
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
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

#define ENTRY_ESP 0x800u
#define THIS_IN   0x00700014u
#define ARG_IN    0x00001234u
#define RET_IN    0x0000AAAAu

static int callee_calls;
static uint32_t callee_esp, callee_this, callee_arg;

/* The callee: stdcall with two stack arguments, `ret 8`. */
void sub_00168230(void) {
    ++callee_calls;
    callee_esp = g_esp;
    callee_this = MEM32(g_esp + 4);
    callee_arg = MEM32(g_esp + 8);
    g_esp += 4 + 8;
}

%BODY%

static uint8_t ram[0x1000];
int main(void) {
    g_xbox_mem_offset = (ptrdiff_t)ram;
    g_esp = ENTRY_ESP;
    MEM32(ENTRY_ESP) = RET_IN;
    MEM32(ENTRY_ESP + 4) = THIS_IN;
    MEM32(ENTRY_ESP + 8) = ARG_IN;
    %THUNK%();
    printf("calls=%d callee_esp=%08X this=%08X arg=%08X esp=%08X\n",
           callee_calls, callee_esp, callee_this, callee_arg, g_esp);
    return (callee_calls == 1
            && callee_esp == ENTRY_ESP
            && callee_this == THIS_IN - 4
            && callee_arg == ARG_IN
            && g_esp == ENTRY_ESP + 12) ? 0 : 1;
}
'''


def _run(body):
    cc = _compiler()
    src = HARNESS.replace("%BODY%", body).replace("%THUNK%", THUNK)
    with tempfile.TemporaryDirectory() as tmp:
        c = Path(tmp) / "t.c"
        c.write_text(src)
        exe = Path(tmp) / "t.exe"
        cmd = [cc, str(c), "-o", str(exe), "-w",
               "-I" + str(TOOLKIT / "templates/runtime")]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("compile failed:\n" + r.stderr[-3000:])
        return subprocess.run([str(exe)], capture_output=True, text=True)


class TailThunkTest(unittest.TestCase):
    def test_thunk_enters_callee_and_inherits_its_ret8(self):
        r = _run(_body())
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertEqual(
            r.stdout.strip(),
            "calls=1 callee_esp=00000800 this=00700010 arg=00001234 esp=0000080C")

    def test_negative_control_deleted_jump_fails(self):
        body = _body()
        old = re.sub(r"g_seh_ebp = ebp; sub_00168230\(\); return; [^\n]*",
                     "(void)0; /* goto loc_00168230 - dead code, "
                     "label not in function */", body)
        # Holds trivially when pointed at a tree that already has the old lift.
        self.assertNotIn("sub_00168230();", old)
        r = _run(old)
        self.assertEqual(r.returncode, 1, r.stdout)
        # The callee never ran and the stack was left where the thunk found it.
        self.assertTrue(r.stdout.startswith("calls=0 "), r.stdout)
        self.assertTrue(r.stdout.strip().endswith("esp=00000800"), r.stdout)


if __name__ == "__main__":
    unittest.main()
