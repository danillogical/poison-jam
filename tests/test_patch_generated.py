"""Controls for `scripts/patch-generated.py` (plan T18).

T18's acceptance: "a regeneration + patch run is idempotent; a missing site fails
the run". Each case builds a scratch generated directory and ledger, so nothing in
the real tree is touched, and every refusal is paired with a case that must pass.
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts' / 'patch-generated.py'

CHUNK = '''#include "recomp_types.h"

void sub_00011000(void)
{
    eax = MEM32(ecx + 0x2C);
    return;
}

void sub_00011040(void)
{
    eax = MEM32(ecx + 0x2C);
    edx = 1;
    return;
}
'''

LEDGER = '''| ID | Path | Class |
|---|---|---|
| L02 | Reviewed recovered bodies | Patched |
'''


class Scratch:
    def __enter__(self) -> 'Scratch':
        self.root = Path(tempfile.mkdtemp(prefix='jsrf-patchgen-'))
        self.gen = self.root / 'gen'
        self.gen.mkdir()
        (self.gen / 'recomp_0000.c').write_text(CHUNK, encoding='utf-8')
        self.ledger = self.root / 'ledger.md'
        self.ledger.write_text(LEDGER, encoding='utf-8')
        self.manifest = self.root / 'patches.json'
        return self

    def __exit__(self, *_exc) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def patches(self, *patches: dict) -> None:
        self.manifest.write_text(json.dumps({'schema': 1, 'patches': list(patches)}),
                                 encoding='utf-8')

    def run(self, *extra: str) -> subprocess.CompletedProcess:
        return subprocess.run(
            [sys.executable, '-X', 'utf8', str(SCRIPT), '--manifest', str(self.manifest),
             '--gen-dir', str(self.gen), '--ledger', str(self.ledger), *extra],
            capture_output=True, text=True)

    def chunk(self) -> str:
        return (self.gen / 'recomp_0000.c').read_text(encoding='utf-8')


def patch(**fields) -> dict:
    base = {'id': 'P1', 'ledger': 'L02', 'reason': 'test',
            'function': '0x00011040', 'before': 'edx = 1;', 'after': 'edx = 2;'}
    base.update(fields)
    return base


class PatchGeneratedTests(unittest.TestCase):
    def test_applies_once_and_a_second_run_changes_nothing(self) -> None:
        with Scratch() as s:
            s.patches(patch())
            first = s.run()
            self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
            self.assertIn('1 applied', first.stdout)
            after_first = s.chunk()
            self.assertIn('edx = 2;', after_first)
            second = s.run()
            self.assertEqual(second.returncode, 0, second.stdout + second.stderr)
            self.assertIn('0 applied, 1 already applied', second.stdout)
            self.assertEqual(s.chunk(), after_first)

    def test_an_insertion_is_idempotent(self) -> None:
        """An "after" that keeps "before" as its anchor is applied once, then left alone."""
        with Scratch() as s:
            s.patches(patch(before='edx = 1;', after='ebx = 9;\n    edx = 1;'))
            first = s.run()
            self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
            once = s.chunk()
            self.assertEqual(once.count('ebx = 9;'), 1)
            second = s.run()
            self.assertEqual(second.returncode, 0, second.stdout + second.stderr)
            self.assertIn('1 already applied', second.stdout)
            self.assertEqual(s.chunk(), once)

    def test_function_scope_keeps_a_patch_out_of_other_functions(self) -> None:
        """The same text in sub_00011000 is outside the patch's scope."""
        with Scratch() as s:
            s.patches(patch(before='eax = MEM32(ecx + 0x2C);',
                            after='eax = MEM32(ecx + 0x30);'))
            result = s.run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            text = s.chunk()
            self.assertEqual(text.count('MEM32(ecx + 0x2C)'), 1)
            self.assertEqual(text.count('MEM32(ecx + 0x30)'), 1)
            self.assertLess(text.index('0x2C'), text.index('sub_00011040'))

    def test_a_missing_site_fails_and_writes_nothing(self) -> None:
        with Scratch() as s:
            s.patches(patch(id='OK'), patch(id='GONE', before='edx = 7;', after='edx = 8;'))
            result = s.run()
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('GONE', result.stdout)
            self.assertIn('no file was written', result.stdout)
            self.assertEqual(s.chunk(), CHUNK)

    def test_an_ambiguous_site_fails(self) -> None:
        with Scratch() as s:
            whole_file = patch(file='recomp_0000.c', before='eax = MEM32(ecx + 0x2C);',
                               after='eax = 0;')
            del whole_file['function']
            s.patches(whole_file)
            result = s.run()
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('found 2 time(s)', result.stdout)

    def test_an_unknown_ledger_id_is_refused(self) -> None:
        with Scratch() as s:
            s.patches(patch(ledger='L99'))
            result = s.run()
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn('L99', result.stderr)

    def test_a_missing_function_is_refused(self) -> None:
        with Scratch() as s:
            s.patches(patch(function='0x00012000'))
            result = s.run()
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('defined 0 times', result.stdout)

    def test_check_mode_reports_without_writing(self) -> None:
        with Scratch() as s:
            s.patches(patch())
            pending = s.run('--check')
            self.assertEqual(pending.returncode, 1, pending.stdout + pending.stderr)
            self.assertIn('not applied', pending.stdout)
            self.assertEqual(s.chunk(), CHUNK)
            s.run()
            done = s.run('--check')
            self.assertEqual(done.returncode, 0, done.stdout + done.stderr)

    def test_the_real_manifest_is_valid(self) -> None:
        sys.path.insert(0, str(ROOT / 'scripts'))
        import importlib.util
        spec = importlib.util.spec_from_file_location('patch_generated', SCRIPT)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.load_manifest(module.MANIFEST, module.ledger_ids(module.LEDGER))


if __name__ == '__main__':
    unittest.main(verbosity=2)
