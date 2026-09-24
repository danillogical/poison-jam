"""Audit the agent-facing documents for stale or duplicated authority.

P0.3 makes the single-authority split checkable.  The failure this exists to
catch is not a typo: it is a **fact copied into several files**, which is how five
stale claims survived on 2026-09-22 — including one that forbade the reviewer the
policy required.  A duplicated roster is the same mechanism even when labelled
"summary", so this checker fails on duplication rather than trusting the label.

Usage::

    python -X utf8 scripts/check-agent-docs.py --check
    python -X utf8 scripts/check-agent-docs.py --check --json

Exit 0 clean, 1 a real finding, 2 the checker could not run (missing input).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER_VERSION = 'jsrf-agent-docs/2'

AGENTS = 'AGENTS.md'
WORKFLOW = 'docs/agent-workflow.md'
PLAN = 'plan-jsrf-bare-minimum.md'
STARTUP_TEMPLATE = 'docs/session-start-template.md'

AGENTS_BUDGET = 65536
# The guide says "keep a conservative budget and retain headroom".  A file within
# 4 KiB of the hard limit has no room for the next session's operating note, and
# the 2026-09-22 truncation happened exactly at the limit.
AGENTS_HEADROOM = 4096

# A fact must have exactly one owner.  These are the active-policy files: a role
# route name appearing in more than one of them means the roster is duplicated.
ROSTER_OWNER = WORKFLOW
ROSTER_PATTERN = re.compile(
    r'\b(?:workbuddy-ai/[\w.\-]+|codex[/:][\w.\-]+|gpt-6-[\w.\-]+|hy4[\w.\-]*)\b')

# Retired names may appear, but only where the surrounding text marks them as
# history.  These are the labels that make a mention historical.
HISTORICAL_MARKERS = (
    'historical', 'retired', 'superseded', 'archive', 'no longer', 'used to',
    'formerly', 'history',
)
# A retired name may also appear in a *factual* statement about what a provider
# serves.  "workbuddy-ai advertises only …, gpt-5.5 and kimi-k3" is a measurement
# of the catalog, not an instruction to use it, and flagging it would be a false
# positive that trains a reader to ignore the check.
CATALOG_MARKERS = (
    'advertis', 'does not serve', 'catalog', 'list_subagent_models', 'serves up to',
    'does not advertise',
)
RETIRED_NAMES = (
    'gpt-5.6-sol', 'gpt-5.6-luna', 'gpt-5.6-terra', 'gpt-5.5', 'grok-cli',
    'hy3', 'glm-5.3',
)

# Commands the documents tell a session to run.  A path that does not exist is a
# broken instruction, and the guide has already carried one (scripts/build-jsrf.ps1).
COMMAND_PATTERNS = (
    re.compile(r'scripts[/\\]([\w.\-]+\.py)'),
    re.compile(r'tests[/\\]([\w.\-]+\.py)'),
)
# Paths the documents name as *absent* on purpose; they are findings about the
# repository, not instructions.
KNOWN_ABSENT_OK = {'build-jsrf.ps1'}

AUTHORITY_LINKS = (
    (AGENTS, WORKFLOW),
    (AGENTS, PLAN),
    (STARTUP_TEMPLATE, WORKFLOW),
)


def read(path: Path) -> str:
    return path.read_text(encoding='utf-8', errors='replace')


def utf8_size(path: Path) -> int:
    return len(path.read_bytes())


def check_agents_budget() -> list[dict]:
    findings: list[dict] = []
    path = ROOT / AGENTS
    if not path.is_file():
        return [{'check': 'agents_budget', 'reason': 'missing_input',
                 'detail': f'{AGENTS} is missing'}]
    size = utf8_size(path)
    if size >= AGENTS_BUDGET:
        findings.append({
            'check': 'agents_budget', 'reason': 'over_budget',
            'detail': f'{AGENTS} is {size} bytes, at or over the {AGENTS_BUDGET} budget; '
                      f'its tail is silently never read',
            'measured_bytes': size, 'limit': AGENTS_BUDGET,
        })
    elif size > AGENTS_BUDGET - AGENTS_HEADROOM:
        findings.append({
            'check': 'agents_budget', 'reason': 'insufficient_headroom',
            'detail': f'{AGENTS} is {size} bytes, within {AGENTS_HEADROOM} of the '
                      f'{AGENTS_BUDGET} budget; a section appended past the limit is '
                      f'silently never read',
            'measured_bytes': size, 'limit': AGENTS_BUDGET,
        })
    return findings


def check_roster_single_authority() -> list[dict]:
    """The roster must live in the workflow, not be restated in AGENTS.md."""
    findings: list[dict] = []
    workflow = ROOT / WORKFLOW
    agents = ROOT / AGENTS
    if not workflow.is_file() or not agents.is_file():
        return [{'check': 'roster_single_authority', 'reason': 'missing_input',
                 'detail': 'workflow or AGENTS.md is missing'}]

    if not ROSTER_PATTERN.search(read(workflow)):
        findings.append({
            'check': 'roster_single_authority', 'reason': 'owner_has_no_roster',
            'detail': f'{WORKFLOW} owns the roster but names no route',
        })

    agents_text = read(agents)
    # A roster *table* puts one route per row, so a per-line count never sees it.
    # Collect route names across the whole file instead, skipping lines whose
    # context marks them historical or catalog-factual.
    lines = agents_text.split('\n')
    offenders: list[tuple[int, str, list[str]]] = []
    for line_number, line in enumerate(lines, start=1):
        names = ROSTER_PATTERN.findall(line)
        if not names:
            continue
        lowered = line.casefold()
        window = ' '.join(lines[max(0, line_number - 4):line_number]).casefold()
        context = window + ' ' + lowered
        if any(marker in context for marker in HISTORICAL_MARKERS):
            continue
        if any(marker in context for marker in CATALOG_MARKERS):
            continue
        offenders.append((line_number, line, names))

    distinct = {name for _n, _l, names in offenders for name in names}
    if len(distinct) >= 2:
        first = offenders[0]
        findings.append({
            'check': 'roster_single_authority', 'reason': 'duplicated_roster',
            'detail': f'{AGENTS}:{first[0]} and {len(offenders) - 1} other line(s) name '
                      f'{len(distinct)} routes; the roster belongs only in {WORKFLOW}',
            'line': first[1].strip()[:200],
        })
    elif len(distinct) == 1:
        first = offenders[0]
        findings.append({
            'check': 'roster_single_authority', 'reason': 'stray_route_name',
            'detail': f'{AGENTS}:{first[0]} names route {sorted(distinct)[0]!r} outside '
                      f'{WORKFLOW} without marking it historical',
            'line': first[1].strip()[:200],
        })
    return findings


def check_retired_names_labelled() -> list[dict]:
    """A retired model name may appear only where it is marked as history."""
    findings: list[dict] = []
    for name in (AGENTS, WORKFLOW, PLAN):
        path = ROOT / name
        if not path.is_file():
            findings.append({'check': 'retired_names', 'reason': 'missing_input',
                             'detail': f'{name} is missing'})
            continue
        lines = read(path).split('\n')
        for index, line in enumerate(lines, start=1):
            lowered = line.casefold()
            for retired in RETIRED_NAMES:
                if retired not in lowered:
                    continue
                # Look at a small window: a list of retired names is often
                # introduced by a heading on an earlier line.
                window = ' '.join(lines[max(0, index - 4):index]).casefold()
                context = window + ' ' + lowered
                if any(marker in context for marker in HISTORICAL_MARKERS):
                    continue
                if any(marker in context for marker in CATALOG_MARKERS):
                    continue
                findings.append({
                    'check': 'retired_names', 'reason': 'unlabelled_retired_name',
                    'detail': f'{name}:{index} names retired route {retired!r} '
                              f'without a historical marker',
                    'line': line.strip()[:200],
                })
    return findings


def check_command_paths() -> list[dict]:
    """Every script/test path a document names must exist."""
    findings: list[dict] = []
    for name in (AGENTS, WORKFLOW, PLAN, STARTUP_TEMPLATE):
        path = ROOT / name
        if not path.is_file():
            continue
        for line_number, line in enumerate(read(path).split('\n'), start=1):
            for pattern in COMMAND_PATTERNS:
                for match in pattern.finditer(line):
                    filename = match.group(1)
                    if filename in KNOWN_ABSENT_OK:
                        continue
                    relative = match.group(0).replace('\\', '/')
                    if not (ROOT / relative).is_file():
                        findings.append({
                            'check': 'command_paths', 'reason': 'broken_command_path',
                            'detail': f'{name}:{line_number} names {relative}, which '
                                      f'does not exist',
                            'line': line.strip()[:200],
                        })
    return findings


def check_authority_links() -> list[dict]:
    """The files that defer authority must name files that exist and own it."""
    findings: list[dict] = []
    for source, target in AUTHORITY_LINKS:
        source_path = ROOT / source
        if not source_path.is_file():
            findings.append({'check': 'authority_links', 'reason': 'missing_input',
                             'detail': f'{source} is missing'})
            continue
        if not (ROOT / target).is_file():
            findings.append({
                'check': 'authority_links', 'reason': 'stale_authority_link',
                'detail': f'{source} defers to {target}, which does not exist',
            })
            continue
        if target not in read(source_path):
            findings.append({
                'check': 'authority_links', 'reason': 'missing_authority_link',
                'detail': f'{source} no longer names its authority {target}',
            })
    return findings


def check_next_packet_agreement() -> list[dict]:
    """AGENTS.md must not treat a packet as outstanding that the plan accepts.

    This used to also compare the plan with a session report's CURRENT STATE
    block.  Session reports are no longer kept in the repository, so the plan is
    the only status source and the comparison is AGENTS.md against the plan.
    """
    findings: list[dict] = []
    for name in (AGENTS, PLAN):
        path = ROOT / name
        if not path.is_file():
            findings.append({'check': 'next_packet', 'reason': 'missing_input',
                             'detail': f'{name} is missing'})
    if findings:
        return findings

    # Read acceptance from the whole plan, not from one `**Status:**` line: the
    # plan records accepted packets in its own sections, and a condensed plan has
    # no single Status line to anchor on.
    plan_status_text = read(ROOT / PLAN)

    # AGENTS.md must not carry a *status* claim that contradicts the plan.
    #
    # Measured before this check existed: AGENTS.md said "P0.1 awaits advisor
    # adjudication ... do not begin P0.2 yet" while the plan said P0.1 accepted.
    # A stale status in the one file every session loads automatically is the
    # exact failure the file's own header warns about.
    #
    # The comparison is on the **verb for the same packet**, not on whether the
    # packet is mentioned: an earlier version only flagged packets absent from the
    # plan, and both P0.1 and P0.2 appear there, so it still missed the case.
    agents_text = read(ROOT / AGENTS)
    blocking_re = re.compile(
        r'(?i)\b(P0\.\d)\b[^.\n]{0,80}?\b(awaits?|awaiting|pending|'
        r'not\s+accepted|do\s+not\s+begin|in\s+progress|blocked)\b'
        r'|\b(awaits?|awaiting|pending|not\s+accepted|do\s+not\s+begin|blocked)\b'
        r'[^.\n]{0,80}?\b(P0\.\d)\b')
    for line_number, line in enumerate(agents_text.split('\n'), start=1):
        stripped = line.strip()
        if stripped.startswith('>') or stripped.startswith('|'):
            continue          # quoted policy and tables are historical/procedural
        match = blocking_re.search(stripped)
        if not match:
            continue
        if any(marker in stripped.casefold() for marker in HISTORICAL_MARKERS):
            continue
        named = {g for g in match.groups()
                 if g and re.fullmatch(r'P0\.\d', g)}
        for packet in sorted(named):
            # The plan says this packet is accepted while AGENTS.md still blocks it.
            if re.search(rf'{re.escape(packet)}\b[^.\n]{{0,40}}?\baccepted\b',
                         plan_status_text, re.IGNORECASE):
                findings.append({
                    'check': 'next_packet', 'reason': 'agents_status_disagreement',
                    'detail': f'{AGENTS}:{line_number} still treats {packet} as '
                              f'outstanding while {PLAN} records it accepted; one of '
                              f'them is stale',
                    'line': stripped[:200],
                })
    return findings


CHECKS = (
    ('agents_budget', check_agents_budget),
    ('roster_single_authority', check_roster_single_authority),
    ('retired_names', check_retired_names_labelled),
    ('command_paths', check_command_paths),
    ('authority_links', check_authority_links),
    ('next_packet', check_next_packet_agreement),
)


def run_checks() -> list[dict]:
    findings: list[dict] = []
    for _name, check in CHECKS:
        findings.extend(check())
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='run the audit')
    parser.add_argument('--json', action='store_true', help='emit JSON')
    parser.add_argument('--root', help='repository root override (fixtures)')
    args = parser.parse_args()

    global ROOT
    if args.root:
        ROOT = Path(args.root).resolve()

    if not args.check:
        parser.error('pass --check to run the audit')

    try:
        findings = run_checks()
        agents_path = ROOT / AGENTS
        measured = utf8_size(agents_path) if agents_path.is_file() else None
    except Exception as error:  # pragma: no cover - defensive
        print(f'checker could not run: {error}', file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps({'checker_version': CHECKER_VERSION,
                          'agents_bytes': measured,
                          'agents_budget': AGENTS_BUDGET,
                          'findings': findings}, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'{AGENTS}: {measured} bytes (budget {AGENTS_BUDGET}, '
              f'headroom target {AGENTS_BUDGET - AGENTS_HEADROOM})')
        if findings:
            print(f'{len(findings)} finding(s):')
            for finding in findings:
                print(f'  [{finding["check"]}/{finding["reason"]}] {finding["detail"]}')
        else:
            print('no findings')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
