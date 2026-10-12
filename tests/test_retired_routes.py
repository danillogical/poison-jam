"""Guard the active documents against reintroducing provider/model routing."""
from __future__ import annotations

import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ACTIVE_DOCS = ('AGENTS.md', 'plan-jsrf-gameplay.md')
ROUTING = re.compile(r'\b(?:provider|model)\s*:', re.IGNORECASE)


def routing_findings(paths: tuple[Path, ...]) -> list[str]:
    findings = []
    for path in paths:
        for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            if ROUTING.search(line):
                findings.append(f'{path.name}:{number}: {line.strip()}')
    return findings


class ActiveDocumentRoutingTests(unittest.TestCase):
    def test_active_documents_contain_no_provider_model_assignments(self):
        findings = routing_findings(tuple(ROOT / relative for relative in ACTIVE_DOCS
                                          if (ROOT / relative).is_file()))
        self.assertEqual(findings, [], '\n'.join(findings))

    def test_the_guard_catches_a_new_routing_assignment(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'AGENTS.md'
            path.write_text('routing:\n  provider: old\n  model: old\n', encoding='utf-8')
            self.assertEqual(routing_findings((path,)),
                             ['AGENTS.md:2: provider: old', 'AGENTS.md:3: model: old'])

    def test_historical_documents_are_outside_the_active_guard(self):
        self.assertNotIn('docs/jsrf-operating-history.md', ACTIVE_DOCS)


if __name__ == '__main__':
    unittest.main(verbosity=2)
