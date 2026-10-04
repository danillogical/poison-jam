"""`scripts/check-route-allowlist.py`: W8's allow-list coverage check.

Plan W8: "verify a route at first real use; no Advisor probe turns at startup; the
owner names one fallback route per senior role in §1 (owner decision), and **the
session allow-list includes it**; during a senior-route outage DeepSeek continues
pre-authorised chores and discovery execution".

**The measured failure it answers**: "on 09-25 the owner-authorised fallback was
missing from the allow-list and needed a new session". A fallback that exists in
policy but not in the allow-list is a fallback that cannot be selected at the moment
it is needed -- and the moment it is needed is a senior-route outage, which is the
worst time to discover it.

**What this checks mechanically.** The roster in `docs/agent-workflow.md` §1 is the
only persisted staffing policy, and the allow-list lives in the active DSH profile
patch. Both are text, so the check is: every route the roster names must appear in
the allow-list. It does **not** resolve routes live -- that is `list_subagent_models`
at first real use (W8's own point), and a static check that claimed to resolve would
be substituting a text match for a capability probe.

**Fallbacks are reported, never required.** W8 makes naming them an owner decision.
If the roster names none -- which it does today -- the check reports that and passes:
it cannot fail a repository for a decision the owner has not made. What it will not
do is invent one, and `check-agent-docs.py` independently fails a document that names
a route outside §1.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / 'docs' / 'agent-workflow.md'

CHECKER_VERSION = 'jsrf-route-allowlist/1'

# Where the active DSH profile patch lives.  Outside both repositories: the
# allow-list is session configuration, not repository content.
PROFILE_PATCHES = (
    Path.home() / '.dsh' / 'profiles' / 'web' / 'cordis.patch.yml',
)

# `provider/model` in the roster table, e.g. `workbuddy-ai/deepseek-v4.1-flash`, or
# a provider and model written separately.  A model id may contain single spaces
# (`grok/Grok 4.7`); cutting it at the space would name a route that does not exist.
MODEL_ID = r'[A-Za-z0-9._-]+(?: [A-Za-z0-9._-]+)*'
ROUTE = re.compile(r'`([a-z0-9-]+)/(' + MODEL_ID + r')`')
# `provider: codex` beside a bare model name in backticks.
PROVIDER_HINT = re.compile(r'provider:\s*([a-z0-9-]+)', re.IGNORECASE)
MODEL_IN_BACKTICKS = re.compile(r'`([A-Za-z0-9][A-Za-z0-9._-]*)`')

# The allow-list entries, from the YAML patch; a value may be plain or quoted.
ALLOWED_ENTRY = re.compile(
    r'-\s*provider:\s*["\']?([a-z0-9-]+)["\']?\s*\n\s*model:\s*["\']?(' + MODEL_ID + r')')


def roster_routes() -> tuple[set[tuple[str, str]], list[str]]:
    """(provider, model) pairs named by §1's roster, plus any it could not pair.

    The table writes routes two ways: a full `provider/model` in one span, or a
    bare model with a `provider:` qualifier beside it.  Both are read; an earlier
    roster used the second form for its reviewer row.
    """
    if not WORKFLOW.is_file():
        raise SystemExit(f'{WORKFLOW} is missing')
    text = WORKFLOW.read_text(encoding='utf-8')
    # Only the roster table: from the §1 heading to the next heading.
    heading = re.search(r'^## 1\. ', text, re.MULTILINE)
    if heading is None:
        raise SystemExit(f'{WORKFLOW} has no §1 roster heading')
    start = heading.start()
    ends = [i for i in (text.find('\n## ', start + 1), text.find('\n### ', start + 1))
            if i > 0]
    section = text[start:min(ends) if ends else len(text)]

    routes: set[tuple[str, str]] = set()
    unpaired: list[str] = []
    for line in section.splitlines():
        if not line.startswith('|'):
            continue
        if 'Role' in line or line.startswith('|---'):
            continue
        # The top-level session is chosen at launch, not spawned, so the
        # subagent allow-list does not have to contain it.
        if 'not spawned' in line:
            continue
        found = ROUTE.findall(line)
        for provider, model in found:
            routes.add((provider, model))
        hint = PROVIDER_HINT.search(line)
        if hint and not found:
            # `| **Reviewer** | **GPT-6.1 Sol** @ `high`
            #  (`provider: codex`, `route: LIVE_RESOLVE`) |`
            #
            # The FIRST bold span is the ROLE, the second is the MODEL. Measured:
            # taking the first produced `codex/acceptance-reviewer` and a false
            # "not allowed" finding -- a check that would have demanded a
            # nonexistent route be added to the allow-list.
            spans = re.findall(r'\*\*([^*]+)\*\*', line)
            if len(spans) >= 2:
                name = spans[1].strip().lower()
                name = re.sub(r'\s+', '-', name)   # `gpt-6.1 sol` -> `gpt-6.1-sol`
                routes.add((hint.group(1), name))
            else:
                unpaired.append(line.strip()[:160])
    return routes, unpaired


def allowlist_routes() -> tuple[set[tuple[str, str]], list[str]]:
    """(provider, model) pairs the active profile patch allows."""
    routes: set[tuple[str, str]] = set()
    searched: list[str] = []
    for path in PROFILE_PATCHES:
        searched.append(str(path))
        if not path.is_file():
            continue
        text = path.read_text(encoding='utf-8', errors='replace')
        for provider, model in ALLOWED_ENTRY.findall(text):
            routes.add((provider, model))
    return routes, searched


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    roster, unpaired = roster_routes()
    allowed, searched = allowlist_routes()

    findings: list[dict] = []
    for path in searched:
        if not Path(path).is_file():
            findings.append({
                'check': 'route_allowlist', 'reason': 'profile_patch_missing',
                'detail': (f'{path} does not exist, so no allow-list could be read; '
                           f'a route cannot be pinned without one'),
            })
    if unpaired:
        findings.append({
            'check': 'route_allowlist', 'reason': 'unreadable_roster_row',
            'detail': (f'{len(unpaired)} roster row(s) name a provider but no model '
                       f'this check could read'),
        })

    # The comparison is one-directional on purpose: the allow-list may hold more
    # routes than the roster (it is a capability list), but every roster route must
    # be in it, or the route cannot be selected.
    missing = sorted(roster - allowed)
    for provider, model in missing:
        findings.append({
            'check': 'route_allowlist', 'reason': 'roster_route_not_allowed',
            'detail': (f'the roster names {provider}/{model}, which the active '
                       f'allow-list does not contain; a route that is policy but '
                       f'not allowed cannot be selected when it is needed'),
        })

    record = {
        'checker': CHECKER_VERSION,
        'workflow': str(WORKFLOW),
        'profile_patches_searched': searched,
        'roster_routes': sorted(f'{p}/{m}' for p, m in roster),
        'allowlist_routes': sorted(f'{p}/{m}' for p, m in allowed),
        'missing_from_allowlist': [f'{p}/{m}' for p, m in missing],
        'fallbacks_named': [],
        'fallback_note': ('Plan W8 makes naming fallback routes an OWNER decision '
                          '(§3.4). The roster names none today, so this check '
                          'reports that rather than failing: it cannot fail a '
                          'repository for a decision the owner has not made, and it '
                          'must never invent one.'),
        'findings': findings,
    }

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'  roster routes    : {len(roster)}')
        for route in sorted(f'{p}/{m}' for p, m in roster):
            mark = 'OK  ' if (tuple(route.split('/', 1)) in allowed) else 'MISS'
            print(f'    {mark} {route}')
        print(f'  allow-list routes: {len(allowed)}')
        print(f'  fallbacks named  : none (owner-reserved; see the note)')
        if findings:
            print(f'  {len(findings)} finding(s):')
            for finding in findings:
                print(f'    [{finding["reason"]}] {finding["detail"]}')
        else:
            print('  no findings')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
