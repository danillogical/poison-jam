"""Gate: does an archived run carry synthetic-completion or bypass overrides?

`docs/jsrf-run-profiles.md` splits runs into **strict** (no overrides) and
**exploratory** (any override). The distinction is load-bearing: a synthetic
override answers a poll without doing the work, so a run carrying one **cannot
satisfy boot, audio, GPU or liveness acceptance**, and a packet claiming a strict
run must not rest on an artifact that has one.

This exists because the claim was wrong once. A2g was recorded as a strict run in
the plan and accepted by a reviewer on that basis, while its artifact carries
`RECOMP_AC97_READY=1` and `RECOMP_APU_DSP_ACK=0x803C0810`.

**Three defects this file had, all found by the advisor, all now fixed:**

1. It looked for `settings.environment` while `metadata.json` stores `settings`
   as the **list itself** -- so it printed CLEAN for every run. A false negative
   that would have blessed the bad run and closed the question.
2. It flagged a variable **by name regardless of value**, so `RECOMP_AC97_READY=0`
   -- an override explicitly *disabled* -- was reported as exploratory. A
   disabled override is not an override.
3. It let a **named run with no metadata exit 0**, and silently dropped malformed
   environment entries and collapsed duplicate names. Missing input must not pass.

Usage:
    python -X utf8 scripts/check-run-profile.py [run-dir ...]
    python -X utf8 scripts/check-run-profile.py --all

Exit status is 1 if any run is exploratory, unreadable, has no environment
recorded, or was named and not found -- so this is usable as a gate.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Overrides that fake completion. Mirrors docs/jsrf-run-profiles.md.
SYNTHETIC = {
    'RECOMP_APU_DSP_ACK': 'clears the DSP pending word without the DSP running',
    'RECOMP_AC97_READY': 'codec-ready poll succeeds with no codec',
    'RECOMP_GPU_ACK': 'busy-bit acks with no engine behind them',
    'RECOMP_VBLANK': 'removed 2026-09-22 (A2); still flagged if present',
}
# Overrides that continue past a failure rather than completing work.
BYPASS = {
    'JSRF_ALLOW_UNRESOLVED': 'continues past unresolved calls',
    'JSRF_ABI_CONTINUE': 'continues past an ABI contract failure',
}

# A value that means "this override is not actually doing anything". Setting a
# flag to 0 is how a run turns an override OFF, and that is a strict run.
DISABLED_VALUES = {'', '0', 'false', 'no', 'off', 'none'}

STRICT = 'strict'
EXPLORATORY = 'exploratory'
UNKNOWN = 'UNKNOWN'
MISSING = 'MISSING'


def environment_of(metadata):
    """The run's environment as a list of (name, value, raw) triples, or None.

    `metadata.json` has stored `settings` two ways: as the environment list
    itself, and nested as `settings.environment`. Read both -- assuming only one
    is how this check silently passed a run it should have failed.

    Returns a **list**, not a dict, so duplicate names stay visible instead of
    one silently overwriting the other.
    """
    settings = metadata.get('settings')
    entries = None
    if isinstance(settings, list):
        entries = settings
    elif isinstance(settings, dict):
        for key in ('environment', 'env'):
            if isinstance(settings.get(key), list):
                entries = settings[key]
                break
    if entries is None:
        for key in ('environment', 'env'):
            if isinstance(metadata.get(key), list):
                entries = metadata[key]
                break
    if entries is None:
        return None

    out = []
    malformed = 0
    for entry in entries:
        if isinstance(entry, dict) and 'name' in entry:
            out.append((entry['name'], entry.get('value'), entry))
        else:
            malformed += 1
    return out, malformed


def is_enabled(value):
    """Is this override actually in effect?

    Absent value counts as enabled -- a bare name in an environment block is how
    a flag is normally set. Explicit 0/false/off does not.
    """
    if value is None:
        return True
    return str(value).strip().lower() not in DISABLED_VALUES


def classify(pairs):
    """Split (name, value, raw) triples into active synthetic and bypass hits.

    **The lookup tables are read-only here.** An earlier version wrote results
    into `table[name]`, which is `SYNTHETIC` or `BYPASS` itself -- so it both
    failed to populate the output dicts (reporting every run strict, including
    the four known-exploratory A2 runs) and mutated the module's own table.
    """
    synthetic, bypass, disabled, duplicates = {}, {}, [], []
    seen = {}
    for name, value, _raw in pairs:
        seen.setdefault(name, []).append(value)
        if name in SYNTHETIC:
            target = synthetic
        elif name in BYPASS:
            target = bypass
        else:
            continue
        if is_enabled(value):
            target[name] = value
        else:
            disabled.append('%s=%s' % (name, value))
    for name, values in seen.items():
        if len(values) > 1 and name in {**SYNTHETIC, **BYPASS}:
            duplicates.append('%s appears %d times (%s)'
                              % (name, len(values), ', '.join(str(v) for v in values)))
    return synthetic, bypass, disabled, duplicates


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('runs', nargs='*', help='run directory names or paths')
    parser.add_argument('--all', action='store_true',
                        help='check every run under logs/runs/')
    args = parser.parse_args()

    if args.all:
        targets = sorted((ROOT / 'logs' / 'runs').glob('*'))
    elif args.runs:
        targets = []
        for name in args.runs:
            path = Path(name)
            if not path.is_dir():
                path = ROOT / 'logs' / 'runs' / name
            targets.append(path)
    else:
        parser.error('name at least one run, or pass --all')

    failures = 0
    checked = 0
    tally = {STRICT: 0, EXPLORATORY: 0, UNKNOWN: 0, MISSING: 0}
    for run in targets:
        metadata_path = run / 'metadata.json'
        if not run.is_dir() or not metadata_path.exists():
            # A named run that does not exist is a FAILURE, not a silent pass.
            tally[MISSING] += 1
            failures += 1
            print('%-52s %s (no metadata.json)' % (run.name, MISSING))
            continue
        checked += 1
        try:
            metadata = json.loads(metadata_path.read_text(encoding='utf-8', errors='replace'))
        except Exception as exc:
            tally[UNKNOWN] += 1
            failures += 1
            print('%-52s %s: unreadable metadata (%s)' % (run.name, UNKNOWN, exc))
            continue
        parsed = environment_of(metadata)
        if parsed is None:
            tally[UNKNOWN] += 1
            failures += 1
            print('%-52s %s: no environment recorded -> cannot call it strict'
                  % (run.name, UNKNOWN))
            continue
        pairs, malformed = parsed
        synthetic, bypass, disabled, duplicates = classify(pairs)

        notes = []
        if malformed:
            notes.append('%d malformed env entr%s ignored'
                         % (malformed, 'y' if malformed == 1 else 'ies'))
        if duplicates:
            notes.extend(duplicates)
            failures += 1
        if synthetic or bypass:
            tally[EXPLORATORY] += 1
            failures += 1
            print('%-52s %s' % (run.name, EXPLORATORY.upper()))
            for name, value in sorted({**synthetic, **bypass}.items()):
                reason = SYNTHETIC.get(name) or BYPASS.get(name)
                print('      %s=%s (%s)' % (name, value, reason))
        elif disabled:
            tally[STRICT] += 1
            print('%-52s %s (%s explicitly disabled)'
                  % (run.name, STRICT, ', '.join(disabled)))
        else:
            tally[STRICT] += 1
            print('%-52s %s' % (run.name, STRICT))
        for note in notes:
            print('      note: %s' % note)

    print()
    print('checked %d; strict %d, exploratory %d, unknown %d, missing %d'
          % (checked, tally[STRICT], tally[EXPLORATORY], tally[UNKNOWN], tally[MISSING]))
    if failures:
        print()
        print('A run that is not strict cannot satisfy boot, audio, GPU or liveness '
              'acceptance. See docs/jsrf-run-profiles.md.')
        print('This is a FLAGGED-RUN count, not an audited total: a run flagged here '
              'must have its claims re-checked, but flagging does not by itself '
              'establish that any particular claim was wrong.')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
