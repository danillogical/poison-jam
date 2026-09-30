"""Controls for W8's route allow-list coverage check.

Plan W8's measured failure: "on 09-25 the owner-authorised fallback was missing from
the allow-list and needed a new session". A fallback that is policy but not allowed
cannot be selected at the moment it is needed -- and that moment is a senior-route
outage, the worst time to discover it.

**What the check can and cannot do.** It compares two texts: the roster in
`docs/agent-workflow.md` §1 and the allow-list in the active DSH profile patch. It
does NOT resolve routes live -- W8's own point is "verify a route at first real use",
and a static check that claimed to resolve would substitute a text match for a
capability probe. These controls exercise the comparison, not a resolution.

**Fallbacks are never required.** W8 makes naming them an owner decision, so the
check reports that the roster names none and passes. A control asserts that, because
a check that failed until the owner made a decision would be red from its first run.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_route_allowlist', ROOT / 'scripts' / 'check-route-allowlist.py')
assert _spec and _spec.loader
routes = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(routes)


ROSTER = """# Workflow

## 1. Supported harnesses and roster

| Role | Codex | DeepSeek Harness (DSH) |
|---|---|---|
| **Session** | `gpt-6.1-luna` @ `max` | `workbuddy-ai/deepseek-v4.1-flash` @ `max` |
| **Acceptance reviewer** | `gpt-6.1-luna` @ `max` | **GPT-6.1 Sol** @ `high` (`provider: codex`, `route: LIVE_RESOLVE`) |

### Live verification

Not part of the roster.
"""

PATCH = """- id: subagent-model-selection-settings
  config:
    enabled: true
    allowedModels:
      - provider: workbuddy-ai
        model: deepseek-v4.1-flash
      - provider: codex
        model: gpt-6.1-sol
"""


class RosterParsingTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'docs').mkdir(parents=True)
        self._saved = (routes.ROOT, routes.WORKFLOW)
        routes.ROOT = self.root
        routes.WORKFLOW = self.root / 'docs' / 'agent-workflow.md'

    def tearDown(self) -> None:
        routes.ROOT, routes.WORKFLOW = self._saved
        self._tmp.cleanup()

    def write_roster(self, body: str) -> None:
        routes.WORKFLOW.write_text(body, encoding='utf-8')

    def test_a_full_provider_model_span_is_read(self) -> None:
        self.write_roster(ROSTER)
        found, _ = routes.roster_routes()
        self.assertIn(('workbuddy-ai', 'deepseek-v4.1-flash'), found)

    def test_the_acceptance_row_yields_the_model_not_the_role(self) -> None:
        """The regression: the first bold span is the ROLE, the second the MODEL.

        Reading the first produced `codex/acceptance-reviewer`, which would have
        demanded a nonexistent route be added to the allow-list.
        """
        self.write_roster(ROSTER)
        found, _ = routes.roster_routes()
        self.assertIn(('codex', 'gpt-6.1-sol'), found)
        self.assertNotIn(('codex', 'acceptance-reviewer'), found)

    def test_only_the_roster_section_is_read(self) -> None:
        """A route named outside §1 is not a staffing policy."""
        self.write_roster(ROSTER + '\n`codex/not-a-role` appears in prose.\n')
        found, _ = routes.roster_routes()
        self.assertNotIn(('codex', 'not-a-role'), found)


class AllowlistParsingTests(unittest.TestCase):
    def test_entries_are_read(self) -> None:
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'cordis.patch.yml'
            path.write_text(PATCH, encoding='utf-8')
            saved = routes.PROFILE_PATCHES
            routes.PROFILE_PATCHES = (path,)
            try:
                found, searched = routes.allowlist_routes()
            finally:
                routes.PROFILE_PATCHES = saved
        self.assertIn(('codex', 'gpt-6.1-sol'), found)
        self.assertIn(('workbuddy-ai', 'deepseek-v4.1-flash'), found)
        self.assertEqual(len(searched), 1)


class CoverageTests(unittest.TestCase):
    """The comparison itself, on the real repository and profile."""

    def test_the_real_roster_is_covered(self) -> None:
        """Every roster route must be in the active allow-list."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-route-allowlist.py')],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('no findings', result.stdout)

    def test_the_allowlist_is_not_empty(self) -> None:
        """A check against an empty allow-list would fail everything, or nothing."""
        allowed, _ = routes.allowlist_routes()
        self.assertGreater(len(allowed), 5)

    def test_no_fallback_is_required(self) -> None:
        """W8 makes naming fallbacks an owner decision; the check must not demand it.

        A check that failed until the owner made a decision would be red from its
        first run and ignored -- the failure mode this project names repeatedly.
        """
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-route-allowlist.py'), '--json'],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        import json
        record = json.loads(result.stdout)
        self.assertEqual(record['fallbacks_named'], [])
        self.assertIn('OWNER decision', record['fallback_note'])

    def test_the_workflow_states_that_no_fallback_is_authorised(self) -> None:
        """The policy must say so, rather than being silent.

        A silent roster lets a session assume a fallback exists, which is the
        measured failure: an authorised fallback that was missing from the
        allow-list and needed a new session.
        """
        text = (ROOT / 'docs' / 'agent-workflow.md').read_text(encoding='utf-8')
        self.assertIn('Fallback routes: NONE AUTHORISED', text)
        self.assertIn('owner-reserved', text.casefold())


if __name__ == '__main__':
    unittest.main(verbosity=2)
