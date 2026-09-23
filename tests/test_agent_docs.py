"""P0.3 fixtures: the document audit checker.

Every negative control is an **injected** stale document in a temporary corpus, and
each must fail for its own stated reason.  The checker passing the real documents
proves nothing on its own -- a checker that always returns "clean" would do that
too -- so the negative controls carry the weight here.
"""
from __future__ import annotations

import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

_spec = importlib.util.spec_from_file_location(
    'check_agent_docs', SCRIPTS / 'check-agent-docs.py')
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)

CHECKER = SCRIPTS / 'check-agent-docs.py'

WORKFLOW_BODY = """# Workflow

| Role | DSH |
|---|---|
| **Persistent advisor** | `workbuddy-ai/kimi-k3` |

Retired: `gpt-5.6-sol`, `hy3`, `glm-5.3` are historical names.

The plan is `plan-jsrf-bare-minimum.md`.
"""

AGENTS_BODY = """# Guide

Read `docs/agent-workflow.md` for the roster.

Read `plan-jsrf-bare-minimum.md` for acceptance.

Read `report-deepseek.md` for the current blocker.

Run `C:\\Python313\\python.exe -X utf8 scripts\\check-agent-docs.py --check`.
"""

PLAN_BODY = """# Plan

**Status:** P0.3 in progress. P0.4 pending.

**Next action:** finish P0.3.
"""

REPORT_BODY = """# Report

## CURRENT STATE

Last updated today. P0.3 is the active packet; P0.4 is next.

**Next action:** finish P0.3.

## Historical session narrative

Old material.
"""

TEMPLATE_BODY = """# Startup receipt

Read `docs/agent-workflow.md` at session start.
"""


class CorpusMixin:
    """Build a synthetic document corpus that the checker can audit."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'docs').mkdir(parents=True)
        (self.root / 'scripts').mkdir(parents=True)
        self.write('docs/agent-workflow.md', WORKFLOW_BODY)
        self.write('AGENTS.md', AGENTS_BODY)
        self.write('plan-jsrf-bare-minimum.md', PLAN_BODY)
        self.write('report-deepseek.md', REPORT_BODY)
        self.write('docs/session-start-template.md', TEMPLATE_BODY)
        self.write('scripts/check-agent-docs.py', '# placeholder\n')

    def tearDown(self):
        self._tmp.cleanup()

    def write(self, relative: str, body: str):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(body, encoding='utf-8')

    def audit(self) -> list[dict]:
        previous = checker.ROOT
        checker.ROOT = self.root
        try:
            return checker.run_checks()
        finally:
            checker.ROOT = previous

    def reasons(self, findings: list[dict]) -> set[str]:
        return {finding['reason'] for finding in findings}


class PositiveControlTests(CorpusMixin, unittest.TestCase):
    def test_clean_corpus_has_no_findings(self):
        self.assertEqual(self.audit(), [])

    def test_real_repository_is_clean(self):
        """The actual documents must pass, and AGENTS.md must be under budget."""
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--check', '--json'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload['findings'], [])
        self.assertLess(payload['agents_bytes'], payload['agents_budget'])

    def test_real_agents_md_reports_measured_bytes(self):
        size = len((ROOT / 'AGENTS.md').read_bytes())
        self.assertLess(size, 65536)
        self.assertGreater(size, 1000)


class BudgetTests(CorpusMixin, unittest.TestCase):
    def test_over_budget_agents_md_is_rejected(self):
        self.write('AGENTS.md', AGENTS_BODY + ('x' * 70000))
        findings = self.audit()
        self.assertIn('over_budget', self.reasons(findings))

    def test_insufficient_headroom_is_rejected(self):
        self.write('AGENTS.md', AGENTS_BODY + ('x' * (65536 - len(AGENTS_BODY) - 100)))
        findings = self.audit()
        self.assertIn('insufficient_headroom', self.reasons(findings))

    def test_comfortable_size_is_accepted(self):
        self.assertNotIn('over_budget', self.reasons(self.audit()))
        self.assertNotIn('insufficient_headroom', self.reasons(self.audit()))


class RosterDuplicationTests(CorpusMixin, unittest.TestCase):
    def test_duplicated_roster_table_in_agents_is_rejected(self):
        self.write('AGENTS.md', AGENTS_BODY + """
| Role | DSH |
|---|---|
| **Session** | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Acceptance reviewer** | `workbuddy-ai/hy4-preview-f` @ `high` |
""")
        findings = self.audit()
        self.assertIn('duplicated_roster', self.reasons(findings))

    def test_stray_route_name_without_history_marker_is_rejected(self):
        self.write('AGENTS.md', AGENTS_BODY + '\nThe advisor is `workbuddy-ai/kimi-k3`.\n')
        findings = self.audit()
        self.assertIn('stray_route_name', self.reasons(findings))

    def test_pointer_without_route_names_is_accepted(self):
        self.assertEqual(self.audit(), [])

    def test_workflow_must_actually_carry_a_route(self):
        self.write('docs/agent-workflow.md', '# Workflow\n\nNo routes here.\n')
        findings = self.audit()
        self.assertIn('owner_has_no_roster', self.reasons(findings))


class RetiredNameTests(CorpusMixin, unittest.TestCase):
    def test_unlabelled_retired_name_is_rejected(self):
        self.write('plan-jsrf-bare-minimum.md',
                   PLAN_BODY + '\nUse gpt-5.6-sol for this packet.\n')
        findings = self.audit()
        self.assertIn('unlabelled_retired_name', self.reasons(findings))

    def test_labelled_retired_name_is_accepted(self):
        self.write('plan-jsrf-bare-minimum.md',
                   PLAN_BODY + '\nThese are historical: gpt-5.6-sol and hy3.\n')
        findings = self.audit()
        self.assertNotIn('unlabelled_retired_name', self.reasons(findings))

    def test_catalog_statement_about_a_provider_is_not_flagged(self):
        """A measured catalog fact is not an instruction to use the route.

        This was a real false positive: the workflow records that
        `workbuddy-ai` advertises `gpt-5.5`, which is a fact about the provider,
        not a mandate.  Flagging it would train readers to ignore the check.
        """
        self.write('docs/agent-workflow.md', WORKFLOW_BODY + (
            '\n- **`workbuddy-ai` does not serve `gpt-6-astra`** (it advertises only\n'
            '  `hy4-preview-f`, `deepseek-v4.1-flash`, `gpt-5.5` and `kimi-k3`).\n'))
        findings = self.audit()
        self.assertNotIn('unlabelled_retired_name', self.reasons(findings))


class CommandPathTests(CorpusMixin, unittest.TestCase):
    def test_broken_script_path_is_rejected(self):
        self.write('AGENTS.md', AGENTS_BODY + '\nRun `scripts/does-not-exist.py`.\n')
        findings = self.audit()
        self.assertIn('broken_command_path', self.reasons(findings))

    def test_existing_script_path_is_accepted(self):
        self.write('scripts/real-helper.py', '# real\n')
        self.write('AGENTS.md', AGENTS_BODY + '\nRun `scripts/real-helper.py`.\n')
        findings = self.audit()
        self.assertNotIn('broken_command_path', self.reasons(findings))

    def test_known_absent_path_is_not_flagged(self):
        """The guide documents `build-jsrf.ps1` as absent on purpose."""
        self.write('AGENTS.md', AGENTS_BODY + '\n`scripts/build-jsrf.ps1` is not in the repo.\n')
        findings = self.audit()
        self.assertNotIn('broken_command_path', self.reasons(findings))

    def test_broken_test_path_is_rejected(self):
        self.write('docs/agent-workflow.md', WORKFLOW_BODY + '\nRun `tests/nope.py`.\n')
        findings = self.audit()
        self.assertIn('broken_command_path', self.reasons(findings))


class AuthorityLinkTests(CorpusMixin, unittest.TestCase):
    def test_missing_authority_link_is_rejected(self):
        self.write('AGENTS.md', '# Guide\n\nNo pointers at all.\n')
        findings = self.audit()
        self.assertIn('missing_authority_link', self.reasons(findings))

    def test_stale_authority_link_is_rejected(self):
        (self.root / 'docs' / 'agent-workflow.md').unlink()
        findings = self.audit()
        self.assertIn('stale_authority_link', self.reasons(findings))


    def test_agents_status_contradicting_the_plan_is_rejected(self):
        """AGENTS.md must not still treat an accepted packet as outstanding.

        Measured blind spot: the docstring named AGENTS.md but the code only
        compared PLAN against REPORT, so AGENTS.md said "P0.1 awaits advisor
        adjudication … do not begin P0.2 yet" while the plan and CURRENT STATE
        both recorded P0.1 accepted, and the checker returned no findings. A stale
        status in the one file every session loads automatically is the exact
        failure that file's own header warns about.
        """
        # The plan must record P0.1 as accepted for the contradiction to exist.
        self.write('plan-jsrf-bare-minimum.md',
                   '# Plan\n\n**Status:** P0.1 accepted. P0.3 in progress. P0.4 pending.\n\n'
                   '**Next action:** finish P0.3.\n')
        self.write('AGENTS.md', AGENTS_BODY
                   + '\nP0.1 awaits advisor adjudication; do not begin P0.2 yet.\n')
        findings = self.audit()
        self.assertIn('agents_status_disagreement', self.reasons(findings))

    def test_agents_historical_status_note_is_not_flagged(self):
        """A sentence marked historical is not a live status claim."""
        self.write('AGENTS.md', AGENTS_BODY
                   + '\nHistorically, P0.1 awaited advisor adjudication.\n')
        findings = self.audit()
        self.assertNotIn('agents_status_disagreement', self.reasons(findings))

    def test_agents_mentioning_a_packet_without_a_status_verb_is_fine(self):
        """Operating prose legitimately names packets; only status claims count."""
        self.write('AGENTS.md', AGENTS_BODY
                   + '\nSee the P0.4 section for the dump controls.\n')
        findings = self.audit()
        self.assertNotIn('agents_status_disagreement', self.reasons(findings))

    def test_agents_agreeing_with_the_plan_is_not_flagged(self):
        self.write('AGENTS.md', AGENTS_BODY + '\nP0.3 is accepted.\n')
        findings = self.audit()
        self.assertNotIn('agents_status_disagreement', self.reasons(findings))


class NextPacketTests(CorpusMixin, unittest.TestCase):
    def test_status_disagreement_is_rejected(self):
        self.write('report-deepseek.md', REPORT_BODY.replace('P0.3', 'P0.6'))
        self.write('plan-jsrf-bare-minimum.md',
                   PLAN_BODY.replace('P0.3', 'P0.4').replace('P0.4', 'P0.5'))
        findings = self.audit()
        self.assertIn('status_disagreement', self.reasons(findings))

    def test_missing_current_state_is_rejected(self):
        self.write('report-deepseek.md', '# Report\n\nNo state block.\n')
        findings = self.audit()
        self.assertIn('unreadable_status', self.reasons(findings))

    def test_large_current_state_block_is_still_found(self):
        """Regression: a fixed character window silently missed a grown block.

        The real CURRENT STATE passed 4,000 characters, and the check reported
        "no CURRENT STATE block" -- a false failure that would have been blamed
        on the documents rather than on the checker.
        """
        padding = '\n'.join(f'line {i} of narrative detail' for i in range(400))
        self.write('report-deepseek.md',
                   '# Report\n\n## CURRENT STATE\n\nP0.3 active. P0.4 next.\n\n'
                   + padding + '\n\n## Historical narrative\n\nOld.\n')
        findings = self.audit()
        self.assertNotIn('unreadable_status', self.reasons(findings))
        self.assertNotIn('status_disagreement', self.reasons(findings))


class CliTests(CorpusMixin, unittest.TestCase):
    def test_exit_codes_and_json_shape(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--check', '--json',
             '--root', str(self.root)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        payload = json.loads(result.stdout)
        for field in ('checker_version', 'agents_bytes', 'agents_budget', 'findings'):
            self.assertIn(field, payload)

    def test_dirty_corpus_exits_1(self):
        self.write('AGENTS.md', '# Guide\n\nNo pointers.\n')
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--check',
             '--root', str(self.root)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 1, result.stdout)

    def test_missing_corpus_exits_1_with_missing_input(self):
        empty = self.root / 'empty'
        empty.mkdir()
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--check', '--json',
             '--root', str(empty)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        payload = json.loads(result.stdout)
        self.assertIn('missing_input', {f['reason'] for f in payload['findings']})

    def test_checker_is_read_only(self):
        before = {p: p.read_bytes() for p in self.root.rglob('*.md')}
        subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--check', '--root', str(self.root)],
            capture_output=True, text=True)
        after = {p: p.read_bytes() for p in self.root.rglob('*.md')}
        self.assertEqual(before, after)


if __name__ == '__main__':
    unittest.main(verbosity=2)
