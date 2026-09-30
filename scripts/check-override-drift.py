"""`scripts/check-override-drift.py`: W15's override-drift check.

Plan W15's failure, stated in its own words:

> `AGENTS.md:92` and `docs/jsrf-run-profiles.md:51,54` still describe
> `RECOMP_AC97_READY`/`RECOMP_APU_DSP_ACK` as live (removed in toolkit
> `c97ce2c`/`8f8f6e4`); stale `RETIRED_NAMES`

and its check: "`check-agent-docs.py` fails on override names the runtime no longer
reads and on retired routes".

**Why this is a separate tool rather than a check inside `check-agent-docs.py`.**
It needs two things that document checker does not: the toolkit **source** (to know
which override names the runtime actually reads) and `scripts/jsrf_run_profile.py`
(to know which names this project has classified). Keeping it separate also means it
can be run against a toolkit checkout on its own, which is how a merge is checked.

**What "the runtime no longer reads" means mechanically.** An override is read by
`getenv("NAME")` or `GetEnvironmentVariable` in the toolkit's build inputs. A name
that appears only in a comment, a doc, a test, or an unbuilt template is not read.
So the search is over the **source files**, matching the quoted string literal that
carries the semantics -- `run-profiles.md` calls this "each name in the form that
carries its semantics", and searching for the bare token would match the comment
that documents its removal.

**The three verdicts**, and why the middle one matters:

  * `LIVE` -- the runtime reads it. A document may describe it as active.
  * `RETIRED` -- this project classified it as retired (`RETIRED_OVERRIDES`) and the
    runtime no longer reads it. A document must mark it as history.
  * `DOCUMENTED_BUT_UNREAD` -- the document names it and the runtime does not read
    it, but it is not in `RETIRED_OVERRIDES`. **This is not automatically a defect**:
    most documented overrides are observation or feature enablement, and a name the
    runtime does not read cannot make a run exploratory (`jsrf_run_profile.py` says
    exactly this). It is reported as information, because the interesting case is a
    name a document calls *synthetic completion* while the runtime ignores it.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLKIT = ROOT.parent / 'xboxrecomp'
sys.path.insert(0, str(ROOT / 'scripts'))

from jsrf_run_profile import RETIRED_OVERRIDES  # noqa: E402

CHECKER_VERSION = 'jsrf-override-drift/1'

# Documents that describe override behaviour to a reader.
DOCUMENTS = ('AGENTS.md', 'docs/jsrf-run-profiles.md', 'docs/agent-workflow.md',
             'plan-jsrf-bare-minimum.md')

# `RECOMP_*` / `JSRF_*` names as they appear in a document.
NAME = re.compile(r'\b((?:RECOMP|JSRF)_[A-Z0-9_]+)\b')

# The form that carries semantics in source: a QUOTED string literal. A bare token
# in a comment documents a name; it does not read one.
def quoted_reader(name: str) -> re.Pattern[str]:
    return re.compile(r'"' + re.escape(name) + r'"')

# Toolkit build inputs. A file that is not compiled cannot make a name live, and
# `run-profiles.md` requires the scope to be "the build inputs of the evidence
# binary, named as a path list".
SOURCE_ROOTS = ('src', 'include')
SOURCE_SUFFIXES = ('.c', '.h', '.cpp', '.hpp')

# Where a document marks a mention as history.  Reused from the document checker's
# vocabulary so the two cannot disagree about what "historical" looks like.
HISTORICAL_MARKERS = (
    'historical', 'retired', 'superseded', 'archive', 'no longer', 'used to',
    'formerly', 'history', 'removed', 'deleted', 'inert',
)


def source_files() -> list[Path]:
    files: list[Path] = []
    for root_name in SOURCE_ROOTS:
        root = TOOLKIT / root_name
        if not root.is_dir():
            continue
        for path in root.rglob('*'):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                files.append(path)
    return files


def read_names(files: list[Path]) -> dict[str, list[str]]:
    """Override names the runtime actually reads, and where.

    A name counts as read when a **quoted string literal** spells it, which is what
    `getenv`/`GetEnvironmentVariable` take. Matching the bare token would count the
    comment that documents a removal as evidence the name is live -- the exact
    confusion that produced W15.
    """
    found: dict[str, list[str]] = {}
    for path in files:
        try:
            text = path.read_text(encoding='utf-8', errors='replace')
        except OSError:
            continue
        for match in re.finditer(r'"((?:RECOMP|JSRF)_[A-Z0-9_]+)"', text):
            found.setdefault(match.group(1), []).append(
                str(path.relative_to(TOOLKIT)).replace('\\', '/'))
    return found


def document_mentions() -> dict[str, list[dict]]:
    """Every override name each document names, with whether it is marked history."""
    mentions: dict[str, list[dict]] = {}
    for relative in DOCUMENTS:
        path = ROOT / relative
        if not path.is_file():
            continue
        lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
        for number, line in enumerate(lines, 1):
            for name in NAME.findall(line):
                window = ' '.join(lines[max(0, number - 4):number]).casefold()
                context = window + ' ' + line.casefold()
                mentions.setdefault(name, []).append({
                    'document': relative,
                    'line': number,
                    'marked_historical': any(marker in context
                                             for marker in HISTORICAL_MARKERS),
                    'text': line.strip()[:160],
                })
    return mentions


def claims_synthetic_completion(text: str) -> bool:
    """Does this line assert the name is synthetic completion?

    That is the specific claim W15's stale text made, and the one that matters: a
    document may mention an unread name freely, but calling it synthetic completion
    when the runtime ignores it tells a reader a strict run is impossible for a
    reason that no longer exists.
    """
    lowered = text.casefold()
    return ('synthetic' in lowered or 'bypass' in lowered
            or 'answers a poll' in lowered)


def main() -> int:
    global TOOLKIT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--toolkit', type=Path, default=TOOLKIT)
    args = parser.parse_args()

    if args.toolkit != TOOLKIT:
        TOOLKIT = args.toolkit

    if not TOOLKIT.is_dir():
        print(f'no toolkit at {TOOLKIT}; the check needs its source', file=sys.stderr)
        return 2

    files = source_files()
    if not files:
        print(f'no C sources under {TOOLKIT}/src or /include', file=sys.stderr)
        return 2

    read = read_names(files)
    mentions = document_mentions()
    findings: list[dict] = []

    for name, places in sorted(mentions.items()):
        is_read = name in read
        is_retired = name in RETIRED_OVERRIDES
        if is_read:
            continue  # the runtime reads it; the document may describe it as live
        for place in places:
            if is_retired and not place['marked_historical']:
                findings.append({
                    'check': 'override_drift', 'reason': 'retired_described_as_live',
                    'detail': (f"{place['document']}:{place['line']} names {name}, "
                               f"which this project retired and the toolkit no "
                               f"longer reads, without a historical marker"),
                    'line': place['text'],
                })
            elif not is_retired and claims_synthetic_completion(place['text']):
                findings.append({
                    'check': 'override_drift',
                    'reason': 'unread_name_called_synthetic_completion',
                    'detail': (f"{place['document']}:{place['line']} calls {name} "
                               f"synthetic completion, but the toolkit reads no "
                               f"such name -- a strict run is not made exploratory "
                               f"by a name the runtime ignores"),
                    'line': place['text'],
                })

    record = {
        'checker': CHECKER_VERSION,
        'toolkit': str(TOOLKIT),
        'source_files_scanned': len(files),
        'names_read_by_the_runtime': len(read),
        'retired_overrides': sorted(RETIRED_OVERRIDES),
        'documented_names': len(mentions),
        'unread_documented_names': sorted(n for n in mentions if n not in read),
        'findings': findings,
    }

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'  toolkit sources scanned : {len(files)}')
        print(f'  names the runtime reads : {len(read)}')
        print(f'  retired overrides       : {sorted(RETIRED_OVERRIDES)}')
        print(f'  names in the documents  : {len(mentions)}')
        unread = record['unread_documented_names']
        if unread:
            print(f'  documented but unread   : {unread} '
                  f'(information, not a finding)')
        if findings:
            print(f'  {len(findings)} finding(s):')
            for finding in findings:
                print(f'    [{finding["check"]}/{finding["reason"]}] '
                      f'{finding["detail"]}')
        else:
            print('  no findings')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
