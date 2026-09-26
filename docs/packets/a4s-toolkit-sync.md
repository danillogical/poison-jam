# A4s-r6 — merge toolkit `v0.11.0` into local `main` with all 9 hunks pre-ruled; rebuild without regeneration; test; locate the strict stop

## Sketch

- **Bounded claim:** produce merge commit `M` (parents `0d7929c` + `766ecef`) resolving all 9 recorded hunks per the binding Advisor rulings (hunks 1–2 COMBINED exact C text; 3 H1→LOCAL; 4 HA→LOCAL; 5 per-line H2 edit-application; 6 H2; 7 H2; 8 LOCAL; 9 full union), with the H1/H2 merge rules repaired to represent deletions, a new structural post-resolution check before the build, then build (no regeneration), test, and one strict run selecting exactly one row.
- **Packet class:** change (revision of `A4s-r5`; §5.4(3) — Advisor hunk ruling recorded).
- **Material unknowns:** none. All five prior UNDECIDED hunks are pre-ruled with exact text (`docs/reviews/a4s-r6-advisor-hunk-ruling.md`). The H1/H2 repair is Advisor-ruled and Session-verified on the corpus (`logs/a4s/verify-advisor-h2.py`, `verify-hunk5-output.py`). Set-G baseline is re-measured per execution (10 pass / 1 fail over 11 files at this revision; carve-out names E2 by test identity). The structural scanner is a tracked, SHA-256-pinned tool with Advisor-specified properties, predicates and positive/negative controls.
- **Proposed merge procedure / experiment:** identical to `A4s-r5` except step 3 resolves every hunk by a **complete pre-ruled table** (none reaches UNDECIDED) using the repaired H1 (containment over additions **and** deletions) and H2 (**edit application**, never a union by line presence); then a new `AC-STRUCT` scans the resolved tree over `SCOPE` before the build (conflict markers; duplicate `case` values per switch per preprocessor branch path; duplicate file-scope definitions per branch path), selecting `R-CONFLICT` on any hit. `AC-MERGE`(d) gains the ruled deletion twin witness; `AC-TEST` gains the set-G carve-out.
- **Outcome rows:** unchanged in shape — `R-PUSH` / `R-CONFLICT` (now reachable via `AC-STRUCT` FAIL, `AC-MERGE`/`AC-KEEP`/`AC-INV` FAIL, a conflict-set divergence, an attribution-ambiguous hunk, or the `R-BUILD` backstop) / `R-REGEN` / `R-BUILD` (gated by the C2084/C2196/C2371 backstop) / `R-TEST` / `R-PRE` / `R-INVALID` / `R-MOVED` / `R-SAME` / `R-UNKNOWN`. Expected first result: a successful merge `M` reaching `R-SAME` (forecast only).
- **Wrong outcomes guarded:** (1) a clean-hunk structural defect misattributed to `R-BUILD` → `AC-STRUCT` + backstop; (2) the H2 presence-union resurrecting a deleted line → edit-application rule + `AC-MERGE`(d) twin; (3) set-G baseline copied stale → re-measure + carve-out by identity.

### Sketch checkpoint trail (§5.1.4)

- **Sketch 1 (tool-call 10, ≤20):** the sketch above. Sent to the Advisor for shape preflight via the Session at tool-call 11. **Forecast made then:** expand into the full packet by transcribing the binding rulings into the `A4s-r5` structure; no further investigation; the only fact that could change the sketch was the Advisor returning `REDIRECT`/`DISCOVERY_FIRST`.
- **Advisor SHAPE verdict:** **PROCEED**, with three bounded POLICY_ISSUE items, all folded in: (1) gating tools tracked + SHA-256-pinned (scanner `scripts/check-merge-structure.py` `A1FDCE26…`, fixtures `tests/test_merge_structure.py` `A42F4A1E…`; the verifiers kept as non-gating provenance) — controls as packet criteria; (2) the pre-ruled table does not exempt clean hunks — `AC-INV` and the `SCOPE` guard kept, and a conflict-set/text divergence → `R-CONFLICT`; (3) "expected `R-SAME`" is a forecast only; `R-PUSH` follows the owner push policy (five pre-push checks; `R-CONFLICT`/rollback/INADEQUATE are no-push). **Yield against forecast:** the packet body below is exactly the forecast expansion; the three policy items and two measured refinements (final pinned hashes; set-G moved to 11 files/10-1) were incorporated without changing the sketch's shape. Planning stayed within budget (no 40/60-call checkpoint reached).

---

## A4s — merge toolkit `v0.11.0` into local `main` with all 9 hunks pre-ruled, rebuild without regeneration, test, and locate the strict stop

**Class:** change   **Contract revision:** `A4s-r6`   **Status:** draft
**r6 delta (from `A4s-r5`, per the binding Advisor hunk ruling `docs/reviews/a4s-r6-advisor-hunk-ruling.md` — a §5.4(3) revision):** step 3 resolves **every** hunk from a **complete pre-ruled table** (no hunk reaches UNDECIDED on the recorded conflict set) using the **repaired H1** (containment over additions **and** deletions) and **H2 as edit application** (never a union by line presence); hunks 1–2 are pre-ruled **HA-COMBINED** with exact C text plus two pre-ruled HA edits; new **`AC-STRUCT`** runs a structural post-resolution check on the resolved tree over `SCOPE` before the build (its FAIL selects `R-CONFLICT`, never `R-BUILD`), with a `C2084`/`C2196`/`C2371` backstop on `R-BUILD`; `AC-MERGE`(d) gains the ruled **deletion twin witness**; `AC-TEST` gains a **set-G carve-out** (naming failures by test identity, re-derived per execution, never carried forward). The gating scanner and its fixtures are **tracked** tools pinned by SHA-256. The `SCOPE` boundary guard, `AC-INV` by-content inventory, `AC-KEEP`, `AC-GEN`, `AC-BUILD`, `AC-NOPUSH`, `AC-RUN`, the stop predicate, the rerun cap and the claim limits are carried over unchanged.
**Governing requirement:** owner instruction 2026-09-24, verbatim in `docs/reviews/toolkit-sync-instruction.md` (merge v0.11.0 into local `main`, resolving conflicts; rebuild **without** regenerating `src/recomp/gen`; run both repositories' tests; rerun one strict baseline; if the strict stop moves, that is the next brief), **as amended by the owner's toolkit fork and push policy** recorded in `AGENTS.md` § "Toolkit remotes" and `docs/reviews/owner-push-policy-xboxrecomp-fork.md`: the instruction's `origin/main` (sp00nznet) is now **`upstream/main`**; `origin` is the owner's fork; **nothing is pushed during execution**; after ACCEPT `main` is pushed to `origin` (Closure); **never push to `upstream`**.
**Depends on:** `A4p-r1` ACCEPTED (plan, 2026-09-24); the executed `A4s-r5` (row `R-CONFLICT`; `docs/reviews/a4s-execution-evidence.md`); the binding Advisor ruling `docs/reviews/a4s-r6-advisor-hunk-ruling.md`.
**Baseline:** toolkit `main` = `0d7929c86771dd0b971941592fd4f15436116e82` (clean, **tracking `upstream/main`**); `upstream/main` = `origin/main` = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` = tag `v0.11.0`; remotes `origin` = `https://github.com/danillogical/xboxrecomp.git` (fetch+push), `upstream` = `https://github.com/sp00nznet/xboxrecomp.git` (fetch; push `DISABLED`); merge base `051a128df5ec27ef14f1ceaaead11c5457321eef`; game = the commit that adds this packet (Session records it at promotion; clean tree except the owner's uncommitted `docs/agent-workflow.md`, P0.8). Reference strict run: `A4a-r2` R0 `logs/runs/20260924-191906-091-a4a-r2-default` (exe `9597ff7c2a377265aba8dbb90b461ebe763e02d65432e9dfa13acd925539c553`, toolkit `0d7929c`), `diagnostic_deadline`, `F = 2`.
**Revision log:** `docs/reviews/a4s-revision-history.md` (non-authoritative).

### Motivating evidence
- The executed `A4s-r5` merge attempt (`git -c merge.conflictStyle=diff3 merge --no-ff --no-commit 766ecef` from `0d7929c`) produced **9 conflict hunks across exactly the 5 both-side files**, classified **1 HA, 1 H1, 2 H2, 5 UNDECIDED**, selecting `R-CONFLICT` (`docs/reviews/a4s-r5-conflict-inventory.md`; raw base/ours/theirs text `logs/a4s/conflict-hunk-inventory.txt`; conflicted files preserved verbatim as `logs/a4s/conflicted-*`).
- The five UNDECIDED hunks, the H1/H2 rule property, and the post-resolution check are now **ruled, binding, and verbatim** in `docs/reviews/a4s-r6-advisor-hunk-ruling.md`. The repaired H1 + edit-application H2 was **Session-verified on the real corpus**: hunk 5 changes from UNDECIDED to `H2 applies`, every other hunk keeps its classification, and edit application produces the ruled `_FLAGS_UNDEFINED` text (`logs/a4s/verify-advisor-h2.py`, `logs/a4s/verify-hunk5-output.py` — provenance, not gating).
- Three **clean-hunk structural defects** the conflict-only rules cannot see (duplicate `case 138` in **both** dispatch switches; duplicate `bridge_KeResetEvent` definition) are measured in `docs/reviews/a4s-r6-structural-findings.md`. The tracked scanner that detects them, its pinned hashes, its fixtures, and its verified control numbers are in `docs/reviews/a4s-r6-ac-struct-tool.md` and `docs/reviews/a4s-r6-ac-struct-controls.md`.
- Current strict stop: `loc_001A18D0` spin, `recomp_0005.c:6748-6751` in `sub_001A1769`, pending word `MEM32(0x803C0810) = 3`, `diagnostic_deadline` — `A4a-r2` R0, profile STRICT (`docs/reviews/a4a-execution-evidence.md`).

### Claim and boundaries
- **Establishes:** local toolkit `main` is a merge commit `M` with parents `0d7929c` and `766ecef` that (a) contains every file exactly as one side left it, except the 10 both-side files, which are resolved per the pre-ruled table (both sides' work kept under the repaired H1/H2 rules and the HA/HA-COMBINED pre-rulings); (b) preserves `0d7929c`'s `src/apu/apu_mmio_hook.c` byte-for-byte and `c97ce2c`'s AC'97 model lines; (c) is free of conflict markers, duplicate `case` values per switch per branch path, and duplicate file-scope definitions per branch path over `SCOPE` (`AC-STRUCT`); (d) builds with `scripts\build-jsrf.py` **without** regeneration, with the game's `src/recomp/gen` byte-unchanged; (e) passes the test sets defined in `AC-TEST`; (f) was not pushed anywhere during execution, and `upstream` was never pushed to; and (g) one strict default run on that build selects one row below.
- **Does not establish:** that the merged lifter/translator produce correct output; that `src/recomp/gen` matches the merged lifter — **it does not, by construction** (the chunks, `recomp_funcs.h`, `recomp_dispatch.c` and `recomp_stubs_unresolved.c` are the products of the pre-merge generator, an owner-chosen bound; likewise the merged `templates/runtime/recomp_types.h` is not built); that upstream's runtime changes are correct for JSRF; that the merged timer model (`ke_shadow_*`) is exercised by JSRF (ordinal 113 declared but not measured-called); any guest behaviour beyond the one run's stop; that a moved stop is deterministic; any audio/DSP/GPU/liveness/boot claim. `AC-STRUCT` is a textual, non-preprocessing check — it certifies the three listed structural properties over `SCOPE`, not general compile-correctness (the `R-BUILD` backstop covers the compile class it cannot express).
- **Non-goals:** regeneration of any kind; fixing any failing test, build error or guest stop; editing game source, scripts, config or CMake beyond the pinned scanner/fixtures already present; revising `A4b1`/`A4b2`; any push before ACCEPT; any push to `upstream` at any time; any `git fetch`; merging anything other than `766ecef`; changing remotes; adding a remote to the game repository; resolving any conflict hunk **not** among the 9 recorded, or any recorded hunk whose base/ours/theirs text differs from `logs/a4s/conflict-hunk-inventory.txt` (that is `R-CONFLICT`, never resolved by analogy); a broader semantic "every reachable difference" review (ruled out by the Advisor). Upstream's new C test trees (`tests/apu_mixdown`, `tests/d3d8_smoke`, `tests/fist`, `tests/kernel_*`, …) and `tools/conformance` are out of scope; whether they register into the game's CTest is **not asserted**.

### Readiness
- **Tools (Session verifies each runs before freezing):** `git` 2.39.2; `python -X utf8 scripts\build-jsrf.py`; `ctest --test-dir build -C Release [-N] --output-on-failure`; game `tests\test_*.py` run as files; toolkit `C:\Python313\python.exe -m unittest …` with `PYTHONPATH=C:\Users\logic\AppData\Roaming\Python\Python313\site-packages`; `scripts\check-generation-provenance.py --check`; `scripts\run-jsrf.py … --profile strict`; `scripts\check-run-profile.py`; `scripts\check-dump-mapping.py`; `scripts\inspect-jsrf.py memory`; and the pinned structural scanner below.
- **Pinned gating tools (`AC-STRUCT`; Advisor POLICY_ISSUE (1) — gating tools live at a tracked path, pinned; never run from gitignored `logs/a4s/`):**
  - `scripts/check-merge-structure.py` — **SHA-256 `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF`**, 358 lines. Invocation: `python -X utf8 scripts\check-merge-structure.py --rev <rev> --expect <n>` (revision scan), `python -X utf8 scripts\check-merge-structure.py` (worktree scan), `python -X utf8 scripts\check-merge-structure.py --rev <rev> --json`. Exit `0` = zero findings and zero UNKNOWN; exit `1` = findings present, an UNKNOWN file, or an expectation mismatch (a mismatch prints `EXPECTATION FAILED: expected N findings, found M`, so the two are distinguishable).
  - `tests/test_merge_structure.py` — **SHA-256 `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C`**; 14 tests, all pass (`python -X utf8 tests\test_merge_structure.py`). These are the evidence the scanner is trustworthy (they caught a real false negative — one-line function definitions — in an earlier version); cite them as the scanner's known-good/known-bad control.
  - The scanner's bytes are verified against the pinned SHA-256 before each use (P0.10); any byte change reopens `AC-STRUCT` (§2.4.7).
- **Provenance tools (NOT gating):** `logs/a4s/verify-advisor-h2.py` and `logs/a4s/verify-hunk5-output.py` are cited only as the Session's measurement that the repaired rules reproduce the Advisor's expected classifications and hunk-5 text. The packet specifies the repaired H1/H2 rule text in full below, so **no criterion invokes them**; they need no tracked copy.
- **Preconditions P0 (all must hold, else stop before any write):** items 1–9 as in `A4s-r5` — toolkit `status --porcelain` empty; toolkit `rev-parse HEAD` = `0d7929c86771dd0b971941592fd4f15436116e82`, branch `main`, `rev-parse --abbrev-ref "@{u}"` = `upstream/main`; `rev-parse upstream/main`, `rev-parse origin/main`, `rev-parse "v0.11.0^{commit}"` all = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` (already local — **no `git fetch`**); `git -C $T remote -v` prints exactly the four required lines with `upstream` push `DISABLED`; game `git remote` prints nothing; `git -C $T branch --list "a4s-*"` prints nothing; game `git status --porcelain` empty **except** the owner's uncommitted `docs/agent-workflow.md` — the recorded P0.8 exception `docs/reviews/a4s-r5-execution-startup-ruling.md` (Q2), whose conditions (only that file plus Session-created records dirty; its SHA-256 unchanged) are re-checked at step 1 and step 8; lines 6748 and 6751 of `src\recomp\gen\recomp_0005.c` read `loc_001A18D0: ;` and `… goto loc_001A18D0; …`. **P0.10:** `Get-FileHash -Algorithm SHA256 scripts\check-merge-structure.py` = `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF`.
- **Stop if:**
  - any `git push` (including `--dry-run`), `git fetch`, `git remote` edit, or other remote operation is about to run **during execution** — the only push this packet authorizes is the Closure push to `origin` after ACCEPT; **never push to `upstream`**; never add a remote to the game repository;
  - any of `python -m tools.recomp`, `scripts\recover-functions.py`, `scripts\relift-selected.py`, `build-jsrf.py --allow-regeneration` would run, or anything under game `src/recomp/`, `config/`, `tools/disasm/output/` would change;
  - the merge **appears to require regeneration** — any build error located in `src/recomp/gen/*` or `src/recomp/recovered/*`, or an unresolved/duplicate symbol whose name is `sub_*`/`loc_*` or is declared in `recomp_funcs.h` → `R-REGEN`;
  - a build or test failure would need any edit other than the conflict resolution in step 3 (no fixes in scope);
  - a conflict appears in a file outside the 10 listed, **or the merge's conflict set differs from the 9 recorded hunks**, **or any recorded hunk's base/ours/theirs text differs from `logs/a4s/conflict-hunk-inventory.txt`**, **or a hunk's per-line attribution is ambiguous under H2** → `R-CONFLICT` (never resolve by analogy);
  - the `SCOPE` boundary guard (`AC-KEEP`) trips → roll back (step 1) and return to the Advisor (UNKNOWN; not a decision row);
  - the `AC-STRUCT` scanner's bytes differ from the pinned SHA-256, or it reports UNKNOWN for a merge-changed file → roll back, `R-CONFLICT`;
  - any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE`, `RECOMP_APU_TRAP`, `RECOMP_APU_TRACE` is in the run environment, or the runner refuses `--profile strict`.

### Execution
Toolkit commands run with `-C C:\Users\logic\Repos\xboxrecomp` (written `$T` below: `$T='C:\Users\logic\Repos\xboxrecomp'`); game commands from `C:\Users\logic\Repos\my_xbox_game`. Record every output into `logs\a4s\` (raw) and the values into `docs\reviews\a4s-execution-evidence.md`.

1. **Recovery point.** `git -C $T branch a4s-pre-sync 0d7929c86771dd0b971941592fd4f15436116e82`. **The rollback target is commit `0d7929c86771dd0b971941592fd4f15436116e82`** (branch `a4s-pre-sync`, local only). Rollback procedure, used by every failure row: if a merge is in progress `git -C $T merge --abort`; else `git -C $T branch -f a4s-merge-attempt HEAD` then `git -C $T reset --hard 0d7929c86771dd0b971941592fd4f15436116e82`; then `python -X utf8 scripts\build-jsrf.py` and record the exe SHA-256 (it must equal the step-2 pre-merge SHA; if not, record — the restored tree is still `0d7929c`; the relink-timestamp behaviour is measured in `docs/reviews/a4s-r5-rollback-exe-hash.md` and is **not** an acceptance gate).
2. **Pre-merge controls** (on `0d7929c`): `python -X utf8 scripts\build-jsrf.py`; record `build\Release\jsrf_recomp.exe` SHA-256 (record, not a gate); `ctest --test-dir build -C Release -N` (expect 12) and `ctest --test-dir build -C Release --output-on-failure`; game Python set **G** and toolkit sets **K4** and **KX** (defined in `AC-TEST`) — record per-test/per-module pass/fail. **Set-G baseline is re-measured here every execution** — it moved three times in one session (8/2 → 9/1 → **10 pass / 1 fail** after the AC-STRUCT fixtures joined set G). Derive the carve-out set from **this** step-2 run by test identity (currently E2 = `tests/test_ac2_provenance.py` only); **never copy a count or a prior baseline forward** (`docs/reviews/a4s-execution-evidence.md`, `docs/reviews/a4s-r6-ac-struct-tool.md`). These are the controls for `AC-TEST`'s regression rule.
3. **Merge** (from `upstream/main`, pinned by SHA). `git -C $T -c merge.conflictStyle=diff3 merge --no-ff --no-commit 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b`. List conflicts: `git -C $T diff --name-only --diff-filter=U`. **Guard:** the conflicted paths must be exactly the 5 recorded files (`src/kernel/kernel_bridge.c`, `src/kernel/xbox_memory_layout.c`, `tools/recomp/lifter.py`, `tools/recomp/test_icall_feedback.py`, `tools/recomp/translator.py`), all within the 10-file set, **and** the conflict hunks must be exactly the 9 recorded in `logs/a4s/conflict-hunk-inventory.txt` (same files, same count, matching base/ours/theirs text). **Any difference → stop, roll back, `R-CONFLICT`** (the pre-ruled table is incomplete; do not proceed, do not resolve by analogy). Then resolve **each** of the 9 hunks per the **Pre-ruled resolution table** below, applying the rule text in **Merge rules (repaired)**. Record file, hunk, rule, and (for HA/H3) omitted lines.

   #### Pre-ruled resolution table (all 9 hunks; none reaches UNDECIDED on the recorded set)

   | # | File (hunk lines) | Rule | Resolution |
   |---|---|---|---|
   | 1 | `kernel_bridge.c` (1365–1404) | **HA-COMBINED** | Replace the whole of `bridge_KeSetEvent` **and** local's whole `bridge_KeResetEvent` with the exact text in **HA-COMBINED-1** (Appendix A) — one `bridge_KeResetEvent` total. Fixed precedence: (1) in-place KEVENT if `guest_va_is_inplace_kevent` accepts, (2) `ke_shadow_lookup` non-NULL, (3) `XBOX_TO_NATIVE`. **No `bridge_resolve_handle` tier.** |
   | 2 | `kernel_bridge.c` (1425–1445) | **HA-COMBINED** | Replace the whole `bridge_KeWaitForSingleObject` with the exact text in **HA-COMBINED-2** (Appendix A) — same three tiers. |
   | 3 | `kernel_bridge.c` (8981–8988) | **H1** | Upstream's added line is contained in local's version (and both delete the same base line) → take **LOCAL** (`static RECOMP_TLS int g_kernel_dispatch_slot = -1;` plus local's comment). |
   | 4 | `xbox_memory_layout.c` (2123–2145) | **HA** | **LOCAL** (existing `A4s` pre-ruling, `docs/reviews/a4s-ac97-hunk-ruling.md` (b)): keep local's `VirtualProtect`-failure `else`; upstream's `\|= MCPX_AC97_CODEC_READY` and `ac97_arm_write_trap()` call are HA omissions. |
   | 5 | `lifter.py` (400–412) | **H2** (edit application) | Local deletes the base `"lock xadd"` line; upstream keeps it (not an edit) and inserts 6 comment lines + `"popfd",` after it → apply the deletion, then upstream's insertions. Result must match **Expected-5** (Appendix A): no `"lock xadd"`; `"popfd",` present; exactly 6 comment lines. |
   | 6 | `lifter.py` (426–437) | **H2** (edit application) | Upstream replaces base line 1 `"pushfd", "popfd", "pushal",` with `"pushfd", "pushal",` + 2 comment lines; local replaces base line 2 `"sgdt", "ljmp", "sfence",` with `"sgdt", "ljmp", "sfence", "wbinvd",`. Disjoint → apply both edits. Result must match **Expected-6** (Appendix A). |
   | 7 | `lifter.py` (1276–1297) | **H2** (pure insertion) | Base section empty; both sides only inserted at the same point → keep both, **local's lines first, then upstream's** (local's `batch_spans`/`_batch_span_starts` block, then upstream's `imm_code_refs` block). |
   | 8 | `test_icall_feedback.py` (116–124) | **HA-LOCAL** | **LOCAL**, pinned by the Advisor: `assert main(["--db", db, "--functions", fns, "seeds", "--out", out, "--align", "16"]) == 0`. (The surrounding non-conflicted `fns` setup lines exist only because local added them.) |
   | 9 | `translator.py` (987–994) | **HA-UNION** | Full union, local's members then upstream's, keeping upstream's added clause; result must match **Expected-9** (Appendix A) exactly. |

   #### Merge rules (repaired — the operative H-rule text)

   Apply, in order, `HA`, `HA-COMBINED`, `H1`, `H2`, `H3`, else `UNDECIDED`. Every rule represents **deletions as edits**: a deletion is work, and no rule may treat *"this side added zero lines"* as proof its work is contained or absent.

   - **HA — pre-ruled dispositions.** Hunks 4 (LOCAL, AC'97) and 8 (LOCAL) and 9 (full union) and 1–2 (COMBINED) are pre-ruled by the Advisor; apply them exactly as the table and Appendix A state. HA is pre-ruling, not a comparison. Any AC'97/`MCPX` hunk HA does not describe falls through.
   - **H1 — containment (repaired to include deletions).** For side *X* against the hunk's base section: `added(X)` = non-blank trimmed lines in *X* minus those in base; `deleted(X)` = non-blank trimmed lines in base minus those in *X*. **X contains Y iff `added(Y) ⊆ added(X)` AND `deleted(Y) ⊆ deleted(X)`** (multiset). If the two versions are identical, take either. If exactly one direction holds, take the containing side. If neither holds, H1 does not apply → fall through. A side that deleted a base line the other kept is **not** contained in the other.
   - **H2 — disjoint edit union (action = EDIT APPLICATION; never a union of line presence).** *Precondition (original, unchanged):* no base line is changed by **both** sides, where *changed* means **deleted or replaced** ("kept unchanged" is **not** an edit). *Action:* start from the base; apply each side's per-line edit (keep / delete / replace); then insert each side's insertions (**local first at a shared insert point**). Because the precondition guarantees at most one side edited any base line, apply whichever side actually edited it; a line one side deletes and the other keeps is **deleted** (keep is not an edit). **A union by line presence is forbidden** — it would resurrect deleted lines. *Ambiguity:* if the per-line attribution is ambiguous (repeated identical base lines, or several equal-cost alignments), H2 does **not** apply → the hunk is **UNDECIDED** → `R-CONFLICT`.
   - **H3 — version/comment only.** The only base lines changed by both sides are a version string or comment text → take upstream for those lines, apply H2 to the rest. **H3 is not audited** for the deletion class (no H3-classified hunk exists in this merge, so there is no corpus); recorded as a standing limit, per the Advisor.
   - **Otherwise UNDECIDED.** Do not choose a side, rewrite, or merge semantics by judgment. On the recorded 9-hunk set every hunk is pre-ruled above, so a hunk reaching UNDECIDED means the conflict set diverged from the record → roll back, `R-CONFLICT`.
   - **Accepted-content rule:** no resolution may remove or alter a line added by `c97ce2c` (`src/kernel/xbox_memory_layout.c`); if a rule cannot keep them all, the hunk is UNDECIDED.

   #### HA-COMBINED edits outside the conflict hunks (pre-ruled; locate by exact text, never by line number)

   - **(a)** Delete upstream's clean-hunk `bridge_KeResetEvent`: the `/* --- KeResetEvent (ordinal 138 …) --- */` comment through the closing brace of that function (merge-tree `75083476` lines 6733–6746, orientation only).
   - **(b)** Keep exactly **one** `case 138` per dispatch switch — keep local's two positions (`75083476` 8040 and 8502); delete the upstream duplicates `case 138: return  4;  /* KeResetEvent (1) */` and `case 138: return bridge_KeResetEvent;` (`75083476` 8343 and 8868). Both switches stay consistent: one `138` each, args value matching the single surviving bridge's arity.

   After resolving: `git -C $T diff --check` clean; `Select-String -Path <each of the 10> -Pattern '^(<<<<<<<|\|\|\|\|\|\|\||=======|>>>>>>>)( |$)'` returns nothing; commit with a message file: `git -C $T commit -F <msgfile>` (message: `Merge upstream v0.11.0 (766ecef) into local main for A4s; generated tree not regenerated`). Record the merge commit SHA `M`. Then run **`AC-STRUCT`** (step 4) **before** the build.
   - **Rule-(d) inventory** (`AC-INV`): while resolving, and again over the whole merge (clean hunks included), record the inventory `AC-INV` defines.
4. **Structural post-resolution check (`AC-STRUCT`)** on the resolved `M`, before the build. See the criterion. A FAIL here selects `R-CONFLICT` (structural), never `R-BUILD`.
5. **Build (no regeneration).** `python -X utf8 scripts\build-jsrf.py` (no `--allow-regeneration`; if it dies silently at `Checking File Globs`, rerun with `--parallel 1` per `AGENTS.md`). Record exe SHA-256 and the build log. **Row-attribution backstop:** if the build fails, `R-BUILD` may be selected **only if** the first compiler error is **not** a redefinition or duplicate-case diagnostic (`C2084` / `C2196` / `C2371` class) in a merge-changed file; if the first error is one of those in a merge-changed file, select `R-CONFLICT` (structural) instead.
6. **Tests.** Run `AC-TEST`'s sets on `M`.
7. **Strict run** (game root, one run, 30 s):
   ```powershell
   Remove-Item Env:\RECOMP_APU_DSP_ACK,Env:\RECOMP_AC97_READY,Env:\JSRF_ALLOW_UNRESOLVED,Env:\JSRF_ABI_CONTINUE,Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE -ErrorAction SilentlyContinue
   $env:RECOMP_GPU_ACK='0'; $env:RECOMP_KERNEL_LOG_BUDGET='100000'
   python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4s-sync-strict
   ```
   Then `AC-RUN`'s reads (including `AC-KEEP` (vi)'s `[A3A]` witness — no separate run). A rerun is permitted **only** where a row says so, with identical settings and label `a4s-sync-strict-rerun`, and **at most one rerun total** across all rows.
8. **End-of-execution checks** (`AC-GEN`, `AC-NOPUSH`) and the evidence file. Execution ends here; the push to `origin` is **not** an execution step (see Closure).

- **Write scope:** toolkit — the merge commit on `main` and local branches `a4s-pre-sync` / `a4s-merge-attempt` only (no remote writes during execution); game — `docs/reviews/a4s-execution-evidence.md`, `logs/a4s/`, `logs/runs/`, and the build tree. Nothing else. (`scripts/check-merge-structure.py` and `tests/test_merge_structure.py` already exist at their pinned hashes; this packet does not author them.)
- **Build/run owner:** the Session, single owner.

### AC-REC — the pre-merge state is recoverable
- Mandatory: yes. Guards against: losing the 24 unpushed local commits or being unable to return to the accepted baseline.
- Procedure: after step 1, `git -C $T rev-parse a4s-pre-sync` = `0d7929c86771dd0b971941592fd4f15436116e82`; at closure the branch still exists and resolves to that SHA.
- PASS: both reads equal. FAIL: either differs. UNKNOWN: branch absent. Controls: waived — the oracle is `git` itself.

### AC-STRUCT — the resolved tree has no merge-introduced structural defects over `SCOPE`
- Mandatory: yes. Guards against: a **clean-hunk** structural defect (duplicate `case`, duplicate file-scope definition, leftover conflict marker) being silently attributed to the build (`R-BUILD`) instead of the resolution (`R-CONFLICT`) — the §3.1 misattribution the Advisor ruled against.
- Evidence profile: manual (static source check on the resolved tree).
- Pinned tools (P0.10 verifies the scanner bytes before use): `scripts/check-merge-structure.py` SHA-256 `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF`; its fixtures `tests/test_merge_structure.py` SHA-256 `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C` (14 tests, all pass — the scanner's known-good/known-bad control). The scanner is textual and does **not** preprocess: it strips comments and string literals **preserving line count**, assigns each line a preprocessor branch path, treats each `#if` as an opaque branch, and compares only within the same branch path. A file whose braces or switch braces it cannot balance is reported **UNKNOWN**, never silently skipped, and any UNKNOWN fails the run closed.
- Procedure (over `SCOPE` = toolkit `src/`, `include/`, `.c`/`.h`):
  - **On the resolved `M`:** `python -X utf8 scripts\check-merge-structure.py --rev M --expect 0`. Record files/switches/findings/UNKNOWN and every finding.
  - **Positive control (resolved all-ours preview of `75083476`):** construct the conflict-free **resolved** preview with every conflict hunk taken to **ours** (write it to a scratch tree outside both repos, or a throwaway ref, using the recorded `logs/a4s/conflicted-*` for fidelity; do **not** touch `main`), then scan it. It must report **exactly 3 structural findings and 0 markers**: `[dup-case] src/kernel/kernel_bridge.c case 138` in **both** dispatch switches, and `[dup-def] src/kernel/kernel_bridge.c bridge_KeResetEvent`. **The control is named against the all-ours resolution** (all-theirs yields 2, because local's `bridge_KeResetEvent` lives inside hunk 1). Measured all-ours line positions `[8019, 8322]`, `[8481, 8847]`, `[1377, 6713]`; on the raw tree they are `[8040, 8343]`, `[8502, 8868]`, `[1378, 6734]` — line numbers shift when markers are removed (`docs/reviews/a4s-r6-ac-struct-controls.md`).
  - **Negative controls:** `python -X utf8 scripts\check-merge-structure.py --rev 0d7929c --expect 0` and `--rev 766ecef --expect 0`; each must exit 0 with 0 findings, 0 UNKNOWN (measured: `0d7929c` 87 files/109 switches; `766ecef` 87 files/111 switches).
  - **Separate expected observation (not a defect):** the **raw** `75083476` preview reports **12 conflict-marker lines** plus the 3 structural findings (15 total; `--rev 75083476 --expect 15` exits 1) — markers are expected on an unresolved tree and are **not** the property under test; the resolved-tree scans must report 0 markers.
  - **Fixture control:** `python -X utf8 tests\test_merge_structure.py` → `Ran 14 tests … OK`.
- PASS: on the resolved `M`, zero findings **and** zero UNKNOWN (exit 0 with `--expect 0`); **and** the positive control reports exactly the 3 structural findings with 0 markers; **and** both negative controls exit 0 with 0 findings/0 UNKNOWN; **and** the fixtures pass; **and** the scanner's file/switch counts are reported (coverage witness).
- FAIL: any finding on the resolved `M`, or a control mismatch (positive control ≠ 3 structural, a negative control ≠ 0, or a fixture failure). → `R-CONFLICT` (reason "structural"), **never `R-BUILD`**.
- UNKNOWN: the scanner reports UNKNOWN for any merge-changed file, cannot assign braces/branch paths in a changed file, or its bytes differ from the pinned SHA-256 → that file is listed, the check does **not** PASS → roll back, `R-CONFLICT` (fail closed).
- Backstop (cannot be bypassed by attribution): see step 5 — `R-BUILD` may be selected only if the first compiler error is **not** a `C2084`/`C2196`/`C2371`-class redefinition/duplicate-case diagnostic in a merge-changed file.
- Claim limits: textual, non-preprocessing — it cannot resolve `#include` graphs, macro-expanded code, or `#if` conditions that depend on defined values, and misses a duplicate the preprocessor would produce from two *different* branch paths. Deliberate exclusions (each measured on the real merge): a declaration plus one definition; a tentative definition completed later (C11 6.9.2p2); an identical macro redefinition (C11 6.10.3p3); constructs on different branch paths. The `R-BUILD` backstop exists precisely for the compile class it cannot express; the scanner certifies only the three listed properties over `SCOPE`, and the fixtures prove it finds the two measured defect classes without firing on the four measured legal constructs.

### AC-MERGE — both sides are in the result, and nothing else changed
- Mandatory: yes. Guards against: a merge that silently drops upstream work or local (unpushed) work, or a resolution that invents content or resurrects a deleted line.
- Procedure (toolkit): (a) `git -C $T rev-parse M^1 M^2` = `0d7929c…`, `766ecef…`; (b) `git -C $T diff --name-only 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b M` ⊆ the 38 files in `git -C $T diff --name-only 051a128 0d7929c`; (c) `git -C $T diff --name-only 0d7929c M` ⊆ the 153 files in `git -C $T diff --name-only 051a128 766ecef`; (d) **presence witness** for each of the 10 files: every non-blank, whitespace-trimmed `+` line of `git -C $T diff 051a128 0d7929c -- <f>` and of `git -C $T diff 051a128 766ecef -- <f>` occurs (trimmed) in `git -C $T show M:<f>`, except lines recorded as HA or H3 omissions; **(d-twin) deletion witness (Advisor-ruled):** for each of the 10 files, every base line that a **credited** side **deleted** (a `-` line of that side's `git -C $T diff 051a128 <credited-side> -- <f>`) is **ABSENT** from `git -C $T show M:<f>`, **unless the other side replaced it** (its replacement text is present instead) or the line is restored by an HA pre-ruling that is recorded (e.g. HA keeps local lines upstream deleted); (e) the step-3 resolution table lists every conflicted hunk with a rule in {HA, HA-COMBINED, HA-LOCAL, HA-UNION, H1, H2, H3}.
- PASS: (a)–(e) all hold. FAIL: any fails — including (d-twin), i.e. a line a credited side deleted is present in `M` without a replacement or a recorded HA restoration. UNKNOWN: any list or command output missing.
- Controls: (b)/(c) known-bad — the same commands on `0d7929c` itself give a non-empty (b) excess, proving the check can fail. Claim limits: textual presence/absence, not semantic correctness of the combined code.

### AC-KEEP — accepted local work survives
- Mandatory: yes. Guards against: reopening `A3a-r25` (AC'97 model, `c97ce2c`) or `A4a-r2` (trace fix, `0d7929c`).
- Procedure and checks (i)–(vi) are **unchanged from `A4s-r5`**: the four-file byte-equality diff; the `c97ce2c` `+`-line presence and `merge-base --is-ancestor` checks; **(iv)** the three start-anchored fixed-length extractions (7/56/25) compared byte-for-byte on the `HA`-resolved `M` with anchor-uniqueness counts (1/1/1) and a shifted-start can-fail control; the **`SCOPE` boundary guard** (fail closed); **(v)** the deleted-name and code-token greps in semantic form (quoted literals; comment hits recorded, not failed; out-of-scope hits recorded, not failed) with the four deleted-name controls (`766ecef` 2, `051a128` 2, `0d7929c` 0, `75083476` 0); **(vi)** the `[A3A] ac97 witness` behavioural read on the one strict run (GC bit1 and GS bit8 both set), evaluated after the run, a failure selecting `R-CONFLICT` ahead of the run rows. See `A4s-r5` (`docs/packets/a4s-toolkit-sync.md`) lines 79–92 for the full text; no part is relaxed.
- PASS / FAIL / UNKNOWN and controls: as in `A4s-r5`. (iv)/(v) are evaluated before the build; (vi) after the run.

### AC-INV — rule-(d) inventory and merged-tree grep
- **Unchanged from `A4s-r5`** (the complete pre-ruled table does **not** exempt the tree from run-profiles rules 1–5; the by-content inventory still covers **clean** hunks): names searched (classifier-listed, deleted/retired, admitted-models table); triggers T1/T2; the aperture test; the call-site ("dormant") test; the dispositions ("local admitted form kept", "admitted model text unchanged", "dormant", "host mechanism, not device-aperture", "out of scope", "comment mention", "new unclassified variable", FAIL); the classifier-listed-names grep (2/2 control); the **new-environment-names** step with its Advisor-authorized `-f` pattern-file substitute and three controls (`docs/reviews/a4s-r5-command-defect.md`); the worked examples; and the `SCOPE` boundary guard. All within `SCOPE`; hits outside are inventoried as "out of scope" and never FAIL. Expected new names: `RECOMP_APU_MIXDOWN_ALL`, `RECOMP_USB_PORT`, none removed.
- PASS / FAIL / UNKNOWN / controls: as in `A4s-r5` (FAIL → roll back, `R-CONFLICT`). Evaluated with `AC-MERGE`/`AC-KEEP`, before the build; re-run on an amended `M` (D1).

### AC-GEN — the generated tree was not regenerated
- **Unchanged from `A4s-r5`.** Guards against: a silent regeneration that would mix generator provenance, or a claim that the generated tree reflects the merged lifter.
- Procedure (game root, at closure): `git diff --quiet HEAD -- src/recomp config tools/disasm/output` (exit 0) and `git status --porcelain -- src/recomp config` empty; `python -X utf8 scripts\check-generation-provenance.py --check` reports ok with no problems/unknowns.
- PASS: all hold. FAIL: any file differs or the checker reports a problem. UNKNOWN: the checker cannot run. Controls: waived — existing tested tool. Claim limits: **PASS means the generated bytes are the pre-merge generator's; it is evidence they are stale relative to `M`'s lifter, not that they are correct.**

### AC-BUILD — the merged toolkit builds with the unchanged game
- Mandatory: yes. Procedure: step 5. PASS: `build-jsrf.py` exits 0 and `build\Release\jsrf_recomp.exe` exists with a recorded SHA-256 (file present). FAIL: build error → `R-REGEN` if it meets the Stop-if regeneration test, else the **`AC-STRUCT` backstop** decides: if the first compiler error is a `C2084`/`C2196`/`C2371`-class redefinition/duplicate-case diagnostic in a merge-changed file, select `R-CONFLICT` (structural); otherwise `R-BUILD`. Guards against: claiming a sync that does not compile.

### AC-TEST — both repositories' tests
- Mandatory: yes. Guards against: a resolution that breaks what this project relies on (baseline sets) or breaks upstream's own changed code (post-merge additions).
- **Decision on "both repositories' tests": both the baseline set and the post-merge set** (as in `A4s-r5`). Upstream's C test directories and `tools/conformance` are out of scope. If the game's CTest discovers additional tests at `M`, they are recorded and must pass under (i); their existence/absence never selects `R-REGEN`.
- Sets:
  - **C (game ctest):** `ctest --test-dir build -C Release -N` then `ctest --test-dir build -C Release --output-on-failure`. Baseline = the 12 names in step 2. **Named witness for the hunk-1/2 ruling:** `jsrf_inplace_event_bridge` (from local-only `tests/kernel_inplace_event_test.c`, `CMakeLists.txt:118-125`) asserts the guest `KEVENT.SignalState` writes that only the in-place tier performs; it must be present and pass.
  - **G (game Python):** from game root, each file separately: `Get-ChildItem tests\test_*.py | ForEach-Object { python -X utf8 $_.FullName }`. **At this revision set G has 11 files** (it gained `tests/test_merge_structure.py` with the AC-STRUCT tooling).
  - **K4 (toolkit baseline):** from `$T`, `$env:PYTHONPATH='C:\Users\logic\AppData\Roaming\Python\Python313\site-packages'; C:\Python313\python.exe -m unittest tools.recomp.test_lifter_atomics tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry tools.recomp.test_seh_frame_owner`.
  - **KX (toolkit, conflict-adjacent):** same environment, each module separately: every `tools\recomp\test_*.py` and `tools\disasm\test_*.py` present at that moment, as `C:\Python313\python.exe -m unittest tools.<dir>.<module>`. **Pre-merge control** re-recorded at step 2 (was 33 modules / `Ran 133 … OK` at `0d7929c`); if step 2 differs, use the step-2 result as the control.
- **Set-G carve-out (new, modelled on the KX carve-out; Advisor-recommended):** a game-Python test file that **fails in the step-2 pre-merge run** and **fails at `M` with the same failing test identity** is recorded as **pre-existing**, not a `FAIL` — **named by test identity**, **re-derived from each execution's own step-2 run, and never carried forward**. With the current tree the carve-out set is **`tests/test_ac2_provenance.py` only** (E2), and set G is expected at **10 pass / 1 fail over 11 files**. A game-Python file that passed in step 2 and fails at `M`, or a new file that fails, is a `FAIL`. **No count is hard-coded:** the baseline is whatever step 2 measures (it moved 8/2 → 9/1 → 10/1 in one session), and the carve-out follows the failing test's identity, not a number.
- PASS: (i) C discovers all 12 baseline names and every discovered test passes (including `jsrf_inplace_event_bridge`); (ii) K4 all pass, and G all pass **except** the step-2-derived carve-out set above; (iii) every KX module passes, except a module that also failed in step 2 with the same failing test names (pre-existing, recorded); a module **new** in `M` must pass.
- FAIL: any test in (i) fails; a K4 test fails; a G test outside the carve-out set fails; a KX module that passed in step 2 fails; a new KX module fails; a baseline ctest name is missing → `R-TEST`.
- UNKNOWN: a set cannot be run (environment, not code — e.g. `capstone` missing, confined `tempfile` denial) → record; one retry after fixing only the environment; persists → `R-TEST`.
- Controls: the step-2 pre-merge run is the known-good control for every set; the set-G carve-out is derived from that same step-2 run. Claim limits: green tests do not establish lifter correctness on JSRF or match the generated tree.

### AC-NOPUSH — nothing was pushed during execution, and `upstream` is untouched
- **Unchanged from `A4s-r5`.** Guards against: publishing unreviewed work to the fork before acceptance, or any write to `upstream`, against the owner's push policy.
- Procedure (step 8 — before acceptance review): `git -C $T rev-parse origin/main` = `766ecef…`; `git -C $T rev-parse upstream/main` = `766ecef…`; `git -C $T rev-parse --abbrev-ref "@{u}"` = `upstream/main`; `git -C $T status -sb` first line shows `main...upstream/main [ahead 25]` with no `behind` on a success path, or `[ahead 24, behind 169]` on a rollback path; `git -C $T remote -v` equals the four P0 lines exactly; game `git remote` prints nothing; the Session attests no `git push` or `git fetch` was run during execution.
- PASS: all hold. FAIL: `origin/main` or `upstream/main` ≠ `766ecef`, a remote line changed, a game remote exists, or a push/fetch attested → `R-PUSH`. UNKNOWN: a ref cannot be read → `R-PUSH` (fail closed). Controls: waived — git ref state is the oracle.
- Claim limits: a remote-tracking ref shows what this clone last saw; the Session attestation covers the rest. The Closure push is deliberately outside this criterion.

### AC-RUN — where the strict stop is (selects the row; not a pass/fail)
- **Unchanged from `A4s-r5`.** Mandatory: yes. Evidence profile: strict. Reads for run `<run>` (A4a's `B`/`W`/`F` definitions — `W` read at `B+0x810`, never a fixed address): **V** (validity) = `check-run-profile.py` prints `STRICT`, `result.json` readable, `check-dump-mapping.py` gives `matches ≥ 1`, `content-mismatch: 0`; `outcome` from `result.json`; `B` = `MEM32(0x001BA858)` (usable iff readable and `≠ 0`); `W` = `MEM32(B+0x810)` read only when `B` is usable; `F` = the `stacks.txt` `sub_001A1769` frame count on `recomp_0005.c:67(48|49|50|51)` (control: `F = 2` on the reference run); **A3A** (`AC-KEEP` (vi), same run) = the `[A3A] ac97 witness` line with GC bit1 and GS bit8 both set; record SHA-256 of `result.json`, `stacks.txt`, `jsrf_run.log`, `process.dmp`; if `outcome ≠ diagnostic_deadline`, record the first failure and the top live guest frame of each thread. See `A4s-r5` (`docs/packets/a4s-toolkit-sync.md`) lines 154–163.

### Decision rows
Evaluate in order; the first match wins. Gate rows (1–6) are checked before the run is interpreted; the run is launched only if rows 2–6 do not match, and `R-PUSH` is evaluated again at step 8 (`AC-NOPUSH`) regardless of which later row the run selects. **"Expected first result" (`R-SAME`) is a forecast only; no criterion assumes it.**

| ID | Condition | Means | Toolkit `main` left at | Next packet |
|---|---|---|---|---|
| **R-PUSH** | P0 passed, and `AC-NOPUSH` FAIL or UNKNOWN | owner push policy breached (or unverifiable) during execution | as is — do not attempt remote repair, do not force-push | **stop all work**; owner via Advisor (§3.4). **`R-CONFLICT`/rollback/INADEQUATE are no-push states** (owner policy, `docs/reviews/owner-push-policy-xboxrecomp-fork.md`) |
| **R-CONFLICT** | P0 passes and: a conflict outside the 10 files; the merge conflict set differs from the 9 recorded hunks; a recorded hunk's text differs from `logs/a4s/conflict-hunk-inventory.txt`; an attribution-ambiguous (UNDECIDED) hunk under the repaired rules; or `AC-STRUCT`/`AC-MERGE`/`AC-KEEP` (incl. (iv)–(vi))/`AC-INV` FAIL; **or the `AC-BUILD` backstop attributes the first compiler error to a `C2084`/`C2196`/`C2371` redefinition/duplicate-case in a merge-changed file** | the merge cannot be completed by the rules / a structural defect was introduced | `0d7929c` (rollback) | Planner `A4s-r7` with the failing hunk/finding as its brief; side-choosing hunks go to the Advisor first |
| **R-REGEN** | build failure meeting the Stop-if regeneration test, or `AC-GEN` FAIL | a build error in generated/recovered code, or a changed generated tree; regeneration is **one candidate** cause | `0d7929c` (rollback; attempt at `a4s-merge-attempt`) | owner via Advisor: a regeneration or API-compatibility decision, as a **separate packet** |
| **R-BUILD** | `AC-BUILD` FAIL not matching R-REGEN, **and** the first compiler error is **not** a `C2084`/`C2196`/`C2371`-class redefinition/duplicate-case in a merge-changed file (else `R-CONFLICT`) | merged runtime does not build with the unchanged game (non-structural) | `0d7929c` (rollback) | Planner `A4s-r7`, build log as brief |
| **R-TEST** | `AC-TEST` FAIL, or UNKNOWN persisting after one environment retry | a resolution or upstream change breaks a relied-on or upstream test | `0d7929c` (rollback) | Planner `A4s-r7`, failing tests as brief |
| **R-PRE** | P0 fails (including P0.10 scanner-hash mismatch) | baseline not as stated | untouched | Session reports; Planner re-checks premise |
| **R-INVALID** | V fails, or `result.json` unreadable | run not interpretable | `M` | rerun once identically (**at most one rerun total**); still → Planner `A4s-r7` with the failing gate |
| **R-MOVED** | V holds and a **positive** observation, either: **(i)** `outcome ≠ diagnostic_deadline`; or **(ii)** all of `outcome = diagnostic_deadline`, `B` usable **and** `B = 0x803C0000`, `W` readable, `W ≠ 3`, **and** `F = 0` | the strict stop moved (one run; direction recorded, not interpreted). **Recording guidance (D3):** also record whether any thread's top live guest frame or `jsrf_run.log`'s last guest location is at or near the DSOUND AC'97 RR-wait **`0x1A6F88`** | `M` | **the next brief** (owner): Session writes an evidence brief from this run → Planner designs the next packet; `A4b1`/`A4b2` stay parked |
| **R-SAME** | V holds; `outcome = diagnostic_deadline`; `B` usable and `B = 0x803C0000`; `W = 3`; `F ≥ 1` | stop unchanged: still the DSP pending-word spin | `M` | Planner revises **`A4b1` → `A4b1-r2`** (PREMISE_CHANGED, §5.4(2)): new baseline = toolkit `M`, this exe SHA, this run as the reference R0, and upstream's `src/apu/apu_dsp.c`/`CMakeLists.txt` as the starting state; then `A4b2` |
| **R-UNKNOWN** | none of the above. With `outcome = diagnostic_deadline` this includes: `B` not usable, or `B ≠ 0x803C0000`; `W` unread or unreadable; `W ≠ 3` with `F ≥ 1`; `W = 3` with `F = 0` | not decided | `M` | rerun once identically — **at most one rerun total**; still R-INVALID/R-UNKNOWN → Planner `A4s-r7` with `B`, `W`, `F` as its brief |

### Closure
- **Evidence index** in `docs/reviews/a4s-execution-evidence.md`: P0 outputs (incl. P0.10 scanner hash); `a4s-pre-sync` SHA; pre-merge exe SHA and step-2 per-test results (incl. the re-measured set-G baseline and the derived carve-out set); the conflict-set guard result (the merge's conflict set vs the 9 recorded); the per-hunk resolution table (file, lines, rule, omissions); `M` and its parents; `AC-STRUCT` outputs (resolved-`M` counts/findings, positive and negative controls, the raw-preview marker observation, fixture result); `AC-MERGE` (b)–(d) and (d-twin) outputs; `AC-KEEP` (i)–(vi) outputs; `SCOPE` boundary-guard output; deleted-name/classifier-name control counts; `AC-INV` inventory table and grep outputs; the new-variable set difference; build log path/hash and post-merge exe SHA-256 (and whether a relink occurred, per `docs/reviews/a4s-r5-rollback-exe-hash.md`); `ctest -N` count and names; per-set, per-module test results; `AC-GEN` outputs; P0 and `AC-NOPUSH` remote/ref outputs; run directory and artifact hashes; `outcome`, `B`, `W`, `F`; the selected row.
- Every row that leaves toolkit `main` at `0d7929c` also records the rollback rebuild's exe SHA-256.
- **Closure push (owner policy; after ACCEPT only, not part of what the reviewer checks).** Per `docs/reviews/owner-push-policy-xboxrecomp-fork.md`, pushing the fork is normal durable closure for a completed merge packet. If and only if acceptance returns `ACCEPT` and toolkit `main` = `M` (rows `R-INVALID`, `R-MOVED`, `R-SAME`, `R-UNKNOWN` — every row reached only after `AC-REC`, `AC-MERGE`, `AC-KEEP`, `AC-STRUCT`, `AC-GEN`, `AC-BUILD`, `AC-TEST` and `AC-NOPUSH` passed): **verify all five pre-push checks** — the toolkit tree is **clean**; the commit is the **intended durable state**; the **active packet's tests/acceptance passed**; the destination is **the fork, not `upstream`**; and record the local SHA and branch — then from the toolkit root run exactly `git -C $T push -u origin main`, confirm `git -C $T rev-parse origin/main` = `M` and `git -C $T ls-remote origin refs/heads/main` prints `M`, and record `PUSHED_TO: / BRANCH: / COMMIT: / REMOTE_URL: / RESULT:`. No `--force`/`--force-with-lease`, no other refspec, no push to `upstream` (never). The push is a fast-forward (`M` descends from `origin/main` = `766ecef`); if not, or it is rejected, do not force — record and escalate to the owner via the Advisor. `-u` moves `main`'s tracking from `upstream/main` to `origin/main`; recorded, not reverted. **`R-CONFLICT`, a rollback, and `INADEQUATE` are no-push states**: for any accepted row that rolled `main` back to `0d7929c` (not a descendant of `origin/main`), **do not push** — record "no push: main at 0d7929c".
- Post-review edits reopen affected criteria (the pinned scanner's and fixtures' bytes among them). An unrelated next stop is recorded as follow-up; do not expand scope.

### Appendix A — Advisor-ruled verbatim text (the merge oracle)

**HA-COMBINED-1** and **HA-COMBINED-2** are the exact `bridge_KeSetEvent` / `bridge_KeResetEvent` / `bridge_KeWaitForSingleObject` bodies of `docs/reviews/a4s-r6-advisor-hunk-ruling.md` §1 (lines 29–95), reproduced here verbatim so the executor applies one self-contained text. **Expected-5**, **Expected-6** and **Expected-9** are the ruled outputs the executor's result must match. Where this packet and the ruling differ, the ruling is authoritative.

```c
/* HA-COMBINED-1: hunk 1 (KeSetEvent) and the single surviving KeResetEvent. */
static void bridge_KeSetEvent(void)
{
    uint32_t event_ptr = STACK_ARG(0);
    uint32_t increment = STACK_ARG(1);
    uint32_t wait = STACK_ARG(2);
    HANDLE h;

    recomp_diag_record(10, event_ptr, g_xbox_kernel_caller, 0);
    if (guest_va_is_inplace_kevent(event_ptr)) {
        g_eax = (uint32_t)xbox_KeSetInplaceEvent(
            event_ptr, XBOX_TO_NATIVE(event_ptr), BRIDGE_MEM8(event_ptr),
            increment, (BOOLEAN)wait);
    } else if ((h = ke_shadow_lookup(event_ptr)) != NULL) {
        g_eax = (uint32_t)xbox_KeSetEvent(h, increment, (BOOLEAN)wait);
    } else {
        g_eax = (uint32_t)xbox_KeSetEvent(
            XBOX_TO_NATIVE(event_ptr), increment, (BOOLEAN)wait);
    }
    recomp_diag_record(10, event_ptr, g_xbox_kernel_caller, g_eax);
}

static void bridge_KeResetEvent(void)
{
    uint32_t event_ptr = STACK_ARG(0);
    HANDLE h;

    recomp_diag_record(11, event_ptr, g_xbox_kernel_caller, 0);
    if (guest_va_is_inplace_kevent(event_ptr)) {
        g_eax = (uint32_t)xbox_KeResetInplaceEvent(
            event_ptr, XBOX_TO_NATIVE(event_ptr), BRIDGE_MEM8(event_ptr));
    } else if ((h = ke_shadow_lookup(event_ptr)) != NULL) {
        g_eax = (uint32_t)xbox_KeResetEvent(h);
    } else {
        g_eax = (uint32_t)xbox_KeResetEvent(XBOX_TO_NATIVE(event_ptr));
    }
    recomp_diag_record(11, event_ptr, g_xbox_kernel_caller, g_eax);
}

/* HA-COMBINED-2: hunk 2 (KeWaitForSingleObject, ordinal 159). */
static void bridge_KeWaitForSingleObject(void)
{
    uint32_t diag_object = STACK_ARG(0);
    recomp_diag_record(8, diag_object, g_xbox_kernel_caller, 0);
    uint32_t object = STACK_ARG(0);
    uint32_t wait_reason = STACK_ARG(1);
    uint32_t wait_mode = STACK_ARG(2);
    uint32_t alertable = STACK_ARG(3);
    uint32_t timeout_ptr = STACK_ARG(4);
    HANDLE h;

    if (guest_va_is_inplace_kevent(object)) {
        g_eax = (uint32_t)xbox_KeWaitInplaceEvent(
            object, XBOX_TO_NATIVE(object), BRIDGE_MEM8(object),
            (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
    } else if ((h = ke_shadow_lookup(object)) != NULL) {
        g_eax = (uint32_t)xbox_KeWaitForSingleObject(
            h, wait_reason, wait_mode,
            (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
    } else {
        g_eax = (uint32_t)xbox_KeWaitForSingleObject(
            XBOX_TO_NATIVE(object), wait_reason, wait_mode,
            (BOOLEAN)alertable, XBOX_TO_NATIVE(timeout_ptr));
    }

    recomp_diag_record(9, diag_object, g_xbox_kernel_caller, g_eax);
}
```

```python
# Expected-5: lifter.py `_FLAGS_UNDEFINED` after hunk 5 (edit application).
_FLAGS_UNDEFINED = frozenset({
    "mul", "div", "idiv",  # Flags partially undefined
    "rdtsc", "cpuid",      # Special instructions
    # popfd REPLACES every flag with whatever was pushed. Its flags are not
    # architecturally undefined -- they are simply not knowable from the
    # instruction stream -- but the tracking action is the same: whatever the
    # last comparison left is gone, and a jcc after it must not be resolved
    # from that comparison. It used to sit in _EFLAGS_PRESERVE, next to
    # pushfd, which does read-and-preserve and does belong there.
    "popfd",
})
# `_EFLAGS_SETTERS` keeps local's `"xadd", "lock xadd", "lock cmpxchg",` line
# (already the merged text; "lock xadd" does NOT appear in _FLAGS_UNDEFINED).

# Expected-6: lifter.py `_EFLAGS_PRESERVE` after hunk 6 (edit application).
_EFLAGS_PRESERVE = frozenset({
    # pushfd READS the flags and leaves them alone, so it belongs here.
    # popfd does NOT -- see _FLAGS_UNDEFINED.
    "pushfd", "pushal",
    "sgdt", "ljmp", "sfence", "wbinvd",
})

# Expected-9: translator.py `_fa` declaration guard after hunk 9 (full union).
        if any(insn.mnemonic in ("cmp", "test", "bsf", "bsr", "cmpxchg",
                                 "lock cmpxchg", "xadd", "lock xadd", "inc", "dec")
               or insn.mnemonic in _RESULT_SNAPSHOT_SETTERS
               for insn in instructions):
```

### Follow-up leads (recorded, not executed — not `A4s-r6` obligations)
- The `Size` convention (4 vs upstream's 16/40): needs a primary source.
- Tier 3's handle-typed treatment of a non-handle Ke* object (pre-existing type confusion in base and local).
- `ke_shadow_remove` has no callers, so shadow entries are never retired.
- The upstream timer model's observable behaviour for JSRF (ordinals 113/149 reachability witness).
- An indirect-thunk-call search behind "not declared ⇒ unreachable".
- Case 207.
- Prior leads carry forward unchanged: `MIXDOWN_ALL`/`USB_PORT` classification, E2, the function-style KX modules, and the exe-relink advisory.

### Planner adequacy self-check (§5.3 form; the binding review is by a FRESH Planner, §5.1.5)

```text
REVISION:          A4s-r6 draft (SHA-256 reported to the Session on return). r6 resolves the 5
                   UNDECIDED hunks via the binding Advisor ruling (hunks 1-2 HA-COMBINED exact text +
                   HA edits (a)/(b); 5 H2 edit-application; 8 HA-LOCAL; 9 HA-UNION), repairs H1 (deletions
                   in containment) and H2 (edit application, not presence-union), adds AC-STRUCT (pinned
                   tracked scanner A1FDCE26… + fixtures A42F4A1E…, positive/negative controls, R-CONFLICT
                   row + C2084/C2196/C2371 R-BUILD backstop), AC-MERGE (d-twin) deletion witness, and the
                   AC-TEST set-G carve-out (E2 by identity, re-derived per execution, no count hard-coded).
                   Carries over SCOPE guard, AC-KEEP (iv)-(vi), AC-INV, AC-GEN, AC-BUILD, AC-NOPUSH, AC-RUN,
                   rows, stop predicate, rerun cap, claim limits. Folds in the owner push policy (5
                   pre-push checks; no-push states) for Closure.
READ:              docs/agent-workflow.md (all); docs/reviews/a4s-r6-advisor-hunk-ruling.md (all);
                   docs/reviews/a4s-r6-planning-brief-draft.md (all); docs/reviews/a4s-r5-conflict-inventory.md (all);
                   logs/a4s/conflict-hunk-inventory.txt (all); docs/reviews/a4s-execution-evidence.md (all);
                   docs/packets/a4s-toolkit-sync.md (all, A4s-r5); docs/reviews/a4s-r6-evidence-index.md (all);
                   docs/reviews/a4s-r6-h1-repair-proposal.md (all); docs/reviews/a4s-r5-command-defect.md (all);
                   docs/reviews/a4s-r6-ac-struct-controls.md (all); docs/reviews/a4s-r6-ac-struct-tool.md (all);
                   scripts/check-merge-structure.py (all); logs/a4s/verify-advisor-h2.py,
                   logs/a4s/verify-hunk5-output.py, logs/a4s/ac-struct-prototype.py (all).
                   Scanner SHA-256 A1FDCE26… and fixture SHA-256 A42F4A1E… and the control reproduction
                   (0d7929c 87/109/0 exit 0; 766ecef 87/111/0 exit 0; 75083476 raw 90/111/15 exit 1;
                   fixtures 14/14 OK) OBSERVED by this Planner this session.
PREMISE_FRESHNESS: BOUNDED — the conflict inventory, the Advisor rulings, the structural findings, the
                   pinned tool hashes and the scanner controls are current-session records; the merge
                   base/tags/remotes are unchanged from A4s-r5; the reference stop is A4a-r2 R0; any stale
                   premise (P0 fail, conflict-set divergence, scanner-hash mismatch, V fail, F=0) routes to
                   R-PRE/R-CONFLICT/R-INVALID/R-UNKNOWN, never to a false PASS.
BLOCKING:          NONE (self-assessment; the fresh Planner must re-check the items in DEFERRED)
DEFERRED:          (1) The resolved all-ours preview positive control needs a resolve-to-ours step the
                   scanner does not itself perform; the packet names logs/a4s/conflicted-* as the fidelity
                   source and expects exactly the 3 structural findings — the fresh Planner should confirm
                   that construction procedure is mechanical enough for a literal executor (it is the one
                   step not reducible to a single pinned command). (2) verify-advisor-h2.py /
                   verify-hunk5-output.py are cited as provenance, NOT gating (the rule text is fully
                   specified in-packet and AC-TEST is the independent gate) — flagged to the Session to
                   confirm this satisfies POLICY_ISSUE (1). (3) HA-COMBINED-1/2 and Expected-5/6/9 are
                   transcribed from the ruling; the packet defers to the ruling on any divergence. (4) The
                   set-G carve-out names E2 by identity; the fresh Planner should confirm "same failing
                   test identity" is literally decidable and that no count is hard-coded.
DECISIONS:         Packet class = change (A4s-r5 revision per §5.4(3)); reason: the Advisor ruling changed
                   a policy the packet depends on; reverse if the Advisor retracts the ruling.
                   All 9 hunks pre-ruled (complete table); reason: the Advisor ruled each, so execution is
                   transcription, not judgment; reverse if the real conflict set/text diverges (then
                   R-CONFLICT, never analogy).
                   H2 action = edit application (not presence-union); reason: Advisor-ruled, and a
                   presence-union resurrects deleted lines (the true H2 vacuity analogue); reverse if a
                   real/synthetic hunk shows edit application loses an edit.
                   AC-STRUCT structural-only, before build, R-CONFLICT on FAIL + R-BUILD backstop; reason:
                   Advisor-ruled against a broader semantic review and against R-BUILD misattribution;
                   reverse if a merge-introduced compile-class defect (a)-(c) cannot express appears.
                   AC-STRUCT scanner + fixtures are tracked pinned tools; verifiers are provenance not
                   gating; reason: POLICY_ISSUE (1); reverse if the Advisor rules the verifiers must gate.
                   Set-G carve-out by identity, re-derived per execution, no count hard-coded; reason:
                   baseline moved 8/2 -> 9/1 -> 10/1 in one session; reverse if it moves again (re-derive).
                   "Expected R-SAME" is a forecast only, no criterion assumes it; reason: POLICY_ISSUE (3).
                   No push during execution; Closure push to origin only after ACCEPT with 5 pre-push
                   checks; R-CONFLICT/rollback/INADEQUATE no-push; reason: owner push policy.
VERDICT:           ADEQUATE (self-assessment only; not binding for a change packet)
```
