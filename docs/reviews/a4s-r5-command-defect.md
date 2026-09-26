# A4s-r5 readiness probe: one operative command cannot be executed as written

**Date:** 2026-09-25  **Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`
**Packet under execution:** `docs/packets/a4s-toolkit-sync.md`, `A4s-r5`, SHA-256
`09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB`.
**Status of this record:** measurement + escalation. It does **not** revise the packet and does not
authorize a deviation. No execution step has begun.

## The defect

`A4s-r5` `AC-INV` → **"New environment names"** (packet line 115) specifies this command:

```powershell
git -C $T grep -h -o -E 'getenv\("[A-Za-z0-9_]+"' <rev> -- src include
```

Run **exactly as written** on this host, it produces **no output and exit code 1** — for every
revision, including revisions that certainly contain `getenv("…")` calls.

The packet states an explicit expectation for this command (Advisor-observed at `75083476`): the set
difference should yield **`RECOMP_APU_MIXDOWN_ALL`** and **`RECOMP_USB_PORT`**, with none removed. A
faithful literal execution therefore reports **"no new environment names"**, contradicting the
packet's own stated expectation.

## Measured evidence (all read-only, this session)

Host: **Windows PowerShell 5.1.26100.9444** (`PSEdition: Desktop`). **No `pwsh` 7 is installed**
(`where.exe pwsh` finds nothing; `C:\Program Files\PowerShell\7\pwsh.exe` and the WindowsApps shim are
both absent). PowerShell 5.1 does not escape embedded double quotes when building a native command
line, so git receives a pattern with the `"` characters removed; the resulting ERE requires `getenv(`
to be followed immediately by an alphanumeric, which never matches the real text `getenv("RECOMP_…`.

| # | Command (as run) | Result | Verdict |
|---|---|---|---|
| 1 | `git -C $T grep -h -o -E 'getenv\("[A-Za-z0-9_]+"' 0d7929c -- src include` | no output, exit 1 | **the packet's command — broken** |
| 2 | same, from a `.ps1` file via `powershell -ExecutionPolicy Bypass -File`, literal `-C` path | no output, exit 1 | **not a shell-interaction artifact** |
| 3 | `git -C $T grep -h -o -E 'getenv' 0d7929c -- src include` | **54 lines** | control: `-o -E` works; the failure is the embedded `"` |
| 4 | `git -C $T grep -h -o -E 'getenv\(' 0d7929c -- src include` | matches | control: `\(` works |
| 5 | `git -C $T grep -h -o -E -f <pattern-file> 0d7929c -- src include`, pattern file holding `getenv\("[A-Za-z0-9_]+"` | **matches, 42 unique names** | equivalent form works (pattern read from a file is not re-quoted) |

Attempts that also fail (recorded so they are not retried): `--%` stop-parsing, `cmd /c`, and passing
the pattern through a variable. Only the `-f <pattern-file>` form is unaffected.

## Measured result of the equivalent form (what the packet intends)

Using form 5 on the two revisions the criterion names:

| Revision | Unique quoted `getenv` names in `SCOPE` (`src include`) |
|---|---|
| `0d7929c` | **42** |
| `75083476` (conflict-free merge tree) | **44** |

Set difference, both ways:

- present at `75083476`, **not** at `0d7929c`: **`RECOMP_APU_MIXDOWN_ALL`**, **`RECOMP_USB_PORT`**
- present at `0d7929c`, **not** at `75083476`: **none**

**This reproduces the packet's stated expectation exactly** (both names, none removed). So the
*requirement* is satisfiable and unambiguous; only the *spelled procedure* is unexecutable on this
host.

## Why this is raised rather than worked around

- §2.2.2: "Run commands exactly as written. A step that cannot be performed as written is a blocker to
  escalate, not something to improvise around."
- §4.2: a contract role escalates when "a criterion or step is ambiguous, contradicted, or cannot be
  executed as written".
- §3.1 blocking test — **concrete failure scenario:** an executor runs the command as written, gets
  empty output, records "no new environment names", and the `AC-INV` inventory silently omits
  `RECOMP_APU_MIXDOWN_ALL`. The plan records that `MIXDOWN_ALL`'s default-on path is **unverified for
  guest visibility** and that `A4b1` — which rewrites `apu_dsp.c` — must classify it before any strict
  APU claim. Dropping it from the inventory loses a load-bearing follow-up premise.
- Counter-consideration, recorded honestly: the packet prints the expected two names in the same
  sentence, so an attentive executor would notice the contradiction. That makes the defect
  **fail-visible**, not fail-silent — which is why the Session is escalating for an interpretation
  rather than treating execution as impossible.

## Scope of the finding

The Session checked **every other command in `A4s-r5`** for the same hazard. Exactly **one** command
carries an embedded double quote inside a native-tool argument; all others use `-f <file>` pattern
files, quote-free `-E` patterns, or PowerShell cmdlets (`Select-String`), which are unaffected. Verified
working this session: the deleted-name `-F -f` greps, the classifier-name `-F -f` greps, the two
`AC-KEEP` (v) code-token `-E` greps (`|= MCPX_AC97_CODEC_READY` → the `:2131` HA-hunk line;
`\bac97_arm_write_trap\s*\(` → definition `:411` and HA-hunk call `:2134`), the conflict-marker
`Select-String`, the `F` control (`= 2`) and the `[A3A]` witness `Select-String`
(`gc=0x00000002 gs=0x00000100`).

## Disposition

**RULED — Advisor interpretation ruling (b): execution proceeds with a controlled substitute.** The
ruling is recorded in full in **`docs/reviews/a4s-r5-execution-startup-ruling.md`**, section
"Q3 — `AC-INV` "New environment names" command", with the Advisor child ID
(`5c555969-dea9-4b47-be05-62aa0835cde2`) and route (`claude/claude-opus-5-5` @ `high`).

Operative points of that ruling:

1. The defect is **transport only** (PowerShell 5.1 strips the embedded `"` when building git's
   command line). The criterion — its claim, regex, revisions, `SCOPE`, dispositions, expected set and
   decision logic — is **correct and unchanged**, and **the packet file is left untouched**.
2. The **only** authorized substitute is a pattern file at `logs\a4s\getenv-pattern.txt` containing
   exactly the pattern bytes, ASCII, **no BOM, no trailing newline**, used as
   `git -C $T grep -h -o -E -f <pattern-file> <rev> -- src include`. On PS 5.1 `-Encoding utf8` writes
   a BOM and `Out-File` writes UTF-16LE; either would silently corrupt the pattern.
3. **Required controls** (any failure makes the step `UNKNOWN` → `FAIL` per `AC-INV`'s own rule):
   (i) `0d7929c` yields exactly **42** unique names; (ii) `75083476` yields **44**, with
   `{RECOMP_APU_MIXDOWN_ALL, RECOMP_USB_PORT}` added and **none removed**; (iii) the **literal packet
   command's** empty output/exit 1 on `0d7929c` is recorded as the known-bad witness.
4. **Scope: this one command only**, at both revisions and at the D1 re-run on an amended `M`. It
   authorizes no other substitution. Any other command that fails as written must be escalated
   separately.
5. **Acceptance:** reviewers bind to the frozen contract **plus recorded Advisor rulings** (§3.3). A
   reviewer checks this step against the substitute form and its controls, not the literal quoting.

This is **not** an `A4s-r6` revision: under §5.4 a post-`ADEQUATE` revision is *permitted, not
required*, and revising for a quoting artifact would force an adequacy round that protects nothing.

**Session note on one number in the ruling.** The ruling states the pattern file is "exactly 24 ASCII
bytes". The Session **measures** and records the actual byte length and SHA-256 in the execution
evidence; the operative requirement is the exact byte content, not the predicted count.
