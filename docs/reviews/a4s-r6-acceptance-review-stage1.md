# A4s-r6 — first-stage ACCEPTANCE review (Acceptance reviewer, stage 1)

**Reviewer role:** Acceptance reviewer, first stage (`docs/agent-workflow.md` §2.2).
**Contract reviewed:** `docs/packets/a4s-toolkit-sync.md`, revision `A4s-r6`.
**Observed SHA-256:** `75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3`
— **matches the frozen hash.**
**Observed line count:** **374** lines (`Get-Content … Measure-Object -Line` = 325 blank-stripped;
`len(bytes.decode().split('\n'))` = 375 elements because the file ends with a newline; newline
count = 374). The packet's own footer says "(End of file - total 374 lines)". **374 confirmed.**
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`. **Date:** 2026-09-25.
**Binding rulings applied:** Q-A (pinned bytes), Q-B (`a4s-pre-sync` verify-and-reuse),
Q-C (KX/`pytest` disposition) in `docs/reviews/a4s-r6-execution-rulings.md`.

**DISPOSITION: ACCEPT** — every mandatory criterion `AGREED`.

---

## Mandatory criteria

| Criterion | Verdict |
|---|---|
| AC-REC | AGREED |
| AC-STRUCT | AGREED |
| AC-MERGE | AGREED |
| AC-KEEP | AGREED |
| AC-INV | AGREED |
| AC-GEN | AGREED |
| AC-BUILD | AGREED |
| AC-TEST | AGREED |
| AC-NOPUSH | AGREED |
| AC-RUN | AGREED |

Ten mandatory `AC-*` criteria enumerated from the packet; no other `AC-*` section exists.

---

### AC-REC — AGREED

`git -C $T rev-parse a4s-pre-sync` = `0d7929c86771dd0b971941592fd4f15436116e82` at both reads.
`git -C $T reflog show a4s-pre-sync` = one entry, `branch: Created from 0d7929c…` (no move).
Branch still exists at closure. P0.7's literal FAIL (the branch already existed) is covered by the
binding Advisor ruling **Q-B**, whose four fail-closed conditions I re-verified and all hold
(exactly one `a4s-*` branch; `a4s-merge-attempt` absent; resolves to `0d7929c`; single reflog entry).

### AC-STRUCT — AGREED

- Resolved `M`: `python -X utf8 scripts\check-merge-structure.py --rev 3f8bf67c… --expect 0`
  → `files=90 switches=111 findings=0 unknown=0`, **exit 0**.
- Negative controls: `0d7929c` → `87/109/0/0` exit 0; `766ecef` → `87/111/0/0` exit 0.
- Raw `75083476`: **15** = 12 markers + 3 structural — matches the packet's separate expected
  observation, and matches the stated line positions (`8040/8343`, `8502/8868`, `1378/6734`).
- Fixtures: `python -X utf8 tests\test_merge_structure.py` → `Ran 14 tests … OK`.
- Scanner bytes: `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF` (P0.10, pin);
  fixtures `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C`. Both re-verified
  after all my work and unchanged.

**Coverage/falsification note (advisory, not a criterion).** The packet's *positive control* is the
**resolved all-ours preview** (3 structural, 0 markers). The evidence file records only the
**raw** `75083476` = 15 under the heading "positive control". The resolved-all-ours number (3) is
recorded as measured in `docs/reviews/a4s-r6-ac-struct-tool.md:56` and
`docs/reviews/a4s-r6-adequacy-review.md:143-146`, but the *executed* step-4 table shows the raw
number where the criterion names the resolved one. I re-ran the raw scan myself (15) and confirmed
the resolved-`M` scan is 0; the property the criterion actually protects — no finding on the
resolved tree — is independently confirmed by my own run, and the 3-finding all-ours control is
attested with exact line positions in the session's own records. This is a **labelling** gap in the
evidence table, not a failed control; it does not change the verdict. See ADVISORIES.

**Claim limit 2 (Q-C) — verified.** `AC-STRUCT` detects duplicates, not omissions. The post-condition
(exactly one `case 138` per switch) holds at `M`, and I verified it with a positive control:

```
switch138.py 3f8bf67c…  -> 5 switches; case138 only in idx3 (line 8008) and idx4 (line 8469); total 2
switch138.py 75083476   -> 5 switches; case138 at 8040,8343 (idx3) and 8502,8868 (idx4); total 4
```
So the omission class is excluded by the post-condition count, exactly as the ruling requires.

### AC-MERGE — AGREED

- (a) `M^1` = `0d7929c…`, `M^2` = `766ecef…`.
- (b) `diff --name-only 766ecef M` = 38 files ⊆ the 38 of `diff --name-only 051a128 0d7929c`; excess empty.
- (c) `diff --name-only 0d7929c M` = 153 ⊆ the 153 of `diff --name-only 051a128 766ecef`; excess empty.
- **Positive controls** (the packet says (b)/(c) are known-bad): running (b) on `0d7929c` itself
  gives **143** excess files; running (c) on `766ecef` gives **28** excess. Both non-empty → the
  checks can fail. This is the control the criterion demands and I reproduced it.
- (d)/(d-twin): the 16 targeted per-region checks in `docs/reviews/a4s-r6-ac-merge.md` are
  region-scoped, which is the right instrument; I independently confirmed the two headline
  post-conditions (`bridge_KeResetEvent` definitions = **1**; `case 138` sites = **2**, one per
  switch) and the Appendix A oracle (below).
- (e) all 9 hunks carry a rule in the allowed set.
- I independently confirmed the **conflict-set guard** read-only via
  `git merge-tree --write-tree 0d7929c 766ecef`: **exactly the 5 recorded files, exactly 9 hunks**
  (kernel_bridge 3, xbox_memory_layout 1, lifter 3, test_icall_feedback 1, translator 1), and the
  recorded inventory's 9 line spans match.

**Merge oracle (Appendix A) — verified independently.** I extracted the fenced blocks from the
frozen packet bytes and compared against `M`: HA-COMBINED-1 verbatim = **True**; HA-COMBINED-2
verbatim = **True**; Expected-5 verbatim = **True** (6 comment lines, `lock xadd` absent, `popfd`
present); Expected-9 verbatim = **True**. Expected-6's Appendix A text is an **abbreviated** excerpt
(the real `_EFLAGS_PRESERVE` has ~170 members), so a literal whole-block match is not the right
test; I verified it semantically by AST: `M == upstream_set ∪ {wbinvd}` (**True**), `popfd` absent
from `M` (**True**), `pushfd`/`pushal` present, `wbinvd` present. Hunk 5 likewise verified by AST:
`M == base − {lock xadd} ∪ {popfd}` (**True**). Hunks 3, 4, 7, 8, 9 verified individually
(LOCAL TLS line; `|= MCPX_AC97_CODEC_READY` = 0 at `M` vs 1 at upstream; local `batch_spans` block
before upstream's `imm_code_refs`; `--functions fns` in the conflicted function; union has both
sides' members plus `_RESULT_SNAPSHOT_SETTERS`).

### AC-KEEP — AGREED

- (i) `diff --quiet 0d7929c M -- <4 apu files>` → **exit 0**.
- (ii) all 125 non-blank `c97ce2c` `+` lines present in `M` (0 missing).
- (iii) `merge-base --is-ancestor c97ce2c M` and `… 0d7929c M` both **exit 0**.
- (iv) three start-anchored fixed-length extractions: anchor counts **1/1/1** at `M` lines
  **442 / 890 / 2103**; all three **byte-identical** to `0d7929c` 261-267, 664-719, 1716-1740;
  **can-fail control differs** for all three (start+1). Reproduced exactly.
- (v) deleted names in SCOPE at `M` = **0**; controls `766ecef` **2**, `051a128` **2**,
  `0d7929c` **0**, `75083476` **0** — all four match the recorded values (positive control present).
  `|= MCPX_AC97_CODEC_READY` = **0** code hits at `M` (positive control: **1** on `766ecef`, so the
  grep can fire). `ac97_arm_write_trap(` = 1 hit, the **definition line** only, no call site.
- SCOPE boundary guard: 48 added lines across `CMakeLists.txt`/`cmake`/`src/**/CMakeLists.txt`,
  **0** naming `templates/` or `tools/`; game `CMakeLists.txt` diff empty. Not tripped.
- (vi) `[A3A] ac97 witness: gc=0x00000002 gs=0x00000100` — GC bit1 set (**True**), GS bit8 set
  (**True**).

### AC-INV — AGREED

- Classifier-listed names: `0d7929c` = 2 trimmed lines, `M` = the **same 2** → no new line; control
  `75083476` = 2.
- Deleted names: 0 in SCOPE at `M` (with the four controls as in AC-KEEP (v)).
- New environment names: `0d7929c` **42** → `M` **44**; added = exactly `RECOMP_APU_MIXDOWN_ALL`
  and `RECOMP_USB_PORT`; **none removed**.
- T1 hits: **0**. T2 hits: **6** — 3 aperture-true (`g_ac97_page`, `AC97_`) inside
  `ac97_write_veh` and `ac97_arm_write_trap`, both with **zero call sites** → **dormant**; 3
  host-mechanism (`XBOX_TO_NATIVE(base_va)`, `g_memory_base`, and the AVEH registration line whose
  handler-body aperture test I ran on the handler's body). Disposition complete; none FAIL.
- Call-site test positive control: the packet says the test *can* find a call site; I confirmed the
  test discriminates (it reports 1 code hit — the definition — for a dormant function).

### AC-GEN — AGREED

`git diff --quiet HEAD -- src/recomp config tools/disasm/output` → **exit 0**;
`git status --porcelain -- src/recomp config` → **empty**;
`python -X utf8 scripts\check-generation-provenance.py --check` → `ok : True`, exit 0.

### AC-BUILD — AGREED

`build\Release\jsrf_recomp.exe` exists, SHA-256
`E45026C3DF5AACAF3D66FCC1E17D9C6C1A12247864D58D0911CDBA8435A2A3C7`, mtime 2026-09-25 20:49:20 —
matches the recorded post-merge exe and matches the exe recorded in the run's `build-source.json`.
No `C2084`/`C2196`/`C2371` in the build log; the backstop therefore does not re-attribute to
`R-CONFLICT` (the build succeeded, so `R-BUILD` is not reached either).
**Independent link-falsification:** all **147** toolkit+game sources recorded in the run's
`build-source.json` hash-match the current `M` worktree (0 mismatched, 0 missing), so the exe that
produced the strict run was built from the merged toolkit, not a stale tree.

### AC-TEST — AGREED

- **C:** `ctest -N` → 12; `ctest` → **12/12 passed**, including the named witness
  `jsrf_inplace_event_bridge` (Test #9).
- **G:** 11 files, **10 pass / 1 fail**; the single failure is `tests/test_ac2_provenance.py`
  ("A0 correct run PASSes", "P1 differently-cased archive keys still PASS") — **same identity** as
  the step-2 carve-out. I confirmed the file is untouched by this execution (last changed at
  `a16350f`, the A3a era; the game tree carries only the owner's `docs/agent-workflow.md`), so the
  failure is pre-existing and not merge-caused.
- **K4:** `Ran 30 tests … OK`.
- **KX:** 56 modules at `M`; step-2 baseline re-measured by me in a throwaway worktree outside both
  repos: **33 modules, Ran sum 133**; all 33 present at `M`, **0 regressed**, **0 lost coverage**;
  23 new modules (9 exit 0 PASS; 7 exit 5 "not exercised (new)"; **7 exit 1**).

**Claim limit 1 (Q-C) — all four fail-closed conditions verified by me:**
(a) each of the 7 has **exactly one** error, `ModuleNotFoundError: No module named 'pytest'`, from
its own top-level import, `Ran=1` each, no `tools.*`/in-repo name — **PASS**;
(b) blob at `M` == blob at `766ecef` for all 7 — **PASS**;
(c) `C:\Python313` 3.13.2, `find_spec('pytest')` → `None` — **PASS** (and still `None` after my run,
so nothing was installed);
(d) 33/33 step-2 modules pass with `Ran ≥` step-2 count, 0 regressions, 0 lost coverage — **PASS**.
The 7 modules are exactly those named. Under Q-C these are "not exercised: new at M, dependency
absent (pytest)", **not a FAIL**, and Q-C governs over AC-TEST's literal "a new KX module fails".

### AC-NOPUSH — AGREED

`origin/main` = `766ecef…`; `upstream/main` = `766ecef…`; `@{u}` = `upstream/main`;
`status -sb` first line `## main...upstream/main [ahead 25]` (success path, no `behind`);
`remote -v` = the four P0 lines exactly with `upstream` push `DISABLED`; game `git remote` prints
nothing. `origin/main` reflog shows one `fetch` and no push; `refs` still at `766ecef`.

### AC-RUN — AGREED (selects the row; reproduces exactly)

- **V:** `check-run-profile.py` → `STRICT` (`checked 1; strict 1, exploratory 0, fixture 0,
  unknown 0, missing 0`); `result.json` readable; `check-dump-mapping.py` → `matches: 1,
  content-mismatch: 0`.
- `outcome` = `diagnostic_deadline`; `exit_code` 3; `dump_ok` true.
- **B** = `MEM32(0x001BA858)` = `0x803C0000` (usable; equals the required value).
- **W** = `MEM32(B+0x810)` read **at the derived address `0x803C0810`** = `00000003` → **W = 3**.
  (Note: my first parse read the address column and produced a spurious failure; the correct read
  at `B+0x810` is 3. The packet's "never a fixed address" rule was honoured.)
- **F** = **2** (`sub_001A1769` frames on `recomp_0005.c:6749`, two lines) — control `F = 2` on the
  A4a-r2 R0 reference.
- **A3A** witness present with both bits set.
- Artifact SHA-256s reproduce **exactly**: `result.json` `C662254D…`, `stacks.txt` `D7791C01…`,
  `jsrf_run.log` `DFA32044…`, `process.dmp` `3898C72C…`.
- Run environment carries only `RECOMP_GPU_ACK=0` and `RECOMP_KERNEL_LOG_BUDGET=100000`; none of
  the six forbidden synthetic-completion variables.

**Row: `R-SAME`.** Gate rows 1–6 all unmatched; R-INVALID no; R-MOVED (i) no and (ii) no
(`W = 3` not ≠ 3; `F = 2` not 0); **R-SAME matches all five conditions**; R-UNKNOWN no. First match
wins → `R-SAME` is correct.

---

## ADVISORIES (outside the contract; do not change the disposition)

1. **AC-STRUCT positive-control labelling.** The evidence table's "positive control" row shows the
   **raw** `75083476` = 15, while the criterion names the **resolved all-ours** preview = 3
   structural / 0 markers. The resolved number is measured elsewhere in the session's own records
   with exact positions, and the property that decides the criterion (0 findings on resolved `M`)
   is confirmed by my own run. Still, the executed step-4 table should show both, labelled, so a
   future reader does not read 15 against a criterion that says 3.
2. **Q-C claim limit (must be seen explicitly, and is).** KX does not exercise the 7
   pytest-dependent upstream modules under `unittest`; the lifter/translator resolutions (hunks
   5–7, 9) are witnessed only by K4 and the existing KX modules. Harmless here because AC-GEN
   holds — no regeneration, so `M`'s lifter produced none of the linked code. The gated lead
   (real pytest before any regeneration/relift packet) is binding on later planning.
3. **AC-STRUCT detects duplicates, not omissions.** The switch-5 omission was invisible to the
   scanner and is excluded only by the explicit post-condition. A future revision should make each
   HA edit's expected-count post-condition an `AC-MERGE` check.
4. **Hunk 8 function name in the evidence record.** `docs/reviews/a4s-r6-ac-merge.md:76-77` names
   the conflicted function `test_seeds_align_16`; the merge-tree hunk at line 116 actually sits in
   `test_seeds_drops_unaligned_targets`. The HA-LOCAL disposition is correct and I verified it in
   the right function (`none.json` = 0, `--functions fns` = 1 there), so this is a documentation
   error in a supporting record, not a resolution defect.
5. **Set-G failure is pre-existing but unrepaired.** `test_ac2_provenance.py` fails at both step 2
   and `M` with the same identity (carve-out correctly applied). It is a live game-repo test
   failure that the carve-out legitimately records rather than fixes.
6. **Step-2 exe vs A4a-r2 exe differ** (`AEC1F0FF…` vs `9597FF7C…`) by 13 PE-timestamp bytes; the
   packet says "record, not a gate", and `docs/reviews/a4s-r5-rollback-exe-hash.md` explains it.
   Carried forward as a known advisory.
7. **Two claim limits on my own review.** (a) I re-measured the step-2 KX control in a throwaway
   `git worktree` outside both repositories and removed it; the toolkit tree was never modified
   (verified: HEAD unchanged, porcelain empty, no leftover worktree). (b) Two of my own first-pass
   measurements were wrong (a regex reading the address column instead of the value; a PowerShell
   `+++` header line counted as an added line). Both were my instrument errors, and both were
   corrected by the targeted re-read shown above — the packet's warning about over-broad and
   substring matches applied to me as well.

---

## Verdict

```
REVISION:      A4s-r6  75207C41B8E9964D3E1467F5E980D1B8DE4CB75A1E0AFE954F59D40FA27D86E3  374 lines
CRITERIA:      AC-REC AGREED; AC-STRUCT AGREED; AC-MERGE AGREED; AC-KEEP AGREED; AC-INV AGREED;
               AC-GEN AGREED; AC-BUILD AGREED; AC-TEST AGREED; AC-NOPUSH AGREED; AC-RUN AGREED
DISPOSITION:   ACCEPT
BLOCKING:      NONE
ADVISORIES:    7 (listed above)
```

---

## Session disposition

**`DISPOSITION: ACCEPT`, `BLOCKING: NONE`, all ten mandatory criteria `AGREED`.** Per §2.2, **a
first-stage `ACCEPT` is final and is not passed to the second stage.** The packet state moves
**delivered → accepted** (§5.2), and the plan names the next work.

**Reviewer route:** child `f6ba7864-07ae-4388-9583-9a265e63c935`, `workbuddy-ai/hy4-preview-f` @ `high`
— the stage-1 Acceptance reviewer from §1, its route **verified live before dispatch** (exactly one
canonical match, `high` advertised).

### Independent verification the reviewer added beyond the Session's own measurements

Recorded because it materially strengthens the result — these are checks the Session had **not** run:

- **Positive controls for `AC-MERGE` (b)/(c):** on `0d7929c` the (b) subset yields **143** excess, and on
  `766ecef` the (c) subset yields **28** — both non-empty, so those subset checks **can** fail. Without
  this, a passing subset check is not evidence.
- **Positive control for the `|= MCPX_AC97_CODEC_READY` grep:** **1** hit on `766ecef`, so the **zero** at
  `M` is meaningful rather than a dead pattern.
- **Link falsification for `AC-BUILD`:** all **147** sources recorded in the run's `build-source.json`
  hash-match the current `M` worktree (0 mismatched, 0 missing) — independent evidence that the exe which
  produced the strict run was built from the **merged** toolkit. (The blob-vs-worktree delta is CRLF, the
  same phenomenon **Q-A** rules on.)
- **An independent conflict-set guard** via read-only `git merge-tree --write-tree 0d7929c 766ecef`,
  reproducing exactly the 5 files and 9 hunks (3/1/3/1/1) with all 9 spans matching.
- **AST-based verification** of hunks 5, 6 and 9 instead of text comparison — the right instrument for
  those, and it sidesteps the over-broad-text-match failure mode.
- It re-measured the **step-2 KX control itself** in a throwaway worktree **outside both repositories**,
  and removed it.

### Advisory dispositions

- **Advisory 4 — a real documentation error in a Session record — is CORRECTED.** The reviewer was right:
  `docs/reviews/a4s-r6-ac-merge.md` named the conflicted function `test_seeds_align_16`, which **does not
  exist in the file**. The Session re-verified (`logs/a4s/verify-advisory4.py`): the hunk-8 assertion sits
  in **`test_seeds_drops_unaligned_targets`** (defined L96), whose body contains `--functions fns` and
  **no** `none.json`, so **HA-LOCAL is correct**. A documentation error, **not** a resolution defect; it
  changes no criterion result. The record now carries the correction inline.
- **Advisories 1–3 and 5–7 are recorded and DEFERRED.** None changes the disposition, and none reopens
  the frozen packet (§5.4: advisories, wording, and record pointers do not reopen it). Advisory 1 concerns
  labelling in an evidence table — the raw-preview (15) and resolved-all-ours (3) numbers are both
  measured and both recorded, but the step-4 table should label them separately for a future reader.
  Advisory 3 is already carried as the **Q-C `AC-STRUCT` claim limit** and as a binding note for future
  revisions.

### Session note on the reviewer's own disclosed instrument errors

The reviewer disclosed that **two of its own first-pass measurements were wrong** (a regex reading the
address column instead of the value; a PowerShell `+++` header counted as an added line), both corrected
by targeted re-read. This is recorded because it is the **same failure mode** that produced every
verification error on the Session side during this execution — **over-broad or mis-scoped text
matching** — and it is now a documented pattern across two independent roles, not an isolated slip.

### What acceptance does and does not authorize

- **Authorizes** the Closure push of `M` to `origin` **once the five pre-push checks hold** (owner push
  policy) — see the plan.
- **Does not** authorize a push to `upstream` (never), a force push, or any change to `M`.
- **Carries forward** the two claim limits the Advisor required the reviewer to see (KX's 7
  pytest-dependent modules; `AC-STRUCT`'s blindness to omissions) and the **gated lead**: before any
  packet regenerates or relifts with the toolkit at `M` or later, all 56+ KX modules must run under real
  `pytest` in an owner-authorized environment.
