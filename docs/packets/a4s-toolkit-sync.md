## A4s — merge toolkit `v0.11.0` into local `main`, rebuild without regeneration, test, and locate the strict stop

**Class:** change   **Contract revision:** `A4s-r1`   **Status:** draft
**Governing requirement:** owner instruction 2026-09-24, verbatim in `docs/reviews/toolkit-sync-instruction.md` (merge v0.11.0 into local `main`, resolving conflicts; rebuild **without** regenerating `src/recomp/gen`; run both repositories' tests; rerun one strict baseline; if the strict stop moves, that is the next brief), **as amended by the owner's toolkit fork and push policy** recorded in `AGENTS.md` § "Toolkit remotes" (game commit `3ec4563`): the instruction's `origin/main` (sp00nznet) is now **`upstream/main`**; `origin` is the owner's fork; "push toolkit `main` to `origin` when a packet closes; never push to `upstream`". So: **nothing is pushed during execution; after ACCEPT, `main` is pushed to `origin` (Closure); nothing is ever pushed to `upstream`.**
**Depends on:** `A4p-r1` ACCEPTED (plan, 2026-09-24). No `A4b` code exists yet.
**Baseline:** toolkit `main` = `0d7929c86771dd0b971941592fd4f15436116e82` (clean, **tracking `upstream/main`**); `upstream/main` = `origin/main` = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` = tag `v0.11.0` (the fork currently equals v0.11.0); remotes `origin` = `https://github.com/danillogical/xboxrecomp.git` (fetch+push), `upstream` = `https://github.com/sp00nznet/xboxrecomp.git` (fetch; push `DISABLED`); merge base `051a128df5ec27ef14f1ceaaead11c5457321eef`; game = `3ec4563` or the later commit that adds this packet (Session records it at promotion; clean tree). Reference strict run: `A4a-r2` R0 `logs/runs/20260924-191906-091-a4a-r2-default` (exe `9597ff7c2a377265aba8dbb90b461ebe763e02d65432e9dfa13acd925539c553`, toolkit `0d7929c`), `diagnostic_deadline`, `F = 2`.
**Revision log:** `docs/reviews/a4s-revision-history.md` (non-authoritative).

### Motivating evidence
- Toolkit divergence 169 upstream / 24 local, `HEAD` not an ancestor of `upstream/main` (= `766ecef`); exactly 10 files changed on both sides — Session-verified, `docs/reviews/toolkit-sync-instruction.md`; re-observed by this Planner (`rev-list --left-right --count` = `169 24`; name-only diffs 153 upstream / 38 local / 10 both).
- Upstream changed `src/apu/apu_dsp.c` and `src/apu/CMakeLists.txt` (upstream-only), the files `A4b1` modifies — so `A4b1`'s baseline is invalidated by this merge.
- Current strict stop: `loc_001A18D0` spin, `recomp_0005.c:6748-6751` in `sub_001A1769`, pending word `MEM32(0x803C0810) = 3`, `diagnostic_deadline` — `A4a-r2` R0, profile STRICT (`docs/reviews/a4a-execution-evidence.md`).

### Claim and boundaries
- **Establishes:** local toolkit `main` is a merge commit with parents `0d7929c` and `766ecef` that (a) contains every file exactly as one side left it, except the 10 both-side files, which contain both sides' added lines under the rules below; (b) preserves `0d7929c`'s `src/apu/apu_mmio_hook.c` byte-for-byte and `c97ce2c`'s AC'97 model lines; (c) builds with `scripts\build-jsrf.py` **without** regeneration, with the game's `src/recomp/gen` byte-unchanged; (d) passes the test sets defined in `AC-TEST`; (e) was not pushed anywhere during execution, and `upstream` was never pushed to; and (f) one strict default run on that build selects one row below.
- **Does not establish:** that the merged lifter/translator produce correct output; that `src/recomp/gen` matches the merged lifter — **it does not, by construction**: the chunks, `recomp_funcs.h`, `recomp_dispatch.c` and `recomp_stubs_unresolved.c` are the products of the pre-merge generator, and leaving them so is a deliberate owner-chosen bound, not an oversight (likewise the merged `templates/runtime/recomp_types.h` is not built — the game compiles `src/recomp/gen/recomp_types.h`, which only a translation pass refreshes); that upstream's runtime changes are correct for JSRF; any guest behaviour beyond the one run's stop; that a moved stop is deterministic (one run, per the owner); any audio/DSP/GPU/liveness/boot claim.
- **Non-goals:** regeneration of any kind; fixing any failing test, build error or guest stop; editing game source, scripts, config or CMake; revising `A4b1`/`A4b2` (that is the next Planner's job under `R-SAME`); any push before ACCEPT; any push to `upstream` at any time; any `git fetch`; merging anything other than `766ecef`; changing remotes; adding a remote to the game repository. Upstream's new C test trees (`tests/apu_mixdown`, `tests/d3d8_smoke`, `tests/fist`, `tests/kernel_*`, …) and `tools/conformance` are out of scope: the packet checks the game's own 12 CTest names; whether upstream's C suites register into the game's CTest is **not asserted** (upstream's root `CMakeLists.txt` at `766ecef` has no `include(CTest)` or `tests/` subdirectory — observed — but subdirectory wiring is unread).

### Readiness
- **Tools (Session verifies each runs before freezing):** `git` 2.39.2; `python -X utf8 scripts\build-jsrf.py`; `ctest --test-dir build -C Release [-N] --output-on-failure`; game `tests\test_*.py` run as files; toolkit `C:\Python313\python.exe -m unittest …` with `PYTHONPATH=C:\Users\logic\AppData\Roaming\Python\Python313\site-packages`; `scripts\check-generation-provenance.py --check`; `scripts\run-jsrf.py … --profile strict`; `scripts\check-run-profile.py`; `scripts\check-dump-mapping.py`; `scripts\inspect-jsrf.py memory`.
- **Preconditions P0 (all must hold, else stop before any write):** `git -C C:\Users\logic\Repos\xboxrecomp status --porcelain` empty; toolkit `rev-parse HEAD` = `0d7929c86771dd0b971941592fd4f15436116e82`, branch `main`, `rev-parse --abbrev-ref "@{u}"` = `upstream/main`; `rev-parse upstream/main`, `rev-parse origin/main` and `rev-parse "v0.11.0^{commit}"` all = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` (already local — **no `git fetch` is required or authorized**); `git -C $T remote -v` prints exactly the four lines `origin https://github.com/danillogical/xboxrecomp.git (fetch)`, `origin … (push)`, `upstream https://github.com/sp00nznet/xboxrecomp.git (fetch)`, `upstream DISABLED (push)`; game `git remote` prints nothing; game `git status --porcelain` empty; lines 6748 and 6751 of `src\recomp\gen\recomp_0005.c` read `loc_001A18D0: ;` and `… goto loc_001A18D0; …`.
- **Stop if:**
  - any `git push` (including `--dry-run`), `git fetch`, `git remote` edit, or other remote operation is about to run **during execution** — the only push this packet authorizes is the Closure push to `origin` after ACCEPT; **never push to `upstream`** (its push URL is `DISABLED`, so an attempt fails, but it must not be attempted); never add a remote to the game repository;
  - any of `python -m tools.recomp`, `scripts\recover-functions.py`, `scripts\relift-selected.py`, `build-jsrf.py --allow-regeneration` would run, or anything under game `src/recomp/`, `config/`, `tools/disasm/output/` would change;
  - the merge **appears to require regeneration** — any build error located in `src/recomp/gen/*` or `src/recomp/recovered/*`, or an unresolved/duplicate symbol whose name is `sub_*`/`loc_*` or is declared in `recomp_funcs.h` → `R-REGEN`;
  - a build or test failure would need any edit other than the conflict resolution in step 3 (no fixes in scope);
  - a conflict appears in a file outside the 10 listed, or a conflict hunk is not decided by rules H1–H3 → `R-CONFLICT`;
  - any of `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`, `JSRF_ABI_CONTINUE`, `RECOMP_APU_TRAP`, `RECOMP_APU_TRACE` is in the run environment, or the runner refuses `--profile strict`.

### Execution
Toolkit commands run with `-C C:\Users\logic\Repos\xboxrecomp` (written `$T` below: `$T='C:\Users\logic\Repos\xboxrecomp'`); game commands from `C:\Users\logic\Repos\my_xbox_game`. Record every output into `logs\a4s\` (raw) and the values into `docs\reviews\a4s-execution-evidence.md`.

1. **Recovery point.** `git -C $T branch a4s-pre-sync 0d7929c86771dd0b971941592fd4f15436116e82`. **The rollback target is commit `0d7929c86771dd0b971941592fd4f15436116e82`** (branch `a4s-pre-sync`, local only). Rollback procedure, used by every failure row: if a merge is in progress `git -C $T merge --abort`; else `git -C $T branch -f a4s-merge-attempt HEAD` (keeps the attempt as evidence) then `git -C $T reset --hard 0d7929c86771dd0b971941592fd4f15436116e82`; then `python -X utf8 scripts\build-jsrf.py` and record the exe SHA-256 (it must equal the step-2 pre-merge SHA; if not, record — the restored tree is still `0d7929c`).
2. **Pre-merge controls** (on `0d7929c`): `python -X utf8 scripts\build-jsrf.py`; record `build\Release\jsrf_recomp.exe` SHA-256 (expected `9597ff7c…`; record, not a gate); `ctest --test-dir build -C Release -N` (expect 12) and `ctest --test-dir build -C Release --output-on-failure`; game Python set **G** and toolkit sets **K4** and **KX** (defined in `AC-TEST`) — record per-test/per-module pass/fail. These are the controls for `AC-TEST`'s regression rule.
3. **Merge** (from `upstream/main`, pinned by SHA so a later fetch cannot change the input). `git -C $T -c merge.conflictStyle=diff3 merge --no-ff --no-commit 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` — this is `upstream/main` as of P0. List conflicts: `git -C $T diff --name-only --diff-filter=U`. Every conflicted path must be one of the 10: `CMakeLists.txt`, `src/kernel/kernel.h`, `src/kernel/kernel_bridge.c`, `src/kernel/xbox_memory_layout.c`, `src/kernel/xbox_memory_layout.h`, `templates/runtime/recomp_types.h`, `tools/disasm/functions.py`, `tools/recomp/lifter.py`, `tools/recomp/test_icall_feedback.py`, `tools/recomp/translator.py`. Resolve **each conflict hunk** by the first rule that applies, and record file, hunk line range, rule, and (for H3) the omitted lines:
   - **H1 — containment.** Every non-blank line one side added in the hunk is also present in the other side's version of the hunk → take the containing side.
   - **H2 — disjoint union.** Each base line in the hunk was changed by at most one side (the sides edited different lines, or both only inserted at the same point) → keep both sides' edits; for pure same-point insertions, local lines first, then upstream lines.
   - **H3 — version/comment only.** The only base lines changed by both sides are a version string (e.g. `project(… VERSION …)`) or comment text → take upstream for those lines, apply H2 to the rest.
   - **Otherwise UNDECIDED** — the same base line was changed differently by both sides in code, or taking the union would not be syntactically valid. **Do not choose a side, rewrite, or merge semantics by judgment.** Record the hunk (base/ours/theirs text) and roll back (step 1) → `R-CONFLICT`.
   - Rule for the accepted-packet content: no resolution may remove or alter a line added by `c97ce2c` (`src/kernel/xbox_memory_layout.c`); if H1–H3 cannot keep them all, the hunk is UNDECIDED.
   After resolving: `git -C $T diff --check` clean; `Select-String -Path <each of the 10> -Pattern '^(<<<<<<<|\|\|\|\|\|\|\||=======|>>>>>>>)( |$)'` returns nothing; commit with a message file: `git -C $T commit -F <msgfile>` (message: `Merge upstream v0.11.0 (766ecef) into local main for A4s; generated tree not regenerated`). Record the merge commit SHA `M`.
4. **Build (no regeneration).** `python -X utf8 scripts\build-jsrf.py` (no `--allow-regeneration`; if it dies silently at `Checking File Globs`, rerun with `--parallel 1` per `AGENTS.md`). Record exe SHA-256 and the build log.
5. **Tests.** Run `AC-TEST`'s sets on `M`.
6. **Strict run** (game root, one run, 30 s):
   ```powershell
   Remove-Item Env:\RECOMP_APU_DSP_ACK,Env:\RECOMP_AC97_READY,Env:\JSRF_ALLOW_UNRESOLVED,Env:\JSRF_ABI_CONTINUE,Env:\RECOMP_APU_TRAP,Env:\RECOMP_APU_TRACE -ErrorAction SilentlyContinue
   $env:RECOMP_GPU_ACK='0'; $env:RECOMP_KERNEL_LOG_BUDGET='100000'
   python -X utf8 scripts\run-jsrf.py --seconds 30 --profile strict --label a4s-sync-strict
   ```
   Then `AC-RUN`'s reads. A rerun is permitted **only** where a row says so, with identical settings and label `a4s-sync-strict-rerun`.
7. **End-of-execution checks** (`AC-GEN`, `AC-NOPUSH`) and the evidence file. Execution ends here; the push to `origin` is **not** an execution step (see Closure).

- **Write scope:** toolkit — the merge commit on `main` and local branches `a4s-pre-sync` / `a4s-merge-attempt` only (no remote writes during execution); game — `docs/reviews/a4s-execution-evidence.md`, `logs/a4s/`, `logs/runs/`, and the build tree. Nothing else.
- **Build/run owner:** the Session, single owner.

### AC-REC — the pre-merge state is recoverable
- Mandatory: yes. Guards against: losing the 24 unpushed local commits or being unable to return to the accepted baseline.
- Procedure: after step 1, `git -C $T rev-parse a4s-pre-sync` = `0d7929c86771dd0b971941592fd4f15436116e82`; at closure the branch still exists and resolves to that SHA.
- PASS: both reads equal. FAIL: either differs. UNKNOWN: branch absent. Controls: waived — the oracle is `git` itself.

### AC-MERGE — both sides are in the result, and nothing else changed
- Mandatory: yes. Guards against: a merge that silently drops upstream work or local (unpushed) work, or a resolution that invents content.
- Procedure (toolkit): (a) `git -C $T rev-parse M^1 M^2` = `0d7929c…`, `766ecef…`; (b) `git -C $T diff --name-only 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b M` ⊆ the 38 files in `git -C $T diff --name-only 051a128 0d7929c` (every upstream-only file is exactly upstream's); (c) `git -C $T diff --name-only 0d7929c M` ⊆ the 153 files in `git -C $T diff --name-only 051a128 766ecef` (every local-only file is exactly local's); (d) **union witness** for each of the 10 files: every non-blank, whitespace-trimmed `+` line of `git -C $T diff 051a128 0d7929c -- <f>` and of `git -C $T diff 051a128 766ecef -- <f>` occurs (trimmed) in `git -C $T show M:<f>`, except lines recorded as H3 omissions; (e) the step-3 resolution table lists every conflicted hunk with a rule in {H1, H2, H3}.
- PASS: (a)–(e) all hold. FAIL: any fails. UNKNOWN: any list or command output missing.
- Controls: (b)/(c) known-bad — the same commands on `0d7929c` itself give a non-empty (b) excess (upstream-only files differ), proving the check can fail. Claim limits: textual presence, not semantic correctness of the combined code.

### AC-KEEP — accepted local work survives
- Mandatory: yes. Guards against: reopening `A3a-r25` (AC'97 model, `c97ce2c`) or `A4a-r2` (trace fix, `0d7929c`).
- Procedure: `git -C $T diff --quiet 0d7929c M -- src/apu/apu_mmio_hook.c src/apu/apu_mmio_hook.h src/apu/apu.h src/apu/apu_core.c` (exit 0: upstream did not touch them, so they must be local's bytes); every non-blank trimmed `+` line of `git -C $T show c97ce2c -- src/kernel/xbox_memory_layout.c` occurs in `git -C $T show M:src/kernel/xbox_memory_layout.c`; `git -C $T merge-base --is-ancestor c97ce2c M` and `… 0d7929c M` both true.
- PASS: all hold. FAIL: any fails (→ this is an H-rule breach → roll back, `R-CONFLICT`). Controls: waived — git ancestry and byte equality are the oracle. Claim limits: presence of the model's text, not that it still behaves as accepted with upstream's surrounding changes (the strict run is the only behavioural witness, and only for the one wait it reaches).

### AC-GEN — the generated tree was not regenerated
- Mandatory: yes. Guards against: a silent regeneration that would mix generator provenance, or a claim that the generated tree reflects the merged lifter.
- Procedure (game root, at closure): `git diff --quiet HEAD -- src/recomp config tools/disasm/output` (exit 0) and `git status --porcelain -- src/recomp config` empty; `python -X utf8 scripts\check-generation-provenance.py --check` reports ok with no problems/unknowns.
- PASS: all hold. FAIL: any file differs or the checker reports a problem. UNKNOWN: the checker cannot run.
- Controls: waived — the checker is an existing tested tool (`tests\test_generation_provenance.py`, including a changed-output known-bad). Claim limits: **PASS means the generated bytes are the pre-merge generator's; it is evidence they are stale relative to `M`'s lifter, not that they are correct.**

### AC-BUILD — the merged toolkit builds with the unchanged game
- Mandatory: yes. Procedure: step 4. PASS: `build-jsrf.py` exits 0 and `build\Release\jsrf_recomp.exe` exists with a recorded SHA-256 different from nothing (file present). FAIL: build error → `R-REGEN` if it meets the Stop-if regeneration test, else `R-BUILD`. Guards against: claiming a sync that does not compile.

### AC-TEST — both repositories' tests
- Mandatory: yes. Guards against: a resolution that breaks what this project relies on (baseline sets) or breaks upstream's own changed code (post-merge additions).
- **Decision on "both repositories' tests": both the baseline set and the post-merge set.** The baseline set is what the project and `AGENTS.md` rely on and is comparable before/after; the post-merge additions are the only direct witnesses that the resolved conflict files (`test_icall_feedback.py` is itself one) still do what upstream's `lifter.py`/`translator.py`/`functions.py` changes intended. Upstream's C test directories (`tests/*`) and `tools/conformance` (msvc-wine/Docker) are out of scope (see Non-goals). If the game's CTest happens to discover additional tests at `M`, they are recorded and must pass under (i); their mere existence, or their absence from CTest, never selects `R-REGEN`.
- Sets:
  - **C (game ctest):** `ctest --test-dir build -C Release -N` then `ctest --test-dir build -C Release --output-on-failure`. Baseline = the 12 names in step 2.
  - **G (game Python):** from game root, each file separately: `Get-ChildItem tests\test_*.py | ForEach-Object { python -X utf8 $_.FullName }` (file form, not `-m unittest tests.x`).
  - **K4 (toolkit baseline):** from `$T`, `$env:PYTHONPATH='C:\Users\logic\AppData\Roaming\Python\Python313\site-packages'; C:\Python313\python.exe -m unittest tools.recomp.test_lifter_atomics tools.recomp.test_lifter_string_compare tools.recomp.test_lifter_carry tools.recomp.test_seh_frame_owner`.
  - **KX (toolkit, conflict-adjacent):** same environment, each module separately: every `tools\recomp\test_*.py` and `tools\disasm\test_*.py` present in the tree at that moment, as `C:\Python313\python.exe -m unittest tools.<dir>.<module>` (package form verified by the Session: `tools/__init__.py`, `tools/recomp/__init__.py`, `tools/disasm/__init__.py` exist). **Pre-merge control, Session-observed at `0d7929c`: 33 modules (26 `tools/recomp` + 7 `tools/disasm`), `Ran 133 tests … OK`, no pre-existing failures** — so at `M` every KX failure is new. Step 2 re-records this control; if it differs from 33/133/OK, record it and use the step-2 result as the control.
- PASS: (i) C discovers all 12 baseline names and every discovered test passes; (ii) G, K4 all pass; (iii) every KX module passes, except a module that also failed in step 2 with the same failing test names (pre-existing, recorded); a module **new** in `M` must pass.
- FAIL: any test in (i)/(ii) fails; a KX module that passed in step 2 fails; a new KX module fails; a baseline ctest name is missing → `R-TEST`.
- UNKNOWN: a set cannot be run (environment, not code — e.g. `capstone` missing, confined `tempfile` denial) → record; one retry after fixing only the environment; persists → `R-TEST`.
- Controls: the step-2 pre-merge run is the known-good control for every set. Claim limits: green tests do not establish lifter correctness on JSRF or match the generated tree.

### AC-NOPUSH — nothing was pushed during execution, and `upstream` is untouched
- Mandatory: yes. Guards against: publishing unreviewed work to the fork before acceptance, or any write to `upstream`, against the owner's push policy.
- Procedure (at the end of execution, step 7 — before acceptance review): `git -C $T rev-parse origin/main` = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` (a push of `main` to `origin` would move this remote-tracking ref to `M`); `git -C $T rev-parse upstream/main` = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b`; `git -C $T rev-parse --abbrev-ref "@{u}"` = `upstream/main`; `git -C $T status -sb` first line shows `main...upstream/main [ahead 25]` with no `behind` (24 local + `M`) on a success path, or `[ahead 24, behind 169]` on a rollback path; `git -C $T remote -v` equals the four P0 lines exactly (push URL of `upstream` still `DISABLED`); game `git remote` prints nothing; the Session attests no `git push` or `git fetch` was run during execution.
- PASS: all hold. FAIL: `origin/main` or `upstream/main` ≠ `766ecef`, a remote line changed, a game remote exists, or a push/fetch attested → `R-PUSH`. UNKNOWN: a ref cannot be read → `R-PUSH` (fail closed). Controls: waived — git ref state is the oracle.
- Claim limits: a remote-tracking ref shows what this clone last saw, not the remote server; the Session attestation covers the rest. The Closure push is deliberately outside this criterion: it happens after acceptance and is not reviewed here.

### AC-RUN — where the strict stop is (selects the row; not a pass/fail)
- Mandatory: yes. Evidence profile: strict. Guards against: calling a missing frame, a non-strict run, or a displaced dump a "moved stop".
- Reads for run `<run>` (A4a's predicate, unchanged):
  - **V (validity):** `python -X utf8 scripts\check-run-profile.py <run>` prints `STRICT`; `result.json` readable; `python -X utf8 scripts\check-dump-mapping.py <run>` gives `matches ≥ 1`, `content-mismatch: 0`.
  - `outcome` from `result.json`.
  - `B` = `MEM32(0x001BA858)` and `W` = `MEM32(0x803C0810)`: `python -X utf8 scripts\inspect-jsrf.py memory <run> 0x001BA858 4` and `… 0x803C0810 4` (little-endian).
  - `F` = `(Select-String -Path <run>\stacks.txt -Pattern 'sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:67(4[89]|5[01])\b').Count` (offset unpinned by design; line range pinned by P0). Control: gives `F = 2` on `logs/runs/20260924-191906-091-a4a-r2-default`.
  - Record SHA-256 of `result.json`, `stacks.txt`, `jsrf_run.log`, `process.dmp`; if `outcome ≠ diagnostic_deadline`, record the first failure reported in `result.json`/`jsrf_run.log` and the top live guest frame of each thread in `stacks.txt`.

### Decision rows
Evaluate in order; the first match wins. Gate rows (1–6) are checked before the run is interpreted; the run is launched only if rows 2–6 do not match, and `R-PUSH` is evaluated again at step 7 (`AC-NOPUSH`) regardless of which later row the run selects.

| ID | Condition | Means | Toolkit `main` left at | Next packet |
|---|---|---|---|---|
| **R-PUSH** | `AC-NOPUSH` FAIL or UNKNOWN | owner push policy breached (or unverifiable) during execution | as is — do not attempt remote repair, do not force-push | **stop all work**; owner via Advisor (§3.4) |
| **R-CONFLICT** | P0 passes and: a conflict outside the 10 files, an UNDECIDED hunk, or `AC-MERGE`/`AC-KEEP` FAIL | the merge cannot be completed by the H-rules | `0d7929c` (rollback) | Planner `A4s-r2` with the recorded hunks as its brief (Advisor if a policy question) |
| **R-REGEN** | build failure meeting the Stop-if regeneration test, or `AC-GEN` FAIL | the merge requires, or produced, regeneration — outside the owner's scope | `0d7929c` (rollback; attempt at `a4s-merge-attempt`) | owner via Advisor: regeneration is a **separate packet** |
| **R-BUILD** | `AC-BUILD` FAIL not matching R-REGEN | merged runtime does not build with the unchanged game | `0d7929c` (rollback) | Planner `A4s-r2`, build log as brief |
| **R-TEST** | `AC-TEST` FAIL, or UNKNOWN persisting after one environment retry | a resolution or upstream change breaks a relied-on or upstream test | `0d7929c` (rollback) | Planner `A4s-r2`, failing tests as brief |
| **R-PRE** | P0 fails | baseline not as stated | untouched | Session reports; Planner re-checks premise |
| **R-INVALID** | V fails, or `result.json` unreadable | run not interpretable | `M` | rerun once identically; still → Planner `A4s-r2` with the failing gate |
| **R-MOVED** | V holds and a **positive** observation: `outcome ≠ diagnostic_deadline`; **or** `outcome = diagnostic_deadline`, `W` readable and `W ≠ 3` | the strict stop moved (one run; direction — earlier or later — recorded, not interpreted) | `M` | **the next brief** (owner): Session writes an evidence brief from this run → Planner designs the next packet; `A4b1`/`A4b2` stay parked |
| **R-SAME** | V holds; `outcome = diagnostic_deadline`; `W = 3`; `F ≥ 1`; `B = 0x803C0000` | stop unchanged: still the DSP pending-word spin | `M` | Planner revises **`A4b1` → `A4b1-r2`** (PREMISE_CHANGED, §5.4(2)): new baseline = toolkit `M`, this exe SHA, this run as the reference R0, and upstream's `src/apu/apu_dsp.c`/`CMakeLists.txt` as the starting state; then `A4b2` |
| **R-UNKNOWN** | none of the above — including `F = 0` with `W = 3` (a missing frame is a sampling gap, **never** a moved stop), `W` or `B` unreadable, or `B ≠ 0x803C0000` | not decided | `M` | rerun once identically; the rerun alone is evaluated against R-INVALID…R-UNKNOWN; still R-UNKNOWN → Planner `A4s-r2` |

### Closure
- **Evidence index** in `docs/reviews/a4s-execution-evidence.md`: P0 outputs; `a4s-pre-sync` SHA; pre-merge exe SHA and step-2 per-test results; conflict list and per-hunk resolution table (file, lines, rule, H3 omissions); `M` and its parents; `AC-MERGE` (b)–(d) outputs; `AC-KEEP` outputs; build log path/hash and post-merge exe SHA-256; `ctest -N` count and names; per-set, per-module test results; `AC-GEN` outputs; P0 and `AC-NOPUSH` remote/ref outputs; run directory and artifact hashes; `outcome`, `B`, `W`, `F`; the selected row.
- Every row that leaves toolkit `main` at `0d7929c` also records the rollback rebuild's exe SHA-256.
- **Closure push (owner policy; after ACCEPT only, not part of what the reviewer checks).** If and only if acceptance returns `ACCEPT` and toolkit `main` = `M` (rows `R-INVALID`, `R-MOVED`, `R-SAME`, `R-UNKNOWN` — every row reached only after `AC-REC`, `AC-MERGE`, `AC-KEEP`, `AC-GEN`, `AC-BUILD`, `AC-TEST` and `AC-NOPUSH` passed): from the toolkit root run exactly `git -C $T push -u origin main`, then confirm `git -C $T rev-parse origin/main` = `M` and `git -C $T ls-remote origin refs/heads/main` prints `M`. No `--force`, no other refspec, no push to `upstream` (never, in any form). The push is a fast-forward (`M` descends from `origin/main` = `766ecef`); if git reports it is not, or it is rejected, do not force — record and escalate to the owner via the Advisor. `-u` moves `main`'s tracking from `upstream/main` to `origin/main`; that is the owner's specified command and is recorded, not reverted. Record the push output and both confirmations in the plan's closure record (the evidence file is frozen at acceptance). For `R-PUSH`, and for any accepted row that rolled `main` back to `0d7929c` (not a descendant of `origin/main`), **do not push** — a push would be non-fast-forward; record "no push: main at 0d7929c".
- Post-review edits reopen affected criteria. An unrelated next stop is recorded as follow-up; do not expand scope.

### Planner adequacy self-check (§5.3 form; the binding review is by a fresh Planner, §5.1.5)

```text
REVISION:          A4s-r1 draft (SHA-256 reported to the Session on return)
READ:              docs/agent-workflow.md §5.1–§6.3 (380-719); docs/reviews/toolkit-sync-instruction.md (all);
                   plan-jsrf-bare-minimum.md 1-55, 215-254; docs/packets/a4a-dsp-pending-word.md (all);
                   docs/packets/a3a-ac97-codec-model.md 318-333; docs/packets/a4b1-gp-core-port.md (baseline lines);
                   docs/reviews/a4a-execution-evidence.md (R0 dir, exe SHA, F); scripts/check-generation-provenance.py 140-359;
                   tests/test_generation_provenance.py (head, check-only); scripts/build-jsrf.py (grep); build/CTestTestfile.cmake;
                   toolkit git: rev-parse HEAD/origin/main/v0.11.0, rev-list counts (169/24), log 051a128..HEAD,
                   show --stat c97ce2c 0d7929c, name-only diffs (153/38/10), diff --stat on shared files, CMakeLists diffs both sides;
                   after PREMISE_CHANGED (owner fork): AGENTS.md "Toolkit remotes"; toolkit remote -v, rev-parse
                   main/origin/main/upstream/main, @{u}, status -sb; game log -1 (3ec4563); 766ecef:CMakeLists.txt (no
                   include(CTest)/tests/); name-only test-module diff 0d7929c..766ecef.
PREMISE_FRESHNESS: BOUNDED — divergence, tags, conflict surface, accepted commits and the new remote layout re-observed
                   today; the reference stop is A4a-r2 R0 on exe 9597ff7c; the pre-merge exe SHA is recorded (not gated)
                   and any stale premise (P0 fail, V fail, F=0) routes to R-PRE/R-INVALID/R-UNKNOWN, never to R-SAME or
                   R-MOVED.
BLOCKING:          NONE
DEFERRED:          (1) resolved: KX control is 33 modules / 133 tests / OK at 0d7929c (Session-observed), so every KX
                   failure at M is new. (2) templates/runtime/recomp_types.h is NOT consumed by the game build (game
                   sources include src/recomp/gen/recomp_types.h, refreshed only by the translation pass) — so the
                   merged template is a further unexercised divergence under the no-regeneration bound; noted, not
                   tested. (3) Whether upstream's new C test trees register into the game's CTest is not asserted;
                   stated as a bound; neither outcome selects R-REGEN.
DECISIONS:         Resolution = H1 containment / H2 disjoint union / H3 version-comment→upstream, everything else stops;
                     reason: no side-picking by judgment on unpushed or upstream work; reverse if the Advisor rules a
                     side-preference policy.
                   Tests = baseline set AND post-merge conflict-adjacent set with a pre-merge control; reason: baseline
                     is comparable, post-merge is the only witness for the resolved generator files; reverse if KX's
                     control shows it is unrunnable.
                   Failure rows roll main back to 0d7929c and keep the attempt at a4s-merge-attempt; reason: A4b must not
                     start on a half-synced tree, evidence is kept; reverse if the owner prefers the merge kept.
                   R-MOVED requires a positive observation (outcome or W); F=0 alone is R-UNKNOWN; reason: A4a's defect.
                   One strict run, one identical rerun only for R-INVALID/R-UNKNOWN; reason: owner fixed "one run".
                   Merge input pinned by SHA 766ecef (= upstream/main), no fetch; reason: a moving ref would change the
                     conflict surface; reverse only by owner instruction to sync a later upstream.
                   No push during execution (AC-NOPUSH, decidable from refs + attestation); push -u origin main only in
                     Closure after ACCEPT and only when main = M; never upstream; reason: owner push policy; a rolled-back
                     main is not a fast-forward of the fork.
VERDICT:           ADEQUATE (self-assessment only; not binding for a change packet)
```
