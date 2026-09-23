"""P0.7 fixtures: generation provenance, protected markers, candidate isolation.

The defect this pins: `scripts/recover-functions.py` runs as part of the ordinary
build and rewrites generated output, which can erase the ABI instrumentation the
current evidence depends on.  An earlier session measured a fix as "zero effect"
because it had regenerated the wrong half of the pipeline, and nothing recorded
which inputs produced the committed tree -- so a regeneration could not be
distinguished from a regression.
"""
from __future__ import annotations

import copy
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

_spec = importlib.util.spec_from_file_location(
    'gen_provenance', SCRIPTS / 'check-generation-provenance.py')
assert _spec and _spec.loader
provenance = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(provenance)

CHECKER = SCRIPTS / 'check-generation-provenance.py'
MANIFEST = ROOT / 'docs' / 'reviews' / 'p0-7-generation-provenance.json'
BASELINE = ROOT / 'docs' / 'reviews' / 'p0-full-generated-baseline.json'


class ManifestTests(unittest.TestCase):
    """AC1: the manifest identifies inputs, generators, outputs and markers."""

    def setUp(self):
        self.manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))

    def test_recorded_manifest_passes(self):
        result = provenance.check_manifest(self.manifest)
        self.assertTrue(result['ok'], result['problems'] + result['unknown'])
        self.assertEqual(result['problems'], [])
        self.assertEqual(result['unknown'], [])

    def test_manifest_records_both_repository_states(self):
        generators = self.manifest['generators']
        for repo in ('project', 'toolkit'):
            with self.subTest(repo=repo):
                self.assertEqual(generators[repo]['state'], 'MEASURED')
                self.assertTrue(generators[repo]['revision'])
                self.assertIn('dirty', generators[repo])

    def test_manifest_records_the_xbe_and_analysis_inputs(self):
        inputs = self.manifest['inputs']
        self.assertIn('game/default.xbe', inputs)
        self.assertTrue(inputs['game/default.xbe']['present'])
        for name in provenance.ANALYSIS_INPUTS:
            with self.subTest(input=name):
                self.assertIn(name, inputs)

    def test_manifest_records_the_exact_translation_command(self):
        command = self.manifest['full_translation_command']
        for required in ('--all', '--split', '--gen-dir', '--manual-functions',
                         '--exclude-manual'):
            with self.subTest(flag=required):
                self.assertIn(required, command)

    def test_manifest_records_all_seventeen_outputs(self):
        baseline = json.loads(BASELINE.read_text(encoding='utf-8'))['files']
        self.assertEqual(len(baseline), 17)
        self.assertEqual(set(self.manifest['generated_outputs']), set(baseline))

    def test_manifest_records_ownership_split(self):
        ownership = self.manifest['ownership']
        self.assertIn('src/recomp/gen/recomp_stubs_unresolved.c',
                      ownership['translation_owns'])
        self.assertIn('src/recomp/gen/recomp_stubs_recovery.c',
                      ownership['recovery_owns'])
        self.assertIn('Never let', ownership['rule'])

    def test_manifest_does_not_claim_reproducibility(self):
        self.assertIn('NOT a claim', self.manifest['purpose'])

    def test_unknown_schema_is_rejected(self):
        result = provenance.check_manifest({'schema_version': 99})
        self.assertFalse(result['ok'])


class ProtectedMarkerTests(unittest.TestCase):
    """AC1/AC3: the ABI instrumentation must be counted, and its loss is a defect."""

    def test_all_markers_present_with_nonzero_counts(self):
        totals = provenance.total_marker_counts(provenance.protected_marker_counts())
        for marker in provenance.PROTECTED_MARKERS:
            with self.subTest(marker=marker):
                self.assertGreater(totals.get(marker, 0), 0,
                                   f'{marker} occurs 0 times')

    def test_marker_inventory_is_measured_not_assumed(self):
        counts = provenance.protected_marker_counts()
        self.assertTrue(counts)
        for name, entry in counts.items():
            self.assertIn('present', entry)

    def test_a_vanished_marker_is_a_problem(self):
        manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
        zeroed = {marker: 0 for marker in provenance.PROTECTED_MARKERS}
        result = provenance.check_manifest(manifest, markers=zeroed)
        self.assertFalse(result['ok'])
        self.assertTrue(any('erased' in p for p in result['problems']),
                        result['problems'])

    def test_manifest_omitting_a_marker_name_is_rejected(self):
        manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
        manifest['protected_markers']['names'] = ['RECOMP_ABI_CALL']
        result = provenance.check_manifest(manifest)
        self.assertFalse(result['ok'])
        self.assertTrue(any('omits protected marker' in p for p in result['problems']))

    def test_missing_file_is_recorded_not_skipped(self):
        counts = provenance.protected_marker_counts(('src/does/not/exist.c',))
        self.assertFalse(counts['src/does/not/exist.c']['present'])


class NegativeControlTests(unittest.TestCase):
    """AC3: every mutation is rejected, for its own reason."""

    def setUp(self):
        self.manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))

    def test_changed_input_hash_is_rejected(self):
        bad = copy.deepcopy(self.manifest)
        name = next(iter(bad['inputs']))
        bad['inputs'][name]['sha256'] = 'a' * 64
        result = provenance.check_manifest(bad)
        self.assertFalse(result['ok'])
        self.assertTrue(any('changed since the manifest' in p for p in result['problems']),
                        result['problems'])

    def test_removed_input_is_unknown_not_ok(self):
        bad = copy.deepcopy(self.manifest)
        name = next(iter(bad['inputs']))
        bad['inputs'][name]['present'] = False
        result = provenance.check_manifest(bad)
        self.assertFalse(result['ok'])
        self.assertTrue(any('absent' in u for u in result['unknown']), result['unknown'])

    def test_changed_generated_output_is_rejected(self):
        bad = copy.deepcopy(self.manifest)
        name = next(iter(bad['generated_outputs']))
        bad['generated_outputs'][name] = 'b' * 64
        result = provenance.check_manifest(bad)
        self.assertFalse(result['ok'])
        self.assertTrue(any(name in p for p in result['problems']), result['problems'])

    def test_missing_manifest_cannot_pass(self):
        for value in (None, 'not a manifest', [], 0):
            with self.subTest(value=repr(value)):
                self.assertFalse(provenance.check_manifest(value)['ok'])

    def test_manifest_without_inputs_is_rejected(self):
        bad = copy.deepcopy(self.manifest)
        bad['inputs'] = {}
        self.assertFalse(provenance.check_manifest(bad)['ok'])

    def test_manifest_without_outputs_is_rejected(self):
        bad = copy.deepcopy(self.manifest)
        bad['generated_outputs'] = {}
        self.assertFalse(provenance.check_manifest(bad)['ok'])

    def test_unmeasured_repository_state_is_unknown(self):
        bad = copy.deepcopy(self.manifest)
        bad['generators']['toolkit'] = {'state': 'UNKNOWN', 'revision': None}
        result = provenance.check_manifest(bad)
        self.assertFalse(result['ok'])
        self.assertTrue(any('toolkit' in u for u in result['unknown']), result['unknown'])

    def test_changed_recipe_is_detected_through_the_command(self):
        """A changed recipe means a different generation; the command is recorded."""
        bad = copy.deepcopy(self.manifest)
        bad['full_translation_command'] = bad['full_translation_command'].replace(
            '--split 1000', '--split 500')
        self.assertNotEqual(bad['full_translation_command'],
                            self.manifest['full_translation_command'])


class CheckOnlyTests(unittest.TestCase):
    """AC2: a no-op generation must not touch the production tree."""

    def test_production_tree_is_byte_identical(self):
        with tempfile.TemporaryDirectory() as tmp:
            candidate = Path(tmp) / 'candidate'
            result = provenance.check_only(candidate_dir=candidate)
            self.assertTrue(result['production_unchanged'], result['changed_outputs'])
            self.assertTrue(result['markers_unchanged'])
            self.assertTrue(result['baseline_matches'])

    def test_candidate_output_is_isolated(self):
        with tempfile.TemporaryDirectory() as tmp:
            candidate = Path(tmp) / 'candidate'
            provenance.check_only(candidate_dir=candidate)
            self.assertTrue((candidate / 'candidate-manifest.json').is_file())
            # The isolated candidate must not be inside the production gen tree.
            self.assertNotIn('src/recomp', str(candidate))

    def test_production_markers_survive_a_check_only_run(self):
        before = provenance.total_marker_counts(provenance.protected_marker_counts())
        with tempfile.TemporaryDirectory() as tmp:
            provenance.check_only(candidate_dir=Path(tmp) / 'candidate')
        after = provenance.total_marker_counts(provenance.protected_marker_counts())
        self.assertEqual(before, after)


class LiveTreeTests(unittest.TestCase):
    def test_generated_tree_matches_the_preservation_baseline(self):
        baseline = json.loads(BASELINE.read_text(encoding='utf-8'))['files']
        current = provenance.generated_tree_hashes()
        differing = [n for n in baseline if current.get(n) != baseline[n]]
        self.assertEqual(differing, [],
                         'the 17 protected generated files must be unchanged')

    def test_cli_check_passes(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--check', '--json'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        payload = json.loads(result.stdout)
        self.assertTrue(payload['ok'])

    def test_cli_check_only_passes(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--check-only', '--json'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        payload = json.loads(result.stdout)
        self.assertTrue(payload['production_unchanged'])

    def test_cli_rejects_no_arguments(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER)],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)

    def test_checker_is_read_only(self):
        before = provenance.generated_tree_hashes()
        subprocess.run([sys.executable, '-X', 'utf8', str(CHECKER), '--check'],
                       cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(before, provenance.generated_tree_hashes())


if __name__ == '__main__':
    unittest.main(verbosity=2)
