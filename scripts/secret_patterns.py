"""The one secret-pattern table, shared by the audit and the pre-commit gate.

**Why this is a module.** `scripts/secret-audit.py` (the pre-push, full-history
scan) and `scripts/precommit-staged-paths.py` (the pre-commit gate) must agree on
what a secret looks like.  They previously carried two hand-copied lists, which is
the exact duplication this repository's document checks exist to prevent: the two
would drift, and the weaker one would decide whether a secret reached the public
repository.  One table, imported by both.

**Why the patterns are assembled from fragments.** The first version of the
pre-commit gate refused the commit that introduced it, because the gate's own
definition file spells out the PEM private-key header markers -- as *patterns*.
A scanner that flags its own definition file is a false positive that trains its
reader to bypass it.

The alternatives were both worse.  A path-based exclusion would blind the check to
a real secret added to that same file.  Deleting the patterns would delete the
check.  Assembling each pattern from adjacent fragments means the source never
contains the contiguous sequence the pattern matches, so the file is scanned like
any other and a genuine key pasted into it is still caught.

This is not obfuscation for its own sake: the fragments are ordinary string
literals, readable in the source, and `PATTERNS` is exactly the list it was.
`tests/test_precommit_hooks.py` asserts that this file and its own test file stay
clean against every pattern, so the property cannot silently regress.
"""
from __future__ import annotations

import re


def _fragments(*parts: str) -> str:
    """Join pattern fragments. Keeps the literal sequence out of this file."""
    return ''.join(parts)


# Assembled rather than written whole, so this file does not match itself.
PRIVATE_KEY_MARKER = _fragments('-----BEGIN ', '[A-Z ]*', 'PRIVATE KEY-----')
OPENSSH_KEY_MARKER = _fragments('BEGIN ', 'OPENSSH ', 'PRIVATE KEY')

PATTERNS: list[tuple[str, str]] = [
    ("github_pat", r"github_pat_[A-Za-z0-9_]{20,}"),
    ("github_token", r"\bgh[pousr]_[A-Za-z0-9]{20,}"),
    ("openai_key", r"\bsk-[A-Za-z0-9]{32,}"),
    ("anthropic_key", r"\bsk-ant-[A-Za-z0-9\-_]{20,}"),
    ("aws_akid", r"\b(?:AKIA|ASIA)[0-9A-Z]{16}\b"),
    ("google_api", r"\bAIza[0-9A-Za-z_\-]{35}\b"),
    ("slack", r"\bxox[baprs]-[A-Za-z0-9\-]{10,}"),
    ("private_key", PRIVATE_KEY_MARKER),
    ("openssh_key", OPENSSH_KEY_MARKER),
    ("bearer", r"[Aa]uthorization:\s*Bearer\s+[A-Za-z0-9._\-]{20,}"),
    ("password_assign", r"(?i)\bpass(?:word|wd)\s*[:=]\s*[\"'][^\"'\s]{6,}[\"']"),
    ("secret_assign", r"(?i)\bsecret\s*[:=]\s*[\"'][^\"'\s]{8,}[\"']"),
    ("apikey_assign", r"(?i)\bapi[_-]?key\s*[:=]\s*[\"'][^\"'\s]{12,}[\"']"),
    ("conn_string", r"(?i)(?:mongodb|postgres|mysql|redis)://[^\s\"']{10,}"),
]

COMPILED: list[tuple[str, re.Pattern[str]]] = [
    (name, re.compile(pattern)) for name, pattern in PATTERNS
]


def redact(text: str) -> str:
    """Shorten anything long enough to be a credential, for a report line.

    The audit's job is to LOCATE a secret, not to copy it into a second artifact
    that might itself be committed or shared.
    """
    return re.sub(r'[A-Za-z0-9_\-]{16,}',
                  lambda match: match.group(0)[:6] + '...REDACTED', text)
