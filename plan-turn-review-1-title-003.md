# Turn Review 1 — `title-003`

Fresh Turn Reviewer (`claude/claude-opus-5-5` @ high). Commits reviewed: `6f2e4e2`, `b2179d2`,
`64945a3`, `d8632ce` (base `b065050`). I reproduced every measurement below myself on the final tree
unless it is marked otherwise.

```
TURN_REVIEW: FIX

PLAN_EVOLUTION:
- Material changes from start plan:
  1. The detector uses a stronger invariant than the plan proposed. The plan wanted "owner
     reachability" alone. The turn requires both (a) a fully enumerated own-walk and (b) independent
     entry evidence. Its verdicts are split into three gating classes (HIDDEN_ENTRY / OVERLAP /
     SHADOWED) and three reported-only classes (MISDISPATCH / OVER_RUN / UNQUALIFIED).
  2. The alias-shim misdispatch census from the plan (0xE9A40, 0x100AB0, 0x1199C0, 0x13A340) was not
     run. MISDISPATCH exists as a verdict, but it measures 0 on the current tree, and those four
     addresses were not examined.
  3. After g07 the turn moved to a new runtime stop (stop 21, under-wide 0xB5EB0). Repairing it took
     a generated-file patch in addition to the manifest change (PLAN_CHANGE 2).
- Evidence supporting those changes:
  - Change 1 is supported. On the base manifest (b065050) the detector gives 43 HIDDEN_ENTRY,
    5 OVERLAP and 2 SHADOWED: 50 containers and 63 consumed addresses. That matches the claim exactly.
    48 of the 50 are `tail_jump_alias` database records. The other two, 0x190FB0 and 0x1910C0, are
    OVERLAP containers with no database record, so "every one is a tail_jump_alias" is slightly loose.
    The SHADOWED split is justified: 0x556D0 and 0xC42E0 really do have generated bodies and no
    manifest entry.
  - Change 3 is supported by the g07 log and by the original bytes (details below).
  - Change 2 is not recorded as a PLAN_CHANGE. It is a scope drop, not an evidence-driven departure.
- Assessment: the evolution was sound and stayed aimed at the objective. Change 1 improved precision
  without suppressing anything: there is no baseline, and UNQUALIFIED is counted and printed.
  Following the runtime stop was the right call. The undocumented drop of the misdispatch census is
  minor (see ADVISORY).

BLOCKING:
- `remove-b5f3a-dispatch` cites the wrong compatibility-ledger entry, so this shortcut has no ledger
  record.
  Evidence: `config/generated-patches.json` gives `"ledger": "L42"`. In
  `docs/jsrf-compatibility-ledger.md`, L42 is "File completion APCs run at the next alertable
  wait…". That is kernel_bridge APC timing and has nothing to do with this patch. The precedent the
  turn names, `remove-54750-stub`, cites L02 ("Reviewed recovered bodies … and boundary fixes"),
  and L02 already describes that patch.
  `scripts/patch-generated.py` only checks that the ledger ID exists, so the mistake passed
  `just check`. The same wrong "(L42)" claim appears in `plan-jsrf-bare-minimum.md` (Current work,
  the stop-21 row and line ~181), in TR §15 (line 2053), in the 64945a3 commit message and in the
  living plan.
  This turn's other changes to L02's subject are also missing from L02:
    - removing manifest entry 0xB5F3A,
    - 50 tightened spans,
    - 48 added entries (3110 -> 3157).
  Consequence: AGENTS.md and workflow §6.9 require every patch a result relies on to be in the
  ledger, and "a shortcut without a ledger entry is not allowed". The final build and the CTest 38/38
  result both rely on this patch. Any future run record that lists "L42" as its justification would
  point at an unrelated entry.
  Fix (records only, no code or behaviour change):
    - set the patch's `ledger` to L02,
    - add an L02 note for the 0xB5F3A removal, the dispatch patch and this turn's manifest delta,
    - correct the "(L42)" citations in the plan and in TR §15.

ADVISORY:
- **Stopped one run short on the runtime chain.** The final build is in place:
  `build/Release/build-source.json` hashes `config/recovered-functions.json` to 8bb404ca…, which is
  64945a3's manifest with CRLF line endings, and it hashes `config/generated-patches.json` to the
  current file. CTest ran against that build. Even so, no run was made on it. Stop 21 is honestly
  marked "NOT yet runtime-confirmed", but one bounded exploratory run (g08) is the obvious next
  critical-path step. It is cheap, it would confirm or refute the 0xB5EB0 repair, and it would show
  stop 22. The plan's Current work also never says this as an explicit next action.
- **Manifest evidence contradicts 46 of the 50 tightened container spans.** Each container's `end`
  was moved, but its `evidence` text was left alone. Example: 0x7DAE0 now has `end` 0x7DBCC, yet its
  evidence still says "Body 0x0007DAE0..0x0007DE20, end tightened to the next function entry". By
  regex count, 46 container evidence strings state a body end that differs from the declared end. No
  container evidence mentions check-hidden-entries. Only 0xB5EB0 was rewritten. Stale "reviewed"
  evidence is how a later pass re-widens or re-narrows a span; 0xB5EB0's own history shows that
  churn. Append one sentence per container giving the new end and naming the detector.
- **The repair introduced a new manifest overlap that no gate reports.** 0x1FFF0
  [0x1FFF0, 0x2015F) contains nine older micro-entries: 0x200A5, 0x200A8, 0x200AD, 0x200AF, 0x200B1,
  0x200B6, 0x200BB, 0x200BF and 0x200F7. Each is 2–5 bytes, most have `stack_args` null, and they
  are instruction boundaries inside 0x1FFF0's call sequence. Their only "pointers" are consecutive
  dwords in a packed `.data` run at 0x22E2B4….
  Base-to-final, overlapping manifest pairs went from 7 to 3. Six old overlaps were removed and this
  one was added.
  `sub_0001FFF0` is self-contained (all internal `loc_`, no calls into 0x200xx), so there is no
  runtime hazard today. But calling this run "a chain of real continuations" / "legitimate splits"
  is inference, and the bytes argue against it. Note also that OVERLAP only fires on the region past
  the body. Its docstring ("two spans that overlap … wrong by construction") reads broader than what
  the code enforces.
- **The "two independent reproductions" claim is neither reproducible nor independent.**
  - The prototype is not in the repository.
  - Today's `check-table-targets.py` reports 4 swallowed candidates: 0x96F80 and 0xAEE90 (the two
    UNQUALIFIED containers) plus 0xC88D5 and 0x1588EA (unaligned noise the §12 filter rejects). The
    baseline had 52.
  - From the committed artifacts I can reconstruct 48 agreements and 4 differences. The claim says
    50 agreements and exactly 2 differences, both opaque-exit containers.
  - Both "reproductions" read the same aligned-dword sweep, so they share their evidence source.
  - 0x96F80 was already listed as known-open in the base plan (line 218), and it is hard-coded in
    the selfcheck and tests. "Re-found unprompted" therefore cannot be re-checked.
  Reword this as a consistency check, not independent reproduction.
- **The "three real defects caught by tests" list is accurate about the defects, not about how they
  were caught.**
  - #3 (0xB3C30) really was caught by `tests/test_recovery_span_ownership.py`.
  - #1 (apply() dropping all additions) was caught by an ad-hoc set-difference count.
  - #2 (LNK2005) was caught by the linker.
  - "Each is now a guard" overstates it: `scripts/repair-hidden-entries.py` has no test at all.
  - Its docstring (lines 27–30) says the script "refuses to write anything unless" the repaired
    manifest passes the checker, every addition is liftable and no new span is introduced. `main()`
    performs none of those checks; it writes straight after `plan()`. That is the same silent-success
    shape as defect #1. Either implement the checks or delete the claim.
- **Detector soundness: holds for the gate, but two docstrings overstate it.**
  - `body()` really does require no truncation, no fall-off, a non-empty exit set and a terminator
    as the last reachable instruction. `evaluate()` really does return UNQUALIFIED (non-gating,
    printed, counted) when any `indirect`/`terminal` exit exists.
  - Checked: of 158 entries where `body()` returns None (65 fall-off, 91 no exit, 2 truncated,
    walk limit hit 0 times), none has an unreached, evidenced, plausible candidate inside its span.
    So nothing gate-relevant is being silently classified CLEAN.
  - Overstated: the docstring says "any jmp the walk could not fully resolve is recorded opaque" and
    "an under-read table degrades the entry to UNQUALIFIED". In fact `Analyzer.walk` silently drops
    out-of-span table arms whenever at least one arm is in span. A table that stops early (a 0 or
    non-code dword mid-table) with one in-span arm is still called "enumerated". The over-run gate
    stays sound, because in-span arms are followed and a hidden entry must also have pointer
    evidence. But the stated guarantee is stronger than the code. For this population, the turn's
    manual check holds: none of the 50 base containers has a dropped arm.
  - The repository docstring header is stale ("48 containers and 59 consumed"; actual 50/63).
- **The under-wide discriminator is more established than the records say.** "The owner's own
  jump-table arm lies outside its own span" is decidable from the bytes, and it is exactly what
  diagnosed 0xB5EB0. Measured on the current manifest: 25 entries have jump tables with arms outside
  their span. Examples:
    - 0xB06E0: arms 0xB0970 and 0xB09D9 are in no dispatch, and recovered.c calls the fatal stub
      `sub_000B09D9`.
    - 0x10A0E0 and 0x433C0: arms resolve nowhere.
    - 0x2DBE0, 0x2F600, 0x129180: arms resolve only through dispatch shims.
  They overlap the existing `KNOWN_OPEN` set in `tests/test_recovery_span_ownership.py`, so they are
  known. Still, this is a better-defined next detector than the 71-pair "adjacent entry" population
  the records call undiscriminated. None of the 25 was touched by this turn.
- **The misdispatch census from the start plan was not performed** (0xE9A40, 0x100AB0, 0x1199C0,
  0x13A340). The MISDISPATCH verdict measures 0 only because the over-run must also be unreached.
  Either record the drop as a PLAN_CHANGE or do the census.
- **Push and record discipline.**
  - The four reviewed commits are on `origin/master` (fast-forward, no force in the reflog).
  - HEAD is now 82a2c40 ("Record the living turn plan"), 1 ahead of origin and unpushed.
  - AGENTS.md requires a `PUSHED_TO: / BRANCH: / COMMIT: / REMOTE_URL: / RESULT:` line for each
    push. None exists for `b065050..d8632ce`.
  - Small internal inconsistency in the plan: the provenance tool defect "hit twice more" versus
    "four times"; the living plan says "four more times".
- **Advisor outage:** handled correctly under §1. Three failed spawns were reported, no fallback
  route was used, and no ruling is claimed. Because a route that fails three times is a staffing
  problem (owner-reserved, §7), it should be raised with the owner directly, not only written in
  the plan.

VERIFIED (reproduced, no action needed):
- Gates:
  - `check-hidden-entries.py --selfcheck`: 5/5 controls, each bad span gives the expected verdict
    and its good span is non-gating; exit 0.
  - Bare gate: 3157 entries, 2890 CLEAN / 265 OVER_RUN / 2 UNQUALIFIED, PASS, exit 0.
  - `check-stack-depth.py --selfcheck` 10/10.
  - `ctest --test-dir build -C Release`: 38/38.
  - `just check`: all checkers passed.
  - No `config/hidden-entries-baseline.json` exists.
- Repairs are real, not suppressions:
  - Running the detector on b065050's manifest reproduces 50/63.
  - The final manifest has 48 added starts, all consumed addresses (38 PROVED, 10 INFERRED).
  - Added bodies range from 50 to 1213 bytes, median 366 (the record's "360" is close).
  - All 50 containers changed end, and every new end equals the byte-derived body end.
  - 0x7DBD0, 0x1FFF0, 0x91C30 and 0x91D70 have their own entries and own bodies in recovered.c.
  - 0x7DBD0 occurs exactly once as an aligned dword (0x20D3C8).
  - recovered.c has 3157 bodies; the manifest has 3157 entries.
- 0xB5F3A removal is justified:
  - The jump table at 0xB6734 holds 9 arms
    [B5F16, B5F82, B6574, B632D, B670E×3, B6574, B632D]. 8 of them are at or beyond 0xB5F3A.
  - Arm 0xB5F16 runs through `call 0x1BAAA0` at 0xB5F35 into 0xB5F3A.
  - The full-span walk of 0xB5EB0 reaches 0xB5F3A and 0xB5F82; it is enumerated with
    exits = `ret 4` @0xB672F only.
  - 0xB5F3A has no rel32 or jcc reference. Its only raw dword (0x228214) sits among non-pointer
    neighbours (0x985100, 0x38009753, 0x700E006C).
  - Its old manifest evidence was generic 2026-09-21 alias-fold boilerplate.
  - Nothing else references `sub_000B5F3A`.
  - The rebuilt body has a real switch with no unresolved `sub_` call. Its residual RECOMP_ITAIL is
    only the default after the `_jt` match chain, the same pattern as 0x2ED30 and others.
  - The patch mechanism is legitimate, as in the precedent. Only the ledger ID is wrong (BLOCKING).
- Runtime (g07, `logs/runs/20261005-211627-927-g07-thunk`):
  - Profile exploratory; outcome `unhandled_exception`; 241.15 s.
  - g07's build-source manifest hash equals 6f2e4e2's manifest in CRLF form, so g07 ran the
    detector-repaired tree.
  - Exactly one `[RECOVERED] 0x00154540 returned; ABI verified` line, at 105311.
  - The last presents increment, `presents=1000`, is at line 75471, and line 82932 still shows 1000.
    The thunk therefore returned after the freeze.
  - `[ICALL] Failed to resolve VA 0x000B5F82` is at line 105318; the last ICALL frame is 0x000B5EB0
    and the exception code is 0xE0424943.
  - 546 ABI-verified lines, 0 ABI FAILURE lines.
  - Disclaimer hash is `5bdaea576b8509f5` (16 hex digits). The brief's `5bdaea576b85509f5` is a
    typo; the records are correct.
  - "Runtime-confirmed" is the right class for stop 20's ABI contract. It is not title progress,
    and the records say so.
- No title-screen or M15 claim.
- Repository safety:
  - `git ls-files game src/recomp` is empty.
  - Generated code is untracked.
  - The toolkit is at 6e6e056, clean, and in sync with origin/main.
  - Provenance history was preserved (amendments 39→41, regenerations 1→3).
```
