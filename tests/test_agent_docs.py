"""Regression tests for the active document and recipe contract."""
from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location('check_agent_docs', SCRIPTS / 'check-agent-docs.py')
assert spec and spec.loader
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)
CHECKER = SCRIPTS / 'check-agent-docs.py'

AGENTS_BODY = """# Guide

Read `.omp/config.yml` for routing and `plan-jsrf-gameplay.md` for current work.

Build with `just build`; run the checkers with `just check`.

Run `C:\\Python313\\python.exe -X utf8 scripts\\check-agent-docs.py --check`.
"""
PLAN_BODY = """# Gameplay plan

Current work: verify the next playable milestone.
"""
CONFIG_BODY = """# OMP configuration
modelRoleStorage: project
"""


class CorpusMixin:
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / 'scripts').mkdir()
        (self.root / '.omp').mkdir()
        self.write('AGENTS.md', AGENTS_BODY)
        self.write('plan-jsrf-gameplay.md', PLAN_BODY)
        self.write('.omp/config.yml', CONFIG_BODY)
        self.write('scripts/check-agent-docs.py', '# fixture')
        self.write('justfile', self.justfile_body())

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, relative: str, text: str):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')

    def justfile_body(self):
        lines = ['python := "python"', 'default:', '    @just --list']
        for recipe in checker.REQUIRED_RECIPES:
            lines += [f'{recipe}:', '    @echo ok']
        return '\n'.join(lines) + '\n'

    def audit(self):
        old = checker.ROOT
        checker.ROOT = self.root
        try:
            return checker.run_checks()
        finally:
            checker.ROOT = old

    def reasons(self, findings):
        return {finding['reason'] for finding in findings}


class DocumentChecks(CorpusMixin, unittest.TestCase):
    def test_clean_corpus(self):
        self.assertEqual(self.audit(), [])

    def test_missing_gameplay_plan_is_reported(self):
        (self.root / 'plan-jsrf-gameplay.md').unlink()
        self.assertIn('stale_authority_link', self.reasons(self.audit()))

    def test_missing_omp_config_is_reported_as_stale_authority(self):
        (self.root / '.omp/config.yml').unlink()
        self.assertIn('stale_authority_link', self.reasons(self.audit()))

    def test_broken_command_path_is_rejected(self):
        self.write('AGENTS.md', AGENTS_BODY + '\nRun `scripts/nope.py`.\n')
        self.assertIn('broken_command_path', self.reasons(self.audit()))

    def test_planned_command_path_is_allowed(self):
        self.write('plan-jsrf-gameplay.md', PLAN_BODY + '\n`scripts/future.py` (planned).\n')
        self.assertNotIn('broken_command_path', self.reasons(self.audit()))

    def test_agents_budget_has_headroom(self):
        self.write('AGENTS.md', AGENTS_BODY + 'x' * (65536 - len(AGENTS_BODY.encode()) - 100))
        self.assertIn('insufficient_headroom', self.reasons(self.audit()))


class RecipeChecks(CorpusMixin, unittest.TestCase):
    def test_missing_recipe_is_rejected(self):
        self.write('justfile', 'default:\n    @just --list\n')
        findings = self.audit()
        self.assertEqual(len([f for f in findings if f['reason'] == 'missing_recipe']),
                         len(checker.REQUIRED_RECIPES))

class CliChecks(CorpusMixin, unittest.TestCase):
    def test_json_cli_shape(self):
        result = subprocess.run([sys.executable, '-X', 'utf8', str(CHECKER), '--check', '--json', '--root', str(self.root)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload['findings'], [])
        self.assertIn('agents_bytes', payload)


if __name__ == '__main__':
    unittest.main(verbosity=2)
