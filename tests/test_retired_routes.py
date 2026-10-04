"""Controls for W15's retired route names.

`check-agent-docs.py` carries `RETIRED_NAMES` so a retired route cannot appear in an
active-policy document without a historical marker. The list was once stale: it named
`hy3` but not `hy4-preview-f`, so a script defaulting to that retired route passed the
check. These controls keep the list honest and keep scripts from defaulting to a
retired route.
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



if __name__ == '__main__':
    unittest.main(verbosity=2)
