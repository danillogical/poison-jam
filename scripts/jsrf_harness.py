"""P0.6: harness permissions, probe expectations and the result schema.

Three separate failures this addresses:

1. **Environmental inability must not read as guest failure.**  Under a confined
   file policy the title cannot write `%LOCALAPPDATA%\\xboxrecomp\\`, so
   `NtOpenFile` returns `ACCESS_DENIED`, the guest calls `HalReturnToFirmware(2)`,
   and the run ends after ~1.8 s with ~515 log lines -- which reads exactly like a
   catastrophic regression.  A denied access is `ENVIRONMENT_BLOCKED`, distinct
   from a guest fault, and no alternate API is used to evade the denial.
2. **Every probe needs one centralized expected checkpoint.**  A `--probe=` run
   returns before `checkpoint("guest_entry")`, so inheriting the ordinary default
   reports a false `checkpoints_passed: false` for a probe that behaved correctly.
3. **Launch, capture, profile and semantic outcomes are separate axes.**  A
   deadline means the capture was bounded; a normal exit means the entry point
   returned.  Neither is liveness, and a single status field cannot express them.
"""
from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_VERSION = 1

# ── outcome vocabulary ───────────────────────────────────────────────────────
ENVIRONMENT_BLOCKED = 'ENVIRONMENT_BLOCKED'
PERMITTED = 'PERMITTED'
UNKNOWN = 'UNKNOWN'

# Launch outcomes.
LAUNCH_STARTED = 'started'
LAUNCH_REFUSED = 'refused'
LAUNCH_ENVIRONMENT_BLOCKED = 'environment_blocked'

# Capture outcomes.  Deliberately not "success"/"failure": they describe what the
# collector did, not what the guest achieved.
CAPTURE_COMPLETE = 'complete'
CAPTURE_DEADLINE = 'diagnostic_deadline'
CAPTURE_NO_DUMP = 'no_dump'
CAPTURE_FAILED = 'collector_failure'

# Semantic outcomes.  `liveness` is NOT here: it needs a modelled cause, so a
# capture outcome can never imply it.
SEMANTIC_UNKNOWN = 'UNKNOWN'
SEMANTIC_GUEST_FAULT = 'guest_fault'
SEMANTIC_ENTRY_RETURNED = 'entry_returned'

# The six emulated hard-disk images the toolkit maps.  Enumerated explicitly:
# "the disk" is not a population, and a missing one must be named.
PARTITION_IMAGES = (
    'Partition0.img', 'Partition1.img', 'Partition2.img',
    'Partition3.img', 'Partition4.img', 'Partition5.img',
)

# Drive letters the toolkit maps onto partitions.  T/U/Z are the ones the title
# resolves at startup, so a fixture must exercise them.
MAPPED_DRIVES = ('T', 'U', 'Z')


class HarnessError(ValueError):
    """The preflight or schema check cannot produce a trustworthy answer."""


# ── probe expectations ───────────────────────────────────────────────────────

# One authority for "what checkpoint does this probe end at".  The runner and the
# tests both read this map; a probe absent from it is an error, never a silent
# fall back to the ordinary default.
ORDINARY_CHECKPOINTS = ('memory_ready', 'guest_entry')

PROBE_CHECKPOINTS: dict[str, tuple[str, ...]] = {
    '': ORDINARY_CHECKPOINTS,
    # Probes that return before guest_entry still reach memory_ready.
    'healthy': ORDINARY_CHECKPOINTS,
    'worker-crash': ORDINARY_CHECKPOINTS,
    'deadlock': ORDINARY_CHECKPOINTS,
    'spin': ORDINARY_CHECKPOINTS,
    'handled': ORDINARY_CHECKPOINTS,
    'dispatch-race': ORDINARY_CHECKPOINTS,
    'video': ORDINARY_CHECKPOINTS,
    # GPU probes return early in main.c, before checkpoint("guest_entry").
    'gpu-progress': ('memory_ready', 'probe_gpu'),
    'gpu-stall': ('memory_ready', 'probe_gpu'),
    'gpu-corrupt': ('memory_ready', 'probe_gpu'),
    'gpu-unreadable': ('memory_ready', 'probe_gpu'),
    'gpu-mmio-owner': ('memory_ready', 'probe_gpu'),
    'gpu-mmio-lifecycle': ('memory_ready', 'probe_gpu'),
    'gpu-ptimer-runtime': ('memory_ready', 'probe_gpu'),
    'gpu-submit-supported': ('memory_ready', 'probe_gpu'),
    'gpu-submit-bound': ('memory_ready', 'probe_gpu'),
    'gpu-submit-blocked': ('memory_ready', 'probe_gpu'),
}


def expected_checkpoints(probe: str) -> tuple[str, ...]:
    """The checkpoints a probe must reach, from the single centralized map.

    An unknown probe raises rather than returning the ordinary default: silently
    inheriting `guest_entry` is the exact defect this replaces.
    """
    if probe not in PROBE_CHECKPOINTS:
        raise HarnessError(
            f'unknown probe {probe!r} has no expected checkpoint; add it to '
            f'PROBE_CHECKPOINTS rather than inheriting the ordinary default')
    return PROBE_CHECKPOINTS[probe]


def probe_is_ordinary(probe: str) -> bool:
    return probe == ''


def probe_population() -> list[str]:
    """Every probe the runner defines, in stable order."""
    return sorted(PROBE_CHECKPOINTS)


def runner_probes(runner: Path | None = None) -> list[str]:
    """Read the PROBES set out of run-jsrf.py, to check the map against it."""
    runner = runner or (ROOT / 'scripts' / 'run-jsrf.py')
    text = runner.read_text(encoding='utf-8', errors='replace')
    match = re.search(r'^PROBES\s*=\s*\{(.*?)\}', text, re.DOTALL | re.MULTILINE)
    if not match:
        raise HarnessError('run-jsrf.py has no PROBES set to compare against')
    return sorted(re.findall(r"'([^']*)'", match.group(1)))


def probe_map_gaps(runner: Path | None = None) -> dict[str, Any]:
    """Which runner probes lack an expected checkpoint, and vice versa."""
    defined = runner_probes(runner)
    mapped = probe_population()
    return {
        'runner_probes': defined,
        'mapped_probes': mapped,
        'defined_but_unmapped': sorted(set(defined) - set(mapped)),
        'mapped_but_undefined': sorted(set(mapped) - set(defined)),
        'complete': set(defined) == set(mapped),
    }


# ── disk access preflight ────────────────────────────────────────────────────

def default_disk_root() -> Path:
    """Where the toolkit keeps the emulated disk images."""
    local = os.environ.get('LOCALAPPDATA')
    if not local:
        return Path.home() / 'AppData' / 'Local' / 'xboxrecomp'
    return Path(local) / 'xboxrecomp'


def classify_access_error(error: OSError) -> str:
    """Map an OS error to a harness verdict.

    A permission denial is `ENVIRONMENT_BLOCKED` -- an environment limitation, not
    a guest or test failure -- and must never be reported as one.

    `ENVIRONMENT_BLOCKED` also covers an obstruction that is not a permission
    denial: a path that exists as a *file* where a directory is required
    (`WinError 183` / `EEXIST`), a missing parent, or a name that is not a valid
    directory.  Those are properties of the host filesystem, not of the guest, and
    reporting them as `UNKNOWN` would leave the caller unable to tell "the
    environment prevents this" from "the checker could not decide".  Measured: the
    scratch-root control returned `UNKNOWN` for a path shadowed by a file.
    """
    import errno
    if isinstance(error, PermissionError):
        return ENVIRONMENT_BLOCKED
    if error.errno in (errno.EACCES, errno.EPERM):
        return ENVIRONMENT_BLOCKED
    # An obstruction rather than an undecidable outcome.
    if error.errno in (errno.EEXIST, errno.ENOTDIR, errno.ENOENT, errno.EISDIR,
                       errno.ENAMETOOLONG, errno.EROFS):
        return ENVIRONMENT_BLOCKED
    # Windows surfaces the same conditions with winerror values that errno may
    # not carry through, so check the mapped errno and the raw winerror.
    if getattr(error, 'winerror', None) in (183, 267, 3, 5, 206):
        return ENVIRONMENT_BLOCKED
    return UNKNOWN


def preflight_disk(root: Path | None = None) -> dict[str, Any]:
    """Check emulated-disk access WITHOUT changing bytes.

    The positive control is a read/write open followed by a close, with
    before/after hashes identical.  If the directory is absent the verdict is
    `UNKNOWN`, not a pass: an absent disk is not evidence that access works.
    """
    root = root or default_disk_root()
    result: dict[str, Any] = {
        'root': str(root),
        'exists': root.is_dir(),
        'images': {},
        'verdict': UNKNOWN,
        'reasons': [],
    }
    if not result['exists']:
        result['reasons'].append(f'emulated disk root does not exist: {root}')
        return result

    for name in PARTITION_IMAGES:
        path = root / name
        entry: dict[str, Any] = {'exists': path.is_file()}
        if path.is_file():
            entry['bytes'] = path.stat().st_size
            entry['sha256_before'] = _digest(path)
        result['images'][name] = entry

    missing = [n for n, e in result['images'].items() if not e['exists']]
    if missing:
        result['reasons'].append('missing partition image(s): ' + ', '.join(missing))

    # Positive access control: open read/write and close, proving bytes unchanged.
    probe_path = root / 'Partition1.img'
    if probe_path.is_file():
        before = _digest(probe_path)
        try:
            with probe_path.open('r+b'):
                pass
        except OSError as error:
            verdict = classify_access_error(error)
            result['verdict'] = verdict
            result['reasons'].append(
                f'read/write open of {probe_path.name} failed ({verdict}): {error}')
            return result
        after = _digest(probe_path)
        if before != after:
            result['verdict'] = UNKNOWN
            result['reasons'].append('preflight changed the image bytes')
            return result
        result['access_control'] = 'read/write open then close; bytes identical'
    else:
        result['reasons'].append('no Partition1.img to exercise access with')

    result['verdict'] = PERMITTED if not missing else UNKNOWN
    return result


def _digest(path: Path) -> str:
    import hashlib
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def scratch_root_control(scratch: Path) -> dict[str, Any]:
    """A permitted scratch-root control: create, write, read back, clean up.

    No alternate API is used to evade a denial -- if the directory cannot be
    created or written, that is `ENVIRONMENT_BLOCKED` and it is reported as such.
    """
    result: dict[str, Any] = {'path': str(scratch), 'verdict': UNKNOWN, 'reasons': []}
    created = False
    try:
        scratch.mkdir(parents=True, exist_ok=True)
        created = True
        probe = scratch / 'jsrf-preflight-probe.txt'
        probe.write_text('preflight\n', encoding='utf-8')
        if probe.read_text(encoding='utf-8') != 'preflight\n':
            result['reasons'].append('read-back did not match what was written')
            return result
        probe.unlink()
        result['verdict'] = PERMITTED
        result['cleanup_verified'] = not probe.exists()
    except OSError as error:
        result['verdict'] = classify_access_error(error)
        result['reasons'].append(f'{result["verdict"]}: {error}')
        result['cleanup_verified'] = False
        return result
    finally:
        if created:
            try:
                scratch.rmdir()
            except OSError:
                # Left in place deliberately: removing a directory that still has
                # contents could destroy evidence.
                result['left_in_place'] = True
    return result


# ── result schema: four separate axes ────────────────────────────────────────

RESULT_FIELDS = ('launch', 'capture', 'profile', 'semantic')


def build_result(*, launch: str, capture: str, profile: str,
                 semantic: str, checkpoints: list[str] | None = None,
                 detail: dict[str, Any] | None = None) -> dict[str, Any]:
    """Compose a result with the four axes kept separate."""
    return {
        'schema_version': SCHEMA_VERSION,
        'launch': launch,
        'capture': capture,
        'profile': profile,
        'semantic': semantic,
        'checkpoints_passed': bool(checkpoints),
        'observed_checkpoints': list(checkpoints or []),
        'detail': dict(detail or {}),
    }


def validate_result(result: Any) -> list[str]:
    """Check a result keeps the axes separate and claims no more than it can."""
    problems: list[str] = []
    if not isinstance(result, dict):
        return ['result root is not an object']
    if result.get('schema_version') != SCHEMA_VERSION:
        problems.append('unknown result schema version')
    for field in RESULT_FIELDS:
        if field not in result:
            problems.append(f'missing {field} outcome; the four axes must be separate')
    if problems:
        return problems

    # A bounded capture is not liveness, and a returned entry point is not
    # satisfaction.  Neither may be reported as a semantic success.
    if result['capture'] == CAPTURE_DEADLINE and result['semantic'] not in (
            SEMANTIC_UNKNOWN, SEMANTIC_GUEST_FAULT):
        problems.append('a diagnostic_deadline cannot carry a resolved semantic outcome')
    if result['launch'] == LAUNCH_ENVIRONMENT_BLOCKED and result['semantic'] != SEMANTIC_UNKNOWN:
        problems.append('an environment-blocked launch cannot carry a semantic outcome')
    if result['launch'] == LAUNCH_REFUSED and result['capture'] not in (
            CAPTURE_NO_DUMP, CAPTURE_FAILED):
        problems.append('a refused launch cannot have produced a capture')
    if result['capture'] == CAPTURE_NO_DUMP and result['semantic'] == SEMANTIC_ENTRY_RETURNED:
        problems.append('a normal exit with no dump is not a semantic success on its own')
    for field in ('liveness', 'boot_success', 'title_satisfied'):
        if field in result:
            problems.append(
                f'{field} must not be a result field: it cannot be established by a '
                f'capture outcome and invites reading a deadline as success')
    return problems


def probe_expectation_matches(result: dict[str, Any], probe: str) -> bool:
    """Whether the observed checkpoints cover the probe's expected set."""
    try:
        expected = set(expected_checkpoints(probe))
    except HarnessError:
        return False
    observed = set(result.get('observed_checkpoints') or [])
    return expected <= observed
