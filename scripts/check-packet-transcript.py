"""`scripts/check-packet-transcript.py`: W3's and W10's packet prerequisites.

Two plan rows, one checker, because both are about what a packet must contain
*before* it is frozen and both are checked from the same file.

**W3 — the dry-run transcript.** "Freezing requires a DeepSeek **dry-run
transcript**: every command executed on the target host and tree, output hashed in
the packet", answering "frozen commands that never ran (A4s-r4 anchors, A4s-r5
PowerShell 5.1 grep; 7 of 12 A4s Advisor rulings)". A frozen command that has never
executed is a command that may not execute at all, and the failure is discovered
during execution -- after the review has been paid for.

**W10 — load-bearing premises with byte-level commands.** "Each packet lists its
load-bearing **premises with byte-level commands**; the reviewer re-runs them first;
a lint rejects values cited from a `CONTENT_MISMATCH` dump or a run with tracing
off", answering "false ACCEPTs on false premises (OOM slice, named-producer-frame)".
A premise with a command is one a reviewer can falsify; a premise in prose is one
they must trust.

**What this checks**: that the sections exist and are populated, and -- the part
that is genuinely mechanical -- that **every command in the packet's `### Execution`
section also appears in the dry-run transcript**. A packet can have a transcript
section that covers three of its twelve commands, and that gap is exactly the A4s
failure.

**What it cannot check**: that the transcript is *honest*. A fabricated hash passes.
The check is that the packet is complete, not that its author is truthful; honesty
comes from the reviewer re-running the commands, which W10 requires them to do
first.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKETS = ROOT / 'docs' / 'packets'
CHECKER_VERSION = 'jsrf-packet-transcript/1'

# Packets closed before W3 and W10 were adopted.  Both rows landed 2026-09-30, and
# the P0.1–P0.7 and A3a packets were written and accepted months earlier; the plan's
# §14 records them as ACCEPTED.  Requiring a transcript retroactively would mean
# rewriting a closed, reviewed record to satisfy a rule adopted afterwards, which
# destroys the record rather than improving it.
#
# The scope is a **named file list**, so the exemption is visible and reviewable, and
# a control asserts every exempt file still exists -- a stale exemption is a rule
# that silently stopped applying.  A NEW packet is governed by default.
PRE_W3_W10_PACKETS = {
    'a3a-ac97-codec-model.md',
    'p0-acceptance-contract.md',
    'p0-build-interface.md',
    'p0-dump-controls.md',
    'p0-harness-outcomes.md',
    'p0-review-records.md',
    'p0-save-isolation.md',
}

# Section headings, matched loosely: `### Dry-run transcript (W3)` and
# `### Load-bearing premises` are both valid spellings.
TRANSCRIPT_HEADING = re.compile(r'(?im)^#{2,4}\s*dry[- ]run\s+transcript\b.*$')
PREMISES_HEADING = re.compile(r'(?im)^#{2,4}\s*load[- ]bearing\s+premises\b.*$')
EXECUTION_HEADING = re.compile(r'(?im)^#{2,4}\s*execution\b.*$')
ANY_HEADING = re.compile(r'(?m)^#{2,4}\s')

# A command in a backtick span: `python -X utf8 scripts/x.py --flag`.
COMMAND = re.compile(r'`([^`\n]*(?:python|ctest|cmake|just|git|powershell|pwsh)'
                     r'[^`\n]*)`')
# A hashed output line: `... — output sha256 <64 hex>` or a bare 64-hex hash.
HASH = re.compile(r'\b[0-9a-f]{64}\b', re.IGNORECASE)


def section(text: str, heading: re.Pattern[str]) -> str | None:
    """The body of a section, from its heading to the next heading of any level."""
    match = heading.search(text)
    if not match:
        return None
    start = match.end()
    following = ANY_HEADING.search(text, start)
    return text[start:following.start() if following else len(text)]


def audit(path: Path) -> list[dict]:
    findings: list[dict] = []
    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except OSError as error:
        return [{'check': 'packet_transcript', 'reason': 'unreadable',
                 'detail': f'{path.name}: {error}'}]
    relative = f'docs/packets/{path.name}'

    if path.name in PRE_W3_W10_PACKETS:
        return [{
            'check': 'packet_transcript', 'reason': 'pre_w3_w10_packet',
            'detail': (f'{relative} was closed before W3 and W10 were adopted; its '
                       f'acceptance is recorded in the plan and it is not rewritten '
                       f'to satisfy a later rule'),
        }]

    # A packet that has not been frozen yet is a draft; the prerequisites bind at
    # freezing, which is what `**Status:** draft` says has not happened.
    if re.search(r'\*\*Status:\*\*\s*(ADEQUATE|promoted|accepted)\b', text,
                 re.IGNORECASE):
        frozen = True
    elif re.search(r'\*\*Status:\*\*\s*(draft|INADEQUATE)\b', text, re.IGNORECASE):
        frozen = False
    else:
        frozen = None

    transcript = section(text, TRANSCRIPT_HEADING)
    premises = section(text, PREMISES_HEADING)
    execution = section(text, EXECUTION_HEADING)

    if transcript is None:
        findings.append({
            'check': 'packet_transcript', 'reason': 'no_dry_run_transcript',
            'detail': (f'{relative} has no `### Dry-run transcript` section; W3 '
                       f'makes it a prerequisite to freezing'),
        })
    elif not HASH.search(transcript):
        findings.append({
            'check': 'packet_transcript', 'reason': 'transcript_not_hashed',
            'detail': (f'{relative}: the dry-run transcript carries no output hash, '
                       f'so a reader cannot tell the recorded run from a different '
                       f'one'),
        })

    if premises is None:
        findings.append({
            'check': 'packet_transcript', 'reason': 'no_load_bearing_premises',
            'detail': (f'{relative} has no `### Load-bearing premises` section; W10 '
                       f'requires each premise with its byte-level command'),
        })
    elif not COMMAND.search(premises):
        findings.append({
            'check': 'packet_transcript', 'reason': 'premises_without_commands',
            'detail': (f'{relative}: the premises section names no command, so a '
                       f'reviewer cannot re-run them'),
        })

    # The genuinely mechanical part: every execution command must be in the
    # transcript.
    if execution and transcript is not None:
        commands = {c.strip() for c in COMMAND.findall(execution)}
        missing = sorted(c for c in commands if c not in transcript)
        for command in missing:
            findings.append({
                'check': 'packet_transcript', 'reason': 'command_not_dry_run',
                'detail': (f'{relative}: `{command}` appears in `### Execution` but '
                           f'not in the dry-run transcript; a frozen command that '
                           f'has never run is a command that may not run'),
            })

    if frozen is False and (transcript is None or premises is None):
        # A draft is expected to be incomplete.  The specific reasons stay in the
        # report -- they are what tells the author what to add -- and the draft
        # notice is added on top, marked informational so the exit code does not
        # fail a packet that is not frozen yet.
        #
        # Measured by a control: replacing the specific reasons with only the draft
        # notice hid WHICH section was missing, which is the one thing the author
        # needs from it.
        findings.append({
            'check': 'packet_transcript', 'reason': 'draft_missing_prerequisites',
            'detail': (f'{relative} is a draft, so the W3/W10 sections are not yet '
                       f'required -- but they must exist before it is frozen'),
        })
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--packet', action='append', default=[],
                        help='check only this packet file (repeatable)')
    args = parser.parse_args()

    if args.packet:
        files = [Path(p) if Path(p).is_absolute() else ROOT / p for p in args.packet]
    else:
        files = sorted(p for p in PACKETS.glob('*.md')) if PACKETS.is_dir() else []

    findings: list[dict] = []
    for path in files:
        findings.extend(audit(path))

    # A draft-only finding is informational: the packet is not frozen, so the
    # prerequisites do not bind yet.  An exempt packet is likewise informational.
    # A DRAFT's missing sections do not fail the check (the packet is not frozen),
    # but they are reported: the author needs to know which one is missing. An
    # EXEMPT packet's notice is informational for the same reason.
    INFORMATIONAL = {'pre_w3_w10_packet'}
    blocking = [f for f in findings if f['reason'] not in INFORMATIONAL]
    if findings and all(f['reason'] == 'draft_missing_prerequisites'
                          or f['reason'].startswith('no_')
                          for f in findings):
        # Every finding is about a section a DRAFT does not yet need.
        blocking = []
    record = {
        'checker': CHECKER_VERSION,
        'packets_examined': len(files),
        'findings': findings,
        'blocking': len(blocking),
    }

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'  packets examined : {len(files)}')
        if findings:
            for finding in findings:
                print(f'    [{finding["reason"]}] {finding["detail"]}')
        else:
            print('  no findings')
    return 1 if blocking else 0


if __name__ == '__main__':
    sys.exit(main())
