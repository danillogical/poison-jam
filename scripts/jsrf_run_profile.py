"""Shared JSRF run-profile rules for launches and archived metadata.

The values here mirror the runtime call sites documented in
``docs/jsrf-run-profiles.md``.  Unknown input is deliberately not treated as a
clean run: the archive checker and the launch gate use the same classifier.
"""
from __future__ import annotations

import hashlib
import json
import ntpath
import re
import subprocess
from pathlib import Path
from typing import Any


STRICT = 'strict'
EXPLORATORY = 'exploratory'
FIXTURE = 'fixture'
UNKNOWN = 'UNKNOWN'
MISSING = 'MISSING'

PROFILE_SCHEMA_VERSION = 1
CLASSIFIER_VERSION = 'jsrf-run-profile/1'
UINT32_MAX = (1 << 32) - 1

GPU_ACK = 'RECOMP_GPU_ACK'
AC97_READY = 'RECOMP_AC97_READY'
APU_DSP_ACK = 'RECOMP_APU_DSP_ACK'
ALLOW_UNRESOLVED = 'JSRF_ALLOW_UNRESOLVED'
ABI_CONTINUE = 'JSRF_ABI_CONTINUE'
DSP_ACK = 'RECOMP_DSP_ACK'
POKE = 'RECOMP_POKE'
FORCE_RETURN = 'RECOMP_FORCE_RETURN'
PAD_PRESS = 'RECOMP_PAD_PRESS'
KMEM_LEGACY = 'RECOMP_KMEM_LEGACY'
NV2A_ACTIONS = 'RECOMP_NV2A_ACTIONS'
GUEST_SERIAL = 'RECOMP_GUEST_SERIAL'

# Retired semantic overrides: names that once changed guest-visible behavior,
# whose implementation was deliberately deleted, and which active policy still
# names.  They are NOT classified by the semantic enumeration below, because a
# name the current binary never reads cannot make a run exploratory -- AC1 asks
# for the runtime's *actual* semantics, and the current runtime has none for
# these.  Their effect is revision-relative, so each entry records the boundary
# commits and the binary's toolkit revision decides whether it was honored.
#
# "Retired" is deliberately narrower than "unknown".  A name the runtime does
# not read is not evidence of contamination -- most documented overrides
# (RECOMP_KERNEL_LOG_BUDGET, RECOMP_MMIO_TRACE, ...) are observation or feature
# enablement and must keep launching.  A retired name is one that is BOTH
# documented as removed AND still asserted as synthetic completion by active
# policy AND verified inert in the current binary.
RETIRED_OVERRIDES = {
    'RECOMP_VBLANK': {
        'added_commit': 'e3caa3718c80a241b31768852fa752aa6577b66c',
        'removed_commit': '7cfbe55a4a609350dac0265c215b2fdb57ee890a',
        'removed_on': '2026-09-22',
        'note': ('asserted vblank by OR-ing into NV_PCRTC_INTR_0/NV_PMC_INTR_0, both '
                 'write-1-to-clear, so it cleared pending bits instead of setting them; '
                 'replaced by the model display clock (nv2a_vblank_pulse)'),
    },
}

PROFILE_NAMES = (STRICT, EXPLORATORY, FIXTURE)
ENV_PREFIXES = ('RECOMP_', 'JSRF_')
ARCHIVED_ARTIFACTS = (
    'jsrf_recomp.exe', 'jsrf_recomp.pdb', 'jsrf_recomp.map',
    'jsrf_collect.exe', 'jsrf_collect.pdb', 'build-source.json',
)
REPOSITORY_FILES = {
    'project': ('project.patch', 'project-status.txt'),
    'toolkit': ('toolkit.patch', 'toolkit-status.txt'),
}


class ProfileError(ValueError):
    """The requested launch profile cannot be safely classified or started."""


def environment_settings(environment: dict[str, str]) -> list[dict[str, str]]:
    """Return the recorded JSRF environment entries in stable order."""
    return [
        {'name': name, 'value': value}
        for name, value in sorted(
            environment.items(), key=lambda item: item[0].casefold())
        if name.upper().startswith(ENV_PREFIXES)
    ]


def _parse_strtoul_token(token: str) -> int | None:
    """Parse a well-formed Windows ``strtoul(..., 0)`` token.

    Runtime accepts C's base prefixes and unsigned sign conversion.  Inputs
    outside this bounded grammar are UNKNOWN instead of being guessed at.
    Values that overflow Windows ``unsigned long`` are also UNKNOWN.
    """
    token = token.strip()
    if not token:
        return None
    sign = 1
    body = token
    if body[0] in '+-':
        sign = -1 if body[0] == '-' else 1
        body = body[1:]
    if not body:
        return None
    if body.lower().startswith('0x'):
        digits = body[2:]
        if not digits or not re.fullmatch(r'[0-9a-fA-F]+', digits):
            return None
        base = 16
    elif body.startswith('0'):
        if not re.fullmatch(r'0[0-7]*', body):
            return None
        digits = body
        base = 8
    else:
        if not re.fullmatch(r'[1-9][0-9]*', body):
            return None
        digits = body
        base = 10
    value = int(digits, base)
    if value > UINT32_MAX:
        return None
    return value if sign > 0 else (-value) & UINT32_MAX


def parse_apu_ack_addresses(value: str) -> list[int] | None:
    """Parse the bounded comma-list accepted by ``dsp_ack_init``.

    ``None`` means malformed/out of the supported input domain.  The C parser
    stores at most eight addresses and ignores zero values.
    """
    if not isinstance(value, str):
        return None
    if len(value) > 255:
        return None
    if value == '':
        return []
    addresses: list[int] = []
    tokens = value.split(',')
    for token in tokens:
        if len(addresses) == 8:
            break
        parsed = _parse_strtoul_token(token)
        if parsed is None:
            return None
        if parsed:
            addresses.append(parsed)
    return addresses


def parse_settings_entries(entries: Any) -> tuple[list[dict[str, str]] | None, str | None]:
    """Validate and normalize a metadata environment list.

    Keep entries as a list until duplicates have been checked.  Dict-building
    first would silently choose one value when an archive has conflicting keys.
    """
    if not isinstance(entries, list):
        return None, 'environment settings are not a list'
    normalized: list[dict[str, str]] = []
    seen: set[str] = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict) or set(entry) != {'name', 'value'}:
            return None, f'environment setting {index} is malformed'
        name, value = entry['name'], entry['value']
        if not isinstance(name, str) or not name.upper().startswith(ENV_PREFIXES):
            return None, f'environment setting {index} has an invalid name'
        if not isinstance(value, str):
            return None, f'environment setting {name} has a non-string value'
        key = name.casefold()
        if key in seen:
            return None, f'duplicate environment setting {name}'
        seen.add(key)
        normalized.append({'name': name, 'value': value})
    normalized.sort(key=lambda item: item['name'].casefold())
    return normalized, None


def parse_save_root_markers(log: str) -> tuple[str | None, str | None]:
    """Read the option-resolution and post-path-init root witnesses."""
    resolved = re.findall(r'(?m)^\[SAVE\] resolved_root=(.+)\r?$', log)
    path_layer = re.findall(r'(?m)^\[SAVE\] path_layer_root=(.+)\r?$', log)
    return (
        resolved[0].strip() if len(resolved) == 1 else None,
        path_layer[0].strip() if len(path_layer) == 1 else None,
    )


def _digest_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _verify_archived_bytes(metadata: dict[str, Any], archive_dir: Path,
                           source_xbe: Path | None) -> list[str]:
    """Bind recorded identities to the bytes actually available to the checker."""
    reasons: list[str] = []
    recorded = metadata.get('artifact_sha256')
    if not isinstance(recorded, dict):
        return ['missing archived artifact hashes']
    for name in ARCHIVED_ARTIFACTS:
        expected = recorded.get(name)
        path = archive_dir / name
        if not _sha256(expected) or not path.is_file():
            reasons.append(f'missing or malformed archived artifact {name}')
            continue
        if _digest_file(path) != expected.lower():
            reasons.append(f'archived artifact hash mismatch for {name}')

    field_to_artifact = {
        'exe_sha256': 'jsrf_recomp.exe',
        'pdb_sha256': 'jsrf_recomp.pdb',
        'map_sha256': 'jsrf_recomp.map',
        'collector_sha256': 'jsrf_collect.exe',
        'build_source_sha256': 'build-source.json',
    }
    for field, name in field_to_artifact.items():
        if metadata.get(field) != recorded.get(name):
            reasons.append(f'{field} conflicts with archived artifact hash')

    recorded_xbe_path = metadata.get('xbe_path')
    if source_xbe is None or not source_xbe.is_file():
        reasons.append('original XBE source is missing')
    elif (not isinstance(recorded_xbe_path, str) or not _is_absolute_path(recorded_xbe_path)
          or _normalized_path(recorded_xbe_path) != _normalized_path(str(source_xbe.resolve()))):
        reasons.append('recorded original XBE path conflicts with the available source')
    elif not _sha256(metadata.get('xbe_sha256')) or _digest_file(source_xbe) != metadata['xbe_sha256'].lower():
        reasons.append('original XBE source hash mismatch')

    identities = metadata.get('repository_identities')
    if isinstance(identities, dict):
        for repo_name, filenames in REPOSITORY_FILES.items():
            identity = identities.get(repo_name)
            if not isinstance(identity, dict):
                continue
            for field, filename in zip(('patch_sha256', 'status_sha256'), filenames):
                path = archive_dir / filename
                expected = identity.get(field)
                if not _sha256(expected) or not path.is_file():
                    reasons.append(f'missing or malformed archived {repo_name} {field}')
                elif _digest_file(path) != expected.lower():
                    reasons.append(f'archived {repo_name} {field} mismatch')

    build_source_path = archive_dir / 'build-source.json'
    if build_source_path.is_file():
        try:
            build_source = load_json_unique(build_source_path)
            if not isinstance(build_source, dict):
                raise ValueError('build-source root is not an object')
            if build_source.get('exe_sha256') != metadata.get('exe_sha256'):
                reasons.append('build-source executable hash conflicts with metadata')
            build_artifacts = build_source.get('artifacts')
            if not isinstance(build_artifacts, dict):
                reasons.append('build-source artifact identity is missing')
            else:
                for name in ARCHIVED_ARTIFACTS:
                    if name == 'build-source.json':
                        continue
                    if build_artifacts.get(name) != recorded.get(name):
                        reasons.append(f'build-source hash conflicts for {name}')
        except Exception as error:
            reasons.append(f'build-source.json is unreadable: {error}')

    command = metadata.get('command')
    save_root = metadata.get('save_root')
    probe = metadata.get('probe')
    if isinstance(command, list) and isinstance(save_root, dict) and isinstance(probe, str):
        expected_length = 6 if probe else 5
        expected_command = [
            str(archive_dir / 'jsrf_collect.exe'),
            str(metadata.get('seconds')),
            str(archive_dir),
            str(archive_dir / 'jsrf_recomp.exe'),
            f"--save-root={save_root.get('expected_resolved_path', '')}",
        ]
        if probe:
            expected_command.append(f'--probe={probe}')
        if (len(command) != expected_length or len(command) != len(expected_command)
                or any(not isinstance(part, str) for part in command)
                or any(_normalized_command_part(actual) != _normalized_command_part(expected)
                       for actual, expected in zip(command, expected_command))):
            reasons.append('recorded command conflicts with archived run/profile inputs')

    log_path = archive_dir / 'jsrf_run.log'
    expected_log_hash = metadata.get('run_log_sha256')
    if not log_path.is_file() or not _sha256(expected_log_hash):
        reasons.append('missing archived runtime log identity')
    elif _digest_file(log_path) != expected_log_hash.lower():
        reasons.append('archived runtime log hash mismatch')
    else:
        log = log_path.read_bytes().decode('utf-8', 'replace')
        resolved, path_layer = parse_save_root_markers(log)
        if (resolved != save_root.get('observed_resolved_path')
                or path_layer != save_root.get('observed_path_layer_root')
                or not resolved or not path_layer):
            reasons.append('archived runtime root markers conflict with metadata')

    return reasons


def _git_is_ancestor(toolkit_root: Path, ancestor: str, revision: str) -> bool | None:
    """True/False from git ancestry; None when the answer is not available.

    ``None`` is a distinct outcome from ``False``: an unreachable toolkit tree
    or an unknown revision cannot be reported as "did not honor it", because
    that would silently downgrade a contaminated archive to clean.
    """
    try:
        result = subprocess.run(
            ['git', '-C', str(toolkit_root), 'merge-base', '--is-ancestor', ancestor, revision],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
    except (OSError, subprocess.SubprocessError):
        return None
    if result.returncode == 0:
        return True
    if result.returncode == 1:
        return False
    return None


def resolve_retired_overrides(settings: list[dict[str, str]],
                              toolkit_revision: str | None,
                              toolkit_root: Path | None) -> tuple[list[dict[str, Any]], list[str]]:
    """Resolve each retired override against the revision that produced a run.

    Returns ``(notes, unresolved)``.  A note records the name and whether the
    recorded toolkit revision could still honor it.  ``unresolved`` names the
    overrides whose contribution could not be decided; the caller must treat
    those as UNKNOWN rather than as clean.
    """
    present = {entry['name'].casefold(): entry['name'] for entry in settings}
    notes: list[dict[str, Any]] = []
    unresolved: list[str] = []
    for name, info in RETIRED_OVERRIDES.items():
        recorded_name = present.get(name.casefold())
        if recorded_name is None:
            continue
        honored: bool | None = None
        if not isinstance(toolkit_revision, str) or not re.fullmatch(
                r'[0-9a-fA-F]{7,40}', toolkit_revision or ''):
            honored = None
        elif toolkit_root is None:
            honored = None
        else:
            removed = _git_is_ancestor(toolkit_root, info['removed_commit'], toolkit_revision)
            added = _git_is_ancestor(toolkit_root, info['added_commit'], toolkit_revision)
            if removed is None or added is None:
                honored = None
            else:
                # Honored only in the window between the add and the removal.
                honored = added and not removed
        notes.append({
            'name': recorded_name,
            'retired_on': info['removed_on'],
            'removal_commit': info['removed_commit'],
            'recorded_toolkit_revision': toolkit_revision,
            'honored_by_that_binary': honored,
            'note': info['note'],
        })
        if honored is None:
            unresolved.append(recorded_name)
    return notes, unresolved


def retired_override_names() -> tuple[str, ...]:
    """The fail-closed launch set: recognized, and no longer overridable."""
    return tuple(sorted(RETIRED_OVERRIDES))


def classify_settings(entries: Any) -> dict[str, Any]:
    """Classify actual runtime settings and resolved NV2A ack default.

    This evaluates the *current* binary's semantics only.  Retired overrides are
    resolved separately against a recorded toolkit revision, because their
    effect depends on which binary produced the run -- see
    ``resolve_retired_overrides``.
    """
    settings, error = parse_settings_entries(entries)
    if error:
        return {'classification': UNKNOWN, 'reasons': [error]}
    values = {item['name'].casefold(): item['value'] for item in settings or []}
    active: list[str] = []
    retired: list[str] = []

    gpu_key = GPU_ACK.casefold()
    gpu_present = gpu_key in values
    gpu_value = values.get(gpu_key)
    gpu_enabled = not (gpu_present and gpu_value == '0')
    if gpu_enabled:
        if gpu_present:
            active.append(f'{GPU_ACK} is enabled; only the exact value "0" disables it')
        else:
            active.append(f'{GPU_ACK} defaults to enabled when absent')

    for name, reason in (
        (AC97_READY, 'was: forced the AC97 codec-ready bit; the runtime no longer reads it'),
        (ALLOW_UNRESOLVED, 'continues after unresolved indirect calls when present'),
        (ABI_CONTINUE, 'continues after ABI failures when present'),
        # Upstream v0.12 bring-up switches (toolkit merge 2925f0b). New device
        # behaviour from an upstream merge enters dormant only, and a variable that
        # means synthetic completion is exploratory (jsrf-run-profiles.md,
        # "Upstream merges never silently change admitted evidence semantics",
        # rule 2). Presence is enough, whatever the value: over-classifying a run
        # as exploratory is the safe direction.
        (DSP_ACK, 'zeroes the listed guest dwords whenever they are non-zero when present'),
        (POKE, 'holds guest globals at fixed values when present'),
        (FORCE_RETURN, 'makes force-return functions answer a constant when present'),
        (PAD_PRESS, 'synthesises controller button presses when present'),
        # Owner-directed toolkit fixes (toolkit db96e30..2a349c8). The legacy switch restores
        # kernel memory semantics now known to be wrong; the NV2A switch arms
        # modelled device behaviour that has not been admitted yet
        # (jsrf-run-profiles.md, "Unconditional modeled hardware causes").
        (KMEM_LEGACY, 'restores the previous, unfaithful kernel memory semantics when present'),
        (NV2A_ACTIONS, 'arms unadmitted NV2A semaphore/software-method/flip-stall behaviour when present'),
        # Serialised guest mode (toolkit 179439b, 2026-09-30) replaces the scheduling
        # model itself and lets a waiter overrun the lock; presence alone is enough.
        (GUEST_SERIAL, 'runs one guest thread at a time with bounded-wait overruns when present'),
    ):
        if name.casefold() in values:
            active.append(f'{name} {reason}')

    apu_key = APU_DSP_ACK.casefold()
    if apu_key in values:
        addresses = parse_apu_ack_addresses(values[apu_key])
        if addresses is None:
            return {
                'classification': UNKNOWN,
                'reasons': [f'{APU_DSP_ACK} is outside the supported C-parser input domain'],
            }
        if addresses:
            active.append(f'{APU_DSP_ACK} parses {len(addresses)} nonzero address(es)')

    for name in RETIRED_OVERRIDES:
        if name.casefold() in values:
            retired.append(
                f'{name} is retired and inert in the current binary '
                f'(removed {RETIRED_OVERRIDES[name]["removed_on"]})')

    defaults = {
        GPU_ACK: {
            'source': 'environment' if gpu_present else 'runtime-default',
            'present': gpu_present,
            'value': gpu_value,
            'effective': 'enabled' if gpu_enabled else 'disabled',
        }
    }
    return {
        'classification': EXPLORATORY if active else STRICT,
        'reasons': active,
        'retired_overrides': retired,
        'effective_settings': settings,
        'resolved_defaults': defaults,
    }


def resolve_requested_profile(requested: str | None, probe: str) -> tuple[str, str]:
    """Resolve CLI default and reject contradictory fixture/ordinary requests."""
    if requested is None:
        return (FIXTURE, 'probe-default') if probe else (STRICT, 'ordinary-default')
    if requested not in PROFILE_NAMES:
        raise ProfileError(f'unknown profile {requested!r}')
    if requested == FIXTURE and not probe:
        raise ProfileError('fixture profile requires a named --probe')
    if requested != FIXTURE and probe:
        raise ProfileError('--probe runs use the fixture profile; omit --profile or pass --profile fixture')
    return requested, 'explicit'


def validate_launch_profile(requested: str, probe: str,
                            inherited_settings: list[dict[str, str]]) -> dict[str, Any]:
    """Fail closed before any child process is created."""
    if (requested == FIXTURE) != bool(probe):
        raise ProfileError('fixture profile and --probe must be requested together')
    classified = classify_settings(inherited_settings)
    if classified['classification'] == UNKNOWN:
        raise ProfileError('; '.join(classified['reasons']))
    if requested == STRICT:
        values = {entry['name'].casefold(): entry['value'] for entry in inherited_settings}
        # A retired override cannot be evaluated against the binary that is about
        # to run: its effect is revision-relative, and the launch is for the
        # current build.  Refuse rather than silently treat it as inert.
        retired = [entry['name'] for entry in inherited_settings
                   if entry['name'].casefold() in
                   {name.casefold() for name in RETIRED_OVERRIDES}]
        if retired:
            raise ProfileError(
                'strict profile carries retired override(s) that cannot be evaluated '
                'against the binary being launched: ' + ', '.join(sorted(retired)))
        if GPU_ACK.casefold() not in values or values[GPU_ACK.casefold()] != '0':
            raise ProfileError(
                'strict profile requires the caller to set RECOMP_GPU_ACK=0; '
                'the runner will not insert it')
        if classified['classification'] != STRICT:
            raise ProfileError('strict profile has prohibited effective settings: '
                               + '; '.join(classified['reasons']))
    return classified


def retired_names_in(settings: Any) -> list[str]:
    """Names from the retired registry present in a settings list."""
    keys = {name.casefold() for name in RETIRED_OVERRIDES}
    return sorted(entry['name'] for entry in (settings or [])
                  if isinstance(entry, dict) and isinstance(entry.get('name'), str)
                  and entry['name'].casefold() in keys)


def make_profile_record(requested: str, inherited_settings: Any,
                        effective_settings: Any, probe: str) -> dict[str, Any]:
    """Build versioned archive fields using the same classifier as the checker."""
    inherited, inherited_error = parse_settings_entries(inherited_settings)
    if inherited_error:
        raise ProfileError(inherited_error)
    effective, effective_error = parse_settings_entries(effective_settings)
    if effective_error:
        raise ProfileError(effective_error)
    base = classify_settings(effective)
    if base['classification'] == UNKNOWN:
        raise ProfileError('; '.join(base['reasons']))
    if (requested == FIXTURE) != bool(probe):
        raise ProfileError('fixture profile and --probe must be requested together')
    if requested == STRICT:
        validate_launch_profile(requested, probe, inherited or [])
        # The launch gate refuses a retired name in what was inherited; an
        # effective setting that gained one is equally unreachable, so a strict
        # record must not claim it.  Checked separately from
        # _same_profile_settings, which covers only the semantic variables.
        gained = retired_names_in(effective)
        if gained:
            raise ProfileError(
                'strict profile cannot record retired override(s) in its effective '
                'settings: ' + ', '.join(gained))
        # Strict mode may not rewrite any classification-relevant inherited key.
        if not _same_profile_settings(inherited or [], effective or []):
            raise ProfileError('strict profile changed a classification-relevant inherited setting')
        classification = STRICT
    elif requested == EXPLORATORY:
        classification = EXPLORATORY
    elif requested == FIXTURE:
        classification = FIXTURE
    else:
        raise ProfileError(f'unknown profile {requested!r}')
    return {
        'schema_version': PROFILE_SCHEMA_VERSION,
        'classifier_version': CLASSIFIER_VERSION,
        'requested_profile': requested,
        'classification': classification,
        'environment_classification': base['classification'],
        'reasons': base['reasons'],
        'inherited_settings': inherited,
        'effective_settings': effective,
        'resolved_defaults': base.get('resolved_defaults'),
    }


def _same_profile_settings(left: list[dict[str, str]],
                           right: list[dict[str, str]]) -> bool:
    """Compare presence and values for every setting that affects eligibility."""
    left_values = {entry['name'].casefold(): entry['value'] for entry in left}
    right_values = {entry['name'].casefold(): entry['value'] for entry in right}
    for name in (GPU_ACK, AC97_READY, APU_DSP_ACK, ALLOW_UNRESOLVED, ABI_CONTINUE):
        key = name.casefold()
        if (key in left_values) != (key in right_values):
            return False
        if left_values.get(key) != right_values.get(key):
            return False
    return True


def _legacy_settings(metadata: dict[str, Any]) -> Any:
    settings = metadata.get('settings')
    if isinstance(settings, list):
        return settings
    if isinstance(settings, dict):
        for key in ('environment', 'env'):
            if isinstance(settings.get(key), list):
                return settings[key]
    return None


def _sha256(value: Any) -> bool:
    return isinstance(value, str) and re.fullmatch(r'[0-9a-fA-F]{64}', value) is not None


def _valid_new_archive(metadata: dict[str, Any], profile: dict[str, Any],
                      archive_dir: Path | None,
                      source_xbe: Path | None) -> dict[str, Any]:
    if (type(profile.get('schema_version')) is not int
            or profile.get('schema_version') != PROFILE_SCHEMA_VERSION):
        return {'classification': UNKNOWN, 'reasons': ['unknown run-profile schema version']}
    if profile.get('classifier_version') != CLASSIFIER_VERSION:
        return {'classification': UNKNOWN, 'reasons': ['unknown classifier version']}
    requested = profile.get('requested_profile')
    if requested not in PROFILE_NAMES:
        return {'classification': UNKNOWN, 'reasons': ['missing or unknown requested profile']}
    effective, error = parse_settings_entries(profile.get('effective_settings'))
    if error:
        return {'classification': UNKNOWN, 'reasons': [error]}
    inherited, error = parse_settings_entries(profile.get('inherited_settings'))
    if error:
        return {'classification': UNKNOWN, 'reasons': [error]}
    top_settings, error = parse_settings_entries(metadata.get('settings'))
    if error or top_settings != effective:
        return {'classification': UNKNOWN, 'reasons': [error or 'top-level and profile settings conflict']}
    base = classify_settings(effective)
    if base['classification'] == UNKNOWN:
        return base
    if profile.get('environment_classification') != base['classification']:
        return {'classification': UNKNOWN, 'reasons': ['environment classification conflicts with settings']}
    if profile.get('reasons') != base['reasons']:
        return {'classification': UNKNOWN, 'reasons': ['classification reasons conflict with settings']}
    if profile.get('resolved_defaults') != base.get('resolved_defaults'):
        return {'classification': UNKNOWN, 'reasons': ['resolved runtime defaults conflict with settings']}
    probe = metadata.get('probe')
    if not isinstance(probe, str):
        return {'classification': UNKNOWN, 'reasons': ['missing or malformed probe field']}
    if (requested == FIXTURE) != bool(probe):
        return {'classification': UNKNOWN, 'reasons': ['requested profile conflicts with probe field']}
    if requested == STRICT:
        inherited_class = classify_settings(inherited)
        if inherited_class['classification'] != STRICT:
            return {'classification': UNKNOWN, 'reasons': ['strict profile has prohibited inherited settings']}
        inherited_values = {entry['name'].casefold(): entry['value'] for entry in inherited or []}
        if inherited_values.get(GPU_ACK.casefold()) != '0':
            return {'classification': UNKNOWN, 'reasons': ['strict profile did not inherit explicit RECOMP_GPU_ACK=0']}
        if not _same_profile_settings(inherited or [], effective or []):
            return {'classification': UNKNOWN, 'reasons': ['strict profile changed inherited classification settings']}
        # A strict archive cannot have carried a retired name on either side:
        # the current binary does not read it, so recording one means the record
        # and the binary disagree.  The revision-aware layer below is what
        # decides an older binary's real contribution.  Both sides are checked
        # because a retired name in the inherited set alone still clears the
        # semantic gates above -- it is not an "active" setting.
        retired = sorted(set(retired_names_in(effective)) | set(retired_names_in(inherited)))
        if retired:
            return {'classification': UNKNOWN,
                    'reasons': ['strict archive records retired override(s): '
                                + ', '.join(retired)]}
    if requested == STRICT and base['classification'] != STRICT:
        return {'classification': UNKNOWN, 'reasons': ['strict request conflicts with exploratory environment']}
    expected = FIXTURE if requested == FIXTURE else (
        EXPLORATORY if requested == EXPLORATORY or base['classification'] == EXPLORATORY
        else STRICT)
    if profile.get('classification') != expected:
        return {'classification': UNKNOWN, 'reasons': ['declared profile classification conflicts with request/settings']}

    command = metadata.get('command')
    if (not isinstance(command, list) or not command
            or any(not isinstance(part, str) or not part for part in command)):
        return {'classification': UNKNOWN, 'reasons': ['missing or malformed exact command']}
    if not _sha256(metadata.get('xbe_sha256')) or not _sha256(metadata.get('exe_sha256')):
        return {'classification': UNKNOWN, 'reasons': ['missing or malformed XBE/executable identity']}
    identities = metadata.get('repository_identities')
    if not isinstance(identities, dict):
        return {'classification': UNKNOWN, 'reasons': ['missing repository identities']}
    for repo_name in ('project', 'toolkit'):
        identity = identities.get(repo_name)
        if not isinstance(identity, dict):
            return {'classification': UNKNOWN, 'reasons': [f'missing {repo_name} repository identity']}
        if not isinstance(identity.get('revision'), str) or not identity['revision']:
            return {'classification': UNKNOWN, 'reasons': [f'missing {repo_name} repository revision']}
        if not _sha256(identity.get('patch_sha256')) or not _sha256(identity.get('status_sha256')):
            return {'classification': UNKNOWN, 'reasons': [f'missing or malformed {repo_name} worktree identity']}
    if metadata.get('project_archived') is not True:
        return {'classification': UNKNOWN, 'reasons': ['project repository state was not archived']}

    if type(metadata.get('seconds')) is not int or not 1 <= metadata['seconds'] <= 300:
        return {'classification': UNKNOWN, 'reasons': ['missing or invalid run duration']}

    save_root = metadata.get('save_root')
    if not isinstance(save_root, dict):
        return {'classification': UNKNOWN, 'reasons': ['missing save-root identity']}
    archive_root = save_root.get('archive_root')
    expected_root = save_root.get('expected_resolved_path')
    observed_root = save_root.get('observed_resolved_path')
    observed_path_layer = save_root.get('observed_path_layer_root')
    if (archive_dir is None
            or not isinstance(archive_root, str) or not _is_absolute_path(archive_root)
            or _normalized_path(archive_root) != _normalized_path(str(archive_dir.resolve()))
            or not isinstance(expected_root, str) or not _is_absolute_path(expected_root)
            or not isinstance(observed_root, str) or not _is_absolute_path(observed_root)
            or not isinstance(observed_path_layer, str) or not _is_absolute_path(observed_path_layer)
            or _normalized_path(expected_root) != _normalized_path(ntpath.join(archive_root, 'save-root'))
            or _normalized_path(expected_root) != _normalized_path(observed_root)
            or _normalized_path(expected_root) != _normalized_path(observed_path_layer)
            or save_root.get('disposable') is not True
            or save_root.get('verified') is not True):
        return {'classification': UNKNOWN, 'reasons': ['save-root identity is missing, conflicting, or not verified']}

    byte_errors = _verify_archived_bytes(metadata, archive_dir, source_xbe)
    if byte_errors:
        return {'classification': UNKNOWN, 'reasons': byte_errors}

    return {
        'classification': expected,
        'reasons': base['reasons'],
        'environment_classification': base['classification'],
        'effective_settings': effective,
    }


def recorded_toolkit_revision(metadata: dict[str, Any]) -> str | None:
    """Read the toolkit revision an archive records, in either schema shape."""
    revision = metadata.get('toolkit_revision')
    if isinstance(revision, str) and revision.strip():
        return revision.strip()
    identities = metadata.get('repository_identities')
    if isinstance(identities, dict):
        toolkit = identities.get('toolkit')
        if isinstance(toolkit, dict):
            revision = toolkit.get('revision')
            if isinstance(revision, str) and revision.strip():
                return revision.strip()
    return None


def apply_retired_overrides(result: dict[str, Any], settings: Any,
                            metadata: dict[str, Any],
                            toolkit_root: Path | None) -> dict[str, Any]:
    """Fold revision-resolved retired overrides into a classification result.

    A retired override that the recorded binary could still honor is a real
    semantic override for that artifact and therefore makes the run exploratory.
    One that the binary had already lost is annotated but does not change the
    verdict.  An override whose contribution cannot be decided is UNKNOWN --
    never silently clean.
    """
    parsed, error = parse_settings_entries(settings)
    if error:
        return result
    revision = recorded_toolkit_revision(metadata)
    notes, unresolved = resolve_retired_overrides(parsed or [], revision, toolkit_root)
    if not notes:
        return result
    result = dict(result)
    result['retired_overrides'] = notes
    honored = [note['name'] for note in notes if note['honored_by_that_binary'] is True]
    if unresolved:
        result['classification'] = UNKNOWN
        result['reasons'] = list(result.get('reasons', [])) + [
            f'{name} is retired and the recorded toolkit revision could not be '
            f'resolved, so its contribution is undecidable' for name in unresolved]
        return result
    if honored:
        result['classification'] = EXPLORATORY
        result['reasons'] = list(result.get('reasons', [])) + [
            f'{name} was still honored by recorded toolkit revision {revision} '
            f'(retired later, {RETIRED_OVERRIDES[name]["removed_on"]})'
            for name in honored]
        result['environment_classification'] = EXPLORATORY
    return result


def reclassify_metadata(metadata: Any, archive_dir: Path | None = None,
                        source_xbe: Path | None = None,
                        toolkit_root: Path | None = None) -> dict[str, Any]:
    """Recompute an archive's profile; legacy shapes are supported explicitly."""
    if not isinstance(metadata, dict):
        return {'classification': UNKNOWN, 'reasons': ['metadata root is not an object']}
    if toolkit_root is None:
        toolkit_root = Path(__file__).resolve().parents[2] / 'xboxrecomp'
    if 'run_profile' in metadata:
        profile = metadata.get('run_profile')
        if not isinstance(profile, dict):
            return {'classification': UNKNOWN, 'reasons': ['run_profile is malformed']}
        result = _valid_new_archive(metadata, profile, archive_dir, source_xbe)
        if result.get('classification') == UNKNOWN:
            return result
        return apply_retired_overrides(result, profile.get('effective_settings'),
                                       metadata, toolkit_root)
    if any(key in metadata for key in (
            'schema_version', 'profile_schema_version', 'classifier_version')):
        return {'classification': UNKNOWN, 'reasons': ['versioned metadata is missing run_profile']}
    entries = _legacy_settings(metadata)
    if entries is None:
        return {'classification': UNKNOWN, 'reasons': ['legacy metadata has no supported environment shape']}
    legacy = classify_settings(entries)
    if legacy['classification'] == EXPLORATORY:
        return apply_retired_overrides({
            'classification': EXPLORATORY,
            'reasons': legacy['reasons'],
            'environment_classification': legacy['classification'],
            'profile_evidence': 'legacy settings only; no verified strict profile/provenance',
        }, entries, metadata, toolkit_root)
    if legacy['classification'] == STRICT:
        # A legacy archive that looks strict is still UNKNOWN, but a retired
        # override it carried must not be dropped from the record.
        return apply_retired_overrides({
            'classification': UNKNOWN,
            'reasons': ['legacy settings have no verified strict profile or provenance'],
            'environment_classification': STRICT,
        }, entries, metadata, toolkit_root)
    return legacy


def load_json_unique(path: Path) -> Any:
    """Load JSON while rejecting duplicate object keys at every nesting level."""
    def object_hook(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'duplicate JSON object key {key!r}')
            result[key] = value
        return result

    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=object_hook)


def classify_run_directory(run_dir: Path, source_xbe: Path | None = None) -> dict[str, Any]:
    """Read and classify one archived run directory."""
    metadata_path = run_dir / 'metadata.json'
    if not run_dir.is_dir() or not metadata_path.is_file():
        return {'classification': MISSING, 'reasons': ['run directory or metadata.json is missing']}
    try:
        metadata = load_json_unique(metadata_path)
    except Exception as error:
        return {'classification': UNKNOWN, 'reasons': [f'unreadable metadata: {error}']}
    if source_xbe is None:
        source_xbe = Path(__file__).resolve().parents[1] / 'game' / 'default.xbe'
    return reclassify_metadata(metadata, run_dir.resolve(), source_xbe)


def _normalized_path(value: str) -> str:
    """Compare resolved paths using Windows path/case rules on every host."""
    return ntpath.normcase(ntpath.normpath(value))


def _normalized_command_part(value: str) -> str:
    if value.startswith('--save-root='):
        return '--save-root=' + _normalized_path(value.partition('=')[2])
    if ntpath.isabs(value):
        return _normalized_path(value)
    return value


def _is_absolute_path(value: str) -> bool:
    return Path(value).is_absolute() or ntpath.isabs(value)
