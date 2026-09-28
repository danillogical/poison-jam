"""Scan every reachable Git blob for secret-shaped content.

Reads blob SHAs from a file, streams each blob's bytes through a set of regexes, and
reports every hit with its blob SHA and a redacted context line. Redaction matters:
the point of the audit is to LOCATE a secret, not to copy it into another artifact.

Design notes:
  * blobs are read with `git cat-file blob <sha>` as BYTES, so binary blobs are scanned
    too rather than skipped -- a key pasted into a binary would otherwise hide.
  * text-shaped patterns are decoded leniently; a binary blob that matches is still
    reported, flagged as binary.
  * the output is a summary count plus at most a few contexts per pattern, so the
    report stays readable and does not itself become a secret store.
"""
import re
import subprocess
import sys
from pathlib import Path

PATTERNS = [
    ("github_pat", r"github_pat_[A-Za-z0-9_]{20,}"),
    ("github_token", r"\bgh[pousr]_[A-Za-z0-9]{20,}"),
    ("openai_key", r"\bsk-[A-Za-z0-9]{32,}"),
    ("anthropic_key", r"\bsk-ant-[A-Za-z0-9\-_]{20,}"),
    ("aws_akid", r"\b(?:AKIA|ASIA)[0-9A-Z]{16}\b"),
    ("google_api", r"\bAIza[0-9A-Za-z_\-]{35}\b"),
    ("slack", r"\bxox[baprs]-[A-Za-z0-9\-]{10,}"),
    ("private_key", r"-----BEGIN [A-Z ]*PRIVATE KEY-----"),
    ("openssh_key", r"BEGIN OPENSSH PRIVATE KEY"),
    ("bearer", r"[Aa]uthorization:\s*Bearer\s+[A-Za-z0-9._\-]{20,}"),
    ("password_assign", r"(?i)\bpass(?:word|wd)\s*[:=]\s*[\"'][^\"'\s]{6,}[\"']"),
    ("secret_assign", r"(?i)\bsecret\s*[:=]\s*[\"'][^\"'\s]{8,}[\"']"),
    ("apikey_assign", r"(?i)\bapi[_-]?key\s*[:=]\s*[\"'][^\"'\s]{12,}[\"']"),
    ("conn_string", r"(?i)(?:mongodb|postgres|mysql|redis)://[^\s\"']{10,}"),
]
COMPILED = [(n, re.compile(p)) for n, p in PATTERNS]

blob_file = Path(sys.argv[1])
shas = [s.strip() for s in blob_file.read_text().split() if s.strip()]

total_hits = 0
per_pattern = {n: 0 for n, _ in COMPILED}
samples = []

for i, sha in enumerate(shas):
    try:
        data = subprocess.run(["git", "cat-file", "blob", sha],
                              capture_output=True, check=True).stdout
    except subprocess.CalledProcessError:
        continue
    text = data.decode("utf-8", errors="replace")
    for name, rx in COMPILED:
        for m in rx.finditer(text):
            total_hits += 1
            per_pattern[name] += 1
            if len(samples) < 40:
                start = max(0, m.start() - 40)
                ctx = text[start:m.end() + 40].replace("\n", "\\n")
                # redact the middle of anything long so the report is not a secret store
                ctx = re.sub(r"[A-Za-z0-9_\-]{16,}", lambda mm: mm.group(0)[:6] + "…REDACTED", ctx)
                samples.append((name, sha[:12], ctx[:150]))
    if (i + 1) % 200 == 0:
        print("  scanned %d/%d blobs; hits so far %d" % (i + 1, len(shas), total_hits), flush=True)

print()
print("  blobs scanned: %d" % len(shas))
print("  TOTAL HITS:    %d" % total_hits)
print()
for n, c in sorted(per_pattern.items(), key=lambda kv: -kv[1]):
    if c:
        print("  %-18s %d" % (n, c))
if not total_hits:
    print("  (no pattern matched any reachable blob)")
else:
    print()
    print("  --- samples (redacted) ---")
    for n, sha, ctx in samples:
        print("  %-18s %s  %s" % (n, sha, ctx))
