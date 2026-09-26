# A4s-r6 evidence index

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Purpose:** one place naming every A4s-r6 measurement record, what it establishes, and what it does
**not**. Written so the Advisor's ruling and the Planner's brief can cite exact records instead of
re-deriving facts.

## Repository pins

| | Revision | State |
|---|---|---|
| toolkit local (`ours`) | `0d7929c86771dd0b971941592fd4f15436116e82` | clean |
| toolkit upstream (`theirs`) | `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` (= `v0.11.0` = `upstream/main` = `origin/main`) | — |
| merge base | `051a128df5ec27ef14f1ceaaead11c5457321eef` | — |
| conflict-free merge tree (read-only preview) | `75083476d3277f5c6d91eb7040f7a3ce5dc25335` | — |
| game | `c1cdb9157bcb26c071c2ad83b69c85925f1c8d92` | owner's `docs/agent-workflow.md` edit uncommitted |

## The nine hunks, as classified by the real merge attempt (`A4s-r5` step 3, row `R-CONFLICT`)

| # | File | Lines | Rule | Status |
|---|---|---|---|---|
| 1 | `kernel_bridge.c` | 1365–1404 | **UNDECIDED** | **Advisor ruling pending** |
| 2 | `kernel_bridge.c` | 1425–1445 | **UNDECIDED** | **Advisor ruling pending** |
| 3 | `kernel_bridge.c` | 8981–8988 | H1 | closed (not reopened) |
| 4 | `xbox_memory_layout.c` | 2123–2145 | **HA** | closed — LOCAL (`a4s-ac97-hunk-ruling.md`) |
| 5 | `lifter.py` | 400–412 | **UNDECIDED** + H1 rule defect | **Advisor ruling pending** |
| 6 | `lifter.py` | 426–437 | H2 | closed (not reopened) |
| 7 | `lifter.py` | 1276–1297 | H2 | closed (not reopened) |
| 8 | `test_icall_feedback.py` | 116–124 | **UNDECIDED** | **Advisor ruling pending** |
| 9 | `translator.py` | 987–994 | **UNDECIDED** | **Advisor ruling pending** |

Raw hunk text: `logs/a4s/conflict-hunk-inventory.txt`; conflicted files preserved verbatim as
`logs/a4s/conflicted-*`.

## Evidence records

| Record | Establishes | Does NOT establish |
|---|---|---|
| `docs/reviews/a4s-r6-advisor-hunk-brief.md` | the evidence package sent to the Advisor: exact base/local/upstream text for hunks 1, 2, 5, 8, 9 plus surrounding facts | — (a brief, not a measurement) |
| `docs/reviews/a4s-r6-structural-findings.md` | **Finding 1:** the merge produces duplicate `case 138` labels in **both** dispatch switches (clean hunks; survive both resolutions; all sites unconditional). **Finding 2:** `bridge_KeResetEvent` is defined twice (local's in hunk 1, upstream's in a clean hunk); LOCAL resolution → 2 definitions, UPSTREAM → 1. Plus the general "both sides added it" population: exactly **1** function and exactly **1** label (`138`, in each of 2 switches). | that any *other* construct class is clean — the scan is textual, does not preprocess, and was applied to `kernel_bridge.c` / `kernel.h`, not all 153 upstream-changed files |
| `docs/reviews/a4s-r6-contract-discrimination.md` | the **accepted, currently-passing** ctest `jsrf_inplace_event_bridge` (from local-only `tests/kernel_inplace_event_test.c`) asserts guest `KEVENT.SignalState` transitions; the test's `signal_state(va)` and the runtime's `XBOX_TO_NATIVE(va)+4` are **provably the same address**; upstream's `KeSetEvent`/`KeResetEvent`/`NtClearEvent` write **no** guest memory, so upstream's form cannot satisfy those assertions | the *mechanical* build-and-run outcome (no merged tree was compiled) |
| `docs/reviews/a4s-r6-ordinal-reachability.md` | JSRF's XBE declares **120** kernel imports; **ordinal 138 (`KeResetEvent`) and 108 (`KeInitializeEvent`) are NOT declared**; coverage witness: all **27** measured-called ordinals are present in the declared table (zero missing) | that the guest never computes an ordinal dynamically and calls a thunk slot directly (no guest-code search done); the table's provenance (from `game/mygame_analysis.json`, not re-derived this session) |
| `docs/reviews/a4s-r6-event-ordinals.md` | **reachable** event ordinals: 145 `KeSetEvent`, 159 `KeWaitForSingleObject`, 189 `NtCreateEvent`, 186 `NtClearEvent`, 225 `NtSetEvent`, 16 `ExEventObjectType`. **Unreachable:** 108, 110, 138, 146. Positive control: 190 `NtCreateFile` (known called) is declared | that `declared` means *observed called* — none of 145/159/186/189/225 is in the measured-called set, because the run stops at the DSP spin first |
| `docs/reviews/a4s-r6-size-convention-hazard.md` | local's dispatcher guard requires guest `+2` (Size) `== 4`; local never writes it; upstream's `KeInitializeEvent` writes `16`; the accepted test writes `4`; the merge tree contains both. **Latent** type-confusion scenario if an upstream-created event reaches local's `else` branch | that the hazard is **live** — it requires ordinal 108, which is not imported; and which `Size` convention is *correct* (an inference from the numbers, not a cited source) |
| `docs/reviews/a4s-r6-hunk5-provenance.md` | local **moved** `lock xadd` out of `_FLAGS_UNDEFINED` into `_EFLAGS_SETTERS` (commit `b3de85c`), it did not merely delete it; upstream added `popfd`; precedence makes `_FLAGS_UNDEFINED` win. Includes an explicit **correction/retraction** of an earlier hunk-5/6 coupling claim | — |
| `docs/reviews/a4s-r6-hunk8-equivalence.md` | measured by **executing** the pinned `load_function_bodies`: local's existing-empty-file form → `[]`, upstream's missing-path form → `None`, both falsy, consumer is `if bodies:` → identical test outcome | — |
| `docs/reviews/a4s-r6-hunk9-refinement.md` | the hunk-9 **union is syntactically valid and lossless** (corrects the earlier "mutually exclusive" wording); `_RESULT_SNAPSHOT_SETTERS` does not cover `inc`/`dec`/`xadd`/`lock xadd` | that the union is *correct* — that depends on the merged snapshot machinery |
| `docs/reviews/a4s-r6-clean-hunk-semantics.md` | a **clean-hunk semantic change to REACHABLE code**: `bridge_KeInitializeTimerEx` (ordinal **113, declared**) merges silently to **upstream's** form (local's `MEM16` type write gone; upstream's `Size = 40` + `ke_shadow_insert` present), outside every conflict hunk. The result is **coherent** — upstream already had the `g_timers` table, so the merged timer machinery is upstream's version wholesale, not a hybrid | that upstream's timer model is *correct* for JSRF (only that it is coherent); that the path is exercised (ordinal 113 is declared but not measured-called) |
| `docs/reviews/a4s-r6-h1-repair-proposal.md` | a **tested** repair for the H1 deletion-vacuity defect (containment must include deletions), verified against all 9 real hunks: hunk 3 still resolves `H1 → OURS` (unchanged), hunk 5 now correctly refuses. **Plus the measured finding that H2 has the same defect class** — hunk 5 satisfies H2's precondition while H2's action is undefined for a base line one side deletes and the other keeps — with a matching H2 repair tested against hunks 5, 6, 7 | that H3 has the same defect (no H3-classified hunk exists in this merge, so there is no corpus to test); that the repair is adopted (the Planner owns wording, the Advisor owns the property) |
| `docs/reviews/a4s-r6-hunk5-decidability.md` | hunk 5 is **decidable from source**: in the merge tree `"lock xadd"` sits in **both** `_FLAGS_UNDEFINED` (upstream) and `_EFLAGS_SETTERS` (local), and both revisions test the former first — a self-contradiction. Local's move is corroborated inside the merge tree by two independent places; upstream's kept line by none. So the correct form is a **per-line combination**, not a side choice | that a *generic* checker can detect the contradiction — it can detect the **shape** (token added to one set, removed from another by the same hunk), but "these two sets are mutually exclusive" needs the consumer's precedence |
| `docs/reviews/a4s-r6-advisor-ruling.md` | **PLACEHOLDER — no ruling received yet.** Exists so the ruling can be recorded verbatim on arrival; must not be cited as a ruling | — |
| `docs/reviews/a4s-r6-planning-brief-draft.md` | the **draft** planning brief for the Kimi Planner, with every established input, the measured facts, the H1/H2 requirement, and the workflow constraints | — (a draft; **not sent** until the Advisor's rulings are recorded) |

## Corrections made during this work (recorded, not hidden)

1. **Hunk 9 wording** — the inventory's "union is not a union of intent" was a pre-check judgment;
   `a4s-r6-hunk9-refinement.md` records the mechanical result and reframes the question as
   consistency, not invalidity.
2. **Hunk 5/6 coupling** — claimed in a delta to the Advisor, then **retracted** with measurement:
   hunk 6's own H2 union already drops `popfd`, so no coupling is required
   (`a4s-r6-hunk5-provenance.md`, "Note on the other half of the hunk").
3. **Size-convention hazard scope** — presented as a live hazard in one delta, then narrowed to
   **latent** once ordinal 108 was measured as not imported (`a4s-r6-ordinal-reachability.md`).
4. **`A4s-r5` rollback exe hash** — an earlier "deterministic rebuild" inference was **retracted**
   after the Advisor identified the flaw; the mechanism is now measured
   (`docs/reviews/a4s-r5-rollback-exe-hash.md`).
5. **H1-repair parser bug** — the first test run used the **2-way** preview tree `75083476`, which has
   no `|||||||` base sections, so a parser that assumed `base_m < sep` mis-read every hunk. Corrected
   to run against the **real diff3** hunks saved as `logs/a4s/conflicted-*`, which is the corpus the
   packet's H-rules actually operate on. Recorded because the first run's numbers were wrong.
6. **H2 same-defect finding** — initially reported to the Advisor as "not established"; then measured
   and reported as **established**, with hunk 5 as the instance.
7. **My H2 repair was rejected by the Advisor** and is superseded. I proposed an added delete-vs-keep
   precondition; the Advisor ruled that *"kept unchanged" is not an edit* and that H2's **action** must
   instead be defined as **edit application**. My precondition would have sent a correctly decidable
   hunk to `UNDECIDED` — safe, but wrong. The Advisor's expectation was then verified exactly
   (`logs/a4s/verify-advisor-h2.py`). Recorded because a rejected proposal is part of the trail.
8. **A `.text`-only scan gave a wrong answer.** An early guest-call search reported "0 references" to
   ordinals 145/159 because it scanned only the `.text` section; the XBE also has `D3D` and `XPP` code
   sections with their own thunk references. The same "one spelling / one section" mistake `AGENTS.md`
   warns about. The Advisor's ruling did **not** depend on my result — it reasoned from the reachable
   timer populator instead — but the error is recorded so the scan is not reused as-is.

## Known record defects (pre-existing, not introduced here)

- **`docs/reviews/a4s-revision-history.md` does not exist**, although the `A4s-r5` packet cites it as
  its revision log (`docs/packets/a4s-toolkit-sync.md`, "Revision log" line). Other packets do have
  such files (`a4a-`, `a4b-`, `a4p-revision-history.md`). The `A4s` revision narrative currently lives
  only in the git log (`dbacb11`, `945bef2`, `23d3266`, `45c9cee`, `5530e08`). **Follow-up lead** —
  a record-pointer defect is a §3.2 advisory, not a blocking one, and it reopens nothing.
- **The frozen `A4s-r5` packet is safe.** `docs/packets/a4s-toolkit-sync.md` still matches `HEAD`
  (`git status --porcelain` empty) and its SHA-256 is still
  `09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB`, so it is recoverable from git
  even after `A4s-r6` replaces it.

## Standing constraints the ruling must respect

- **`SCOPE`** = toolkit `src/` + `include/`; deleted names match only as quoted string literals;
  comments/docs/tests/unbuilt scaffolds are **inventoried, never failed**
  (`docs/reviews/a4s-ac97-hunk-ruling.md`, "Interpretation ruling 2: scope").
- **Hunk 4 (`HA`)** resolves to LOCAL; upstream's NABM trap enters **unarmed with zero call sites**.
- **`docs/jsrf-run-profiles.md`** rules 1–5 govern merges (local admitted form wins; new upstream
  device behaviour enters dormant; greps search `SCOPE` in semantic form; inventory by content, not
  conflict status; evidence-semantic scope = what the evidence binary can execute).
- **Hunks 3, 6, 7** keep their existing mechanical classifications; not reopened.
- The **set-G baseline changed** after the staffing cleanup — any criterion depending on set G must
  re-measure it (currently 9 pass / 1 fail), never copy 8/2.
