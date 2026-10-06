"""Report whether a run actually EXERCISED the recovery entries it is cited for.

**Why this exists (measured failure).** This session recorded `0x00032610` as
"run-confirmed" by run `f25`, which never dispatched the address: f25's log has
ZERO occurrences of `[RECOVERED] 0x00032610 returned; ABI verified`, and all 14
textual `32610` matches were the run's own directory name in `[SAVE]`/`[FBWIN]`
path strings. A clean run that never reaches the changed code proves nothing, and
the error survived three commits. The Turn Reviewer caught it (finding B3); this
script makes the check mechanical so it cannot recur.

**Why a run can be clean and still prove nothing.** The boot path is
nondeterministic: `f25` and `f26` share `exe_sha256 = ca867957...`, yet f25
reached the disclaimer at 960 presents while f26 stalled on the Smilebit card at
193 presents for its whole 810 s. `f24` never reached `0x9CC40` at all. So
acceptance is **path-aware**: a run counts for an address only when its log
contains that address's ABI-verified return line.

**Why a return line is not enough (measured).** The `returned; ABI verified` line is
written once, on the first successful return, and it is **not exclusive with a
later failure for the same address**. In the strict run
`20260930-225440-580-f3-alias-fix-strict`, `0x00026780` logs the return at line
75094 and then

    [RECOVERED] ABI FAILURE 0x00026780 esp 00F7FEE0->00F7FCBC expected +4

at line 77608 -- so that address has a broken stack contract in that run, yet this
script reported PASS for it. An address that both returned and failed is **not**
clean. `JSRF_ABI_CONTINUE` is the other half: it turns the ABI check into a report,
so a run under it can log many failures and keep going, and its returns do not
establish that the contracts held.

Usage:
    python -X utf8 scripts/check-run-exercised.py <run-dir> <va> [<va> ...]
    python -X utf8 scripts/check-run-exercised.py <run-dir> --list

Exit 0 when every requested address was exercised, 1 otherwise. `--list` prints
every address the run exercised and exits 0.
"""
from pathlib import Path
import argparse
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

# `[RECOVERED] 0x0013B750 returned; ABI verified (ESP/EBX/ESI/EDI)`
RETURNED = re.compile(r'\[RECOVERED\]\s+0x([0-9A-Fa-f]{8})\s+returned;\s*ABI verified')
# `[RECOVERED] ABI FAILURE 0x00026780 esp ... expected +4`
ABI_FAILURE = re.compile(r'\[RECOVERED\]\s+ABI FAILURE\s+0x([0-9A-Fa-f]{8})')
# `[ALIAS-ICALL] target=0x00032610 owner=0x00033800` -- an alias fold ran the
# wrong body, which is itself a defect worth surfacing next to the acceptance.
ALIAS_ICALL = re.compile(r'\[ALIAS-ICALL\]\s+target=0x([0-9A-Fa-f]{8})\s+owner=0x([0-9A-Fa-f]{8})')


def normalise(va: str) -> str:
    return '0x%08X' % int(va, 16)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_dir', help='an archived run directory')
    parser.add_argument('addresses', nargs='*',
                        help='guest VAs whose ABI-verified return must appear')
    parser.add_argument('--list', action='store_true',
                        help='list every address the run exercised, then exit 0')
    args = parser.parse_args()

    run = Path(args.run_dir)
    if not run.is_dir():
        run = ROOT / args.run_dir
    if not run.is_dir():
        # A bare run name is how the run archive is normally cited, so resolve it
        # the way every other script does.  Without this the checker looked in
        # the repository root, found nothing, and reported "cannot establish
        # coverage" -- a FAIL for a run that exists, which is the same
        # silent-misdirection shape as crediting a run that never ran.
        run = ROOT / 'logs' / 'runs' / args.run_dir
    log = run / 'jsrf_run.log'
    if not log.is_file():
        print(f'{log} is missing; cannot establish coverage', file=sys.stderr)
        return 1

    text = log.read_text(encoding='utf-8', errors='replace')
    returned = {normalise('0x' + m) for m in RETURNED.findall(text)}
    failed = {normalise('0x' + m) for m in ABI_FAILURE.findall(text)}
    # An address that returned AND later failed is not clean: the return line is
    # written once and is not exclusive with a later failure for the same address.
    clean = returned - failed
    aliased = {(normalise('0x' + t), normalise('0x' + o))
               for t, o in ALIAS_ICALL.findall(text)}

    print(f'run      : {run.name}')
    print(f'exercised: {len(clean)} recovered entr(y/ies) with a CLEAN ABI-verified return')
    if failed:
        both = sorted(returned & failed)
        print(f'abi-failure lines: {len(failed)}; returned AND failed: {len(both)}')
        for va in both:
            print(f'   {va}  <-- returned first, then failed ABI: NOT clean')
    if aliased:
        print(f'alias-icall lines: {len(aliased)}')
        for t, o in sorted(aliased):
            print(f'   target={t} owner={o}  <-- a folded alias ran the WRONG body')

    if args.list or not args.addresses:
        for va in sorted(clean):
            print(f'   {va}')
        return 0

    failures = 0
    for raw in args.addresses:
        va = normalise(raw)
        if va in clean:
            print(f'PASS  {va} was exercised')
        elif va in returned:
            print(f'FAIL  {va} returned but ALSO logged an ABI FAILURE in this run, so '
                  f'its stack contract is broken here; it is not clean evidence')
            failures += 1
        else:
            print(f'FAIL  {va} was NOT exercised by this run -- it proves nothing '
                  f'about that address; rerun on the branch that reaches it')
            failures += 1
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
