"""`scripts/gen-startup-receipt.py`: fill the startup receipt from live checks.

Plan T13. The receipt at `docs/reviews/startup-current.md` is the rolling record of
a session's route readiness (`docs/agent-workflow.md` §0.6). It was filled by hand
every session, and hand-filling is how a receipt acquires a value nobody measured:
the fields are route identities, revisions and child ids, all of which are either
mechanically obtainable or must be reported as `UNKNOWN`.

**What this fills and what it will not.** It fills every field it can *measure* --
repository revisions, dirty paths, the `CURRENT PACKET` block, the tool versions,
the free-space state, the route table as resolved from the live catalog. It does
**not** invent the fields only a probe can establish: the packet-reviewer
probe's token, the advisor's continuation marker, the session's own model. Those
are emitted as explicit `UNVERIFIED` placeholders with the command that would fill
them, because a receipt that guesses is worse than one that is visibly incomplete.

**The rule it follows.** `docs/agent-workflow.md` §0.6 and the template both say
"do not mark readiness PASS with a failed or unknown required item". A generator
that defaulted an unmeasured field to a plausible value would violate that in the
one document whose whole purpose is to record what was actually verified.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLKIT = ROOT.parent / 'xboxrecomp'
PLAN = ROOT / 'plan-jsrf-bare-minimum.md'
TEMPLATE = ROOT / 'docs' / 'session-start-template.md'
OUTPUT = ROOT / 'docs' / 'reviews' / 'startup-current.md'

# The marker a field carries when only a probe can establish it.  Spelled so a
# reader cannot mistake it for a value: it is not "unknown", it is "not measured
# here, and here is what would measure it".
UNVERIFIED = 'UNVERIFIED (not measurable by this generator; see the note)'


def git(repo: Path, *args: str) -> str | None:
    try:
        completed = subprocess.run(['git', '-C', str(repo), *args],
                                   capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode not in (0, 1):
        return None
    return completed.stdout


def repo_state(repo: Path) -> dict:
    state: dict = {'path': str(repo), 'exists': repo.is_dir()}
    if not repo.is_dir():
        return state
    state['revision'] = (git(repo, 'rev-parse', 'HEAD') or '').strip() or None
    state['branch'] = (git(repo, 'rev-parse', '--abbrev-ref', 'HEAD') or '').strip() or None
    status = git(repo, 'status', '--porcelain')
    state['dirty'] = None if status is None else bool(status.strip())
    state['dirty_paths'] = ([] if not status else
                            [line[3:] for line in status.splitlines() if line.strip()])
    remotes = git(repo, 'remote', '-v') or ''
    state['remotes'] = sorted({line.split()[0] for line in remotes.splitlines()
                               if line.strip()})
    return state


def current_packet() -> dict:
    """The plan's `CURRENT PACKET` block, quoted rather than summarised."""
    if not PLAN.is_file():
        return {'present': False, 'reason': f'{PLAN.name} is missing'}
    text = PLAN.read_text(encoding='utf-8', errors='replace')
    match = re.search(r'^##\s*CURRENT PACKET.*?$(.*?)(?=^##\s|\Z)',
                      text, re.MULTILINE | re.DOTALL)
    if not match:
        return {'present': False, 'reason': 'the plan has no CURRENT PACKET heading'}
    body = match.group(1).strip()
    return {
        'present': True,
        'body': body,
        'plan_sha256': hashlib.sha256(PLAN.read_bytes()).hexdigest(),
        'names_a_packet': not re.search(r'\bnone\b', body, re.IGNORECASE) is None,
    }


def tool_versions() -> dict:
    """Versions of the tools the receipt names, from the tools themselves."""
    versions: dict = {}
    for name in ('just', 'pre-commit', 'clang-cl', 'ttd'):
        path = shutil.which(name)
        entry: dict = {'path': path}
        if path:
            try:
                completed = subprocess.run([path, '--version'],
                                           capture_output=True, text=True, timeout=30)
                first = (completed.stdout or completed.stderr).strip().splitlines()
                entry['version'] = first[0].strip() if first else None
            except (OSError, subprocess.TimeoutExpired):
                entry['version'] = None
        versions[name] = entry
    try:
        import duckdb
        versions['duckdb'] = {'version': duckdb.__version__}
    except ImportError:
        versions['duckdb'] = {'version': None}
    versions['python'] = {'path': sys.executable,
                          'version': sys.version.split()[0]}
    return versions


def disk_state() -> dict:
    try:
        usage = shutil.disk_usage(str(ROOT))
    except OSError:
        return {'measured': False}
    floor_gb = 15.0   # the pre-run gate's default (scripts/check-disk-gate.py)
    return {
        'measured': True,
        'free_gb': round(usage.free / 1024 ** 3, 2),
        'floor_gb': floor_gb,
        'above_floor': usage.free >= floor_gb * 1024 ** 3,
    }


def existing_field_values(path: Path) -> dict[str, str]:
    """Values already filled into an existing receipt, keyed by field label.

    **Why this exists.** Measured: re-running the generator over a receipt whose
    probe fields had been filled replaced all 14 of them with `UNVERIFIED`. The
    generator is *supposed* to be re-runnable -- it is the tool that fills the
    mechanical fields -- but regenerating must not silently discard the one part of
    the receipt a human or a probe supplied, because that part is the evidence.

    Only lines whose value is NOT the placeholder are carried over, so a receipt
    that was never filled is regenerated cleanly.
    """
    if not path.is_file():
        return {}
    carried: dict[str, str] = {}
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        stripped = line.lstrip('- ').strip()
        if not stripped or ':' not in stripped:
            continue
        label, _, value = stripped.partition(':')
        value = value.strip()
        if not value or value.startswith('UNVERIFIED'):
            continue
        if value.startswith(('`', '**')) or len(value) > 4:
            carried[label.strip()] = value
    return carried


def render(record: dict) -> str:
    game = record['repositories']['game']
    toolkit = record['repositories']['toolkit']
    packet = record['current_packet']
    disk = record['disk']
    carried: dict[str, str] = record.get('carried', {})

    def field(label: str, generated: str) -> str:
        """The carried value when one exists, else the generated text."""
        return carried.get(label, generated)
    lines = [
        '# Fresh-session startup receipt',
        '',
        'Filled per `docs/agent-workflow.md` §0.6 by '
        '`scripts/gen-startup-receipt.py`; this is a receipt, not a second policy.',
        'Fields marked **UNVERIFIED** cannot be measured by a generator and must be',
        'filled from the probe that establishes them.',
        '',
        f'Generated: {record["generated_utc"]}',
        '',
        '## Identity and handoff',
        '',
        f'- Session ID/date/harness: {field("Session ID/date/harness", UNVERIFIED)}',
        f'  (harness: `DSH_PROFILE={os.environ.get("DSH_PROFILE", "UNKNOWN")}`, '
        f'`DSH_WEB_URL={os.environ.get("DSH_WEB_URL", "UNKNOWN")}`)',
        f'- Actual main model/effort: {field("Actual main model/effort", UNVERIFIED)}',
        '- Workflow/plan/run-profile revisions and dirty diff identity: '
        f'workflow `{record["authority_hashes"].get("docs/agent-workflow.md", "?")[:16]}`, '
        f'plan `{record["authority_hashes"].get("plan-jsrf-bare-minimum.md", "?")[:16]}`, '
        f'run profiles `{record["authority_hashes"].get("docs/jsrf-run-profiles.md", "?")[:16]}`',
        f'- Game revision/status: `{game.get("branch")}` `{game.get("revision")}` '
        f'({"clean" if game.get("dirty") is False else "DIRTY" if game.get("dirty") else "unknown"})',
        f'- Toolkit revision/status: `{toolkit.get("branch")}` `{toolkit.get("revision")}` '
        f'({"clean" if toolkit.get("dirty") is False else "DIRTY" if toolkit.get("dirty") else "unknown"})',
        f'- Unrelated edits preserved: '
        f'{len(game.get("dirty_paths") or [])} game, '
        f'{len(toolkit.get("dirty_paths") or [])} toolkit',
        f'- CURRENT PACKET: {"present" if packet.get("present") else "ABSENT"}',
    ]
    if packet.get('present'):
        lines.append(f'  - plan hash `{packet["plan_sha256"]}`')
        lines.append(f'  - block: {packet["body"][:400]}')
    else:
        lines.append(f'  - {packet.get("reason", "not found")}')
    lines.extend([
        f'- Dependencies and their recorded acceptance reviews: '
        f'{field("Dependencies and their recorded acceptance reviews", UNVERIFIED)}',
        f'- Next exact authorized action: '
        f'{field("Next exact authorized action", UNVERIFIED)}',
        f'- Build/run owner and worker write ownership: '
        f'{field("Build/run owner and worker write ownership", UNVERIFIED)}',
        '',
        '## Route resolution — PASS / BLOCKED',
        '',
        'Resolved live in this session by the Session, not by this generator:',
        '',
        f'- Planner: {field("Planner", UNVERIFIED)} (Claude Opus 5.5 @ high, `route: LIVE_RESOLVE`, fresh child)',
        f'- Persistent advisor: {field("Persistent advisor", UNVERIFIED)} (Claude Opus 5.5 @ xhigh, '
        f'`route: CONTINUABLE_PINNED`)',
        f'- Muse guardrail: {field("Muse guardrail", UNVERIFIED)} (Grok 4.7 @ xhigh, `route: CONTINUABLE_PINNED`, '
        f'one session-continuable child)',
        f'- Packet reviewer: {field("Packet reviewer", UNVERIFIED)} (Claude Opus 5.5 @ medium, '
        f'`route: LIVE_RESOLVE`, fresh child per packet review)',
        f'- Turn reviewer: {field("Turn reviewer", UNVERIFIED)} (GPT-6.1 Sol @ high, '
        f'`route: LIVE_RESOLVE`, fresh child per turn, continued through its re-reviews; verified on first use)',
        f'- Workers: {field("Workers", UNVERIFIED)}',
        '- Exact error or ambiguity, if any:',
        '',
        '## Packet reviewer probe — PASS / FAIL / UNKNOWN',
        '',
        f'- Child ID; fresh token; response reference; empty-evidence answer; '
        f'command and output hash; effort; result: '
        f'{field("Child ID; fresh token; response reference; empty-evidence answer; command and output hash; effort; result", UNVERIFIED)}',
        '- Exact error or missing evidence:',
        '',
        '## Muse guardrail probe — PASS / FAIL / UNKNOWN',
        '',
        f'- Muse child ID; route; effort; workspace; named fact read; turn-2 marker; result: '
        f'{field("Muse child ID; route; effort; workspace; named fact read; turn-2 marker; result", UNVERIFIED)}',
        '- Exact error or missing evidence:',
        '',
        '## Persistent advisor probe — PASS / FAIL / UNKNOWN',
        '',
        f'- Child ID: {field("Child ID", UNVERIFIED)}',
        '- Turn 1 reference; unique marker given:',
        '- Named file and the fact deliberately omitted from the brief:',
        "- Advisor's answer; checked against the file:",
        '- Turn 2 reference (same child, marker not repeated); returned marker:',
        '- Result:',
        '- Exact error or missing evidence:',
        '',
        '## Packet readiness — PASS / BLOCKED / UNKNOWN',
        '',
        f'- Frozen revision/hash matches `CURRENT PACKET`: '
        f'{"N/A — no packet" if not packet.get("names_a_packet") else UNVERIFIED}',
        f'- Adequacy review record and verdict: '
        f'{field("Adequacy review record and verdict", UNVERIFIED)}',
        '- Deferred advisories (recorded, not acted on):',
        '- Prerequisites / tooling checks:',
    ])
    for name, entry in record['tools'].items():
        lines.append(f'  - {name}: {entry.get("version") or "UNKNOWN"} '
                     f'({entry.get("path") or "not on PATH"})')
    if disk.get('measured'):
        verdict = 'above' if disk['above_floor'] else 'BELOW'
        lines.append(f'  - free space: {disk["free_gb"]} GB ({verdict} '
                     f'{disk["floor_gb"]} GB floor)')
    else:
        lines.append('  - free space: UNKNOWN (could not measure)')
    lines.extend([
        '- State/plan disagreements and how they were escalated:',
        f'- Overall disposition and next action: '
        f'{field("Overall disposition and next action", UNVERIFIED)}',
        '',
        'Do not mark readiness PASS with a failed or unknown required item. A provider',
        'catalog entry is not a completed invocation. A new advisor answering the',
        'follow-up is not continuation. A readiness response is not acceptance of a',
        'game packet.',
        '',
    ])
    return '\n'.join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=OUTPUT)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--dry-run', action='store_true',
                        help='print the receipt instead of writing it')
    args = parser.parse_args()

    authority = {}
    for relative in ('docs/agent-workflow.md', 'plan-jsrf-bare-minimum.md',
                     'docs/jsrf-run-profiles.md', 'AGENTS.md'):
        path = ROOT / relative
        authority[relative] = (hashlib.sha256(path.read_bytes()).hexdigest()
                               if path.is_file() else 'MISSING')

    record = {
        'generator': 'jsrf-startup-receipt/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'repositories': {
            'game': repo_state(ROOT),
            'toolkit': repo_state(TOOLKIT),
        },
        'authority_hashes': authority,
        'current_packet': current_packet(),
        'tools': tool_versions(),
        'disk': disk_state(),
    }
    # Carry over any field an earlier run or a probe already filled, so a
    # re-run refreshes the mechanical fields without discarding the evidence.
    record['carried'] = existing_field_values(args.out)
    record['receipt'] = render(record)

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
        return 0

    if args.dry_run:
        print(record['receipt'])
        return 0

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(record['receipt'], encoding='utf-8')
    digest = hashlib.sha256(record['receipt'].encode('utf-8')).hexdigest()
    print(f'wrote {args.out.relative_to(ROOT)}')
    print(f'  receipt sha256: {digest}')
    game = record['repositories']['game']
    toolkit = record['repositories']['toolkit']
    print(f'  game    : {game.get("revision")} [{game.get("branch")}]')
    print(f'  toolkit : {toolkit.get("revision")} [{toolkit.get("branch")}]')
    print(f'  packet  : {"present" if record["current_packet"].get("present") else "absent"}')
    print()
    print('  The receipt contains UNVERIFIED placeholders for every field only a')
    print('  probe can establish. Fill them from the probe results; do not replace')
    print('  one with a plausible value.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
