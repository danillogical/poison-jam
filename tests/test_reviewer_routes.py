"""Controls for the reviewer-route defaults (plan T11) and W15's retired names.

**T11's failure.** `scripts/record-review.py` defaulted `--requested-model` to
`workbuddy-ai/hy4-preview-f`, a **retired** route, while `docs/agent-workflow.md`
§1 assigned review to a different route. A stale default is worse than no default: it silently
attributes a review to a model that did not produce it, and the resulting record
reads as evidence about the wrong reviewer. The tool's own guard binds a record to
the route that *answered*, so a wrong default makes the record internally
inconsistent rather than obviously broken -- which is how it survived.

**W15's failure.** `check-agent-docs.py` carries `RETIRED_NAMES` so a retired
route cannot appear in an active-policy document without a historical marker. The
list was stale: it named `hy3` but not `hy4-preview-f`, so the very name T11 was
about would have passed the check.

These controls are cheap and do not need a reviewer child: the default is a
constant in one file, and the retired-name list is a constant in another.
"""
from __future__ import annotations

import importlib.util
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

# The checker's filename has a hyphen, so it is not importable by name; load it
# the same way tests/test_agent_docs.py does.
_spec = importlib.util.spec_from_file_location(
    'check_agent_docs', SCRIPTS / 'check-agent-docs.py')
assert _spec and _spec.loader
check_agent_docs = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(check_agent_docs)

WORKFLOW = ROOT / 'docs' / 'agent-workflow.md'


def workflow_reviewer_route() -> tuple[str, str]:
    """The Packet reviewer's provider and effort, read from §1's roster.

    Read from the roster table rather than hard-coded here, so this control
    follows a legitimate staffing change instead of freezing today's answer. The
    row's route cell has the shape
    ``claude/claude-opus-5-5`` @ ``high`` (Claude Opus 5.5; ...), each in
    single backticks.
    """
    text = WORKFLOW.read_text(encoding='utf-8')
    for line in text.splitlines():
        if not line.startswith('| **Packet reviewer**'):
            continue
        cell = line.strip().strip('|').split('|')[-1]
        route = re.search(r'`([\w.\-]+)/[\w.\-]+`\s*@\s*`([a-z]+)`', cell)
        if route:
            return route.group(1), route.group(2)
        raise AssertionError(
            f'the Packet reviewer row is present but its route cell does not carry '
            f'a `provider/model` @ `effort`: {cell.strip()!r}')
    raise AssertionError('the workflow §1 roster has no Packet reviewer row')


class RecordReviewDefaultTests(unittest.TestCase):
    def test_default_is_not_a_retired_route(self) -> None:
        """The regression T11 exists for: the default was hy4-preview-f."""
        source = (ROOT / 'scripts' / 'record-review.py').read_text(encoding='utf-8')
        default = re.search(r"--requested-model',\s*default='([^']+)'", source)
        self.assertIsNotNone(default, 'record-review.py has no --requested-model '
                                      'default to check')
        value = default.group(1)
        for retired in check_agent_docs.RETIRED_NAMES:
            self.assertNotIn(retired, value,
                             f'record-review.py defaults to the retired route '
                             f'{retired!r}')

    def test_default_matches_the_workflow_roster(self) -> None:
        """The default must be the route the workflow actually assigns."""
        provider, effort = workflow_reviewer_route()
        source = (ROOT / 'scripts' / 'record-review.py').read_text(encoding='utf-8')
        default_model = re.search(
            r"--requested-model',\s*default='([^']+)'", source).group(1)
        default_effort = re.search(
            r"--requested-effort',\s*default='([^']+)'", source).group(1)
        self.assertTrue(default_model.startswith(provider + '/'),
                        f'record-review.py defaults to {default_model!r}, but the '
                        f'workflow assigns the Packet reviewer to provider '
                        f'{provider!r}')
        self.assertEqual(default_effort, effort,
                         f'record-review.py defaults to effort {default_effort!r}, '
                         f'but the workflow assigns {effort!r}')

    def test_the_default_is_overridable(self) -> None:
        """A default must not be the only value the tool accepts.

        A review that ran on a different route (an authorised fallback, or a
        historical review being re-recorded) must still be recordable.
        """
        source = (ROOT / 'scripts' / 'record-review.py').read_text(encoding='utf-8')
        self.assertIn("parser.add_argument('--requested-model'", source)
        self.assertNotIn("parser.add_argument('--requested-model', required=True",
                         source)


class RetiredNamesTests(unittest.TestCase):
    """W15: the retired-name list must actually name the routes that retired."""

    def test_known_retired_routes_are_listed(self) -> None:
        # Plan W15 names three additions.  Two of them are genuinely retired as
        # *assignments* and are checked here; the third, `claude-opus-5-5`, is the
        # CURRENT Persistent Advisor (docs/agent-workflow.md §1), so listing it
        # would make the checker flag the workflow's own roster row.  The next
        # test enforces that side of the boundary.
        for name in ('hy4-preview-f', 'hy3', 'kimi-k3'):
            self.assertTrue(
                any(name in retired for retired in check_agent_docs.RETIRED_NAMES),
                f'{name!r} is not covered by RETIRED_NAMES; a document could name '
                f'it without a historical marker and the check would pass')

    def test_the_list_is_not_empty(self) -> None:
        self.assertTrue(check_agent_docs.RETIRED_NAMES)

    def test_current_roster_routes_are_not_retired(self) -> None:
        """A current assignment must never be listed as retired.

        Otherwise the checker would flag the workflow's own roster row.
        """
        text = WORKFLOW.read_text(encoding='utf-8')
        # Only the active column's names, taken from the roster rows.
        current = set()
        for line in text.splitlines():
            if not line.startswith('|') or 'DSH' in line:
                continue
            for name in re.findall(r'`([\w.\-]+/[\w.\-]+)`', line):
                current.add(name.split('/')[-1])
        for name in current:
            self.assertNotIn(
                name, check_agent_docs.RETIRED_NAMES,
                f'{name!r} is on the current roster and must not be listed retired')


class NoStaleRouteInScriptsTests(unittest.TestCase):
    def test_no_script_defaults_to_a_retired_route(self) -> None:
        """A sweep, because one stale default was found by accident.

        The T11 default was noticed while reading the file for another reason. A
        sweep makes the next one a test failure instead of a discovery.
        """
        offenders = []
        for path in (ROOT / 'scripts').glob('*.py'):
            text = path.read_text(encoding='utf-8', errors='replace')
            for line_number, line in enumerate(text.splitlines(), start=1):
                if 'default=' not in line:
                    continue
                # The comment explaining the T11 fix names the old route; prose is
                # not a default.
                code = line.split('#', 1)[0]
                for retired in check_agent_docs.RETIRED_NAMES:
                    if retired in code:
                        offenders.append(f'{path.name}:{line_number}: {line.strip()}')
        self.assertEqual(offenders, [],
                         'scripts default to retired routes:\n  ' +
                         '\n  '.join(offenders))


class StartupReceiptCarryOverTests(unittest.TestCase):
    """T13: regenerating a receipt must not discard the probe results.

    Measured: re-running the generator over a filled receipt replaced all 14 probe
    fields with `UNVERIFIED`. The generator is *supposed* to be re-runnable -- it
    fills the mechanical fields -- but the probe fields are the evidence, and a
    tool that silently discards evidence on a routine re-run is worse than one that
    never filled it.
    """

    def setUp(self) -> None:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            'receipt', ROOT / 'scripts' / 'gen-startup-receipt.py')
        self.receipt = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.receipt)

    def test_filled_fields_are_carried_over(self) -> None:
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'receipt.md'
            path.write_text(
                '# Receipt\n'
                '- Session ID/date/harness: 2026-09-30, DSH\n'
                '- Actual main model/effort: `workbuddy-ai/deepseek-v4.1-flash` @ `max`\n'
                '- Child ID: UNVERIFIED (not measurable)\n',
                encoding='utf-8')
            carried = self.receipt.existing_field_values(path)
        self.assertEqual(carried.get('Session ID/date/harness'), '2026-09-30, DSH')
        self.assertIn('deepseek-v4.1-flash', carried.get('Actual main model/effort', ''))

    def test_placeholders_are_not_carried_over(self) -> None:
        """A never-filled receipt must regenerate cleanly, not inherit placeholders."""
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'receipt.md'
            path.write_text(
                '- Child ID: UNVERIFIED (not measurable by this generator)\n',
                encoding='utf-8')
            carried = self.receipt.existing_field_values(path)
        self.assertNotIn('Child ID', carried)

    def test_a_missing_receipt_yields_no_carry_over(self) -> None:
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            carried = self.receipt.existing_field_values(
                Path(temporary) / 'absent.md')
        self.assertEqual(carried, {})

    def test_the_real_receipt_keeps_its_route_facts(self) -> None:
        """The end-to-end property, on the receipt this session produced."""
        receipt = ROOT / 'docs' / 'reviews' / 'startup-current.md'
        if not receipt.is_file():
            self.skipTest('no receipt present')
        text = receipt.read_text(encoding='utf-8')
        # The route-resolution section must still name the probed routes.
        self.assertIn('claude-opus-5-5', text)
        self.assertIn('gpt-6.1-sol', text)
        self.assertIn('deepseek-v4.1-flash', text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
