"""A4s-r6 fixtures: the structural post-resolution merge check.

The defect this pins: a three-way merge can produce **duplicate `case` labels** and
**duplicate file-scope definitions** in hunks git never flagged as conflicts, so the
merge rules that decide conflicts never see them.  `A4s-r5` measured both in the
`v0.11.0` sync -- `case 138` twice in each dispatch switch, and `bridge_KeResetEvent`
defined twice -- and both are compile errors.  Left alone they surface as a build
failure and get attributed to the merge's *build* rather than its *resolution*.

Each fixture states the wrong outcome it prevents:

* a real duplicate case label must be **caught** (else a broken merge passes);
* a legal declaration-plus-definition, a tentative definition, and an identical macro
  redefinition must **not** be reported (else a correct merge fails, and the check
  trains its reader to ignore it);
* an unparseable file must be **UNKNOWN**, never a silent pass.
"""
from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

_spec = importlib.util.spec_from_file_location(
    'merge_structure', SCRIPTS / 'check-merge-structure.py')
assert _spec and _spec.loader
ms = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ms)


def scan(text: str):
    """Scan one synthetic translation unit; return (findings, unknown_reason)."""
    lines = text.split('\n')
    findings, _counts, reason = ms.scan_file('synthetic.c', lines)
    return findings, reason


def kinds(findings):
    return sorted({f['check'] for f in findings})


class DuplicateCaseTests(unittest.TestCase):
    def test_duplicate_case_in_one_switch_is_caught(self):
        """The A4s-r5 defect: both sides added `case 138` in different places."""
        text = (
            'int f(int x) {\n'
            '    switch (x) {\n'
            '    case 1: return 1;\n'
            '    case 2: return 2;\n'
            '    case 1: return 3;\n'
            '    }\n'
            '    return 0;\n'
            '}\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertIn('dup-case', kinds(findings))
        dup = [f for f in findings if f['check'] == 'dup-case'][0]
        self.assertEqual(dup['lines'], [3, 5])

    def test_distinct_cases_are_not_reported(self):
        text = (
            'int f(int x) {\n'
            '    switch (x) {\n'
            '    case 1: return 1;\n'
            '    case 2: return 2;\n'
            '    }\n'
            '    return 0;\n'
            '}\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertEqual(findings, [])

    def test_same_label_on_different_branches_is_not_reported(self):
        """Alternative preprocessor branches are alternatives, not duplicates."""
        text = (
            'int f(int x) {\n'
            '#if defined(_WIN32)\n'
            '    switch (x) {\n'
            '    case 1: return 1;\n'
            '    }\n'
            '#else\n'
            '    switch (x) {\n'
            '    case 1: return 2;\n'
            '    }\n'
            '#endif\n'
            '    return 0;\n'
            '}\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertNotIn('dup-case', kinds(findings))

    def test_case_inside_a_comment_is_not_a_label(self):
        text = (
            'int f(int x) {\n'
            '    switch (x) {\n'
            '    case 1: return 1;\n'
            '    /* case 1: a comment, not a label */\n'
            '    }\n'
            '    return 0;\n'
            '}\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertNotIn('dup-case', kinds(findings))


class DuplicateDefinitionTests(unittest.TestCase):
    def test_duplicate_function_definition_is_caught(self):
        """The A4s-r5 defect: both sides added `bridge_KeResetEvent`."""
        text = (
            'static void twice(void)\n'
            '{\n'
            '    return;\n'
            '}\n'
            '\n'
            'static void twice(void)\n'
            '{\n'
            '    return;\n'
            '}\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertIn('dup-def', kinds(findings))

    def test_declaration_plus_definition_is_not_reported(self):
        text = (
            'static void once(void);\n'
            '\n'
            'static void once(void)\n'
            '{\n'
            '    return;\n'
            '}\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertNotIn('dup-def', kinds(findings))

    def test_tentative_definition_completed_later_is_not_reported(self):
        """C11 6.9.2p2: `static const T x;` then `static const T x = {...};` is legal."""
        text = (
            'static const int table[];\n'
            '\n'
            'static const int table[] = { 1, 2, 3 };\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertNotIn('dup-def', kinds(findings))

    def test_one_line_function_definition_is_counted(self):
        text = (
            'static void a(void) { return; }\n'
            'static void a(void) { return; }\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertIn('dup-def', kinds(findings))


class MacroTests(unittest.TestCase):
    def test_identical_macro_redefinition_is_not_reported(self):
        """C11 6.10.3p3 permits redefinition when the replacement lists are identical."""
        text = (
            '#define VALUE 0x00000600\n'
            '#define VALUE 0x00000600\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertNotIn('dup-macro', kinds(findings))

    def test_differing_macro_redefinition_is_reported(self):
        text = (
            '#define VALUE 0x00000600\n'
            '#define VALUE 0x00000604\n'
        )
        findings, reason = scan(text)
        self.assertIsNone(reason)
        self.assertIn('dup-macro', kinds(findings))


class MarkerTests(unittest.TestCase):
    def test_conflict_markers_are_reported(self):
        text = (
            'int f(void)\n'
            '{\n'
            '<<<<<<< ours\n'
            '    return 1;\n'
            '=======\n'
            '    return 2;\n'
            '>>>>>>> theirs\n'
            '}\n'
        )
        findings, reason = scan(text)
        self.assertEqual(len([f for f in findings if f['check'] == 'marker']), 3)


class FailClosedTests(unittest.TestCase):
    def test_unparseable_file_is_unknown_not_pass(self):
        """A file whose braces do not balance must be UNKNOWN, never a silent pass."""
        text = (
            'int f(void)\n'
            '{\n'
            '    if (1) {\n'
            '    return 0;\n'
            '}\n'
        )
        findings, reason = scan(text)
        self.assertIsNotNone(reason)
        self.assertIn('brace', reason)


class RealRepositoryTests(unittest.TestCase):
    """Positive and negative controls on the pinned trees.

    These are the controls the packet cites, so they are asserted here rather than
    only observed by hand.
    """

    def _scan(self, rev):
        return ms.scan(rev)

    def test_merge_preview_reports_the_three_structural_defects(self):
        rep = self._scan('75083476')
        self.assertEqual(rep['unknown'], [])
        structural = [f for f in rep['findings'] if f['check'] != 'marker']
        self.assertEqual(len(structural), 3, structural)
        self.assertEqual(sum(1 for f in structural if f['check'] == 'dup-case'), 2)
        self.assertEqual(sum(1 for f in structural if f['check'] == 'dup-def'), 1)
        self.assertTrue(all('kernel_bridge.c' in f['file'] for f in structural))
        # the raw preview is unresolved, so markers are expected
        self.assertEqual(sum(1 for f in rep['findings'] if f['check'] == 'marker'), 12)

    def test_both_parents_are_clean(self):
        for rev in ('0d7929c', '766ecef'):
            rep = self._scan(rev)
            self.assertEqual(rep['findings'], [], f'{rev} should be clean')
            self.assertEqual(rep['unknown'], [], f'{rev} should parse cleanly')


if __name__ == '__main__':
    unittest.main()
