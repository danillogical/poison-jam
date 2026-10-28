"""Cross-check of the vertex-program input-mask rule between its two implementations.

The rule lives in C once (`nv2a_vsh_ins_input_mask`, toolkit `src/video/nv2a_vsh_hlsl.h`, used by
the interpreter's `nv2a_vsh_program_info` and by the host tool). `scripts/find-vertex-programs.py`
scans the XBE for thousands of candidate runs, so it cannot spawn the tool per candidate and keeps a
Python copy; this test pins that copy to the tool. For each synthetic program the tool is run with a
deliberately wrong `--mask`, which makes it print the mask the instructions imply, and that mask must
equal the script's; the script's own mask must then be accepted.

Usage:  python -X utf8 tests/test_vsh_mask_rule.py <path to nv2a_vsh_hlsl[.exe]>
Exits 77 (CTest skip) when no tool is given or found.
"""
from __future__ import annotations

import importlib.util
import os
import random
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = None


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


fvp = load(ROOT / 'scripts' / 'find-vertex-programs.py', 'find_vertex_programs')
IMPLIED = re.compile(r'imply \(([0-9A-F]{4})\)')


def programs(count=300):
    rng = random.Random(0x4D41534B)
    for _ in range(count):
        n = rng.randint(1, 12)
        words = []
        for i in range(n):
            ins = [0, rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32) & ~1]
            if i == n - 1 or rng.random() < 0.1:       # an early FINAL ends the scan
                ins[3] |= 1
            words.append(tuple(ins))
        yield words


class MaskRule(unittest.TestCase):
    def run_tool(self, binp, mask):
        return subprocess.run([TOOL, '--bin', binp, '--mask', f'0x{mask:X}'], capture_output=True,
                              text=True)

    def test_python_mask_equals_tool_mask(self):
        seen = set()
        with tempfile.TemporaryDirectory() as tmp:
            binp = os.path.join(tmp, 'p.bin')
            for words in programs():
                with open(binp, 'wb') as f:
                    f.write(b''.join(struct.pack('<4I', *ins) for ins in words))
                want = fvp.program_info(words)['input_mask']
                seen.add(want)
                wrong = self.run_tool(binp, want ^ 1)
                m = IMPLIED.search(wrong.stderr)
                self.assertEqual(wrong.returncode, 3, wrong.stderr)
                self.assertIsNotNone(m, wrong.stderr)
                self.assertEqual(int(m.group(1), 16), want, f'tool and script disagree on {words}')
                right = self.run_tool(binp, want)
                self.assertNotIn('imply', right.stderr)
        # A rule that never produced a nonzero or varied mask would make the loop vacuous.
        self.assertGreater(len(seen), 50)


if __name__ == '__main__':
    TOOL = sys.argv.pop(1) if len(sys.argv) > 1 else os.environ.get('JSRF_NV2A_VSH_HLSL')
    if not TOOL or not Path(TOOL).is_file():
        print('SKIP: no nv2a_vsh_hlsl tool given (argv[1] or JSRF_NV2A_VSH_HLSL)')
        sys.exit(77)
    unittest.main()
