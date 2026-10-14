"""Fixtures for `scripts/check-vsh-aot-table.py` (plan step V5, orchestrator-owned).

The checker is the independent read of the generated corpus: it re-derives each
entry's identity digest from the words stored beside it, rebuilds the shader from
those same words and compares bytes, and re-scans the XBE for coverage. A checker
that silently accepts a corrupt table would make every V5 claim worthless, so its
failure modes are pinned here on synthetic tables -- no XBE, no dxc, no Vulkan.
"""
from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
CHECKER = SCRIPTS / 'check-vsh-aot-table.py'

_spec = importlib.util.spec_from_file_location('check_vsh_aot_table', CHECKER)
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)

SEED = 0xcbf29ce484222325
PRIME = 0x100000001b3
SPIRV_HEAD = [0x07230203, 0x00010000, 0x00000000, 0x00000001]
W_A = [(0x00000000, 0x00000000, 0x00000000, 0x00000000),
       (0x12345678, 0x9ABCDEF0, 0x11223344, 0x00000001)]
W_B = [(0x00000000, 0x00000000, 0x00000000, 0x00000000),
       (0xDEADBEEF, 0xCAFEBABE, 0x0BADF00D, 0x00000001)]
H_A = 0xA53DBB36      # the FNV-1a 32 of W_A, as the back end computes it
H_B = 0x5F9063D2      # ... of W_B


def hash_words(words):
    """The 32-bit FNV-1a the back end searches on; the fixtures must use real ones."""
    h = 2166136261
    for ins in words:
        for w in ins:
            h = ((h ^ w) * 16777619) & 0xFFFFFFFF
    return h


def digest(words):
    h = SEED
    for ins in words:
        for w in ins:
            h = ((h ^ w) * PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def emit(entries, *, ascending=True, bad_digest=False, size_delta=0, count=None, duplicate=False):
    """A table in exactly the shape `scripts/gen-shaders.py` renders."""
    out = []
    for h, length, _mask, words, spirv in entries:
        tag = f'{h:08x}'
        body = ''.join('    { ' + ', '.join(f'0x{w:08x}u' for w in ins) + ' },\n' for ins in words)
        out.append(f'static const uint32_t vp_words_{tag}[{length}][4] = {{\n{body}}};\n')
        spv = '    ' + ' '.join(f'0x{w:08x}u,' for w in spirv) + '\n'
        out.append(f'static const uint32_t vp_spirv_{tag}[] = {{\n{spv}}};\n')
    out.append('const Nv2aVshAotEntry nv2a_vsh_programs[] = {\n')
    order = entries if ascending else list(reversed(entries))
    if duplicate:
        order = order + order[:1]
    for h, length, mask, words, spirv in order:
        tag = f'{h:08x}'
        d = digest(words) ^ (1 if bad_digest else 0)
        size = f'sizeof vp_spirv_{tag}' if not size_delta else str(len(spirv) * 4 + size_delta)
        out.append(f'    {{ 0x{h:08x}u, {length}u, 0x{mask:08x}u, 0x{d:016x}ull, '
                   f'vp_words_{tag}, vp_spirv_{tag}, {size} }},\n')
    out.append('};\n')
    n = len(entries) if count is None else count
    out.append(f'const size_t nv2a_vsh_program_count = {n};\n')
    return ''.join(out)


class TableCheckerTests(unittest.TestCase):
    """Each case is a table defect the checker must name, not merely reject."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory(prefix='vsh-aot-table-')
        self.tmp = Path(self._tmp.name)
        self.entries = [(H_B, 2, 0x0001, W_B, SPIRV_HEAD),
                        (H_A, 2, 0x0003, W_A, SPIRV_HEAD)]

    def tearDown(self):
        self._tmp.cleanup()

    def check(self, text, *extra):
        path = self.tmp / 'table.c'
        path.write_text(text, encoding='utf-8')
        return subprocess.run(
            [sys.executable, str(CHECKER), '--c', str(path), '--skip-spirv',
             '--skip-coverage', *extra],
            capture_output=True, text=True)

    def test_sound_table_passes(self):
        r = self.check(emit(self.entries))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('entries=2', r.stdout)
        self.assertIn('problems=0', r.stdout)

    def test_unsorted_table_is_rejected(self):
        """The back end binary-searches, so order is load-bearing."""
        r = self.check(emit(self.entries, ascending=False))
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('not ascending', r.stdout)

    def test_wrong_digest_is_rejected_and_named(self):
        """The digest is the check that a hash collision cannot select a shader."""
        r = self.check(emit(self.entries, bad_digest=True))
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(f'{H_A:08X}: digest is', r.stdout)
        self.assertIn('recomputed', r.stdout)

    def test_wrong_size_field_is_rejected(self):
        r = self.check(emit(self.entries, size_delta=4))
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('spirv_size is 20, the array is 16 bytes', r.stdout)

    def test_wrong_count_is_rejected(self):
        r = self.check(emit(self.entries, count=3))
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('nv2a_vsh_program_count is 3, but the table has 2 rows', r.stdout)

    def test_duplicate_hash_is_rejected(self):
        r = self.check(emit(self.entries, duplicate=True))
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(f'duplicate hash(es): {H_B:08X}', r.stdout)

    def test_missing_table_is_rejected(self):
        r = self.check('const int nothing_relevant = 1;\n')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('nv2a_vsh_programs[] was not found', r.stdout)

    def test_wrong_hash_for_the_words_is_rejected(self):
        """The reviewer's finding: a row whose hash column is kept while its arrays are
        swapped must fail. The 32-bit hash is what the back end searches on, so a row
        that points it at another program is a corrupted identity."""
        swapped = emit([(H_A, 2, 0x0001, W_B, SPIRV_HEAD),
                        (H_B, 2, 0x0003, W_A, SPIRV_HEAD)])
        r = self.check(swapped)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('but its own words hash to', r.stdout)

    def test_parser_reads_every_field_of_the_generator_shape(self):
        """Guards against the checker silently matching nothing and reporting success."""
        parsed, problems = checker.parse_table(emit(self.entries))
        self.assertEqual(problems, [])
        entries, words_arrays, spirv_arrays = parsed
        self.assertEqual(len(entries), 2)
        self.assertEqual(entries[0]['hash'], H_B)
        self.assertEqual(entries[0]['len'], 2)
        self.assertEqual(entries[0]['mask'], 0x0001)
        self.assertEqual(entries[0]['digest'], digest(W_B))
        self.assertEqual(checker.parse_words(words_arrays[f'{H_B:08X}'], f'{H_B:08X}'), W_B)
        self.assertEqual(checker.parse_dwords(spirv_arrays[f'{H_A:08X}']), SPIRV_HEAD)


if __name__ == '__main__':
    unittest.main()
