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
| **Persistent advisor** | `workbuddy-ai/deepseek-v4.1-flash` |

Retired: `gpt-5.6-sol`, `hy3`, `glm-5.3` are historical names.

The plan is `plan-jsrf-bare-minimum.md`.
"""

AGENTS_BODY = """# Guide

Read `docs/agent-workflow.md` for the roster.

Read `plan-jsrf-bare-minimum.md` for acceptance and the current blocker.

Build with `just build`; run the checkers with `just check`.

Run `C:\\Python313\\python.exe -X utf8 scripts\\check-agent-docs.py --check`.
"""

PLAN_BODY = """# Plan

**Status:** P0.3 in progress. P0.4 pending.

**Next action:** finish P0.3.
"""

TEMPLATE_BODY = """# Startup receipt

Read `docs/agent-workflow.md` at session start.
"""


def python_call(recipe: str) -> str:
    """A plausible body line for a fixture recipe; never executed."""
    return f'python -X utf8 scripts/{recipe}.py'


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
        self.write('docs/session-start-template.md', TEMPLATE_BODY)
        self.write('scripts/check-agent-docs.py', '# placeholder\n')
        # The justfile the T6 recipe check audits.  Built from the checker's own
        # required list so the fixture cannot drift from the contract it stands in
        # for -- a hand-written fixture would silently stop covering a recipe the
        # checker starts requiring.
        self.write('justfile', self.justfile_body())

    @staticmethod
    def justfile_body() -> str:
        lines = ['# fixture justfile', 'python := "python"', 'default:', '    @just --list']
        for recipe in checker.REQUIRED_RECIPES:
            lines.append(f'{recipe}:')
            lines.append(f'    {python_call(recipe)}')
        return '\n'.join(lines) + '\n'

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
        # Uses a route that is NOT retired, so this test is about the stray-name
        # rule and not about the retired-name rule.  Measured: it previously used
        # `kimi-k3`, and when W15 added that name to RETIRED_NAMES this test
        # started passing for the wrong reason -- it would have kept passing even
        # if the stray-name check were deleted.
        self.write('AGENTS.md',
                   AGENTS_BODY + '\nThe advisor is `workbuddy-ai/grok-4.7`.\n')
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

    def test_retired_name_without_a_catalog_marker_is_still_flagged(self):
        """The catalog exemption must not become a blanket exemption.

        A control for the check above: the SAME retired names in a sentence that
        is not a statement about what a provider serves must still be flagged.
        Without this, deleting the retired-name check entirely would leave the
        catalog test passing.
        """
        self.write('docs/agent-workflow.md', WORKFLOW_BODY + (
            '\n- The advisor is `workbuddy-ai/kimi-k3`.\n'))
        findings = self.audit()
        self.assertIn('unlabelled_retired_name', self.reasons(findings))


class JustRecipeTests(CorpusMixin, unittest.TestCase):
    """Plan T6 fixes the recipe names; the checker must fail when one goes."""

    def test_clean_corpus_defines_every_recipe(self):
        self.assertNotIn('missing_recipe', self.reasons(self.audit()))

    def test_missing_recipe_is_rejected(self):
        self.write('justfile', '# fixture justfile\ndefault:\n    @just --list\n')
        findings = self.audit()
        self.assertIn('missing_recipe', self.reasons(findings))
        # Every required recipe should be reported, not just the first.
        reported = [f for f in findings if f['reason'] == 'missing_recipe']
        self.assertEqual(len(reported), len(checker.REQUIRED_RECIPES))

    def test_parameterised_recipe_counts_as_defined(self):
        """`strict-run label="x":` is a definition, not a missing recipe.

        Measured: the first version of the checker's name pattern required the
        colon immediately after the name and reported eight recipes missing from
        a justfile that defined all of them.
        """
        body = self.justfile_body().replace(
            'strict-run:\n    python -X utf8 scripts/strict-run.py',
            'strict-run label="strict":\n    python -X utf8 scripts/strict-run.py')
        self.write('justfile', body)
        self.assertNotIn('missing_recipe', self.reasons(self.audit()))

    def test_assignment_is_not_mistaken_for_a_recipe(self):
        self.write('justfile', self.justfile_body() + '\nnot_a_recipe := "value"\n')
        self.assertNotIn('missing_recipe', self.reasons(self.audit()))

    def test_agents_must_point_at_the_recipes(self):
        self.write('AGENTS.md', AGENTS_BODY.replace('`just build`', '`the build`'))
        self.assertIn('docs_do_not_point_at_recipes', self.reasons(self.audit()))

    def test_absent_justfile_is_reported_not_crashed(self):
        (self.root / 'justfile').unlink()
        findings = self.audit()
        self.assertIn('missing_input', self.reasons(findings))


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

    def test_planned_script_path_is_not_flagged(self):
        """A plan may name a tool it has not written yet.

        Measured need: adopting `plan-jsrf-bare-minimum.md` (2026-09-29) named
        four Phase-1 deliverables (`scripts/logq.py`, `scripts/cite.py`, …) that
        are planned work, not commands.  Treating a deliverable as a broken
        instruction would make adopting a plan that names its own future tools
        impossible without writing them first.
        """
        self.write('plan-jsrf-bare-minimum.md',
                   PLAN_BODY + '\n| **T8** | `scripts/logq.py` (planned) loads lines | ok |\n')
        findings = self.audit()
        self.assertNotIn('broken_command_path', self.reasons(findings))

    def test_planned_marker_does_not_excuse_an_unmarked_mention(self):
        """The marker is per-line: the same path elsewhere still fails."""
        self.write('plan-jsrf-bare-minimum.md',
                   PLAN_BODY + '\n| **T8** | `scripts/logq.py` (planned) loads lines | ok |\n'
                   '\nRun `scripts/logq.py` to check the counts.\n')
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

        Measured blind spot: AGENTS.md said "P0.1 awaits advisor adjudication …
        do not begin P0.2 yet" while the plan recorded P0.1 accepted, and the
        checker returned no findings. A stale
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
    def test_plan_without_a_status_line_still_detects_contradiction(self):
        """Regression: the condensed plan records acceptance in a section, with
        no `**Status:**` line. The check must still read it."""
        self.write('plan-jsrf-bare-minimum.md',
                   '# Plan\n\n## CURRENT PACKET — none\n\n## P0 — ACCEPTED\n\n'
                   'P0.1 accepted with the rest of P0.\n')
        self.write('AGENTS.md', AGENTS_BODY
                   + '\nP0.1 awaits advisor adjudication; do not begin P0.2 yet.\n')
        findings = self.audit()
        self.assertIn('agents_status_disagreement', self.reasons(findings))
        self.assertNotIn('unreadable_status', self.reasons(findings))

    def test_missing_plan_is_rejected(self):
        (self.root / 'plan-jsrf-bare-minimum.md').unlink()
        findings = self.audit()
        self.assertIn('missing_input', self.reasons(findings))

    def test_no_session_report_is_required(self):
        """Session reports are not kept in the repository; none is needed."""
        self.assertFalse((self.root / 'report-deepseek.md').exists())
        self.assertEqual(self.audit(), [])


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
