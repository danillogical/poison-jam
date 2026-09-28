# Synchronized push checkpoint — 2026-09-28

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, DSH.
**Purpose:** the owner's repository-durability session — close the current packet, sanitize and audit both
repositories, and push a verified checkpoint to the correct origins.
**This is NOT a frozen packet and does NOT edit one.** ✓

---

## The push checkpoint

| Item | Value |
|---|---|
| **game local HEAD** | **`19221476bc5adc6d5cfa056fa8f36cf3bd5b7996`** |
| **game `origin/master` HEAD** | **`19221476bc5adc6d5cfa056fa8f36cf3bd5b7996`** — **EQUAL** ✓ |
| **toolkit local HEAD** | **`d6e8f0b10e6612871beed6e16bf039ef25351f88`** |
| **toolkit `origin/main` HEAD** | **`d6e8f0b10e6612871beed6e16bf039ef25351f88`** — **EQUAL** ✓ |
| **toolkit `upstream/main` HEAD** | **`ea60cfa48596be63d95eed2d31e1a4bdd490893c`** — **UNCHANGED** ✓ |

**Both pushes were ordinary fast-forwards. NO `--force`, NO `--force-with-lease`, and `upstream` was never
pushed to.** ✓

## Push records, per the owner's push policy format

```
GAME
PUSHED_TO:   origin (https://github.com/danillogical/poison-jam.git)
BRANCH:      master  (first push; tracking configured via -u)
COMMIT:      19221476bc5adc6d5cfa056fa8f36cf3bd5b7996
RESULT:      OK -- [new branch] master -> master

TOOLKIT
PUSHED_TO:   origin (https://github.com/danillogical/xboxrecomp.git)
BRANCH:      main
COMMIT:      d6e8f0b10e6612871beed6e16bf039ef25351f88
RESULT:      OK -- 571982d..d6e8f0b  main -> main   (15 commits, fast-forward)
UPSTREAM:    NOT PUSHED; ref verified byte-identical before and after
```

## The workflow transaction, closed

| Item | Value |
|---|---|
| **Current packet** | **`A2h-slot-writer-attribution-r2`** |
| **State** | **`ACCEPT` — stage 1, FINAL** |
| **Acceptance result** | **`docs/reviews/a2h-slot-writer-attribution-r2-acceptance-record.md`** |
| **Frozen hashes verified** | **5 of 5 `MATCH`** |

**11 of 12 criteria `AGREED`; criterion #11 escalated and ruled OUT OF SCOPE by the Advisor, so every
criterion is `AGREED`.** **Per §2.2 a first-stage `ACCEPT` is FINAL — no second stage, no re-run.** ✓

**⚠ And the Session records its OWN error that caused the escalation:** **it imported two sub-items (the
no-debugger condition and tiled/contiguous non-overlap) into the stage-1 brief from the SUPERSEDED packet's
carry-forward list. They appear ZERO times in the frozen packet.** **The reviewer correctly marked `CANNOT
VERIFY` and escalated rather than reinterpreting.** **The Advisor: *"Carry-forward lists must be re-justified
per packet — that step was skipped."*** ✓

**The required static non-overlap check was executed:** the contiguous pool is `0x80010000..0x84010000` and
the slot page is `0x0019D000..0x0019DFFF` — **DISJOINT.** ✓

## `/game/` nondistribution audit — the primary safety rule

| Check | Result |
|---|---|
| **`CURRENT TRACKED PATHS UNDER game/`** | **ZERO** ✓ |
| **`game/ IS LOCALLY PRESENT`** | **YES** — `default.xbe` present, `Media/` present, 3 225 files, 2.3 GB ✓ |
| **`game/ IS IGNORED`** | **YES** — `.gitignore:2:/game/` matches `game\default.xbe` ✓ |
| **`REACHABLE HISTORICAL PATHS UNDER game/`** | **ZERO** — `git rev-list --objects --all` filtered on `game/` ✓ |
| **`RENAMED/MOVED RETAIL ASSET COPIES FOUND`** | **ZERO** — 0 matches for `.xbe .iso .xiso .xex .rom .img .zip .7z .rar .cue .mdf .gcm .rvz .pak .xmv .adx .sfd .wad` across **3 378 reachable objects** ✓ |
| **REMOTE TREE** | **511 entries, ZERO `game/` paths** ✓ |

**The audit inspected the COMPLETE REACHABLE OBJECT SET, not merely current filenames.** ✓ **`.gitignore`
alone would not have solved historical tracking; the object-set search is what proves it.** ✓

## Secret audit

**Every reachable blob scanned (1 269 blobs) with 14 pattern families** — GitHub/OpenAI/Anthropic/AWS/Google/
Slack tokens, private keys, bearer headers, password/secret/apikey assignments, connection strings.

**RESULT: 1 hit, and it is a SELF-MATCH** — the literal text `BEGIN OPENSSH PRIVATE KEY` **inside
`scripts/secret-audit.py` itself**, i.e. the scanner's own pattern string. **Verified by path and content, with
a POSITIVE CONTROL: a search for a real key body (base64 after the header) found NOTHING.** ✓

> **PASS.** **And the Session records the control because *"no secrets found"* is an absence claim, and an
> absence claim needs a positive control to be meaningful.** ✓

## Large-blob audit

| Metric | Value |
|---|---|
| **blobs measured** | **1 263** |
| **MAX blob** | **13 339 536 B = 12.72 MB** — `src/recomp/recovered/recovered.c` (legitimate source) |
| **blobs > 10 MB** | **14** — all `recovered.c` / `recomp_NNNN.c` revisions |
| **blobs > 50 MB** | **0** |
| **blobs > 100 MB** (GitHub hard limit) | **0** ✓ |

**No archives containing retail assets, no generated binaries, no disc images.** ✓

## `.gitignore`

**NO CHANGE WAS NEEDED — the existing `.gitignore` already contains the mandatory rule and is complete.** ✓

**`/game/` is present at line 2.** **Every rule was verified to match its intended path, and `docs/`,
`scripts/`, `tests/`, `config/`, `src/`, `tools/` were verified TRACKED (not accidentally ignored):**

| Path | State |
|---|---|
| **`/game/`** | **IGNORED** ✓ (mandatory) |
| **`/build/`, `/logs/`, `/saves/`** | **IGNORED** ✓ |
| **`.git.broken-backup/`** | **IGNORED** ✓ |
| **`/apu_watch_fixture_stderr.txt`, `/gpb9_trace.txt`** | **IGNORED** ✓ (each with a stated reason) |
| **`docs/` (384), `scripts/` (57), `tests/` (24), `config/` (9), `src/` (23), `tools/` (2)** | **TRACKED** ✓ |

**The Session added no rule, because adding one would have been unjustified change.** **⚠ Note the owner's
instruction to show which paths each NEW rule matches — since there are no new rules, there is nothing to
show, and the Session states that explicitly rather than inventing entries to appear thorough.** ✓

## Game tests/guards

| Check | Result |
|---|---|
| **`python -X utf8 scripts/build-jsrf.py`** | **succeeded; identity recorded** ✓ |
| **`ctest --test-dir build -C Release`** | **100% passed, 22 of 22** ✓ |
| **`python -X utf8 scripts/test-harness.py`** | **FULL PASS — 19 probes + 9 A2h delivery fixtures + 1 native-`#DB` control** ✓ |
| **8 game suites** | **all `OK`** — `test_agent_docs` (30), `test_recorded_reviews` (178), `test_markdown_tables` (12), `test_run_profiles` (31), `test_pio_free_demand` (29), `test_a2h_oom_slice` (31), `test_a2h_frame_audit` (29), `test_a2h_null_slot_triage` (17) ✓ |
| **`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`** | **intact** ✓ |
| **`src/` and `src/recomp/gen/`** | **clean — no generated-code edits** ✓ |

## Toolkit tests/guards

| Check | Result |
|---|---|
| **`ctest`** | **100% passed, 22 of 22** ✓ |
| **`test-harness.py`** | **FULL PASS** ✓ |
| **fast-forward verified** | **`origin/main` is an ancestor of `main`; 15 ahead, 0 behind, 0 merges** ✓ |
| **push delta** | **31 new blobs, max 0.37 MB** — all source ✓ |

## ⚠ A CORRECTION THIS SESSION MADE TO THE LINE'S OWN DIAGNOSIS

**The save-root `winerror=5` harness failures were attributed to a transient `CreateFileW` race against an
AV/indexer, and reported as *"proven pre-existing"* via a stashed clean baseline.** **THE ACTUAL CAUSE IS DISK
EXHAUSTION:**

| Drive state | Harness result |
|---|---|
| **0.51 GB free of 931.8 GB** | **FAILED — twice consecutively, on two different probes** |
| **~62 GB free** | **FULL PASS** |

> **A stash-based control varies the CODE and holds the ENVIRONMENT fixed — so it can prove *"not caused by the
> change"* but never *"not caused by the machine."*** ✓ — `a2h-save-root-failure-was-disk-exhaustion.md`
> (`6a97c86`).

**And the Session corrected its OWN misreading while measuring:** **it saw `logs/` at *"2579.6 GB"* on a
932 GB drive and briefly treated that as impossible.** **Sparse-aware measurement shows `logs/` is 167.3 GB on
disk — 2 412 GB of the logical figure is SPARSE** (the `Partition1.img` files are 2.36 GB logical with
`Valid Data Length = 0`). ✓

**⚠ The Session DELETED NO RUN EVIDENCE.** **`logs/runs/` holds 167 GB across 1 172 run directories and nothing
prunes it; it is evidence, and pruning it is not a decision the Session is authorized to make unilaterally.**
**The owner reclaimed space separately.** ✓

## Tools added (checked in, not scratch)

| Tool | Purpose |
|---|---|
| **`scripts/disk-usage.py`** | **sparse-aware REAL allocation** (`GetCompressedFileSizeW`), with a failure path that counts logical and FLAGS it |
| **`scripts/logs-reclaim-plan.py`** | **classifies run dirs into save-root vs evidence, `test-*` vs named**, so reclamation can be scoped |
| **`scripts/secret-audit.py`** | **scans every reachable blob for secret shapes, with redacted samples** |

## What was NOT done, and why

- **No successor packet started** — **per the owner's instruction to stop after the checkpoint.** ✓
- **No history rewrite** — **none was needed: the audits found nothing requiring one.** ✓
- **No force-push, no `upstream` push.** ✓
- **No frozen packet edited.** ✓
- **No row promoted** — **the packet's row remains `O-OPEN`.** ✓
- **No run evidence deleted.** ✓
