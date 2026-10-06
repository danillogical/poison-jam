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


class WritePreservationTests(unittest.TestCase):
    """`--write` must not erase the hand-maintained history.

    Measured defect: `write_manifest()` rebuilt the manifest from
    `build_manifest()` alone and overwrote the file, so every `--write` dropped
    the `amendments` and `regenerations` lists. It did so **four times in one
    session** (39/1, then 40/2, then 41/3 across two more rewrites), each time
    re-attached by hand from a pre-write copy. The loss is silent, which is what
    made it survive so long.
    """

    def setUp(self):
        self.recorded = json.loads(MANIFEST.read_text(encoding='utf-8'))
        # The provenance manifest carries these two; the preservation baseline
        # carries `updates` instead.  Both names are in PRESERVED_KEYS, so the
        # controls below check the keys each file actually has.
        self.here = [k for k in provenance.PRESERVED_KEYS if k in self.recorded]
        self.assertTrue(self.here, 'the provenance manifest carries no history at all')
        for key in self.here:
            self.assertIsInstance(self.recorded[key], list)
        self.assertIn('amendments', self.here)
        self.assertIn('regenerations', self.here)

    def test_write_preserves_amendments_and_regenerations(self):
        """The deciding control: write to a copy, compare the history lists."""
        with tempfile.TemporaryDirectory() as tmp:
            scratch = Path(tmp) / 'manifest.json'
            scratch.write_text(json.dumps(self.recorded), encoding='utf-8')
            provenance.write_manifest(scratch)
            after = json.loads(scratch.read_text(encoding='utf-8'))
        for key in self.here:
            self.assertEqual(after[key], self.recorded[key],
                             f'--write dropped or altered {key!r}')
        self.assertTrue(self.recorded['amendments'], 'the control needs a non-empty history')

    def test_write_is_idempotent_over_the_history(self):
        with tempfile.TemporaryDirectory() as tmp:
            scratch = Path(tmp) / 'manifest.json'
            scratch.write_text(json.dumps(self.recorded), encoding='utf-8')
            provenance.write_manifest(scratch)
            once = json.loads(scratch.read_text(encoding='utf-8'))
            provenance.write_manifest(scratch)
            twice = json.loads(scratch.read_text(encoding='utf-8'))
        self.assertEqual(once, twice)

    def test_write_refuses_a_malformed_history_instead_of_emptying_it(self):
        """Present-but-not-a-list must fail loudly, not become an empty list."""
        with tempfile.TemporaryDirectory() as tmp:
            scratch = Path(tmp) / 'manifest.json'
            broken = dict(self.recorded)
            broken['amendments'] = 'not a list'
            scratch.write_text(json.dumps(broken), encoding='utf-8')
            with self.assertRaises(provenance.ProvenanceError):
                provenance.write_manifest(scratch)

    def test_a_fresh_tree_gets_empty_lists_rather_than_failing(self):
        """The first write of a new tree has nothing to preserve."""
        with tempfile.TemporaryDirectory() as tmp:
            scratch = Path(tmp) / 'absent.json'
            manifest = provenance.write_manifest(scratch)
        # A fresh tree has no history at all: the writer must not invent one.
        for key in provenance.PRESERVED_KEYS:
            self.assertNotIn(key, manifest)

    def test_render_is_byte_identical_on_the_unchanged_record(self):
        """A no-op write must be a no-op, not a reformat.

        The measured failure this prevents: `write_manifest` used a hard-coded
        ``indent=2`` and ``sort_keys=True`` over the whole manifest, so recording
        a two-line generated change rewrote 3,200 lines of
        `p0-7-generation-provenance.json` and reordered 256 lines of the
        baseline's `updates` history. A record nobody can diff is a record nobody
        reviews.
        """
        for path in (MANIFEST, BASELINE):
            with self.subTest(path=path.name):
                original = path.read_text(encoding='utf-8')
                rendered = provenance.render_manifest(
                    json.loads(original), provenance.detect_indent(original) or 1,
                    original)
                self.assertEqual(rendered, original,
                                 f're-rendering {path.name} is not byte-identical')

    def test_render_preserves_entry_key_order(self):
        """History entries keep their own key order, not a sorted one."""
        raw = ('{\n "updates": [\n  {\n   "date": "2026-01-01",\n'
               '   "files": [],\n   "reason": "why"\n  }\n ]\n}\n')
        rendered = provenance.render_manifest(json.loads(raw), 1, raw)
        self.assertEqual(rendered, raw)

    def test_render_does_not_invent_a_history_section(self):
        """A record with only `updates` must not gain `amendments`."""
        raw = '{\n "files": {},\n "updates": []\n}\n'
        rendered = provenance.render_manifest(json.loads(raw), 1, raw)
        self.assertNotIn('amendments', rendered)
        self.assertIn('"updates": []', rendered)


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
