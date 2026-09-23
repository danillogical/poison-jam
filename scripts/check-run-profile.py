"""Gate: does an archived run carry synthetic-completion overrides?

`docs/jsrf-run-profiles.md` splits runs into **strict** (no overrides) and
**exploratory** (any override). The distinction is load-bearing: a synthetic
override answers a poll without doing the work, so a run carrying one **cannot
satisfy boot, audio, GPU or liveness acceptance**, and a packet that claims a
strict run must not be resting on an artifact that has one.

This exists because the claim was wrong once. A2g was recorded as a strict run in
the plan and accepted by a reviewer on that basis, while its artifact carries
`RECOMP_AC97_READY=1` and `RECOMP_APU_DSP_ACK=0x803C0810`. Nothing checked the
artifact, and a first attempt at this check reported CLEAN for every run because
it looked for `settings.environment` while `metadata.json` stores `settings` as
the **list itself** -- a false negative that would have blessed the bad run. So
this reads both shapes and says which it found.

Usage:
    python -X utf8 scripts/check-run-profile.py [run-dir ...]
    python -X utf8 scripts/check-run-profile.py --all

Exits 0 when every named run is strict, 1 when any is exploratory or unreadable.
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


def environment_of(metadata):
    """The run's environment as a name->value dict, or None if absent.

    `metadata.json` has stored `settings` two ways: as the environment list
    itself, and nested as `settings.environment`. Read both -- assuming only one
    is how this check silently passed a run it should have failed.
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
    out = {}
    for entry in entries:
        if isinstance(entry, dict) and 'name' in entry:
            out[entry['name']] = entry.get('value')
    return out


def classify(env):
    synthetic = {k: v for k, v in env.items() if k in SYNTHETIC}
    bypass = {k: v for k, v in env.items() if k in BYPASS}
    return synthetic, bypass


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
    for run in targets:
        metadata_path = run / 'metadata.json'
        if not metadata_path.exists():
            print('%-52s NO metadata.json (not a run)' % run.name)
            continue
        checked += 1
        try:
            metadata = json.loads(metadata_path.read_text(encoding='utf-8', errors='replace'))
        except Exception as exc:
            print('%-52s UNREADABLE: %s' % (run.name, exc))
            failures += 1
            continue
        env = environment_of(metadata)
        if env is None:
            print('%-52s NO environment recorded -> cannot call it strict' % run.name)
            failures += 1
            continue
        synthetic, bypass = classify(env)
        if synthetic or bypass:
            failures += 1
            labels = ['%s=%s (%s)' % (k, v, SYNTHETIC.get(k) or BYPASS.get(k))
                      for k, v in sorted({**synthetic, **bypass}.items())]
            print('%-52s EXPLORATORY' % run.name)
            for label in labels:
                print('      %s' % label)
        else:
            print('%-52s strict' % run.name)

    print()
    print('checked %d run(s); %d not strict' % (checked, failures))
    if failures:
        print('A run that is not strict cannot satisfy boot, audio, GPU or liveness '
              'acceptance. See docs/jsrf-run-profiles.md.')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
