"""`scripts/chore-gate.py`: the scripted gate that replaces review for a chore.

Plan W7 asks for "**Chore class** in §5.8 for owner-directed mechanical work (§3
above), with a scripted gate instead of Planner and acceptance review: build, ctest,
and a strict A/B run showing the same stop (or the change in stop recorded as the
finding)".

**Why a gate rather than a review.** The measured failure is the v0.11 sync taking
**19.6 h and six revisions as packet A4s**, while the owner's direct v0.12 sync (121
upstream commits) landed as one merge commit and one record commit seven minutes
apart. The packet machinery added nothing to a mechanical sync and cost a day. A
chore's question is "did the mechanical steps run, and what did they produce" -- a
question a script answers.

**The three steps, and what each catches:**

  1. **build** -- the tree compiles. A sync that does not build is not a sync.
  2. **ctest** -- the tests pass. Catches the regression a build cannot see.
  3. **strict A/B** -- a strict run before and after, compared on the **stop site**.
     This is the step that makes a chore honest about its effect: the plan records
     that regeneration "exposed the dropped `rcr`", which a build and a ctest would
     both have passed.

**A changed stop is a FINDING, not a failure.** The plan says so explicitly: "a
strict A/B run showing the same stop (**or the change in stop recorded as the
finding**)". The gate records which happened; it does not treat a move as an error,
because a move is often the point of the chore.

**What it will not do.** It does not decide whether the chore was worth doing, does
not classify a finding as blocking, and does not substitute for a change packet when
the work changes admitted evidence semantics. Those are the boundaries §5.8 draws.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GATE_VERSION = 'jsrf-chore-gate/1'

# The stop site, from a strict run's log: the invalid indirect call the run ended on.
STOP = re.compile(
    r'\[ICALL\]\s+invalid target\s+0x([0-9A-Fa-f]+)\s+tid=(\d+)\s+'
    r'esp=([0-9A-Fa-f]+)\s+return=([0-9A-Fa-f]+)')
# The last `[KERNEL] #N: ordinal O (slot S)` before the end, for context.
LAST_KERNEL = re.compile(
    r'\[KERNEL\]\s+#(\d+):\s+ordinal\s+(\d+)\s+\(slot\s+(\d+)\)')


def run(argv: list[str], log: Path) -> tuple[int, str]:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w', encoding='utf-8', errors='replace') as handle:
        completed = subprocess.run(argv, cwd=str(ROOT), stdout=handle,
                                   stderr=subprocess.STDOUT)
    return completed.returncode, log.read_text(encoding='utf-8', errors='replace')


def stop_of(run_dir: Path) -> dict:
    """The stop site and the last kernel call, from a run's own artifacts.

    Read from `jsrf_run.log`, which is what the run archived -- not from a summary
    someone wrote about it.
    """
    log = run_dir / 'jsrf_run.log'
    if not log.is_file():
        return {'found': False, 'reason': f'no jsrf_run.log in {run_dir.name}'}
    text = log.read_text(encoding='utf-8', errors='replace')
    stops = STOP.findall(text)
    kernels = LAST_KERNEL.findall(text)
    result = {
        'found': True,
        'run': run_dir.name,
        'invalid_icalls': len(stops),
        'last_kernel_call': None,
    }
    if stops:
        target, tid, esp, ret = stops[-1]
        result['stop'] = {
            'target': f'0x{target}',
            'tid': int(tid),
            'esp': esp,
            'return_address': ret,
        }
    if kernels:
        number, ordinal, slot = kernels[-1]
        result['last_kernel_call'] = {'n': int(number), 'ordinal': int(ordinal),
                                      'slot': int(slot)}
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', required=True,
                        help='the chore being gated, e.g. "sync-v0.13"')
    parser.add_argument('--before', type=Path,
                        help='an archived strict run taken before the chore')
    parser.add_argument('--after', type=Path,
                        help='an archived strict run taken after the chore')
    parser.add_argument('--skip-build', action='store_true')
    parser.add_argument('--skip-ctest', action='store_true')
    parser.add_argument('--out', type=Path,
                        help='write the gate record here')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    record: dict = {
        'gate': GATE_VERSION,
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'label': args.label,
        'note': ('A chore needs no Planner and no acceptance review (§5.8): this '
                 'gate is its closure. A CHANGED STOP IS A FINDING, not a failure '
                 '-- the plan asks for "the same stop (or the change in stop '
                 'recorded as the finding)".'),
        'steps': {},
    }
    failed: list[str] = []

    if args.skip_build:
        record['steps']['build'] = {'verdict': 'SKIPPED'}
    else:
        code, text = run([sys.executable, '-X', 'utf8', 'scripts/build-jsrf.py'],
                         ROOT / 'logs' / 'chore-gate-build.log')
        record['steps']['build'] = {
            'verdict': 'PASS' if code == 0 else 'FAIL',
            'exit_code': code,
            'log': 'logs/chore-gate-build.log',
            'tail': text.strip().splitlines()[-6:],
        }
        if code != 0:
            failed.append('build')

    if args.skip_ctest:
        record['steps']['ctest'] = {'verdict': 'SKIPPED'}
    else:
        code, text = run(['ctest', '--test-dir', 'build', '-C', 'Release',
                          '--output-on-failure'],
                         ROOT / 'logs' / 'chore-gate-ctest.log')
        summary = [line for line in text.splitlines()
                   if 'tests passed' in line or 'tests failed' in line]
        record['steps']['ctest'] = {
            'verdict': 'PASS' if code == 0 else 'FAIL',
            'exit_code': code,
            'log': 'logs/chore-gate-ctest.log',
            'summary': summary[-1] if summary else None,
        }
        if code != 0:
            failed.append('ctest')

    # The strict A/B.  A chore without both runs is INCOMPLETE, not PASS: the plan
    # asks for the comparison, and "no run" cannot show "the same stop".
    if args.before and args.after:
        before = stop_of(args.before)
        after = stop_of(args.after)
        ab: dict = {'before': before, 'after': after}
        before_stopped = bool(before.get('found') and before.get('stop'))
        after_stopped = bool(after.get('found') and after.get('stop'))
        if not (before.get('found') and after.get('found')):
            ab['verdict'] = 'UNKNOWN'
            ab['detail'] = 'one of the runs has no readable log'
        elif not before_stopped and not after_stopped:
            ab['verdict'] = 'NO_STOP_EITHER_SIDE'
            ab['detail'] = ('neither run reached an invalid indirect call, so the '
                            'A/B cannot show a stop site; it shows that both ran to '
                            'their bound')
        elif before_stopped and not after_stopped:
            # A distinct and important case: the run got FURTHER. That is usually
            # progress, and calling it "changed" would bury the direction.
            ab['verdict'] = 'STOP_REMOVED'
            ab['detail'] = (
                f"before stopped at {before['stop']['return_address']} and the "
                f"after run reached NO invalid indirect call; the chore moved the "
                f"horizon past that site -- RECORD THIS AS THE FINDING")
        elif not before_stopped and after_stopped:
            ab['verdict'] = 'STOP_APPEARED'
            ab['detail'] = (
                f"before reached no invalid indirect call and after stops at "
                f"{after['stop']['return_address']}; the chore moved the horizon "
                f"BACK -- RECORD THIS AS THE FINDING")
        elif before.get('stop') == after.get('stop'):
            ab['verdict'] = 'SAME_STOP'
            ab['detail'] = (f"both runs stop at {after['stop']['return_address']} "
                            f"return address")
        else:
            ab['verdict'] = 'STOP_CHANGED'
            ab['detail'] = (
                f"before: {before.get('stop')} / after: {after.get('stop')} -- "
                f"RECORD THIS AS THE FINDING")
        record['steps']['strict_ab'] = ab
        # An A/B that could not be read is as incomplete as one that was not
        # supplied. Measured by a control: a missing run directory produced
        # `UNKNOWN` inside the step while the overall verdict stayed PASS, because
        # only a MISSING --before/--after added to `failed`.
        if ab['verdict'] in ('UNKNOWN', 'NO_STOP_EITHER_SIDE'):
            failed.append('strict_ab')
    else:
        record['steps']['strict_ab'] = {
            'verdict': 'INCOMPLETE',
            'detail': ('pass --before and --after with archived strict runs; a '
                       'chore without the A/B cannot show "the same stop"'),
        }
        failed.append('strict_ab')

    record['failed'] = failed
    record['verdict'] = ('FAILED' if failed and set(failed) - {'strict_ab'}
                         else 'INCOMPLETE' if failed else 'PASS')

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True),
                            encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'chore gate {GATE_VERSION}  [{args.label}]')
        for name, step in record['steps'].items():
            print(f'  {step["verdict"]:<20} {name}')
            if step.get('summary'):
                print(f'      {step["summary"]}')
            if step.get('detail'):
                print(f'      {step["detail"]}')
            if name == 'strict_ab' and step.get('after', {}).get('stop'):
                print(f"      after stop: {step['after']['stop']}")
                print(f"      last kernel call: {step['after'].get('last_kernel_call')}")
        print(f'  VERDICT: {record["verdict"]}')
    return 0 if record['verdict'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
