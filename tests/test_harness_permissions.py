"""P0.6 fixtures: harness permissions, probe expectations, result schema.

Three failures this pins, each measured on this project:

* A confined file policy makes `NtOpenFile` return `ACCESS_DENIED`, the guest
  calls `HalReturnToFirmware(2)`, and the run ends after ~1.8 s with ~515 log
  lines -- which reads exactly like a catastrophic regression.  A denied access
  must be `ENVIRONMENT_BLOCKED`, not a guest failure.
* A `--probe=` run returns before `checkpoint("guest_entry")`, so inheriting the
  ordinary default reports a false failure for a probe that behaved correctly.
* A bounded capture is not liveness, and a returned entry point is not
  satisfaction, so a single status field cannot express the outcome.
"""
from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

import jsrf_harness as harness  # noqa: E402


class ProbeExpectationTests(unittest.TestCase):
    def test_every_runner_probe_has_a_mapped_expectation(self):
        """The centralized map must cover the runner's population exactly."""
        gaps = harness.probe_map_gaps()
        self.assertEqual(gaps['defined_but_unmapped'], [],
                         'a probe with no expectation would inherit the ordinary '
                         'default, which is the defect this map replaces')
        self.assertEqual(gaps['mapped_but_undefined'], [])
        self.assertTrue(gaps['complete'])

    def test_probe_population_is_not_empty(self):
        self.assertGreater(len(harness.probe_population()), 10)

    def test_ordinary_launch_expects_guest_entry(self):
        self.assertEqual(harness.expected_checkpoints(''),
                         harness.ORDINARY_CHECKPOINTS)
        self.assertIn('guest_entry', harness.expected_checkpoints(''))

    def test_gpu_probes_do_not_expect_guest_entry(self):
        """They return before that checkpoint; expecting it is a false failure."""
        for probe in harness.probe_population():
            if not probe.startswith('gpu-'):
                continue
            with self.subTest(probe=probe):
                expected = harness.expected_checkpoints(probe)
                self.assertNotIn('guest_entry', expected)
                self.assertIn('probe_gpu', expected)

    def test_unknown_probe_raises_rather_than_defaulting(self):
        with self.assertRaises(harness.HarnessError) as raised:
            harness.expected_checkpoints('gpu-not-a-real-probe')
        self.assertIn('no expected checkpoint', str(raised.exception))

    def test_probe_is_ordinary_only_for_the_empty_probe(self):
        self.assertTrue(harness.probe_is_ordinary(''))
        for probe in harness.probe_population():
            if probe:
                with self.subTest(probe=probe):
                    self.assertFalse(harness.probe_is_ordinary(probe))

    def test_expected_set_is_covered_by_observed(self):
        result = harness.build_result(
            launch=harness.LAUNCH_STARTED, capture=harness.CAPTURE_DEADLINE,
            profile='strict', semantic=harness.SEMANTIC_UNKNOWN,
            checkpoints=['memory_ready', 'probe_gpu'])
        self.assertTrue(harness.probe_expectation_matches(result, 'gpu-stall'))
        self.assertFalse(harness.probe_expectation_matches(result, ''))

    def test_runner_probes_are_read_from_the_runner(self):
        probes = harness.runner_probes()
        self.assertIn('gpu-submit-supported', probes)
        self.assertIn('', probes)


class DiskPreflightTests(unittest.TestCase):
    def test_missing_root_is_unknown_not_permitted(self):
        with tempfile.TemporaryDirectory() as tmp:
            absent = Path(tmp) / 'does-not-exist'
            result = harness.preflight_disk(absent)
            self.assertEqual(result['verdict'], harness.UNKNOWN)
            self.assertFalse(result['exists'])

    def test_denied_access_is_environment_blocked(self):
        """A permission denial is an environment limit, never a guest failure."""
        self.assertEqual(harness.classify_access_error(PermissionError()),
                         harness.ENVIRONMENT_BLOCKED)
        self.assertEqual(harness.classify_access_error(OSError(13, 'denied')),
                         harness.ENVIRONMENT_BLOCKED)

    def test_filesystem_obstruction_is_environment_blocked(self):
        """A path shadowed by a file is an environment obstruction.

        Measured: the scratch-root control returned `UNKNOWN` for a path that
        existed as a file, which left the caller unable to distinguish "the
        environment prevents this" from "the checker could not decide".

        The errno values are taken symbolically: they differ between platforms
        (ENAMETOOLONG is 38 here, not 36), and a hardcoded number would assert the
        wrong thing on the host that actually runs this.
        """
        import errno as errno_module
        for name in ('EEXIST', 'ENOTDIR', 'ENOENT', 'EISDIR', 'ENAMETOOLONG', 'EROFS'):
            value = getattr(errno_module, name)
            with self.subTest(errno=name, value=value):
                self.assertEqual(
                    harness.classify_access_error(OSError(value, 'obstructed')),
                    harness.ENVIRONMENT_BLOCKED, name)

    def test_windows_obstruction_codes_are_environment_blocked(self):
        """Windows winerror codes for the same conditions."""
        for winerror in (183, 267, 3, 5, 206):
            with self.subTest(winerror=winerror):
                error = OSError(999, 'windows obstruction')
                error.winerror = winerror
                self.assertEqual(harness.classify_access_error(error),
                                 harness.ENVIRONMENT_BLOCKED)

    def test_unrecognised_error_is_unknown(self):
        self.assertEqual(harness.classify_access_error(OSError(999, 'mystery')),
                         harness.UNKNOWN)

    def test_permitted_root_reports_the_access_control(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in harness.PARTITION_IMAGES:
                (root / name).write_bytes(b'image')
            result = harness.preflight_disk(root)
            self.assertEqual(result['verdict'], harness.PERMITTED)
            self.assertIn('bytes identical', result['access_control'])

    def test_missing_image_is_named_not_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'Partition1.img').write_bytes(b'image')
            result = harness.preflight_disk(root)
            self.assertNotEqual(result['verdict'], harness.PERMITTED)
            self.assertTrue(any('missing partition image' in r for r in result['reasons']))

    def test_all_six_partition_images_are_enumerated(self):
        """"The disk" is not a population; every image must be named."""
        self.assertEqual(len(harness.PARTITION_IMAGES), 6)
        for index in range(6):
            self.assertIn(f'Partition{index}.img', harness.PARTITION_IMAGES)

    def test_preflight_does_not_change_image_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in harness.PARTITION_IMAGES:
                (root / name).write_bytes(b'original bytes')
            before = {n: (root / n).read_bytes() for n in harness.PARTITION_IMAGES}
            harness.preflight_disk(root)
            after = {n: (root / n).read_bytes() for n in harness.PARTITION_IMAGES}
            self.assertEqual(before, after)

    def test_mapped_drives_are_named(self):
        self.assertIn('T', harness.MAPPED_DRIVES)
        self.assertIn('U', harness.MAPPED_DRIVES)
        self.assertIn('Z', harness.MAPPED_DRIVES)


class ScratchRootTests(unittest.TestCase):
    def test_permitted_scratch_root_succeeds_and_cleans_up(self):
        with tempfile.TemporaryDirectory() as tmp:
            scratch = Path(tmp) / 'scratch'
            result = harness.scratch_root_control(scratch)
            self.assertEqual(result['verdict'], harness.PERMITTED)
            self.assertTrue(result['cleanup_verified'])
            self.assertFalse(scratch.exists())

    def test_denied_scratch_root_is_environment_blocked(self):
        """A denial is reported, never worked around with another API."""
        with tempfile.TemporaryDirectory() as tmp:
            blocked = Path(tmp) / 'file-not-a-directory'
            blocked.write_text('x', encoding='utf-8')
            result = harness.scratch_root_control(blocked / 'child')
            self.assertEqual(result['verdict'], harness.ENVIRONMENT_BLOCKED)
            self.assertTrue(result['reasons'])

    def test_scratch_control_does_not_evade_a_denial(self):
        """The control must not fall back to another location on failure."""
        source = (SCRIPTS / 'jsrf_harness.py').read_text(encoding='utf-8')
        self.assertIn('No alternate API is used to evade a denial', source)
        self.assertNotIn('except OSError:\n        scratch = Path(tempfile', source)


class ResultSchemaTests(unittest.TestCase):
    """Launch, capture, profile and semantic outcomes are four separate axes."""

    def valid(self, **overrides):
        base = dict(launch=harness.LAUNCH_STARTED, capture=harness.CAPTURE_DEADLINE,
                    profile='strict', semantic=harness.SEMANTIC_UNKNOWN,
                    checkpoints=['memory_ready', 'guest_entry'])
        base.update(overrides)
        return harness.build_result(**base)

    def test_valid_result_has_no_problems(self):
        self.assertEqual(harness.validate_result(self.valid()), [])

    def test_all_four_axes_are_required(self):
        for field in harness.RESULT_FIELDS:
            with self.subTest(field=field):
                result = self.valid()
                del result[field]
                problems = harness.validate_result(result)
                self.assertTrue(any(field in p for p in problems), problems)

    def test_deadline_cannot_carry_a_resolved_semantic_outcome(self):
        result = self.valid(capture=harness.CAPTURE_DEADLINE,
                            semantic=harness.SEMANTIC_ENTRY_RETURNED)
        problems = harness.validate_result(result)
        self.assertTrue(any('deadline' in p for p in problems), problems)

    def test_environment_blocked_launch_cannot_carry_semantics(self):
        result = self.valid(launch=harness.LAUNCH_ENVIRONMENT_BLOCKED,
                            capture=harness.CAPTURE_NO_DUMP,
                            semantic=harness.SEMANTIC_GUEST_FAULT)
        problems = harness.validate_result(result)
        self.assertTrue(any('environment-blocked' in p for p in problems), problems)

    def test_refused_launch_cannot_have_produced_a_capture(self):
        result = self.valid(launch=harness.LAUNCH_REFUSED,
                            capture=harness.CAPTURE_COMPLETE)
        problems = harness.validate_result(result)
        self.assertTrue(any('refused' in p for p in problems), problems)

    def test_normal_exit_without_a_dump_is_not_a_semantic_success(self):
        result = self.valid(capture=harness.CAPTURE_NO_DUMP,
                            semantic=harness.SEMANTIC_ENTRY_RETURNED)
        problems = harness.validate_result(result)
        self.assertTrue(any('no dump' in p for p in problems), problems)

    def test_liveness_fields_are_rejected_outright(self):
        """A capture outcome cannot establish liveness, so it must not be a field."""
        for field in ('liveness', 'boot_success', 'title_satisfied'):
            with self.subTest(field=field):
                result = self.valid()
                result[field] = True
                problems = harness.validate_result(result)
                self.assertTrue(any(field in p for p in problems), problems)

    def test_unknown_schema_is_reported(self):
        result = self.valid()
        result['schema_version'] = 99
        self.assertTrue(harness.validate_result(result))

    def test_non_object_result_is_reported(self):
        self.assertTrue(harness.validate_result('not a result'))

    def test_capture_vocabulary_is_not_success_shaped(self):
        """The capture axis must not use words that read as achievement."""
        for name in ('CAPTURE_COMPLETE', 'CAPTURE_DEADLINE', 'CAPTURE_NO_DUMP',
                     'CAPTURE_FAILED'):
            value = getattr(harness, name)
            self.assertNotIn(value, ('success', 'boot', 'passed'), name)


if __name__ == '__main__':
    unittest.main(verbosity=2)
