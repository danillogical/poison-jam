"""Audit the active guide and gameplay plan for stale or broken instructions.

The checker keeps the two active authorities present, within the AGENTS byte
budget, and mechanically valid: commands point at real files and documented
recipes exist.  Routing configuration lives in `.omp/config.yml`; project work
lives in `plan-jsrf-gameplay.md`.

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
CHECKER_VERSION = 'jsrf-agent-docs/4'

AGENTS = 'AGENTS.md'
CONFIG = '.omp/config.yml'
PLAN = 'plan-jsrf-gameplay.md'
JUSTFILE = 'justfile'

# Plan T6 fixes these recipe names: they are the mechanical spelling of the host
# workflow, and a record cites `just <recipe>` instead of a drifting command line.
# Renaming or deleting one silently breaks every record that cites it, so the
# check is by name.  `check` and `doctor` are additionally the gate recipes.
REQUIRED_RECIPES = (
    'build', 'test', 'ctest', 'regen', 'shaders', 'strict-run', 'explore-run', 'probe',
    'check', 'ttd-record', 'ttd-writes', 'doctor', 'analyze', 'disk',
    'secret-audit', 'symbols', 'logq', 'enumerate', 'cite-check',
    'horizon-check', 'override-check', 'dump-controls',
    'stack-depth',
    'qualify',
    'record-check',
    'transcribed-check',
    'instrument-check', 'instrument-record',
)

# The recipes a document must point at, and the document that must point at them.
RECIPE_POINTER = (AGENTS, 'just build')

AGENTS_BUDGET = 65536
# The guide says "keep a conservative budget and retain headroom".  A file within
# 4 KiB of the hard limit has no room for the next session's operating note, and
# the 2026-09-22 truncation happened exactly at the limit.
AGENTS_HEADROOM = 4096

# Commands the documents tell a session to run.  A path that does not exist is a
# broken instruction, and the guide has already carried one (scripts/build-jsrf.ps1).
COMMAND_PATTERNS = (
    re.compile(r'scripts[/\\]([\w.\-]+\.py)'),
    re.compile(r'tests[/\\]([\w.\-]+\.py)'),
)
# Paths the documents name as *absent* on purpose; they are findings about the
# repository, not instructions.
KNOWN_ABSENT_OK = {'build-jsrf.ps1'}

# A document may also *plan* a tool it has not written yet.  "`scripts/logq.py`
# (planned)" is a deliverable, not an instruction to run something, and flagging
# it would make adopting a plan that names its own future tools impossible
# without writing them first.  The marker must be on the **same line**, so an
# unmarked mention of the same path elsewhere still fails.
PLANNED_MARKERS = (
    'to be written', 'not yet written', 'to be built', 'not yet built',
    'to be implemented', 'not yet implemented', 'to be added', 'planned',
    'deliverable',
    # A tool marked retired is history in a status table, not an instruction.
    'retired',
)

AUTHORITY_LINKS = ((AGENTS, CONFIG), (AGENTS, PLAN))


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


def check_command_paths() -> list[dict]:
    """Every script/test path a document names must exist, unless it is planned."""
    findings: list[dict] = []
    for name in (AGENTS, PLAN):
        path = ROOT / name
        if not path.is_file():
            continue
        lines = read(path).split('\n')
        for line_number, line in enumerate(lines, start=1):
            # A tool the document is *planning* is a deliverable, not an
            # instruction; see PLANNED_MARKERS.
            if any(marker in line.casefold() for marker in PLANNED_MARKERS):
                continue
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


def recipe_names(text: str) -> set[str]:
    """Recipe names defined in a justfile, without running `just`.

    A recipe line starts at column 0 with `name:` -- optionally followed by
    parameters, some of which carry `=` defaults (`strict-run label="x":`).
    Assignments and settings (`name := value`, `set shell := [...]`) are not
    recipes, and neither are aliases, so each is excluded explicitly rather than
    by hoping the pattern misses it.
    """
    names: set[str] = set()
    for line in text.split('\n'):
        if not line or line[0] in ' \t#':
            continue
        if ':=' in line:
            continue
        first = line.split(None, 1)[0]
        if first in ('set', 'alias', 'export', 'import', 'mod', 'unexport'):
            continue
        match = re.match(r'^([A-Za-z_][\w-]*)(?:\s+[^:]*)?:', line)
        if match:
            names.add(match.group(1))
    return names


def check_just_recipes() -> list[dict]:
    """Plan T6: every named recipe must exist, and AGENTS.md must point at them."""
    findings: list[dict] = []
    path = ROOT / JUSTFILE
    if not path.is_file():
        return [{'check': 'just_recipes', 'reason': 'missing_input',
                 'detail': f'{JUSTFILE} is missing; plan T6 names it as the host workflow'}]

    defined = recipe_names(read(path))
    for recipe in REQUIRED_RECIPES:
        if recipe not in defined:
            findings.append({
                'check': 'just_recipes', 'reason': 'missing_recipe',
                'detail': f'{JUSTFILE} does not define the recipe {recipe!r}, which '
                          f'plan T6 names as part of the host workflow',
            })

    source, pointer = RECIPE_POINTER
    source_path = ROOT / source
    if source_path.is_file() and pointer not in read(source_path):
        findings.append({
            'check': 'just_recipes', 'reason': 'docs_do_not_point_at_recipes',
            'detail': f'{source} does not mention {pointer!r}; T6 requires the build '
                      f'and run instructions to point at the just recipes',
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


CHECKS = (
    ('agents_budget', check_agents_budget),
    ('command_paths', check_command_paths),
    ('just_recipes', check_just_recipes),
    ('authority_links', check_authority_links),
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
