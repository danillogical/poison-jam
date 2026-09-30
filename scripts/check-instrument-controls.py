"""`scripts/check-instrument-controls.py`: W9's control-first gate.

Plan W9: "**Control-first instruments**: `run-jsrf.py` refuses a second ON run, and
checkers refuse to emit a row, until the raw log shows the named positive-control
event at an independently derived address", answering "13 A2h instrument defects; a
positive control silently weakened to a value comparison".

**The two failures, restated.** An instrument was run ON repeatedly while its
positive control had never fired, so every "no hits" conclusion rested on an
instrument nobody had shown could see anything ("15 DR0 ON runs before zero hits were
noticed", from the plan's W2 row). And a positive control was at some point rewritten
from "the event occurred at address X" to "some value changed" -- a weaker predicate
that passes for reasons unrelated to the instrument.

**What this enforces, mechanically.**

  * A run that enabled an instrument is recorded with the **named positive control**
    and the **independently derived address** it must appear at. A run whose control
    did not fire is `UNCONTROLLED`.
  * While an `UNCONTROLLED` run is the most recent instrumented run, **a second ON
    run is refused**. The point is not to punish the first run; it is that running
    the same broken instrument again produces another uninterpretable result.
  * A control that names no address is refused at registration: "some value changed"
    is the weakened form W9 exists to stop.

**Where the state lives.** `logs/instrument-controls.json`, gitignored with the rest
of `logs/`. It is run state, not a record; the durable finding is the run's own log.

**What it does not do.** It cannot check that the address is the RIGHT one -- that is
the "independently derived" part, which is a judgement about method. It records the
derivation the caller states, so a later reader can see what it rested on.
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
STATE = ROOT / 'logs' / 'instrument-controls.json'
CHECKER_VERSION = 'jsrf-instrument-controls/1'

# An override name that enables an instrument.  Observation-only switches are
# excluded: W9 is about instruments whose OUTPUT a decision depends on, and the plan
# lists those separately (`docs/jsrf-run-profiles.md`, "Observation only").
INSTRUMENT_PREFIXES = ('JSRF_TRACE_', 'JSRF_WATCH_', 'RECOMP_KERNEL_WATCH',
                       'RECOMP_WATCH', 'JSRF_DR', 'JSRF_INSTRUMENT')


def load_state() -> dict:
    if not STATE.is_file():
        return {'version': 1, 'runs': []}
    try:
        data = json.loads(STATE.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {'version': 1, 'runs': [], 'unreadable': True}
    data.setdefault('runs', [])
    return data


def save_state(state: dict) -> None:
    STATE.parent.mkdir(parents=True, exist_ok=True)
    STATE.write_text(json.dumps(state, indent=2, sort_keys=True), encoding='utf-8')


def instruments_in(settings: dict[str, str]) -> dict[str, str]:
    """The instrument switches a launch enables, by name."""
    found = {}
    for name, value in settings.items():
        upper = name.upper()
        if not any(upper.startswith(p) for p in INSTRUMENT_PREFIXES):
            continue
        # Presence is what enables these; the value is recorded for the reader.
        found[name] = value
    return found


def control_fired(log: Path, control: str, address: str | None) -> tuple[bool, str]:
    """Did the named positive control appear, at the named address?

    Both are required. Measured failure W9 answers: a control "silently weakened to a
    value comparison" -- the event was found but the ADDRESS stopped being checked,
    so a control that fired anywhere counted as having fired where it should.
    """
    if not log.is_file():
        return False, f'no log at {log}'
    text = log.read_text(encoding='utf-8', errors='replace')
    hits = [line for line in text.splitlines() if control in line]
    if not hits:
        return False, f'the control {control!r} does not appear in {log.name}'
    if address:
        # `str.lstrip('0x')` strips CHARACTERS, not a prefix: it turns `0x001C4064`
        # into `1C4064` (the leading zeros go too) but would also turn `0x00` into
        # an empty string. Measured: that made a control that DID fire at the named
        # address report as never having fired there. The prefix is removed
        # explicitly, and leading zeros are handled by the boundary pattern below.
        digits = address.strip()
        if digits.lower().startswith('0x'):
            digits = digits[2:]
        # The boundary must NOT be `\b` before the optional `0x`: on a real log line
        # (`fault=0x001C4064`), `\b` cannot match between the `x` and the `0` because
        # both are word characters. The lookarounds assert "not part of a longer hex
        # run" instead, which is the property the boundary was standing in for.
        pattern = re.compile(
            r'(?<![0-9A-Fa-f])(?:0x)?0*' + re.escape(digits.lstrip('0') or '0')
            + r'(?![0-9A-Fa-f])',
            re.IGNORECASE)
        at_address = [line for line in hits if pattern.search(line)]
        if not at_address:
            return False, (f'the control {control!r} fired {len(hits)} time(s) but '
                           f'never at {address}; a value comparison is not an '
                           f'address match')
        return True, f'the control fired at {address} ({len(at_address)} line(s))'
    return True, f'the control fired {len(hits)} time(s)'


def record(args) -> int:
    if not args.instrument:
        print('--instrument is required to record a run', file=sys.stderr)
        return 2
    if not args.control:
        print('--control is required: a run whose control is unnamed cannot be '
              'shown to have observed anything', file=sys.stderr)
        return 2
    if not args.address:
        print('--address is required: W9 refuses a control that names no address, '
              'because "some value changed" is the weakened form it exists to stop',
              file=sys.stderr)
        return 2

    log = Path(args.log) if args.log else None
    if log is None:
        print('--log is required: the control is checked against the run\'s own log',
              file=sys.stderr)
        return 2

    fired, detail = control_fired(log, args.control, args.address)
    entry = {
        'instrument': args.instrument,
        'control': args.control,
        'address': args.address,
        'derivation': args.derivation or None,
        'log': str(log),
        'run': args.run or None,
        'recorded_utc': datetime.now(timezone.utc).isoformat(),
        'control_fired': fired,
        'detail': detail,
    }
    state = load_state()
    state['runs'].append(entry)
    save_state(state)

    print(f"recorded {args.instrument}: control "
          f"{'FIRED' if fired else 'DID NOT FIRE'} - {detail}")
    if not fired:
        print()
        print('  This run is UNCONTROLLED. A second ON run of an instrument whose')
        print('  control has not fired is refused until the control fires: running')
        print('  the same broken instrument again produces another uninterpretable')
        print('  result.')
    return 0 if fired else 1


def check(args) -> int:
    """Would a new ON run be allowed?"""
    state = load_state()
    runs = [r for r in state['runs'] if r.get('instrument') == args.instrument] \
        if args.instrument else state['runs']
    uncontrolled = [r for r in runs if not r.get('control_fired')]

    record_out = {
        'checker': CHECKER_VERSION,
        'instrument': args.instrument,
        'runs_recorded': len(runs),
        'uncontrolled': len(uncontrolled),
        'verdict': 'REFUSED' if uncontrolled else 'ALLOWED',
    }
    if uncontrolled:
        last = uncontrolled[-1]
        record_out['detail'] = (
            f"the last run of {last['instrument']} did not fire its control "
            f"{last['control']!r} at {last['address']}: {last['detail']}")
    else:
        record_out['detail'] = 'no uncontrolled instrument run is outstanding'

    if args.json:
        print(json.dumps(record_out, indent=2, sort_keys=True))
    else:
        print(f"instrument controls: {record_out['verdict']} "
              f"({record_out['runs_recorded']} run(s) recorded, "
              f"{record_out['uncontrolled']} uncontrolled)")
        print(f"  {record_out['detail']}")
    return 1 if uncontrolled else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)

    record_parser = sub.add_parser('record', help='record an instrumented run')
    record_parser.add_argument('--instrument', required=True)
    record_parser.add_argument('--control', required=True,
                               help='the positive-control event the log must show')
    record_parser.add_argument('--address', required=True,
                               help='the independently derived address it must be at')
    record_parser.add_argument('--derivation',
                               help='how the address was derived independently')
    record_parser.add_argument('--log', type=Path, required=True)
    record_parser.add_argument('--run', help='the archived run directory name')

    check_parser = sub.add_parser('check', help='would a new ON run be allowed?')
    check_parser.add_argument('--instrument')
    check_parser.add_argument('--json', action='store_true')

    args = parser.parse_args()
    if args.command == 'record':
        return record(args)
    return check(args)


if __name__ == '__main__':
    sys.exit(main())
