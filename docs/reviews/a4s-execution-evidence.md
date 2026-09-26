# A4s-r5 execution evidence

**Packet:** `docs/packets/a4s-toolkit-sync.md`, revision **`A4s-r5`**, class **change**, frozen SHA-256
`09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB` (263 lines).
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25. **Build/run owner:** Session.
**Selected decision row: `R-CONFLICT`.**
**Startup receipt (§0.6):** `docs/reviews/startup-20260925-session-58e86358.md` (linked from this
review record, which is the active record for the executed packet).
**Binding execution rulings:** `docs/reviews/a4s-r5-execution-startup-ruling.md` (Q1–Q4 + KX addendum).
**Supporting measurement records:** `docs/reviews/a4s-r5-conflict-inventory.md`,
`a4s-r5-premise-baseline-failures.md`, `a4s-r5-command-defect.md`, `a4s-r5-rollback-exe-hash.md`.

**Baselines at execution.** Game `c1cdb9157bcb26c071c2ad83b69c85925f1c8d92` (branch `master`).
Toolkit `0d7929c86771dd0b971941592fd4f15436116e82` (branch `main`), `@{u}` = `upstream/main`,
`upstream/main` = `origin/main` = `v0.11.0^{commit}` = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b`,
merge base `051a128df5ec27ef14f1ceaaead11c5457321eef`. No `a4s-*` branch existed before step 1.
**Toolkit `main` is left at `0d7929c`** (rollback path).

---

## P0 — preconditions

**P0.8 FAIL (literal):** ` M docs/agent-workflow.md`
(SHA-256 `CBEF90414E78D14BBF66FFA25D9BC7854D312C84AED9605246F305BE7AB8E6AB`);
**proceeded under Advisor exception** `docs/reviews/a4s-r5-execution-startup-ruling.md` (Q2).
**P0.8 is NOT recorded as PASS.** The exception's conditions were re-checked immediately before step 1
and again at step 7 (below).

| # | P0 item | Required | Measured | Result |
|---|---|---|---|---|
| 1 | toolkit `status --porcelain` | empty | empty | PASS |
| 2 | toolkit `rev-parse HEAD` / branch | `0d7929c…` / `main` | `0d7929c86771dd0b971941592fd4f15436116e82` / `main` | PASS |
| 3 | toolkit `@{u}` | `upstream/main` | `upstream/main` | PASS |
| 4 | `upstream/main`, `origin/main`, `v0.11.0^{commit}` | all `766ecef…` | all `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` | PASS |
| 5 | toolkit `remote -v` | exactly 4 lines | exactly the 4 required lines (below) | PASS |
| 6 | game `git remote` | prints nothing | prints nothing | PASS |
| 7 | toolkit `branch --list "a4s-*"` | prints nothing | prints nothing | PASS |
| 8 | game `status --porcelain` | empty | **` M docs/agent-workflow.md`** | **FAIL (literal), exception** |
| 9 | `recomp_0005.c:6748` / `:6751` | `loc_001A18D0: ;` / the `jne` back-edge | `loc_001A18D0: ;` / `if (CMP_NE(_fa, _fb)) goto loc_001A18D0; /* jne: not equal / not zero */` | PASS |

Toolkit `remote -v`, verbatim:
```text
origin	https://github.com/danillogical/xboxrecomp.git (fetch)
origin	https://github.com/danillogical/xboxrecomp.git (push)
upstream	https://github.com/sp00nznet/xboxrecomp.git (fetch)
upstream	DISABLED (push)
```

**P0.8 exception conditions, step 7 re-check** (Advisor-required): game `status --porcelain` shows
` M docs/agent-workflow.md` plus only Session-created records
(`docs/reviews/a4s-r5-command-defect.md`, `a4s-r5-conflict-inventory.md`,
`a4s-r5-execution-startup-ruling.md`, `a4s-r5-premise-baseline-failures.md`,
`a4s-r5-rollback-exe-hash.md`, `startup-20260925-session-58e86358.md`); no other path dirty;
`docs/agent-workflow.md` SHA-256 **unchanged** at `CBEF9041…E6AB`. **Conditions hold.**
The owner's file was **not** committed, staged, stashed, reverted or normalized.

**Game revision for this execution:** `c1cdb91` (recorded; the plan's `b7d6af5` note is the preceding
startup-receipt commit — an advisory record-pointer discrepancy, per Advisor Q2(c), not revised).

---

## Step 1 — recovery point (`AC-REC`)

`git -C $T branch a4s-pre-sync 0d7929c86771dd0b971941592fd4f15436116e82` → exit 0.
At closure `git -C $T rev-parse a4s-pre-sync` = `0d7929c86771dd0b971941592fd4f15436116e82`.
**`AC-REC` PASS** (both reads equal; the branch still exists at closure).

## Step 2 — pre-merge controls (on `0d7929c`)

| Control | Expected | Measured | Result |
|---|---|---|---|
| Build `scripts\build-jsrf.py` | succeeds | exit 0, "Build succeeded (success)" | PASS |
| Pre-merge exe SHA-256 | record (expected `9597ff7c…`) | `9597FF7C2A377265ABA8DBB90B461EBE763E02D65432E9DFA13ACD925539C553` | matches expected |
| `ctest -N` | 12 | **12** | PASS |
| `ctest` (set C) | all pass | **12/12 passed**, 100%, 22.01 s | PASS |
| Set G (game Python) | all pass | **8 pass / 2 fail** | 2 pre-existing (E1, E2) |
| Set K4 (toolkit baseline) | all pass | **Ran 27 tests, OK** | PASS |
| Set KX (toolkit, per module) | 33 modules | **33 modules, Ran sum = 133** | matches control |

`ctest -N` names: `jsrf_save_root`, `jsrf_memmove`, `jsrf_lifter_flags`, `jsrf_nv2a_registers`,
`jsrf_recovery_11c1`, `jsrf_service_chain`, `jsrf_callback_reentry`, `jsrf_nv2a_hal`,
`jsrf_inplace_event_bridge`, `jsrf_gpu_inspection`, `jsrf_native_gpu_warp`,
`xbox_timestamp_publication`.

### Set G — the two pre-existing failures (E1, E2)

Measured on the **pre-merge** tree, before any merge, and independently reproduced on a **pristine
HEAD** extraction (`git archive --output=<file> HEAD` + `tar -xf`; 199 files; committed workflow
`F0A2BD5B…`). Full record: `docs/reviews/a4s-r5-premise-baseline-failures.md`.

- **E1 — `tests/test_agent_docs.py`.** Failing test set exactly
  `{test_real_repository_is_clean}` (`Ran 30 tests`, `failures=1`). The test does **not** check git
  status; it asserts the document checker reports zero findings. `scripts/check-agent-docs.py:58-61`
  lists `gpt-5.6-sol` in `RETIRED_NAMES`, and the checker reports exactly four `retired_names`
  findings, all in `plan-jsrf-bare-minimum.md` at lines **15, 47, 67, 71**.
- **E2 — `tests/test_ac2_provenance.py`.** Failing check set exactly
  `{"A0 correct run PASSes", "P1 differently-cased archive keys still PASS"}`, with the clause-D reason
  line `the classifier was NOT edited; step 3 requires line 391 to be corrected`.

Both reproduce **byte-identically on pristine HEAD** → **pre-existing**, not introduced by the owner's
edit, this session, or any merge. Excepted by Advisor ruling **Q4** (closed exception set; see
`docs/reviews/a4s-r5-execution-startup-ruling.md`). **The KX carve-out was not extended to G
wholesale**; every other G file is held to the literal rule.

### Set KX — per-module step-2 table

Full table: `logs/a4s/kx-permodule-0d7929c.csv` (`module,exit,ran`). **33 modules; Ran sum = 133**,
matching the packet's recorded control. 17 modules exit 0 with tests; 16 exit 5 / `Ran 0 tests` /
`NO TESTS RAN` (they define no collectable `unittest.TestCase`; e.g. `test_config.py`,
`test_decode_at.py` have zero TestCase/class matches). Per Advisor Q4 addendum, exit 5 where step 2 was
also exit 5 is recorded as **"not exercised (pre-existing)"**, not a FAIL — **not** as a pass witness.
The combined single invocation also reproduces the packet's stated control line
(`Ran 133 tests … OK`), recorded as such and **not** as the per-module control.

## Step 3 — merge attempt and complete hunk classification

```powershell
git -C $T -c merge.conflictStyle=diff3 merge --no-ff --no-commit 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b
```
Merge in progress; **5 conflicted paths, all within the packet's 10-file set**:
`src/kernel/kernel_bridge.c`, `src/kernel/xbox_memory_layout.c`, `tools/recomp/lifter.py`,
`tools/recomp/test_icall_feedback.py`, `tools/recomp/translator.py`. **No conflict outside the 10.**

**9 hunks total: 1 × HA, 1 × H1, 2 × H2, 5 × UNDECIDED.**
Full per-hunk text (base/ours/theirs) in `logs/a4s/conflict-hunk-inventory.txt`; the conflicted files
are preserved verbatim as `logs/a4s/conflicted-*`. Analysis:
`docs/reviews/a4s-r5-conflict-inventory.md`.

| # | File | Lines | Rule | Basis |
|---|---|---|---|---|
| 1 | `kernel_bridge.c` | 1365–1404 | **UNDECIDED** | Base `xbox_KeSetEvent(...)` line changed differently in code by both sides; neither contains the other; union invalid (`guest_va`/`h` undefined on local's side) |
| 2 | `kernel_bridge.c` | 1425–1445 | **UNDECIDED** | Base `xbox_KeWaitForSingleObject(...)` line changed differently in code by both sides |
| 3 | `kernel_bridge.c` | 8981–8988 | **H1** | Upstream's only added line (`static RECOMP_TLS int g_kernel_dispatch_slot = -1;`) is present in local's version → take the containing side = **LOCAL** |
| 4 | `xbox_memory_layout.c` | 2123–2145 | **HA** | Pre-ruled by the Advisor: resolve to **LOCAL** |
| 5 | `lifter.py` | 400–412 | **UNDECIDED** | Local deletes the base `"lock xadd"` line; upstream keeps it and appends `"popfd"`. **H1 read literally is vacuously satisfied** (local added nothing) and would take upstream, silently discarding local's deletion — contradicting H1's purpose; H2 would union a deletion with an insertion. Requires semantic judgment, which step 3 forbids → recorded as an **H-rule gap** |
| 6 | `lifter.py` | 426–437 | **H2** | Each base line changed by at most one side (upstream drops `popfd`; local appends `"wbinvd"`) → keep both |
| 7 | `lifter.py` | 1276–1297 | **H2** | Base section empty; both sides only inserted at the same point → keep both, local first |
| 8 | `test_icall_feedback.py` | 116–124 | **UNDECIDED** | Base `assert main([...])` line changed differently by both sides (`--functions fns` vs `--functions …none.json`) |
| 9 | `translator.py` | 987–994 | **UNDECIDED** | Base `"lock cmpxchg")` line changed differently by both sides (`"xadd", "lock xadd"` vs `"inc", "dec"` plus a new clause line) |

**Row selection.** `R-CONFLICT` matches on "an UNDECIDED hunk". Per step 3 the packet does not
authorize choosing a side or merging semantics by judgment for these; the inventory is the next brief.
The plan predicted this first result (its read-only preview found "at least six undecidable hunks" and
deliberately did not pre-rule them). This attempt's mechanical count is **5**; the difference is not
load-bearing.

### Rollback

A merge **was** in progress, so the packet's `git -C $T merge --abort` branch applies. Executed,
exit 0. Result: toolkit `HEAD` = `0d7929c`, `status --porcelain` **clean**, `@{u}` = `upstream/main`,
no `MERGE_HEAD`, `a4s-pre-sync` intact. Because an aborted merge leaves no commit, there is no `HEAD`
to keep on `a4s-merge-attempt`; the attempt is preserved as the saved conflicted files and hunk
inventory under `logs/a4s/`. Recorded, not silently worked around.

### Rollback rebuild exe SHA-256

`python -X utf8 scripts\build-jsrf.py` → exit 0. exe SHA-256
**`AEC1F0FF7FB944DA44487EA15C3E94FF342751D4168E699F544A58895CF083C3`**, which is **not** equal to the
step-2 value. The packet instructs **record, not gate** ("if not, record — the restored tree is still
`0d7929c`"). Cause **measured** (an earlier inference of "deterministic rebuild" was **wrong** and is
corrected here): the build uses `/Zi` + `/DEBUG:FULL` with no `/Brepro`, so a **relink** stamps fresh PE
timestamps. Step 2's build found the tree already up to date and **did not relink**, which is why it kept
the archived binary and matched `9597FF7C…`; the merge attempt and `merge --abort` then rewrote **58
toolkit files** (mtimes only — `git status` clean, `git diff HEAD` empty), forcing the rollback build to
relink. A **no-op build was measured not to relink** (exe mtime and SHA unchanged). The two exes are
identical in size (15,068,160), section layout and all but **13 bytes** — four 3-byte PE timestamp
fields plus one stamp byte. Full analysis: `docs/reviews/a4s-r5-rollback-exe-hash.md`.

> Secondary finding recorded there: the merge/abort rewrote 4 toolkit files to CRLF
> (`core.autocrlf = true`), and 58 files in total changed mtime. `git status` reports no change; for the
> 3 `.c`/`.h` files the CRLF→LF bytes hash **exactly** to the archived `A4a-r2` reference. Not a content
> change; recorded so a future raw-hash comparison is not misread as a regression.

## `AC-MERGE`, `AC-KEEP`, `AC-INV` — not reached

`R-CONFLICT` is row 2 and is evaluated **before** `AC-MERGE`/`AC-KEEP`/`AC-INV` results are used: the
row matches on the UNDECIDED hunks, and the packet's rollback path leaves toolkit `main` at `0d7929c`
with no merge commit `M` to evaluate them against. Their pre-checks that *were* run read-only on the
merge tree `75083476` are recorded below as supporting evidence only, **not** as criterion results.

| Pre-check (read-only, on `75083476`) | Expected | Measured |
|---|---|---|
| Merge-tree identity | `75083476…` | `75083476d3277f5c6d91eb7040f7a3ce5dc25335` |
| `AC-MERGE` (b)/(c) populations | 38 / 153 / 10 | **38 / 153 / 10**; both-side names match the packet's list exactly |
| `AC-MERGE` (b) known-bad control on `0d7929c` | non-empty excess | **181** |
| `AC-KEEP` (iv) first-line anchors | 1/1/1 at 442/890/2103 | **1/1/1 at 442/890/2103**; lengths 7/56/25 exact |
| `AC-KEEP` (iv) r4 end-anchor defect | ×37 / ×20 | **37 / 20** |
| `AC-KEEP` (v) deleted-name controls | 2 / 2 / 0 / 0 | **2 (`766ecef`) / 2 (`051a128`) / 0 (`0d7929c`) / 0 (`75083476`)** |
| Classifier-name control | 2 / 2, no difference | **2 at `0d7929c`, 2 at `75083476`**, no difference |
| `SCOPE` boundary guard | does not trip | **no hits** (no added `templates/`/`tools/` line) |
| Game `CMakeLists.txt` diff vs HEAD | empty | **empty** |
| `AC-KEEP` (v) code tokens | — | `\|= MCPX_AC97_CODEC_READY` → `:2131`; `\bac97_arm_write_trap\s*\(` → definition `:411` and HA-hunk call `:2134` |

### `AC-INV` "New environment names" — authorized substitute and its three controls

The literal packet command cannot run on this host (PowerShell 5.1 strips the embedded `"` when
building git's command line). Proceeded under Advisor interpretation ruling **Q3** using the **only**
authorized form. Full record: `docs/reviews/a4s-r5-command-defect.md`.

- Pattern file `logs/a4s/getenv-pattern.txt`: **23 bytes**, ASCII, **no BOM, no trailing newline**,
  SHA-256 `C03CC67C6423BC47B2D53BB7F8F5288DC842584841B7DF96FE8119FAE8034B36`
  (the ruling predicted 24 bytes; the operative requirement is the exact byte content, which is
  `getenv\("[A-Za-z0-9_]+"` — the measured length is recorded, not the prediction).
- Command: `git -C $T grep -h -o -E -f logs\a4s\getenv-pattern.txt <rev> -- src include`.

| Control | Required | Measured | Result |
|---|---|---|---|
| (iii) literal packet command on `0d7929c` | empty, exit 1 (known-bad witness) | **0 lines, exit 1** | PASS |
| (i) `0d7929c` unique names | exactly 42 | **42** | PASS |
| (ii) `75083476` unique names | 44, difference exactly `{RECOMP_APU_MIXDOWN_ALL, RECOMP_USB_PORT}` added, none removed | **44**; added: `RECOMP_APU_MIXDOWN_ALL`, `RECOMP_USB_PORT`; removed: **none** | PASS |

All three controls pass, so the transport delivers the quote-bearing pattern and the step is not
UNKNOWN. Raw outputs: `logs/a4s/getenv-names-0d7929c.txt`, `logs/a4s/getenv-names-75083476.txt`,
`logs/a4s/control-iii-literal-command.txt`. Because `R-CONFLICT` was selected at step 3, no `M` exists
and the at-`M` comparison is not performed.

## Step 7 — end-of-execution checks

**`AC-GEN` — PASS.** `git diff --quiet HEAD -- src/recomp config tools/disasm/output` → exit 0;
`git status --porcelain -- src/recomp config` → empty;
`python -X utf8 scripts\check-generation-provenance.py --check` → `ok : True`, exit 0. No problem
attributable to project dirtiness was reported, so the Advisor's flagged uncertainty (Q2 basis) is not
engaged. Per the packet, PASS here means the generated bytes are the pre-merge generator's.

### Premise change to `Q4`'s E1 exception (recorded; `PREMISE_CHANGED`)

Writing the closure record required updating this plan's `CURRENT PACKET` block, which is the plan's own
job (§8: the plan owns the current packet, blocker, next action). That rewrite **replaced a block that
was already superseded** — the stale route-state, temporary-staffing and session-pause text describing a
staffing arrangement that the owner's replaced `docs/agent-workflow.md` had itself superseded. **All
four `gpt-5.6-sol` mentions that `Q4`'s E1 exception named (plan lines 15, 47, 67, 71) sat inside that
replaced block and were removed with it.**

Measured consequence:

| Check | Step 2 (pre-merge) | Now |
|---|---|---|
| `scripts/check-agent-docs.py --check` | exit 1, **4** `retired_names` findings | exit 0, **0 findings** |
| `tests/test_agent_docs.py` (E1) | **FAIL** — `test_real_repository_is_clean` | **PASS** (`Ran 30 tests, OK`) |
| `tests/test_ac2_provenance.py` (E2) | **FAIL** — `A0`, `P1` | **FAIL** — `A0`, `P1` (unchanged) |
| Set G totals | 8 pass / 2 fail | **9 pass / 1 fail** |

**Honest classification of what this is and is not.**

- It is **not** an attempt to make the baseline green. The plan update is mandatory closure bookkeeping,
  and the removed lines were obsolete content that the owner's own workflow replacement had already
  superseded. No test, script, packet or frozen artifact was touched to achieve it; the checker's own
  verdict flipped as a side effect of removing stale prose.
- It **is** a real `PREMISE_CHANGED` for `Q4`'s E1 exception, and it is recorded as one rather than
  glossed. E1's exception named specific plan lines; those lines no longer exist, so the exception has no
  remaining subject. **E1 now passes on its own merits.**
- **No measurement in this packet is invalidated by it.** `AC-TEST` was never reached — `R-CONFLICT` is
  row 2 and was selected at step 3, before any step-5 test run at `M`. There is no `M` and no step-5
  comparison, so no control was changed mid-measurement. Advisor `Q4` item 4 ("keep the baseline fixed
  during execution … before step 5 has run") was aimed at protecting that comparison; there was none to
  protect.
- Advisor `Q4` item 3 provides for exactly this case in the forward direction: *"If either file now
  passes at M, record it; that is not a FAIL, but note it as unexplained, because the merge should not
  affect it."* Here the cause **is** explained and is **not** the merge: it is this Session's plan edit.
  Recorded with the cause rather than as unexplained.
- `Q4`'s **E2 exception remains live and unchanged**: `test_ac2_provenance.py` still fails with exactly
  `{A0, P1}`, and its cause remains unconfirmed.

**Consequence for `A4s-r6` and any re-run of this packet:** a future execution of `A4s-r5` would see a
**different step-2 set-G baseline** (9/1, not 8/2) because the plan no longer trips the checker. Any
re-run must re-measure the baseline rather than inherit the 8/2 figures above. Recorded so the
difference is not later mistaken for a regression.

**Advisor's disclosure re-verified at closure** (its `Q4` addendum asked for this): toolkit
`status --porcelain` contains only merge-staged entries and the 5 `UU` conflicts that were then aborted;
**0 unstaged modifications, 0 untracked non-ignored files**; the 15 ignored entries are pre-existing
`__pycache__`/`build`/`output`/`.pytest_cache` directories. The Advisor's run left nothing behind, and
its 27-module syntax-error results were excluded as non-evidence.

**`AC-NOPUSH` — PASS.** `origin/main` = `upstream/main` = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b`;
`@{u}` = `upstream/main`; `status -sb` first line = `## main...upstream/main [ahead 24, behind 169]`
(the **rollback** form the packet specifies); `remote -v` exactly the four P0 lines with `upstream`
push `DISABLED`; game `git remote` prints nothing. **Session attestation: no `git push` and no
`git fetch` was run at any point during execution.** The only network-capable git operations
performed were the local, read-only `git merge-tree --write-tree` and the local merge attempt, neither
of which contacts a remote. **No push is performed at closure**, because `R-CONFLICT` leaves `main` at
`0d7929c`, which is not a descendant of `origin/main`; per the Closure rule the record is
**"no push: main at 0d7929c"**.

## Selected row and next action

**Row `R-CONFLICT`** — *"the merge cannot be completed by the H-rules."*
**Toolkit `main` left at:** `0d7929c` (rollback), working tree clean.
**Next packet:** Planner **`A4s-r6`** with this full hunk inventory as its brief, naming a resolution
per hunk. Hunks **1, 2, 5, 8, 9** choose between local accepted runtime and an upstream model, so per
the row they **go to the Advisor first**. Hunk 5 additionally raises a **rule gap** (H1's literal
reading is vacuous when one side only deletes), which is itself an Advisor/Planner question.

**Planner route state:** `workbuddy-ai/kimi-k3` resolves (single canonical match) and is invoked
**without** a reasoning-effort qualifier per the owner's direct instruction
("kimi k3 doesn't take an effort, so don't worry about effort for kimi k3"). **No Planner-route
blocker is in force**, so the `A4s-r6` handoff proceeds normally.

## Evidence index

| Criterion | Artifact | Result |
|---|---|---|
| P0 (1–7, 9) | this file, P0 table | PASS |
| P0.8 | this file, P0 table | **FAIL (literal)** — Advisor exception |
| `AC-REC` | this file, step 1 | PASS |
| `AC-TEST` (C, K4, KX) | this file, step 2 | PASS |
| `AC-TEST` (G) | `docs/reviews/a4s-r5-premise-baseline-failures.md`; `logs/a4s/kx-permodule-0d7929c.csv` | step 2: **8 pass / 2 fail (E1, E2)**. **PREMISE_CHANGED at closure:** E1 now **PASSES** (the plan edit removed the four stale route mentions the checker flagged); E2 still fails. Set G = **9 pass / 1 fail** |
| `AC-MERGE` / `AC-KEEP` / `AC-INV` | not reached (row 2 selected first) | pre-checks recorded, non-binding |
| `AC-INV` new env names | `logs/a4s/getenv-pattern.txt`; `getenv-names-*.txt`; `control-iii-literal-command.txt` | 3/3 controls PASS (Q3) |
| `AC-GEN` | this file, step 7 | PASS |
| `AC-NOPUSH` | this file, step 7 | PASS |
| `AC-BUILD` | this file, step 2 + rollback | PASS (step 2); rollback exe `AEC1F0FF…` recorded |
| Conflict inventory | `logs/a4s/conflict-hunk-inventory.txt`; `logs/a4s/conflicted-*`; `docs/reviews/a4s-r5-conflict-inventory.md` | 9 hunks: 1 HA, 1 H1, 2 H2, **5 UNDECIDED** |
| Rollback exe hash | `docs/reviews/a4s-r5-rollback-exe-hash.md` | recorded (not gated) |

**Post-review edits reopen affected criteria.** This file is frozen at acceptance; the evidence above
binds to the revisions named in it. Unrelated next stops are recorded as follow-ups, not scope
expansions.

## Deferred / follow-ups (recorded, not executed)

1. **E1 — RESOLVED as a side effect of closure (recorded as `PREMISE_CHANGED`).** The four
   current-tense mentions of a since-retired route name that tripped `scripts/check-agent-docs.py`'s
   `RETIRED_NAMES` check sat in the plan's superseded staffing block. Rewriting that block for closure
   removed them, and `tests/test_agent_docs.py` now **passes**. No test, script, packet or frozen
   artifact was edited to achieve this. **Any re-run of `A4s-r5` must re-measure the set-G baseline**
   (now 9/1, not 8/2).
2. **E2** — cause unconfirmed. Lead: the test's clause D expects an unedited-classifier state from an
   older packet (A3a-era "line 391"). Diagnose before anyone relies on the AC2 provenance tool.
3. **KX claim limit** — ≥16 function-style test modules are not exercised by unittest, before or after
   the merge. Run them under their own runner or convert them.
4. **H-rule gap (hunk 5)** — H1's literal reading is vacuously true when one side only deletes, which
   would silently discard that side's work. Needs a Planner/Advisor clarification before `A4s-r6`
   relies on H1.
5. **Plan baseline note** — records `b7d6af5` as the promotion baseline; the promotion commit is
   `c1cdb91`. Advisory; not revised.
6. **Exe-equality checks must state whether a relink happened** (Advisor advisory, accepted and now
   measured). Because the project does not pass `/Brepro`, a relink changes the binary's PE timestamps
   even when the source is byte-identical. Measured here: a **no-op build does not relink**; the
   merge/abort rewrote 58 files' mtimes and forced the rollback build to relink. Any future
   "exe equals control" comparison must say whether a relink occurred, and should compare with the PE
   timestamp fields masked (or build with `/Brepro`) where binary equality actually matters. No packet
   claim depends on this.
7. **`AC-TEST` needs a written set-G carve-out** (Advisor Q4 closure note, item 2). The `A4s-r6` Planner
   should add a G exception modelled on the existing KX one, **naming failures by test identity**,
   rather than relying on a case ruling. That closes the literal-clause gap that produced Q4. The
   exception set must be **re-derived from each execution's own step-2 run and never carried forward**:
   with the current tree that means E2 only, with set G expected at **9 pass / 1 fail**.
