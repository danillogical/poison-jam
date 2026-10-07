from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

from jsrf_run_profile import (  # noqa: E402
    EXPLORATORY,
    FIXTURE,
    MISSING,
    PROFILE_SCHEMA_VERSION,
    RETIRED_OVERRIDES,
    STRICT,
    UNKNOWN,
    ProfileError,
    classify_run_directory,
    classify_settings,
    parse_apu_ack_addresses,
    reclassify_metadata,
    resolve_retired_overrides,
    validate_launch_profile,
)
import jsrf_run_profile  # noqa: E402  (module handle: the shared duration bound)
from jsrf_run_profile import ARCHIVED_ARTIFACTS, REPOSITORY_FILES  # noqa: E402

# The toolkit tree is a sibling of this repository; the revision-aware retired
# layer needs it for git ancestry.  Skip those cases honestly rather than
# asserting a PASS when the sibling is unavailable.
TOOLKIT_ROOT = ROOT.parent / 'xboxrecomp'
VBLANK = 'RECOMP_VBLANK'
VBLANK_HONORING_REVISION = '18a0837b0f6de334342f00b64d0e999340e59ba9'   # pre-removal
VBLANK_INERT_REVISION = '484887b88ff39f86d17c819375993340ebae972e'      # post-removal

_runner_spec = importlib.util.spec_from_file_location('run_jsrf', SCRIPTS / 'run-jsrf.py')
assert _runner_spec and _runner_spec.loader
run_jsrf = importlib.util.module_from_spec(_runner_spec)
_runner_spec.loader.exec_module(run_jsrf)


def settings(**values: str) -> list[dict[str, str]]:
    return [{'name': name, 'value': value} for name, value in values.items()]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def artifact_payloads() -> dict[str, bytes]:
    payloads = {name: f'fixture bytes for {name}'.encode('utf-8')
                for name in ARCHIVED_ARTIFACTS if name != 'build-source.json'}
    build_source = {
        'exe_sha256': sha256(payloads['jsrf_recomp.exe']),
        'artifacts': {name: sha256(payloads[name]) for name in payloads},
    }
    payloads['build-source.json'] = json.dumps(build_source, sort_keys=True).encode('utf-8')
    return payloads


def valid_archive() -> dict:
    environment = settings(RECOMP_GPU_ACK='0')
    base = classify_settings(environment)
    artifacts = artifact_payloads()
    artifact_hashes = {name: sha256(payload) for name, payload in artifacts.items()}
    repository_hashes = {}
    for repo_name, filenames in REPOSITORY_FILES.items():
        repository_hashes[repo_name] = {
            'revision': 'c' * 40,
            'patch_sha256': sha256(f'{repo_name} patch'.encode('utf-8')),
            'status_sha256': sha256(f'{repo_name} status'.encode('utf-8')),
        }
    return {
        'seconds': 15,
        'probe': '',
        'settings': environment,
        'run_profile': {
            'schema_version': PROFILE_SCHEMA_VERSION,
            'classifier_version': 'jsrf-run-profile/1',
            'requested_profile': STRICT,
            'classification': STRICT,
            'environment_classification': base['classification'],
            'reasons': base['reasons'],
            'inherited_settings': environment,
            'effective_settings': environment,
            'resolved_defaults': base['resolved_defaults'],
        },
        'command': ['jsrf_collect.exe', '15', 'run dir', 'jsrf_recomp.exe',
                    '--save-root=C:\\runs\\a\\save-root'],
        'xbe_sha256': 'a' * 64,
        'xbe_path': 'C:\\source\\game\\default.xbe',
        'exe_sha256': artifact_hashes['jsrf_recomp.exe'],
        'pdb_sha256': artifact_hashes['jsrf_recomp.pdb'],
        'map_sha256': artifact_hashes['jsrf_recomp.map'],
        'collector_sha256': artifact_hashes['jsrf_collect.exe'],
        'build_source_sha256': artifact_hashes['build-source.json'],
        'artifact_sha256': artifact_hashes,
        'repository_identities': repository_hashes,
        'project_archived': True,
        'save_root': {
            'archive_root': 'C:\\runs\\a',
            'expected_resolved_path': 'C:\\runs\\a\\save-root',
            'observed_resolved_path': 'c:/RUNS/a/save-root',
            'observed_path_layer_root': 'C:\\runs\\a\\SAVE-ROOT',
            'disposable': True,
            'verified': True,
        },
        'run_log_sha256': 'f' * 64,
    }


def write_valid_archive(run_dir: Path, source_xbe: Path) -> dict:
    run_dir.mkdir(parents=True, exist_ok=True)
    source_xbe.parent.mkdir(parents=True, exist_ok=True)
    source_xbe.write_bytes(b'original XBE fixture')
    archive_root = str(run_dir.resolve())
    save_root = str((run_dir / 'save-root').resolve())
    metadata = valid_archive()
    metadata['xbe_path'] = str(source_xbe.resolve())
    metadata['xbe_sha256'] = sha256(source_xbe.read_bytes())
    metadata['save_root'].update({
        'archive_root': archive_root,
        'expected_resolved_path': save_root,
        'observed_resolved_path': save_root,
        'observed_path_layer_root': save_root,
    })
    metadata['command'] = [
        str(run_dir / 'jsrf_collect.exe'), '15', str(run_dir),
        str(run_dir / 'jsrf_recomp.exe'), f'--save-root={save_root}',
    ]
    for name, payload in artifact_payloads().items():
        (run_dir / name).write_bytes(payload)
    for repo_name, filenames in REPOSITORY_FILES.items():
        for kind, filename in zip(('patch', 'status'), filenames):
            (run_dir / filename).write_bytes(f'{repo_name} {kind}'.encode('utf-8'))
    log = (f'[SAVE] resolved_root={save_root}\n'
           f'[SAVE] path_layer_root={save_root}\n').encode('utf-8')
    (run_dir / 'jsrf_run.log').write_bytes(log)
    metadata['run_log_sha256'] = sha256(log)
    (run_dir / 'metadata.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    return metadata


def archived_xbe_for(run_dir: Path) -> Path:
    return run_dir.parents[2] / 'game' / 'default.xbe'


PROFILE_FIXTURE_MANIFEST = (
    ('GPU default absent', [], EXPLORATORY),
    ('GPU exact zero', settings(RECOMP_GPU_ACK='0'), STRICT),
    ('GPU empty', settings(RECOMP_GPU_ACK=''), EXPLORATORY),
    ('AC97 present zero', settings(RECOMP_GPU_ACK='0', RECOMP_AC97_READY='0'), EXPLORATORY),
    ('unresolved present empty', settings(RECOMP_GPU_ACK='0', JSRF_ALLOW_UNRESOLVED=''), EXPLORATORY),
    ('ABI continuation present zero', settings(RECOMP_GPU_ACK='0', JSRF_ABI_CONTINUE='0'), EXPLORATORY),
    ('APU zero addresses', settings(RECOMP_GPU_ACK='0', RECOMP_APU_DSP_ACK='0,0,0'), STRICT),
    ('APU active ninth token', settings(RECOMP_GPU_ACK='0',
                                         RECOMP_APU_DSP_ACK='0,0,0,0,0,0,0,0,1'), EXPLORATORY),
    ('APU unsupported input', settings(RECOMP_GPU_ACK='0', RECOMP_APU_DSP_ACK='bad'), UNKNOWN),
    ('case-insensitive duplicate GPU setting', [
        {'name': 'RECOMP_GPU_ACK', 'value': '0'},
        {'name': 'recomp_gpu_ack', 'value': '1'},
    ], UNKNOWN),
)


class ProfileClassifierTests(unittest.TestCase):
    def test_fixture_manifest_expected_labels(self):
        for name, environment, expected in PROFILE_FIXTURE_MANIFEST:
            with self.subTest(fixture=name):
                self.assertEqual(classify_settings(environment)['classification'], expected)

    def test_gpu_ack_uses_default_and_exact_zero_semantics(self):
        self.assertEqual(classify_settings([])['classification'], EXPLORATORY)
        self.assertEqual(classify_settings(settings(RECOMP_GPU_ACK='0'))['classification'], STRICT)
        for value in ('', 'false', '1', '00'):
            with self.subTest(value=value):
                self.assertEqual(
                    classify_settings(settings(RECOMP_GPU_ACK=value))['classification'],
                    EXPLORATORY)

    def test_presence_flags_are_active_even_when_empty_or_zero(self):
        for name in ('RECOMP_AC97_READY', 'JSRF_ALLOW_UNRESOLVED', 'JSRF_ABI_CONTINUE'):
            for value in ('', '0', 'false', '1'):
                with self.subTest(name=name, value=value):
                    self.assertEqual(
                        classify_settings(settings(RECOMP_GPU_ACK='0', **{name: value}))[
                            'classification'], EXPLORATORY)

    def test_upstream_synthetic_switches_are_exploratory(self):
        """v0.12 bring-up switches that fake an answer (policy: upstream-merge rule 2)."""
        for name in ('RECOMP_DSP_ACK', 'RECOMP_POKE', 'RECOMP_FORCE_RETURN',
                     'RECOMP_PAD_PRESS'):
            for value in ('', '0', '0x804A8810', '1'):
                with self.subTest(name=name, value=value):
                    self.assertEqual(
                        classify_settings(settings(RECOMP_GPU_ACK='0', **{name: value}))[
                            'classification'], EXPLORATORY)

    def test_fork_fix_switches_are_exploratory(self):
        """Legacy kernel memory semantics and unadmitted NV2A behaviour."""
        for name in ('RECOMP_KMEM_LEGACY', 'RECOMP_NV2A_ACTIONS', 'RECOMP_GUEST_SERIAL'):
            for value in ('', '0', '1'):
                with self.subTest(name=name, value=value):
                    self.assertEqual(
                        classify_settings(settings(RECOMP_GPU_ACK='0', **{name: value}))[
                            'classification'], EXPLORATORY)

    def test_unknown_method_admission_and_worker_mode_are_exploratory(self):
        """Admitted-unknown NV2A methods are not executed; inline workers change semantics."""
        for name in ('RECOMP_NV2A_ADMIT_UNKNOWN', 'RECOMP_WORKERS'):
            for value in ('', '0', '1', 'inline'):
                with self.subTest(name=name, value=value):
                    result = classify_settings(
                        settings(RECOMP_GPU_ACK='0', **{name: value}))
                    self.assertEqual(result['classification'], EXPLORATORY)
                    self.assertTrue(any(name in reason for reason in result['reasons']),
                                    result['reasons'])

    def test_live_fence_mirror_is_exploratory(self):
        """The live fence mirror answers the guest's fence read before the walk commits."""
        name = 'RECOMP_FENCE_MIRROR_LIVE'
        for value in ('', '0', '1', 'anything'):
            with self.subTest(value=value):
                result = classify_settings(settings(RECOMP_GPU_ACK='0', **{name: value}))
                self.assertEqual(result['classification'], EXPLORATORY)
                self.assertTrue(any(name in reason for reason in result['reasons']),
                                result['reasons'])

    def test_upstream_capability_switches_stay_strict(self):
        """Real capability or observation, not a faked answer."""
        for name in ('RECOMP_ASYNC_IO', 'RECOMP_USB_HC', 'RECOMP_USB_NDP',
                     'RECOMP_KEYBOARD', 'RECOMP_IRQL_TRACE', 'RECOMP_WATCH',
                     'RECOMP_UNIMPL_TRAP', 'RECOMP_GUEST_METER', 'RECOMP_FFP_TRACE',
                     'RECOMP_TRACE_FLIP', 'RECOMP_VP', 'RECOMP_RDATA_GUARD',
                     'RECOMP_READ_DIRECT'):
            with self.subTest(name=name):
                self.assertEqual(
                    classify_settings(settings(RECOMP_GPU_ACK='0', **{name: '1'}))[
                        'classification'], STRICT)

    def test_apu_ack_counts_nonzero_addresses_not_tokens(self):
        self.assertEqual(parse_apu_ack_addresses('0,0,0,0,0,0,0,0,1'), [1])
        self.assertEqual(parse_apu_ack_addresses('1,2,3,4,5,6,7,8,malformed'),
                         [1, 2, 3, 4, 5, 6, 7, 8])
        self.assertIsNone(parse_apu_ack_addresses('0' * 256))
        for value in ('', '0', '0,0,0x0'):
            self.assertEqual(parse_apu_ack_addresses(value), [])
            self.assertEqual(
                classify_settings(settings(RECOMP_GPU_ACK='0', RECOMP_APU_DSP_ACK=value))[
                    'classification'], STRICT)
        self.assertEqual(
            classify_settings(settings(RECOMP_GPU_ACK='0', RECOMP_APU_DSP_ACK='0x803C0810'))[
                'classification'], EXPLORATORY)
        self.assertEqual(
            classify_settings(settings(RECOMP_GPU_ACK='0', RECOMP_APU_DSP_ACK='not-an-address'))[
                'classification'], UNKNOWN)

    def test_environment_names_are_case_insensitive_and_duplicates_fail_closed(self):
        self.assertEqual(
            classify_settings([{'name': 'recomp_gpu_ack', 'value': '0'}])['classification'],
            STRICT)
        duplicate = [
            {'name': 'RECOMP_GPU_ACK', 'value': '0'},
            {'name': 'recomp_gpu_ack', 'value': '1'},
        ]
        self.assertEqual(classify_settings(duplicate)['classification'], UNKNOWN)
        self.assertEqual(classify_settings([{'name': 'RECOMP_GPU_ACK'}])['classification'], UNKNOWN)


class RelocatedCheckoutTests(unittest.TestCase):
    """A renamed checkout must not invalidate the archives it produced.

    Measured defect (2026-10-05): the game directory moved from
    `C:\\Users\\logic\\Repos\\my_xbox_game` to `...\\poison-jam`.  Every archive
    written before the move records the OLD absolute root in `save_root.*`,
    `xbe_path` and every element of `command`, because those are the paths the run
    really used.  Comparing them literally against the new location classified
    **all 133 archived runs** UNKNOWN -- including **all 26 strict** ones -- while
    `just check` stayed green.  That is the whole strict-evidence base, silently.

    The fix is relocation-aware comparison, never rewriting the archives: an
    archive's recorded paths are a fact about the machine the run happened on, and
    editing them would fabricate provenance.
    """

    OLD = r'C:\Users\logic\Repos\my_xbox_game'
    NEW = r'C:\Users\logic\Repos\poison-jam'

    def test_known_roots_are_exactly_the_recorded_move(self):
        self.assertEqual(jsrf_run_profile.RELOCATED_ROOTS, {self.OLD: self.NEW})

    def test_a_moved_root_denotes_the_same_place(self):
        self.assertTrue(jsrf_run_profile._paths_agree(
            self.OLD + r'\logs\runs\r1', self.NEW + r'\logs\runs\r1'))
        self.assertTrue(jsrf_run_profile._paths_agree(
            self.OLD, self.NEW))

    def test_relocation_is_symmetric_and_case_insensitive(self):
        """Either side may be the relocated one; Windows paths ignore case."""
        self.assertTrue(jsrf_run_profile._paths_agree(
            self.NEW + r'\logs\runs\r1', self.OLD + r'\logs\runs\r1'))
        self.assertTrue(jsrf_run_profile._paths_agree(
            self.OLD.upper() + r'\LOGS\RUNS\R1', self.NEW + r'\logs\runs\r1'))

    def test_a_different_root_still_fails(self):
        """The control that matters: the check must not become a blanket pass."""
        self.assertFalse(jsrf_run_profile._paths_agree(
            r'C:\Users\logic\Repos\some_other_game\logs\runs\r1',
            self.NEW + r'\logs\runs\r1'))
        # A sibling whose name merely shares a prefix must not be relocated:
        # `my_xbox_game_backup` is a different directory.
        self.assertFalse(jsrf_run_profile._paths_agree(
            self.OLD + r'_backup\logs\runs\r1', self.NEW + r'\logs\runs\r1'))
        self.assertFalse(jsrf_run_profile._paths_agree(
            r'D:\elsewhere', self.NEW))

    def test_relocation_does_not_reach_into_the_middle_of_a_path(self):
        self.assertFalse(jsrf_run_profile._paths_agree(
            r'C:\copy\of' + self.OLD + r'\logs', self.NEW + r'\logs'))

    def test_a_moved_archive_reclassifies_strict(self):
        """The end-to-end control: the archive's own paths are left untouched.

        The scratch archive cannot live at the real repository root, so the
        relocation map is pointed at the scratch root instead: the archive is
        recorded under `OLD` and physically lives under `<scratch>`, which is
        structurally identical to the real case (an archive recorded under the
        pre-rename root, physically inside the renamed checkout).  The production
        map is asserted separately, unchanged, above.
        """
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / 'logs' / 'runs' / 'moved'
            source_xbe = root / 'game' / 'default.xbe'
            metadata = write_valid_archive(run_dir, source_xbe)
            self.assertEqual(
                classify_run_directory(run_dir, source_xbe)['classification'], STRICT)

            # Rewrite every recorded path to the pre-rename root, exactly as the
            # 133 real archives have it, and leave the archive where it is.  The
            # substitution is on the CHECKOUT root, so each path keeps its own
            # `logs/runs/<name>` suffix, as the real archives do.
            moved = json.loads(json.dumps(metadata))
            new_root, old_root = str(root.resolve()), self.OLD
            moved['xbe_path'] = moved['xbe_path'].replace(new_root, old_root)
            for key in ('archive_root', 'expected_resolved_path',
                        'observed_resolved_path', 'observed_path_layer_root'):
                moved['save_root'][key] = moved['save_root'][key].replace(new_root, old_root)
            moved['command'] = [part.replace(new_root, old_root)
                                for part in moved['command']]
            (run_dir / 'metadata.json').write_text(json.dumps(moved), encoding='utf-8')

            # Without the relocation the archive is UNKNOWN -- this is the real
            # measured defect, reproduced.
            self.assertEqual(
                classify_run_directory(run_dir, source_xbe)['classification'], UNKNOWN,
                'the pre-fix behaviour must be reproducible or this control proves nothing')

            with patch.dict(jsrf_run_profile.RELOCATED_ROOTS,
                            {self.OLD: str(root.resolve())}):
                self.assertEqual(
                    classify_run_directory(run_dir, source_xbe)['classification'], STRICT,
                    'a relocated checkout must not invalidate an archive')

    def test_a_mixed_root_archive_is_still_unknown(self):
        """Half old, half new: the save-root witnesses must still agree."""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / 'logs' / 'runs' / 'mixed'
            source_xbe = root / 'game' / 'default.xbe'
            metadata = write_valid_archive(run_dir, source_xbe)

            mixed = json.loads(json.dumps(metadata))
            # `expected` moves with the checkout; `observed` names a third place
            # entirely, so the two witnesses genuinely disagree.
            mixed['save_root']['observed_resolved_path'] = r'C:\somewhere\else\save-root'
            result = reclassify_metadata(mixed, run_dir, source_xbe)
            self.assertEqual(result['classification'], UNKNOWN)

    def test_a_foreign_xbe_path_is_still_unknown(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / 'logs' / 'runs' / 'foreign-xbe'
            source_xbe = root / 'game' / 'default.xbe'
            metadata = write_valid_archive(run_dir, source_xbe)
            foreign = json.loads(json.dumps(metadata))
            foreign['xbe_path'] = r'C:\another\title\default.xbe'
            self.assertEqual(
                reclassify_metadata(foreign, run_dir, source_xbe)['classification'],
                UNKNOWN)


class ArchiveClassifierTests(unittest.TestCase):
    def test_versioned_archive_reclassifies_and_rejects_conflicts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / 'logs' / 'runs' / 'valid'
            source_xbe = root / 'game' / 'default.xbe'
            metadata = write_valid_archive(run_dir, source_xbe)
            self.assertEqual(classify_run_directory(run_dir, source_xbe)['classification'], STRICT)

            bad_settings = json.loads(json.dumps(metadata))
            bad_settings['settings'] = []
            self.assertEqual(reclassify_metadata(bad_settings)['classification'], UNKNOWN)

            bad_path = json.loads(json.dumps(metadata))
            bad_path['save_root']['observed_resolved_path'] = 'C:\\other'
            self.assertEqual(reclassify_metadata(bad_path)['classification'], UNKNOWN)

            missing_command = json.loads(json.dumps(metadata))
            missing_command.pop('command')
            self.assertEqual(reclassify_metadata(missing_command)['classification'], UNKNOWN)

    def test_archived_hashes_and_runtime_log_witnesses_are_rechecked(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / 'logs' / 'runs' / 'integrity'
            source_xbe = root / 'game' / 'default.xbe'
            write_valid_archive(run_dir, source_xbe)
            self.assertEqual(classify_run_directory(run_dir, source_xbe)['classification'], STRICT)

            metadata_path = run_dir / 'metadata.json'
            metadata = json.loads(metadata_path.read_text(encoding='utf-8'))
            metadata['xbe_path'] = str(source_xbe.parent / 'other.xbe')
            metadata_path.write_text(json.dumps(metadata), encoding='utf-8')
            self.assertEqual(classify_run_directory(run_dir, source_xbe)['classification'], UNKNOWN)

            exe_path = run_dir / 'jsrf_recomp.exe'
            exe_path.write_bytes(exe_path.read_bytes() + b'changed')
            self.assertEqual(classify_run_directory(run_dir, source_xbe)['classification'], UNKNOWN)

            write_valid_archive(run_dir, source_xbe)
            (run_dir / 'jsrf_recomp.map').unlink()
            self.assertEqual(classify_run_directory(run_dir, source_xbe)['classification'], UNKNOWN)

            write_valid_archive(run_dir, source_xbe)
            log_path = run_dir / 'jsrf_run.log'
            metadata_path = run_dir / 'metadata.json'
            metadata = json.loads(metadata_path.read_text(encoding='utf-8'))
            expected_root = metadata['save_root']['expected_resolved_path']
            changed_log = (f'[SAVE] resolved_root={expected_root}\n'
                           f'[SAVE] path_layer_root={expected_root}-wrong\n').encode('utf-8')
            log_path.write_bytes(changed_log)
            metadata['run_log_sha256'] = sha256(changed_log)
            metadata_path.write_text(json.dumps(metadata), encoding='utf-8')
            self.assertEqual(classify_run_directory(run_dir, source_xbe)['classification'], UNKNOWN)

            write_valid_archive(run_dir, source_xbe)
            metadata_path = run_dir / 'metadata.json'
            metadata = json.loads(metadata_path.read_text(encoding='utf-8'))
            metadata['run_log_sha256'] = sha256((run_dir / 'jsrf_run.log').read_bytes())
            metadata_path.write_text(json.dumps(metadata), encoding='utf-8')
            log_path.unlink()
            self.assertEqual(classify_run_directory(run_dir, source_xbe)['classification'], UNKNOWN)

    def test_unknown_schema_and_boolean_version_never_fall_back_to_strict(self):
        legacy = {'settings': settings(RECOMP_GPU_ACK='0')}
        self.assertEqual(reclassify_metadata(legacy)['classification'], UNKNOWN)
        self.assertEqual(
            reclassify_metadata({'schema_version': 999, **legacy})['classification'], UNKNOWN)
        self.assertEqual(
            reclassify_metadata({'classifier_version': 'future', **legacy})['classification'],
            UNKNOWN)

        metadata = valid_archive()
        metadata['run_profile']['schema_version'] = True
        self.assertEqual(reclassify_metadata(metadata)['classification'], UNKNOWN)

    def test_only_supported_legacy_environment_shapes_are_classified(self):
        self.assertEqual(
            reclassify_metadata({'settings': {'environment': settings(RECOMP_GPU_ACK='0')}})[
                'classification'], UNKNOWN)
        self.assertEqual(
            reclassify_metadata({'settings': {'env': settings(RECOMP_GPU_ACK='0')}})[
                'classification'], UNKNOWN)
        historical_override = {'settings': settings(
            RECOMP_GPU_ACK='0', RECOMP_AC97_READY='1')}
        self.assertEqual(reclassify_metadata(historical_override)['classification'], EXPLORATORY)
        self.assertEqual(reclassify_metadata({'settings': {}})['classification'], UNKNOWN)
        self.assertEqual(reclassify_metadata({})['classification'], UNKNOWN)

    def test_legacy_safe_environment_cannot_pass_without_command_or_repo_identity(self):
        safe_legacy = {'settings': settings(RECOMP_GPU_ACK='0')}
        with_command = {**safe_legacy, 'command': ['jsrf_collect.exe']}
        with_identity = {**safe_legacy, 'repository_identities': {'project': {}, 'toolkit': {}}}
        for record in (safe_legacy, with_command, with_identity):
            with self.subTest(record=record.keys()):
                self.assertEqual(reclassify_metadata(record)['classification'], UNKNOWN)

    def test_checker_cli_returns_nonzero_for_legacy_settings_without_strict_provenance(self):
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = Path(temporary) / 'legacy-safe-settings'
            run_dir.mkdir()
            (run_dir / 'metadata.json').write_text(json.dumps({
                'settings': settings(RECOMP_GPU_ACK='0'),
            }), encoding='utf-8')
            completed = subprocess.run(
                [sys.executable, '-X', 'utf8', str(SCRIPTS / 'check-run-profile.py'),
                 str(run_dir)], capture_output=True, text=True, check=False)
            self.assertEqual(completed.returncode, 1)
            self.assertIn('UNKNOWN', completed.stdout)

    def test_missing_directory_and_duplicate_json_keys_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.assertEqual(classify_run_directory(root / 'absent')['classification'], MISSING)
            run_dir = root / 'duplicate'
            run_dir.mkdir()
            (run_dir / 'metadata.json').write_text(
                '{"settings":[],"settings":[{"name":"RECOMP_GPU_ACK","value":"0"}]}',
                encoding='utf-8')
            self.assertEqual(classify_run_directory(run_dir)['classification'], UNKNOWN)


class RunnerPreflightTests(unittest.TestCase):
    @staticmethod
    def prepare_workspace(root: Path) -> tuple[Path, Path]:
        build = root / 'build' / 'Release'
        for directory in (build, root / 'game', root / 'scripts', root / 'src',
                          root / 'config', root / 'tests', root / 'tools' / 'harness'):
            directory.mkdir(parents=True, exist_ok=True)
        for name in run_jsrf.ARTIFACTS:
            if name != 'build-source.json':
                (build / name).write_bytes(name.encode('utf-8'))
        build_source = {
            'exe_sha256': sha256((build / 'jsrf_recomp.exe').read_bytes()),
            'artifacts': {name: sha256((build / name).read_bytes())
                          for name in run_jsrf.ARTIFACTS if name != 'build-source.json'},
        }
        (build / 'build-source.json').write_text(json.dumps(build_source), encoding='utf-8')
        (root / 'game' / 'default.xbe').write_bytes(b'xbe fixture')
        (root / 'CMakeLists.txt').write_text('# fixture\n', encoding='utf-8')
        for name in ('build-identity.py', 'archive-run-source.py', 'jsrf_gpu.py', 'run-jsrf.py'):
            (root / 'scripts' / name).write_text('# fixture\n', encoding='utf-8')
        return build, root.parent / 'toolkit fixture'

    def run_with_fake_process(self, root: Path, arguments: list[str],
                              environment: dict[str, str],
                              fail_save_root: bool = False,
                              marker_mode: str = 'matching') -> tuple[int, list[list[str]], Path | None]:
        build, toolkit = self.prepare_workspace(root)
        commands: list[list[str]] = []

        class FakeProcess:
            def __init__(self, command, **_kwargs):
                self.command = list(command)
                commands.append(self.command)
                run_dir = Path(command[2])
                save_argument = next(arg for arg in command if arg.startswith('--save-root='))
                save_path = save_argument.partition('=')[2]
                save_dir = Path(save_path)
                if (not save_dir.is_dir() or save_dir.parent.resolve() != run_dir.resolve()
                        or next(save_dir.iterdir(), None) is not None):
                    raise AssertionError('runner did not pass a new empty save-root inside the archive')
                probe = next((arg.partition('=')[2] for arg in command
                              if arg.startswith('--probe=')), '')
                checkpoints = ['probe_gpu'] if probe.startswith('gpu-') else [
                    'memory_ready', 'guest_entry']
                log = f'[SAVE] resolved_root={save_path}\n'
                if marker_mode == 'matching':
                    log += f'[SAVE] path_layer_root={save_path}\n'
                elif marker_mode == 'mismatch':
                    log += f'[SAVE] path_layer_root={save_path}-other\n'
                log += ''.join(f'[CHECKPOINT] {name}\n' for name in checkpoints)
                (run_dir / 'jsrf_run.log').write_text(log, encoding='utf-8')
                (run_dir / 'result.json').write_text(
                    json.dumps({'outcome': 'normal_exit', 'exit_code': 0}), encoding='utf-8')

            def wait(self, timeout=None):
                return 0

            def poll(self):
                return 0

            def kill(self):
                pass

        def archive_project(run_dir: Path) -> bool:
            (run_dir / 'project.patch').write_bytes(b'project patch')
            (run_dir / 'project-status.txt').write_text('', encoding='utf-8')
            return True

        def fake_git(_repo, *args, binary=False):
            if binary:
                return b'toolkit patch'
            if 'rev-parse' in args:
                return 'f' * 40 + '\n'
            return ''

        runner_globals = patch.multiple(
            run_jsrf, ROOT=root, BUILD=build, TOOLKIT=toolkit,
            project_game_running=lambda _root: False,
            archive_project_state=archive_project,
            run_git=fake_git,
        )
        run_mkdir = Path.mkdir

        def maybe_fail_save(path, *args, **kwargs):
            if fail_save_root and path.name == 'save-root':
                raise OSError('isolated root unavailable')
            return run_mkdir(path, *args, **kwargs)

        with (runner_globals, patch.dict(os.environ, environment, clear=True),
              patch.object(sys, 'argv', ['run-jsrf.py', *arguments]),
              patch.object(run_jsrf.subprocess, 'run', return_value=SimpleNamespace(returncode=0)),
              patch.object(run_jsrf.subprocess, 'Popen', side_effect=FakeProcess),
              patch.object(Path, 'mkdir', maybe_fail_save if fail_save_root else run_mkdir),
              contextlib.redirect_stdout(io.StringIO()),
              contextlib.redirect_stderr(io.StringIO())):
            result = run_jsrf.main()
        saved = None
        if commands:
            saved = Path(commands[0][2])
        return result, commands, saved

    def test_default_strict_rejects_before_any_child_without_caller_gpu_ack(self):
        for environment in ({}, {'RECOMP_GPU_ACK': '0', 'RECOMP_AC97_READY': '0'},
                            {'RECOMP_GPU_ACK': 'false'}):
            with self.subTest(environment=environment):
                stdout, stderr = io.StringIO(), io.StringIO()
                with (patch.dict(os.environ, environment, clear=True),
                      patch.object(sys, 'argv', ['run-jsrf.py']),
                      patch.object(run_jsrf.subprocess, 'run') as run,
                      patch.object(run_jsrf.subprocess, 'Popen') as popen,
                      contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr)):
                    self.assertEqual(run_jsrf.main(), 2)
                run.assert_not_called()
                popen.assert_not_called()
                if 'RECOMP_GPU_ACK' not in environment:
                    self.assertIn('will not insert', stderr.getvalue())

    def test_probe_default_records_fixture_and_forwards_save_root_as_one_argument(self):
        with tempfile.TemporaryDirectory(prefix='runner workspace with spaces ') as temporary:
            old_save = Path(temporary) / 'preexisting-save'
            old_save.mkdir()
            sentinel = old_save / 'sentinel.bin'
            sentinel.write_bytes(b'keep this disposable sentinel unchanged')
            original_sentinel = sentinel.read_bytes()
            result, commands, run_dir = self.run_with_fake_process(
                Path(temporary), ['--probe', 'gpu-submit-supported', '--expect-checkpoint',
                                  'probe_gpu', '--label', 'space-test'], {})
            self.assertEqual(result, 0)
            self.assertEqual(len(commands), 1)
            command = commands[0]
            save_argument = next(arg for arg in command if arg.startswith('--save-root='))
            self.assertEqual(command[-1], '--probe=gpu-submit-supported')
            self.assertIn(' ', save_argument)
            metadata = json.loads((run_dir / 'metadata.json').read_text(encoding='utf-8'))
            self.assertEqual(metadata['run_profile']['requested_profile'], FIXTURE)
            self.assertTrue(metadata['save_root']['verified'])
            self.assertTrue(metadata['save_root']['disposable'])
            self.assertEqual(metadata['save_root']['observed_path_layer_root'],
                             metadata['save_root']['expected_resolved_path'])
            self.assertEqual(sentinel.read_bytes(), original_sentinel)
            self.assertEqual(classify_run_directory(run_dir, archived_xbe_for(run_dir))['classification'], FIXTURE)

    def test_strict_launch_keeps_caller_gpu_ack_and_archives_strict_identity(self):
        with tempfile.TemporaryDirectory(prefix='ordinary runner path with spaces ') as temporary:
            result, commands, run_dir = self.run_with_fake_process(
                Path(temporary), ['--label', 'strict-test'], {'RECOMP_GPU_ACK': '0'})
            self.assertEqual(result, 0)
            self.assertEqual(len(commands), 1)
            self.assertTrue(any(arg.startswith('--save-root=') for arg in commands[0]))
            metadata = json.loads((run_dir / 'metadata.json').read_text(encoding='utf-8'))
            self.assertEqual(metadata['settings'], metadata['run_profile']['effective_settings'])
            inherited = {entry['name'].casefold(): entry['value']
                         for entry in metadata['run_profile']['inherited_settings']}
            effective = {entry['name'].casefold(): entry['value']
                         for entry in metadata['run_profile']['effective_settings']}
            self.assertEqual(inherited['recomp_gpu_ack'], '0')
            self.assertEqual(effective['recomp_gpu_ack'], '0')
            self.assertEqual(classify_run_directory(run_dir, archived_xbe_for(run_dir))['classification'], STRICT)

    def test_explicit_exploratory_run_archives_reasons(self):
        with tempfile.TemporaryDirectory() as temporary:
            result, commands, run_dir = self.run_with_fake_process(
                Path(temporary), ['--profile', 'exploratory', '--label', 'exploratory-test'],
                {'RECOMP_GPU_ACK': '0', 'RECOMP_AC97_READY': '0'})
            self.assertEqual(result, 0)
            self.assertEqual(len(commands), 1)
            metadata = json.loads((run_dir / 'metadata.json').read_text(encoding='utf-8'))
            self.assertEqual(classify_run_directory(run_dir, archived_xbe_for(run_dir))['classification'], EXPLORATORY)
            self.assertTrue(any('RECOMP_AC97_READY' in reason
                                for reason in metadata['run_profile']['reasons']))

    def test_missing_or_mismatched_path_layer_witness_is_unknown_and_nonzero(self):
        for mode in ('missing', 'mismatch'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                result, commands, run_dir = self.run_with_fake_process(
                    Path(temporary), ['--label', f'{mode}-root'], {'RECOMP_GPU_ACK': '0'},
                    marker_mode=mode)
                self.assertEqual(len(commands), 1)
                self.assertEqual(result, 2)
                self.assertEqual(classify_run_directory(run_dir, archived_xbe_for(run_dir))['classification'], UNKNOWN)

    def test_save_root_creation_failure_does_not_start_collector(self):
        with tempfile.TemporaryDirectory() as temporary:
            result, commands, _run_dir = self.run_with_fake_process(
                Path(temporary), ['--label', 'no-save-root'], {'RECOMP_GPU_ACK': '0'},
                fail_save_root=True)
            self.assertEqual(result, 2)
            self.assertEqual(commands, [])

    def test_fixture_and_ordinary_probe_combinations_are_rejected_during_argument_parse(self):
        for arguments in (['--profile', 'fixture'],
                          ['--profile', 'exploratory', '--probe', 'gpu-submit-supported']):
            with self.subTest(arguments=arguments), patch.object(
                    sys, 'argv', ['run-jsrf.py', *arguments]), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as exit_info:
                    run_jsrf.parse_args()
                self.assertEqual(exit_info.exception.code, 2)


class RetiredOverrideTests(unittest.TestCase):
    """Retired overrides are revision-relative, and fail closed at launch.

    The population under test is the explicit RETIRED_OVERRIDES registry, not
    "every name the classifier does not enumerate".  A name the runtime does not
    read cannot make a run exploratory, so unknown-but-unread names must keep
    launching -- the negative controls below assert that directly.
    """

    def test_registry_population_is_explicit_and_dated(self):
        self.assertEqual(set(RETIRED_OVERRIDES), {VBLANK},
                         'retired population changed; update controls with it')
        for name, info in RETIRED_OVERRIDES.items():
            with self.subTest(name=name):
                for field in ('added_commit', 'removed_commit', 'removed_on', 'note'):
                    self.assertTrue(info.get(field), f'{name} is missing {field}')
                self.assertRegex(info['removed_commit'], r'^[0-9a-f]{40}$')

    def test_classifier_does_not_treat_retired_name_as_semantic(self):
        """Positive control: the name is annotated, and the verdict is unchanged.

        Current-binary semantics have no VBLANK behavior, so it must NOT flip the
        verdict on its own -- encoding it as permanently exploratory would
        contradict AC1's own method.
        """
        result = classify_settings(settings(RECOMP_GPU_ACK='0', RECOMP_VBLANK='1'))
        self.assertEqual(result['classification'], STRICT)
        self.assertEqual(result['reasons'], [])
        self.assertEqual(len(result['retired_overrides']), 1)
        self.assertIn(VBLANK, result['retired_overrides'][0])
        self.assertIn('retired', result['retired_overrides'][0])

    def test_strict_launch_rejects_retired_override(self):
        """Negative control: the combination the reviewer named must be rejected."""
        with self.assertRaises(ProfileError) as raised:
            validate_launch_profile('strict', '', settings(RECOMP_GPU_ACK='0',
                                                          RECOMP_VBLANK='1'))
        self.assertIn(VBLANK, str(raised.exception))
        self.assertIn('retired', str(raised.exception))

    def test_strict_launch_still_accepts_unread_and_observation_names(self):
        """Negative control for over-rejection: unknown/observation names launch.

        AGENTS.md instructs every session to set RECOMP_KERNEL_LOG_BUDGET, and the
        runtime reads names the profile document never lists.  Neither is
        contamination, so neither may be refused.
        """
        for name in ('RECOMP_KERNEL_LOG_BUDGET', 'RECOMP_MMIO_TRACE',
                     'RECOMP_KERNEL_WATCH_ALL', 'RECOMP_CS_MODE',
                     'JSRF_TRACE_HEAP', 'RECOMP_TRACE_ARGS'):
            with self.subTest(name=name):
                classified = validate_launch_profile(
                    'strict', '', settings(RECOMP_GPU_ACK='0', **{name: '1'}))
                self.assertEqual(classified['classification'], STRICT)

    def test_retired_override_annotation_follows_recorded_revision(self):
        if not TOOLKIT_ROOT.is_dir():
            self.skipTest(f'toolkit tree unavailable at {TOOLKIT_ROOT}')
        cases = (
            (VBLANK_HONORING_REVISION, True),
            (VBLANK_INERT_REVISION, False),
        )
        for revision, expected in cases:
            with self.subTest(revision=revision[:12]):
                notes, unresolved = resolve_retired_overrides(
                    settings(**{VBLANK: '1'}), revision, TOOLKIT_ROOT)
                self.assertEqual(unresolved, [])
                self.assertEqual(notes[0]['honored_by_that_binary'], expected)

    def test_unresolvable_revision_is_unknown_not_clean(self):
        """Missing or bogus revisions must not silently become strict."""
        if not TOOLKIT_ROOT.is_dir():
            self.skipTest(f'toolkit tree unavailable at {TOOLKIT_ROOT}')
        for revision in (None, '', 'not-a-revision', 'deadbeef'):
            with self.subTest(revision=revision):
                notes, unresolved = resolve_retired_overrides(
                    settings(**{VBLANK: '1'}), revision, TOOLKIT_ROOT)
                self.assertEqual(unresolved, [VBLANK])
                self.assertIsNone(notes[0]['honored_by_that_binary'])

    def test_legacy_archive_verdict_follows_recorded_revision(self):
        """The three archive outcomes the advisor ruling distinguishes."""
        if not TOOLKIT_ROOT.is_dir():
            self.skipTest(f'toolkit tree unavailable at {TOOLKIT_ROOT}')
        environment = settings(RECOMP_GPU_ACK='0', **{VBLANK: '1'})

        honored = reclassify_metadata(
            {'toolkit_revision': VBLANK_HONORING_REVISION, 'settings': environment},
            None, None, TOOLKIT_ROOT)
        self.assertEqual(honored['classification'], EXPLORATORY)
        self.assertTrue(any(VBLANK in reason for reason in honored['reasons']))

        inert = reclassify_metadata(
            {'toolkit_revision': VBLANK_INERT_REVISION, 'settings': environment},
            None, None, TOOLKIT_ROOT)
        # The binary had already lost the override, so it contributes nothing;
        # the legacy archive is still UNKNOWN for lack of strict provenance.
        self.assertEqual(inert['classification'], UNKNOWN)
        self.assertFalse(any(VBLANK in reason for reason in inert['reasons']))
        self.assertEqual(len(inert['retired_overrides']), 1)

        missing = reclassify_metadata({'settings': environment}, None, None, TOOLKIT_ROOT)
        self.assertEqual(missing['classification'], UNKNOWN)
        self.assertTrue(any(VBLANK in reason for reason in missing['reasons']))

    def test_archive_without_retired_names_is_unaffected(self):
        """Negative control: the layer is inert when no retired name is present."""
        result = reclassify_metadata(
            {'toolkit_revision': VBLANK_INERT_REVISION,
             'settings': settings(RECOMP_GPU_ACK='0')}, None, None, TOOLKIT_ROOT)
        self.assertNotIn('retired_overrides', result)

    def test_strict_record_refuses_retired_name_on_either_side(self):
        """A strict archive must not be recordable with a retired name.

        Found by this session's own falsification pass: the launch gate only
        inspected *inherited* settings, so a strict record whose effective
        settings had gained a retired name was accepted.  A strict archive whose
        recorded environment names an override the current binary never reads is
        a self-contradicting record.
        """
        from jsrf_run_profile import make_profile_record
        clean = settings(RECOMP_GPU_ACK='0')
        contaminated = settings(RECOMP_GPU_ACK='0', **{VBLANK: '1'})
        for label, inherited, effective in (
                ('effective gained it', clean, contaminated),
                ('inherited carried it', contaminated, clean),
                ('both carried it', contaminated, contaminated)):
            with self.subTest(case=label):
                with self.assertRaises(ProfileError) as raised:
                    make_profile_record('strict', inherited, effective, '')
                self.assertIn(VBLANK, str(raised.exception))
        # Positive controls: a clean strict record and an exploratory record
        # that legitimately carries the retired name both still work.
        self.assertEqual(
            make_profile_record('strict', clean, clean, '')['classification'], STRICT)
        self.assertEqual(
            make_profile_record('exploratory', contaminated, contaminated,
                                '')['classification'], EXPLORATORY)

    def test_strict_archive_validation_rejects_retired_effective_settings(self):
        """The checker side of the same rule.

        The fixture carries the fields validated *before* the retired check, so
        the rejection is attributable to the retired name rather than to an
        earlier missing field.
        """
        contaminated = settings(RECOMP_GPU_ACK='0', **{VBLANK: '1'})
        metadata = {
            'toolkit_revision': VBLANK_INERT_REVISION,
            'probe': '',
            'settings': contaminated,
            'run_profile': {
                'schema_version': PROFILE_SCHEMA_VERSION,
                'classifier_version': 'jsrf-run-profile/1',
                'requested_profile': STRICT,
                'classification': STRICT,
                'environment_classification': STRICT,
                'reasons': [],
                'inherited_settings': settings(RECOMP_GPU_ACK='0'),
                'effective_settings': contaminated,
                'resolved_defaults': classify_settings(contaminated)['resolved_defaults'],
            },
        }
        result = reclassify_metadata(metadata, None, None, TOOLKIT_ROOT)
        self.assertEqual(result['classification'], UNKNOWN)
        self.assertTrue(any(VBLANK in reason for reason in result['reasons']),
                        f'expected a retired-override reason, got {result["reasons"]}')

    def test_strict_archive_validation_rejects_retired_inherited_settings(self):
        """Symmetric case: a retired name in the *inherited* set alone.

        Found by the independent re-review as a defense-in-depth gap: a retired
        name in `inherited` still clears the semantic gates, because
        `classify_settings` correctly reports `strict` for a name the current
        binary never reads.  A hand-forged archive could therefore claim strict.
        The runner cannot mint it (`make_profile_record` calls
        `validate_launch_profile` on the inherited set first), but the checker
        must not accept it either.
        """
        clean = settings(RECOMP_GPU_ACK='0')
        contaminated = settings(RECOMP_GPU_ACK='0', **{VBLANK: '1'})
        metadata = {
            'toolkit_revision': VBLANK_INERT_REVISION,
            'probe': '',
            'settings': clean,
            'run_profile': {
                'schema_version': PROFILE_SCHEMA_VERSION,
                'classifier_version': 'jsrf-run-profile/1',
                'requested_profile': STRICT,
                'classification': STRICT,
                'environment_classification': STRICT,
                'reasons': [],
                'inherited_settings': contaminated,
                'effective_settings': clean,
                'resolved_defaults': classify_settings(clean)['resolved_defaults'],
            },
        }
        result = reclassify_metadata(metadata, None, None, TOOLKIT_ROOT)
        self.assertEqual(result['classification'], UNKNOWN)
        self.assertTrue(any(VBLANK in reason for reason in result['reasons']),
                        f'expected a retired-override reason, got {result["reasons"]}')

    def test_byte_complete_forged_strict_archive_with_retired_inherited_is_unknown(self):
        """Drive the forged-inherited path through the full archive checker.

        The re-review proved the guard was absent but did not build a
        byte-complete archive to reach the printed verdict.  This does, so the
        end-to-end result is measured rather than inferred.
        """
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / 'logs' / 'runs' / 'forged-inherited'
            source_xbe = root / 'game' / 'default.xbe'
            metadata = write_valid_archive(run_dir, source_xbe)
            clean = settings(RECOMP_GPU_ACK='0')
            metadata['settings'] = clean
            metadata['toolkit_revision'] = VBLANK_INERT_REVISION
            metadata['run_profile'].update({
                'inherited_settings': settings(RECOMP_GPU_ACK='0', **{VBLANK: '1'}),
                'effective_settings': clean,
                'environment_classification': STRICT,
                'reasons': [],
            })
            (run_dir / 'metadata.json').write_text(
                json.dumps(metadata, indent=2), encoding='utf-8')
            result = classify_run_directory(run_dir, source_xbe)
            self.assertEqual(result['classification'], UNKNOWN,
                             'a forged strict archive with a retired inherited '
                             'setting must not classify strict')
            self.assertTrue(any(VBLANK in reason for reason in result['reasons']),
                            f'expected a retired-override reason, got {result["reasons"]}')


class DurationCapTests(unittest.TestCase):
    """The run-duration bound is shared by the launcher and the classifier.

    Both sites carried their own literal 300, so raising one alone would let a
    long run launch and then be classified UNKNOWN by the other.  These pin the
    bound at both sites, at the boundary, so the two cannot drift apart again.
    """

    def test_boundary_durations_parse_at_the_launcher(self):
        """1800 is accepted; 1801 and 0 are refused with argparse's exit code 2."""
        with patch.object(sys, 'argv', ['run-jsrf.py', '--seconds', '1800']), \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(run_jsrf.parse_args().seconds, 1800)
        for seconds in ('1801', '0'):
            with self.subTest(seconds=seconds), \
                    patch.object(sys, 'argv', ['run-jsrf.py', '--seconds', seconds]), \
                    contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as exit_info:
                    run_jsrf.parse_args()
                self.assertEqual(exit_info.exception.code, 2)

    def test_existing_durations_are_unaffected(self):
        """The pre-existing accepted values keep working."""
        for seconds in ('1', '300'):
            with self.subTest(seconds=seconds), \
                    patch.object(sys, 'argv', ['run-jsrf.py', '--seconds', seconds]), \
                    contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(run_jsrf.parse_args().seconds, int(seconds))

    def _archive_at(self, run_dir: Path, source_xbe: Path, seconds: int) -> dict:
        metadata = write_valid_archive(run_dir, source_xbe)
        metadata['seconds'] = seconds
        metadata['command'][1] = str(seconds)
        (run_dir / 'metadata.json').write_text(
            json.dumps(metadata, indent=2), encoding='utf-8')
        return metadata

    def test_archive_validity_follows_the_same_bound(self):
        """1800 stays a valid archive for both base profiles; 1801/0 go UNKNOWN.

        The base classification comes from the environment, so a strict archive
        is built from RECOMP_GPU_ACK=0 and an exploratory one from the default
        (GPU ack absent).  A duration the classifier refuses must not be able to
        keep either classification.
        """
        for label, environment, expected in (
                ('strict', settings(RECOMP_GPU_ACK='0'), STRICT),
                ('exploratory', settings(), EXPLORATORY)):
            for seconds, want in ((1800, expected), (1801, UNKNOWN), (0, UNKNOWN)):
                with self.subTest(profile=label, seconds=seconds), \
                        tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    run_dir = root / 'logs' / 'runs' / f'{label}-{seconds}'
                    source_xbe = root / 'game' / 'default.xbe'
                    metadata = self._archive_at(run_dir, source_xbe, seconds)
                    base = classify_settings(environment)
                    metadata['settings'] = environment
                    metadata['run_profile'].update({
                        'requested_profile': expected,
                        'classification': expected,
                        'environment_classification': base['classification'],
                        'reasons': base['reasons'],
                        'inherited_settings': environment,
                        'effective_settings': environment,
                        'resolved_defaults': base['resolved_defaults'],
                    })
                    (run_dir / 'metadata.json').write_text(
                        json.dumps(metadata, indent=2), encoding='utf-8')
                    result = classify_run_directory(run_dir, source_xbe)
                    self.assertEqual(
                        result['classification'], want,
                        f'{label} archive with seconds={seconds} classified '
                        f'{result["classification"]}: {result["reasons"]}')

    def test_one_shared_constant_governs_both_sites(self):
        """The bound is a single shared value, not two literals that can drift."""
        self.assertTrue(hasattr(jsrf_run_profile, 'MAX_RUN_SECONDS'),
                        'the bound must live in one shared constant')
        self.assertEqual(jsrf_run_profile.MAX_RUN_SECONDS, 1800)
        self.assertEqual(run_jsrf.MAX_RUN_SECONDS, jsrf_run_profile.MAX_RUN_SECONDS,
                         'the launcher must use the classifier\'s constant')


if __name__ == '__main__':
    unittest.main(verbosity=2)
