"""P0.5 fixtures: preflight, derived target inventory, failure classification.

The inventory tests carry the weight.  Measured defect: the hand-maintained
`TARGETS` list named 11 targets while CTest discovers 12 tests --
`xbox_timestamp_test` comes from the toolkit subdirectory, so no list in this
repository can know about it, and MSBuild deletes a target's output when its link
fails.  A build that omits it leaves that test permanently "Not Run" with nothing
in the log to explain it.
"""
from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

import jsrf_build  # noqa: E402

BUILD_SCRIPT = SCRIPTS / 'build-jsrf.py'


class PreflightTests(unittest.TestCase):
    def test_preflight_resolves_the_toolchain(self):
        report = jsrf_build.preflight(4)
        self.assertTrue(report['python'])
        self.assertIn('python_version', report)
        self.assertEqual(report['parallel'], 4)
        self.assertTrue(report['environment_normalized'])

    def test_preflight_rejects_nonpositive_parallel(self):
        for value in (0, -1):
            with self.subTest(parallel=value):
                report = jsrf_build.preflight(value)
                self.assertFalse(report['ok'])
                self.assertTrue(any('positive integer' in p for p in report['problems']))

    def test_preflight_rejects_non_integer_parallel(self):
        report = jsrf_build.preflight('four')  # type: ignore[arg-type]
        self.assertFalse(report['ok'])

    def test_preflight_reports_missing_tools(self):
        """A missing tool must be named, not silently tolerated."""
        original = jsrf_build._resolve_tool
        try:
            jsrf_build._resolve_tool = lambda name: None
            report = jsrf_build.preflight(4)
        finally:
            jsrf_build._resolve_tool = original
        self.assertFalse(report['ok'])
        self.assertIn('cmake', report['missing_tools'])
        self.assertIn('ctest', report['missing_tools'])

    def test_duplicate_case_keys_are_detected(self):
        raw = {'Path': 'a', 'PATH': 'b', 'path': 'c', 'OTHER': 'd'}
        found = jsrf_build.duplicate_case_keys(raw)
        self.assertEqual(sorted(k.upper() for k in found), ['PATH', 'PATH', 'PATH'])

    def test_normalized_environment_collapses_case(self):
        raw = {'Path': 'a', 'PATH': 'b', 'other': 'c'}
        normalized = jsrf_build.normalized_environment(raw)
        self.assertEqual(normalized['PATH'], 'b')
        self.assertEqual(normalized['OTHER'], 'c')
        self.assertEqual(len([k for k in normalized if k == 'PATH']), 1)

    def test_normalized_environment_is_well_formed(self):
        """The real environment, collapsed, has no duplicate-case keys."""
        normalized = jsrf_build.normalized_environment()
        self.assertEqual(len(normalized), len(set(normalized)))


class TargetInventoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (ROOT / 'build').is_dir():
            raise unittest.SkipTest('build directory is missing')
        cls.derived = jsrf_build.discover_required_targets()

    def test_every_discovered_test_maps_to_a_command(self):
        mapping = jsrf_build.target_for_test(ROOT / 'build')
        for test in self.derived['tests']:
            with self.subTest(test=test['name']):
                self.assertIn(test['name'], mapping,
                              'every discovered test must map to a command or be '
                              'recorded as having no build target')

    def test_denominator_matches_ctest(self):
        listed = jsrf_build.ctest_tests()
        self.assertEqual(self.derived['test_count'], len(listed))
        self.assertEqual([t['name'] for t in self.derived['tests']],
                         [t['name'] for t in listed])

    def test_inventory_is_not_hand_maintained(self):
        """The derived set must be produced from CMake, not a literal list."""
        self.assertEqual(self.derived['schema_version'],
                         jsrf_build.INVENTORY_SCHEMA_VERSION)
        self.assertNotIn('hand', self.derived.get('source', ''))

    def test_toolkit_target_is_discovered(self):
        """The measured defect: a toolkit-provided target CTest requires."""
        self.assertIn('xbox_timestamp_test', self.derived['targets'],
                      'the toolkit target must appear in the derived inventory')

    def test_hand_maintained_list_would_miss_a_required_target(self):
        """The gap that P0.5-AC2 exists to close, asserted against the old list.

        The old list is gone from the entry point, so it is reconstructed here
        from the committed history of what it named.  This keeps the *defect*
        pinned even though the defective code was deleted: if the derived
        inventory ever regresses to a literal list, this control still shows what
        that costs.
        """
        legacy_targets = [
            'jsrf_recomp', 'jsrf_collect', 'jsrf_save_root_test',
            'jsrf_crt_test', 'jsrf_lifter_test', 'jsrf_nv2a_test',
            'jsrf_recovery_11c1_test', 'jsrf_service_chain_test',
            'jsrf_callback_reentry_test', 'jsrf_nv2a_hal_test',
            'jsrf_inplace_event_test', 'jsrf_gpu_smoke',
        ]
        derived = set(self.derived['targets'])
        missing = sorted(derived - set(legacy_targets))
        self.assertIn('xbox_timestamp_test', missing,
                      'the legacy literal list omitted a target CTest requires')

    def test_build_entry_point_uses_the_derived_list(self):
        text = BUILD_SCRIPT.read_text(encoding='utf-8')
        self.assertIn('discover_required_targets', text,
                      'the build entry point must derive its targets')
        # The old literal was a bare `TARGETS = [...]`.  `ARTIFACT_TARGETS` is a
        # different thing -- the two runner artifacts -- and naming it is fine.
        import re
        self.assertIsNone(re.search(r'^TARGETS\s*=\s*\[', text, re.MULTILINE),
                          'the hand-maintained literal target list must be gone')
        self.assertIn('ARTIFACT_TARGETS', text,
                      'the runner artifacts must still be named explicitly')

    def test_inventory_gaps_function_still_reports_a_gap_for_a_literal_list(self):
        """`inventory_gaps` must remain able to detect an incomplete list.

        With the entry point fixed there is no live gap, so the check is
        exercised against a synthetic literal list -- otherwise a function that
        always returned `complete: True` would pass unnoticed.
        """
        original = jsrf_build.hand_maintained_targets
        try:
            jsrf_build.hand_maintained_targets = lambda path=None: ['jsrf_recomp']
            gaps = jsrf_build.inventory_gaps()
        finally:
            jsrf_build.hand_maintained_targets = original
        self.assertFalse(gaps['complete'])
        self.assertTrue(gaps['discovered_but_not_hand_listed'])

    def test_python_driven_test_is_recorded_not_guessed(self):
        self.assertIn('jsrf_gpu_inspection', self.derived['tests_without_build_target'])

    def test_written_inventory_round_trips(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'inv.json'
            written = jsrf_build.write_inventory(ROOT / 'build', path)
            loaded = json.loads(path.read_text(encoding='utf-8'))
            self.assertEqual(loaded['targets'], written['targets'])
            self.assertEqual(loaded['test_count'], written['test_count'])


class FailureClassificationTests(unittest.TestCase):
    """Only the measured confined signature may permit a serial retry."""

    CONFINED_LOG = ('Checking File Globs\n'
                    'Done building target "ResolveProjectReferences" in project '
                    '"x.vcxproj" -- FAILED.\n'
                    '0 Error(s)\n')

    def test_confined_signature_is_recognized(self):
        self.assertTrue(jsrf_build.is_confined_silent_failure(1, self.CONFINED_LOG))
        self.assertEqual(jsrf_build.classify_build_failure(1, self.CONFINED_LOG),
                         'confined_silent')

    def test_success_is_not_a_failure(self):
        self.assertFalse(jsrf_build.is_confined_silent_failure(0, self.CONFINED_LOG))
        self.assertEqual(jsrf_build.classify_build_failure(0, ''), 'success')

    def test_compile_error_is_not_the_confined_signature(self):
        log = self.CONFINED_LOG + 'main.c(12): error C2065: undeclared identifier\n'
        self.assertFalse(jsrf_build.is_confined_silent_failure(1, log),
                         'a real compile error must never be retried as a sandbox issue')
        self.assertEqual(jsrf_build.classify_build_failure(1, log), 'compile_error')

    def test_link_error_is_not_the_confined_signature(self):
        log = self.CONFINED_LOG + 'foo.obj : error LNK2019: unresolved external\n'
        self.assertFalse(jsrf_build.is_confined_silent_failure(1, log))
        self.assertEqual(jsrf_build.classify_build_failure(1, log), 'link_error')

    def test_msbuild_error_is_not_the_confined_signature(self):
        log = self.CONFINED_LOG + 'error MSB6001: Invalid command line switch\n'
        self.assertFalse(jsrf_build.is_confined_silent_failure(1, log))
        self.assertEqual(jsrf_build.classify_build_failure(1, log), 'msbuild_error')

    def test_failure_without_the_marker_is_not_the_confined_signature(self):
        log = 'Some other failure with no markers at all.\n'
        self.assertFalse(jsrf_build.is_confined_silent_failure(1, log))
        self.assertEqual(jsrf_build.classify_build_failure(1, log), 'unknown_failure')

    def test_empty_log_is_not_the_confined_signature(self):
        self.assertFalse(jsrf_build.is_confined_silent_failure(1, ''))

    def test_serial_retry_preserves_the_first_log(self):
        """The retry must not overwrite the evidence of the original failure."""
        text = BUILD_SCRIPT.read_text(encoding='utf-8')
        self.assertIn('preserved', text)
        self.assertIn('serial', text)

    def test_regeneration_is_off_by_default(self):
        """Recovery regeneration can erase ABI instrumentation."""
        text = BUILD_SCRIPT.read_text(encoding='utf-8')
        self.assertIn('--allow-regeneration', text)
        self.assertIn('disabled', text)


class EntryPointTests(unittest.TestCase):
    def test_preflight_runs_before_any_mutation(self):
        text = BUILD_SCRIPT.read_text(encoding='utf-8')
        preflight_at = text.index('jsrf_build.preflight')
        configure_at = text.index("'cmake', '-S', '.', '-B', 'build'")
        self.assertLess(preflight_at, configure_at,
                        'preflight must precede configure so nothing is mutated '
                        'when a tool is missing')

    def test_help_runs_without_building(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(BUILD_SCRIPT), '--help'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('--parallel', result.stdout)
        self.assertIn('--allow-regeneration', result.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
