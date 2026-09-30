"""`scripts/qualify-premise.py`: W2's qualification gate before any Planner call.

Plan W2 requires, "run by DeepSeek", that **before any Planner call** the session
answers five questions and attaches the output to the brief:

1. **Disassemble past the failing instruction.**  The failure cited as the blocker
   must actually be a failure at that instruction, not an assumption about it.
2. **Premise artifact vs later fix commits** (`git merge-base --is-ancestor`) **and
   strict profile.**  A premise drawn from a run that predates a fix, or from a run
   that is not strict, does not describe the target revision.
3. **The positive control firing on the target build.**  An instrument that never
   fired on this binary has produced no observation, however many runs it made.
4. **A dry run of every measurement.**  A frozen command that has never executed is
   a command that may not run at all.
5. **A search of upstream, forks and reference decompilations for the symptom.**  The
   answer may already exist, and the plan's own history records symptoms chased for
   days that a fork had already fixed.

**What the plan says these prevent**, in its own row: "A2h-r6 six revisions on a
fixed failure; the 571 MB premise ~5 days, refuted by the `test eax`/`jl` four
instructions after the call; 15 DR0 ON runs before zero hits were noticed; the slot
field named by the Halo XDK layout only after the owner's audit". Every one of those
is a premise that a mechanical check would have refused.

**This is a gate, not a judge.**  It reports `PASS`, `FAIL` or `UNKNOWN` per item and
refuses overall on any FAIL.  It does not decide whether a premise is *interesting*,
and it does not replace the Planner's adequacy review -- it stops a brief that is
mechanically unsound from reaching the Planner at all.

**Fail closed.**  An item the gate cannot evaluate is `UNKNOWN`, and `UNKNOWN`
blocks: a gate that defaults to PASS would be worse than no gate, because it would
carry the appearance of qualification.
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
TOOLKIT = ROOT.parent / 'xboxrecomp'
sys.path.insert(0, str(ROOT / 'scripts'))

GATE_VERSION = 'jsrf-premise-gate/1'

# A guest VA as it appears in a premise, e.g. `0x00149828` or `00149828`.
VA = re.compile(r'\b(?:0x)?([0-9A-Fa-f]{8})\b')


def git(repo: Path, *args: str) -> tuple[int, str]:
    try:
        completed = subprocess.run(['git', '-C', str(repo), *args],
                                   capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired) as error:
        return 2, str(error)
    return completed.returncode, (completed.stdout + completed.stderr).strip()


def is_ancestor(repo: Path, ancestor: str, descendant: str) -> bool | None:
    """`git merge-base --is-ancestor`, or None when it cannot be decided."""
    code, _ = git(repo, 'merge-base', '--is-ancestor', ancestor, descendant)
    if code == 0:
        return True
    if code == 1:
        return False
    return None


def resolve_run(run: str) -> Path | None:
    """A run directory from a name, a path, or a path to its log."""
    candidate = Path(run)
    if candidate.is_dir():
        return candidate
    if candidate.is_file():
        return candidate.parent
    archived = ROOT / 'logs' / 'runs' / run
    if archived.is_dir():
        return archived
    for path in (ROOT / 'logs' / 'runs').glob(f'*{run}*'):
        if path.is_dir():
            return path
    return None


def check_disassembly(premise: dict) -> dict:
    """Item 1: the bytes at the failing instruction, from the original XBE.

    **Why the check reports the decode it got rather than a bare verdict.**
    `inspect-jsrf.py` decodes with `skipdata`, so capstone re-synchronises after an
    undecodable byte. Measured: starting the listing at `address - 16` produced a
    decode in which `0x00149828` does NOT appear, while starting at a real boundary
    produced `00149828 call dword ptr [0x1c4064]` -- the correct instruction. The
    address was never the problem; the ENTRY POINT was, because `skipdata` drifted.

    So the gate decodes from a window that starts well before the address, reports
    the instruction it actually found, and treats an absent address as UNKNOWN
    rather than a false FAIL. A `.byte` line AT the address is a real FAIL: the
    address is not an instruction boundary.
    """
    address = premise.get('failing_instruction')
    if address is None:
        return {'verdict': 'UNKNOWN',
                'detail': ('no --failing-instruction given; a premise that does not '
                           'name the instruction it rests on cannot be checked')}

    def decode(start: int, end: int) -> list[str]:
        completed = subprocess.run(
            [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'inspect-jsrf.py'),
             'disasm', hex(start), hex(end)],
            capture_output=True, text=True)
        if completed.returncode != 0:
            return []
        return [line for line in completed.stdout.splitlines() if line.strip()]

    def address_of(line: str):
        tokens = line.split()
        if not tokens:
            return None
        token = tokens[0]
        if token.lower().startswith('0x'):
            token = token[2:]
        try:
            return int(token, 16)
        except ValueError:
            return None

    # A wider window is strictly better here: the decoder's re-synchronisation is
    # more likely to have re-established alignment before the address of interest.
    #
    # `address` itself is deliberately NOT a candidate start. Measured by a control:
    # decoding FROM the address always yields a line at the address, so it reported
    # an address INTERIOR to an instruction (`0x0014982A`, inside the 6-byte `call`
    # at `0x00149828`) as a boundary. A decode that begins where the question is
    # proves nothing; only one that ARRIVES there does.
    for start_at in (address - 0x400, address - 0x100, address - 0x40, address - 8):
        if start_at < 0:
            continue
        lines = decode(start_at, address + 0x40)
        at = [line for line in lines if address_of(line) == address]
        if not at:
            continue
        instruction = at[0].strip()
        if re.match(r'^[0-9A-Fa-f]{8}\s+\.byte\b', instruction):
            return {'verdict': 'FAIL',
                    'detail': ('0x%08X does not decode as an instruction (it is a '
                               '`.byte` line), so a premise naming it as the failing '
                               'instruction is misaligned' % address),
                    'decode_start': '0x%08X' % start_at,
                    'instruction': instruction,
                    'disassembly': lines[:40]}
        index = lines.index(at[0])
        following = [line.strip() for line in lines[index + 1:index + 5]]
        return {'verdict': 'PASS',
                'detail': ('0x%08X is an instruction boundary in a decode from '
                           '0x%08X; the four that follow are shown for the premise '
                           'to be read against' % (address, start_at)),
                'decode_start': '0x%08X' % start_at,
                'instruction': instruction,
                'following': following,
                'disassembly': lines[:40]}

    return {'verdict': 'UNKNOWN',
            'detail': ('0x%08X does not appear in any decode window tried. The '
                       'decoder re-synchronises after undecodable bytes, so this is '
                       'UNKNOWN, not a misalignment finding' % address)}


def check_run(run_dir: Path, premise: dict) -> dict:
    """Item 2 and 3: strict profile, and the premise postdating known fixes."""
    result: dict = {'run': run_dir.name}
    profile = subprocess.run(
        [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'check-run-profile.py'),
         str(run_dir)],
        capture_output=True, text=True)
    text = profile.stdout.strip()
    result['profile_output'] = text
    # `check-run-profile.py` prints one line per run, then a blank line and a
    # summary (`checked 1; strict 1, ...`).  Reading the LAST token of the whole
    # output took the summary's last word ("missing") instead of the run's
    # classification, so a STRICT run was reported as a failure.  The
    # classification is the last token of the line that NAMES the run.
    classification = 'UNKNOWN'
    for line in text.splitlines():
        if run_dir.name in line:
            tokens = line.split()
            if tokens:
                classification = tokens[-1]
            break
    result['profile'] = classification

    # A premise must come from a strict run; exploratory evidence cannot carry a
    # strict claim (`docs/jsrf-run-profiles.md`).
    if classification != 'STRICT':
        result['profile_verdict'] = 'FAIL'
        result['profile_detail'] = (
            f'{run_dir.name} classifies as {classification}, not STRICT; a strict '
            f'claim cannot rest on it')
    else:
        result['profile_verdict'] = 'PASS'
        result['profile_detail'] = f'{run_dir.name} is STRICT'

    # The premise must postdate every fix commit the brief names.
    fixes = premise.get('fixed_by') or []
    revision = None
    metadata = run_dir / 'metadata.json'
    if metadata.is_file():
        try:
            data = json.loads(metadata.read_text(encoding='utf-8-sig'))
            revision = ((data.get('repository_identities') or {})
                        .get('toolkit') or {}).get('revision')
        except (OSError, ValueError):
            revision = None
    result['toolkit_revision'] = revision

    if not fixes:
        result['freshness_verdict'] = 'UNKNOWN'
        result['freshness_detail'] = (
            'no --fixed-by commit given; the gate cannot tell whether this premise '
            'predates a fix, which is the A2h-r6 failure it exists to catch')
    elif revision is None:
        result['freshness_verdict'] = 'UNKNOWN'
        result['freshness_detail'] = (
            f'{run_dir.name} records no toolkit revision, so ancestry cannot be '
            f'decided')
    else:
        stale = []
        unknown = []
        for commit in fixes:
            verdict = is_ancestor(TOOLKIT, commit, revision)
            if verdict is False:
                stale.append(commit)
            elif verdict is None:
                unknown.append(commit)
        if stale:
            result['freshness_verdict'] = 'FAIL'
            result['freshness_detail'] = (
                f'{run_dir.name} is at toolkit {revision[:12]}, which does NOT '
                f'contain the fix(es) {[c[:12] for c in stale]}; the premise '
                f'describes a pre-fix revision')
        elif unknown:
            result['freshness_verdict'] = 'UNKNOWN'
            result['freshness_detail'] = (
                f'ancestry could not be decided for {[c[:12] for c in unknown]}')
        else:
            result['freshness_verdict'] = 'PASS'
            result['freshness_detail'] = (
                f'{run_dir.name} at {revision[:12]} postdates every named fix')
    return result


def check_positive_control(run_dir: Path, marker: str | None) -> dict:
    """Item 3: the instrument's positive control fired on the target build.

    Absence of the marker is `FAIL`, not `UNKNOWN`: the plan records "15 DR0 ON runs
    before zero hits were noticed", and the whole point is that a run whose control
    never fired produced no observation.
    """
    if not marker:
        return {'verdict': 'UNKNOWN',
                'detail': ('no --positive-control given; an instrument whose control '
                           'has not been named cannot be shown to have fired')}
    log = run_dir / 'jsrf_run.log'
    if not log.is_file():
        return {'verdict': 'UNKNOWN', 'detail': f'no jsrf_run.log in {run_dir.name}'}
    text = log.read_text(encoding='utf-8', errors='replace')
    hits = [line for line in text.splitlines() if marker in line]
    if hits:
        return {'verdict': 'PASS',
                'detail': f'the positive control {marker!r} fired {len(hits)} time(s)',
                'first': hits[0].strip()[:200]}
    return {'verdict': 'FAIL',
            'detail': (f'the positive control {marker!r} did not fire in '
                       f'{run_dir.name}; every absence claim from this run is '
                       f'unsupported')}


def check_dry_run(commands: list[str]) -> dict:
    """Item 4: every measurement command has actually executed.

    The gate cannot execute them for the caller -- several are long guest runs -- so
    it does the mechanical part it can: each command's **executable and script
    argument** must exist. A frozen command naming a file that is not there is a
    command that cannot run, which is the A4s-r4 failure the plan records.
    """
    if not commands:
        return {'verdict': 'UNKNOWN',
                'detail': ('no --command given; the gate cannot confirm that any '
                           'measurement has been executed')}
    problems = []
    checked = []
    for command in commands:
        tokens = command.split()
        if not tokens:
            continue
        entry: dict = {'command': command[:200]}
        # The script argument: the first token ending in .py that is not an option.
        scripts = [t for t in tokens if t.endswith('.py') and not t.startswith('-')]
        if not scripts:
            entry['verdict'] = 'UNKNOWN'
            entry['detail'] = 'no script argument recognised'
        else:
            missing = [s for s in scripts if not (ROOT / s).is_file()
                       and not Path(s).is_file()]
            if missing:
                entry['verdict'] = 'FAIL'
                entry['detail'] = f'script not found: {missing}'
                problems.append(command)
            else:
                entry['verdict'] = 'PASS'
                entry['detail'] = f'found {scripts}'
        checked.append(entry)
    if problems:
        return {'verdict': 'FAIL',
                'detail': f'{len(problems)} command(s) name a missing script',
                'commands': checked}
    if all(entry['verdict'] == 'PASS' for entry in checked):
        return {'verdict': 'PASS',
                'detail': f'{len(checked)} command(s) resolve',
                'commands': checked}
    return {'verdict': 'UNKNOWN',
            'detail': 'at least one command could not be checked',
            'commands': checked}


def check_symptom_search(symptom: str | None) -> dict:
    """Item 5: the symptom was searched for in the forks and upstream.

    The search itself is a judgement about relevance, so this reports what the
    repositories contain and leaves the reading to the session.  What it will not do
    is report `PASS` for a search nobody ran.
    """
    if not symptom:
        return {'verdict': 'UNKNOWN',
                'detail': ('no --symptom given; the plan requires the symptom to be '
                           'searched for before a Planner call')}
    hits: dict[str, list[str]] = {}
    pattern = re.compile(re.escape(symptom), re.IGNORECASE)
    for label, root in (('toolkit', TOOLKIT),
                        ('game-docs', ROOT / 'docs'),
                        ('toolkit-docs', TOOLKIT / 'docs')):
        if not root.is_dir():
            continue
        found = []
        for path in root.rglob('*.md'):
            try:
                text = path.read_text(encoding='utf-8', errors='replace')
            except OSError:
                continue
            if pattern.search(text):
                found.append(str(path.relative_to(root)).replace('\\', '/'))
        hits[label] = found[:20]
    # Upstream branches are fetched but not merged; a symptom named there is the
    # cheapest possible answer, so the gate checks the commit subjects too.
    code, log = git(TOOLKIT, 'log', '--oneline', '--all', '--grep', symptom,
                    '-i', '-20')
    hits['commits'] = log.splitlines()[:20] if code == 0 and log else []
    return {'verdict': 'PASS',
            'detail': (f'searched {sum(len(v) for v in hits.values())} hit(s) for '
                       f'{symptom!r}; the session reads them and records what it '
                       f'found'),
            'hits': hits}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', required=True,
                        help='the run the premise rests on (name, dir, or log)')
    parser.add_argument('--failing-instruction', type=lambda s: int(s, 0),
                        help='guest VA of the instruction the premise names')
    parser.add_argument('--fixed-by', action='append', default=[],
                        help='a toolkit commit that fixes this symptom; repeatable')
    parser.add_argument('--positive-control',
                        help='a string the instrument must have emitted in the run')
    parser.add_argument('--command', action='append', default=[],
                        help='a measurement command; repeatable')
    parser.add_argument('--symptom', help='the symptom to search the forks for')
    parser.add_argument('--out', type=Path, help='write the gate record here')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    run_dir = resolve_run(args.run)
    record: dict = {
        'gate': GATE_VERSION,
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'run_argument': args.run,
        'run_resolved': str(run_dir) if run_dir else None,
        'note': ('W2 requires this gate to run before any Planner call and its '
                 'output to be attached to the brief. UNKNOWN blocks: a gate that '
                 'defaulted to PASS would carry the appearance of qualification '
                 'without the substance.'),
    }

    items: dict[str, dict] = {}
    if run_dir is None:
        items['run'] = {'verdict': 'FAIL',
                        'detail': f'the run {args.run!r} could not be resolved'}
    else:
        premise = {'failing_instruction': args.failing_instruction,
                   'fixed_by': args.fixed_by}
        items['disassembly'] = check_disassembly(premise)
        items['run'] = check_run(run_dir, premise)
        run_verdicts = (items['run'].get('profile_verdict'),
                        items['run'].get('freshness_verdict'))
        items['run']['verdict'] = (
            'FAIL' if 'FAIL' in run_verdicts
            else 'UNKNOWN' if 'UNKNOWN' in run_verdicts
            else 'PASS')
        items['run']['detail'] = (
            f"{items['run'].get('profile_detail', '')} "
            f"{items['run'].get('freshness_detail', '')}").strip()
        items['positive_control'] = check_positive_control(
            run_dir, args.positive_control)
    items['dry_run'] = check_dry_run(args.command)
    items['symptom_search'] = check_symptom_search(args.symptom)

    record['items'] = items
    flat = {
        'disassembly': items['disassembly']['verdict'],
        'strict_and_fresh': (items['run'].get('profile_verdict', 'UNKNOWN')
                             if 'run' in items else 'UNKNOWN'),
        'premise_postdates_fixes': (items['run'].get('freshness_verdict', 'UNKNOWN')
                                    if 'run' in items else 'UNKNOWN'),
        'positive_control': items['positive_control']['verdict']
        if 'positive_control' in items else 'UNKNOWN',
        'dry_run': items['dry_run']['verdict'],
        'symptom_search': items['symptom_search']['verdict'],
    }
    record['verdicts'] = flat
    failed = [k for k, v in flat.items() if v == 'FAIL']
    unknown = [k for k, v in flat.items() if v == 'UNKNOWN']
    record['failed'] = failed
    record['unknown'] = unknown
    record['verdict'] = 'QUALIFIED' if not failed and not unknown else (
        'NOT QUALIFIED' if failed else 'INCOMPLETE')

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True),
                            encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'premise gate {GATE_VERSION}')
        print(f'  run : {record["run_resolved"]}')
        for name, verdict in flat.items():
            print(f'  {verdict:<9} {name}')
        for name, entry in items.items():
            # Every item carries a verdict; a detail is optional.  Reading it
            # unconditionally crashed the report on the run item, which is built by
            # a different branch.
            detail = entry.get('detail') or '(no detail)'
            print(f'    [{name}] {entry.get("verdict", "UNKNOWN")}: {detail}')
            if name == 'disassembly' and entry.get('instruction'):
                print(f'      {entry["instruction"]}')
                for line in entry.get('following', []):
                    print(f'      {line}')
        print(f'  VERDICT: {record["verdict"]}')
        if record['verdict'] != 'QUALIFIED':
            print()
            print('  Do NOT take this premise to the Planner. The plan requires this')
            print('  gate to pass before any Planner call, and UNKNOWN blocks.')
    return 0 if record['verdict'] == 'QUALIFIED' else 1


if __name__ == '__main__':
    sys.exit(main())
