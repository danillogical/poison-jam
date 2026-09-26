# A4s-r6 planning brief — FROZEN

**Status:** **FROZEN and dispatched to the Planner.** The Advisor's hunk rulings are recorded in
`docs/reviews/a4s-r6-advisor-hunk-ruling.md`, which was the precondition for opening this brief.

## Planner route

`workbuddy-ai/kimi-k3` with **`reasoning_effort` omitted** — per the owner's direct instruction
(*"kimi k3 doesn't take an effort, so don't worry about effort for kimi k3"*), recorded in
`docs/reviews/startup-20260925-session-58e86358.md` and
`docs/reviews/a4s-r5-execution-startup-ruling.md`.

## THE ADVISOR'S RULINGS — binding, recorded verbatim in `docs/reviews/a4s-r6-advisor-hunk-ruling.md`

**Do not re-litigate these. They are Advisor authority under `docs/agent-workflow.md` §2.3.** Summary
for orientation only; the record is authoritative.

| Ref | Ruling |
|---|---|
| **Hunks 1–2** | **COMBINED form** with fixed precedence: (1) in-place KEVENT if `guest_va_is_inplace_kevent` accepts, (2) `ke_shadow_lookup` handle if non-NULL, (3) `XBOX_TO_NATIVE` fallback. **Upstream's `bridge_resolve_handle` tier is NOT carried over.** The ruling gives the **exact C text** for `bridge_KeSetEvent`, `bridge_KeResetEvent` and `bridge_KeWaitForSingleObject`. **Exactly one** `bridge_KeResetEvent` survives. Two further edits are authorized as pre-ruled HA: (a) delete upstream's clean-hunk `bridge_KeResetEvent` (comment through closing brace); (b) keep exactly one `case 138` per switch — delete the two at the upstream positions, keep local's. Locate each edit **by exact text, not line number**. |
| **Hunk 5** | **Per-line combined form** (derived by rule from repaired H1 + edit-application H2): `_FLAGS_UNDEFINED` loses `"lock xadd"` and keeps upstream's `"popfd"` + its 6 comment lines; `_EFLAGS_SETTERS` keeps local's line; hunk 6's H2 result stands. |
| **Hunk 8** | **LOCAL** — `"--functions", fns,`. Pinned; not left to judgment. |
| **Hunk 9** | **Full union**, local's members then upstream's, with upstream's `or insn.mnemonic in _RESULT_SNAPSHOT_SETTERS` clause kept. |
| **H1** | **Accepted as proposed**: `X contains Y` iff `added(Y) ⊆ added(X)` **and** `deleted(Y) ⊆ deleted(X)`; neither direction → H1 does not apply; identical versions → take either. |
| **H2** | **The Advisor REJECTED the Session's added delete-vs-keep precondition and kept the original one.** H2 applies iff no base line is changed by both sides (changed = deleted **or** replaced). **H2's action must be defined as EDIT APPLICATION** — start from base, apply each side's per-line edit (keep/delete/replace), then insert each side's insertions (local first at a shared point). **A union by line presence is forbidden.** Ambiguous per-line attribution (repeated identical lines, several equal-cost alignments) → H2 does not apply → UNDECIDED. |
| **`AC-MERGE`(d)** | Gets a **twin witness**: every base line a credited side deleted is **ABSENT** from the result, unless the other side replaced it. |
| **Post-resolution check** | **Required — structural only** (option (i)), run on the **resolved** tree **before the build**, over `SCOPE`. Must establish: (a) no conflict markers; (b) no two `case` labels with the same constant in one switch **on the same preprocessor branch path**; (c) no two file-scope definitions of the same identifier on the same branch path (declarations + one definition, tentative definitions, and identical macro redefinitions excluded). **Predicates:** PASS = zero hits **and** the coverage witness (positive control: reports the three known defects on `75083476`; negative control: zero on `0d7929c` and `766ecef`; reports its file/switch/definition counts). FAIL = any hit → **`R-CONFLICT`, never `R-BUILD`**. UNKNOWN = scanner cannot assign braces/branch paths in a changed file → that file is listed and the check does **not** PASS. **Backstop:** `R-BUILD` may be selected only if the first compiler error is **not** a redefinition or duplicate-case diagnostic (C2084 / C2196 / C2371 class) in a merge-changed file. |
| **H3** | Not audited (no corpus) — **say so in the packet**. |

### Session verification of the Advisor's H2 expectation (already done)

The Advisor asked for the corpus to be re-run under its H2 definition. Done:
`logs/a4s/verify-advisor-h2.py` and `logs/a4s/verify-hunk5-output.py` confirm **exactly** what it
predicted — hunk 5 changes from UNDECIDED to **H2 applies**, every other hunk keeps its classification
(hunk 3 → `H1 → OURS`; lifter 6/7 → H2 applies; hunks 1, 2, 8, 9 → H2 does not apply), and edit
application produces the ruled `_FLAGS_UNDEFINED` text (no `lock xadd`, `popfd` present, exactly 6
comment lines).

## The one task

> Design **`A4s-r6`** to resolve the five recorded UNDECIDED hunks using the Advisor's rulings, and
> repair the **H1/H2 rule defect** so the merge procedure remains mechanical and fail-closed.

**The Planner is not to restart the toolkit-sync investigation.**

## Inputs already established — do not re-prove unless a current source read contradicts a load-bearing premise

1. **Complete 9-hunk inventory** from the real merge attempt:
   `docs/reviews/a4s-r5-conflict-inventory.md` + raw text `logs/a4s/conflict-hunk-inventory.txt` +
   the conflicted files preserved verbatim as `logs/a4s/conflicted-*` (diff3 style).
   Classification: **1 HA, 1 H1, 2 H2, 5 UNDECIDED**.
2. **Advisor decisions for hunks 1, 2, 5, 8, 9** — *[to be inserted verbatim once received]*
   → `docs/reviews/a4s-r6-advisor-ruling.md`.
3. **Hunk 4 (`HA`) ruling remains binding:** LOCAL accepted `A3a-r25` model; upstream's NABM trap code
   may enter **only unarmed, with zero call sites** (`docs/reviews/a4s-ac97-hunk-ruling.md`).
4. **Scope ruling remains binding:** `SCOPE` = toolkit `src/` + `include/`; deleted names match only as
   **quoted string literals**; comments/docs/tests/unbuilt scaffolds are **inventoried, never failed**
   (`docs/reviews/a4s-ac97-hunk-ruling.md`, "Interpretation ruling 2: scope").
5. **Hunks 3, 6, 7 already have mechanical classifications** (H1 / H2 / H2) and are **not** reopened.
6. **The PowerShell 5.1 `AC-INV` command defect** and its recorded interpretation ruling already exist
   (`docs/reviews/a4s-r5-command-defect.md`) — the authorized `-f` pattern-file substitute and its three
   controls.
7. **The rollback executable hash difference** is measured as **relink timestamp behaviour** and is
   **not** an acceptance gate (`docs/reviews/a4s-r5-rollback-exe-hash.md`).
8. **The set-G baseline changed** after the staffing cleanup — any future criterion depending on set G
   must **measure the current baseline** (currently **9 pass / 1 fail**), never copy 8/2
   (`docs/reviews/a4s-execution-evidence.md`, "Premise change to `Q4`'s E1 exception").

## Measured facts the Planner may rely on (all in `docs/reviews/a4s-r6-*.md`)

- **Structural, clean-hunk defects the conflict-only rules cannot see:**
  - duplicate `case 138` labels in **both** dispatch switches (survive both resolutions; unconditional)
  - duplicate `bridge_KeResetEvent` **definition** (local's in hunk 1 + upstream's in a clean hunk)
  - `docs/reviews/a4s-r6-structural-findings.md`
- **The accepted contract test discriminates the hunk-1/2 semantics:**
  `jsrf_inplace_event_bridge` (from local-only `tests/kernel_inplace_event_test.c`, registered in the
  game's `CMakeLists.txt:118-125`) requires guest `KEVENT.SignalState` writes that upstream's bodies do
  not perform; the address identity is proven and all 15 helpers survive the merge.
  `docs/reviews/a4s-r6-contract-discrimination.md`
- **Reachability:** JSRF declares 120 kernel imports; **145 `KeSetEvent` and 159 `KeWaitForSingleObject`
  are declared; 108 `KeInitializeEvent`, 110, 138 `KeResetEvent` and 146 are NOT.**
  `docs/reviews/a4s-r6-ordinal-reachability.md`, `a4s-r6-event-ordinals.md`
- **Hunk 5 provenance:** local **moved** `lock xadd` from `_FLAGS_UNDEFINED` to `_EFLAGS_SETTERS`
  (commit `b3de85c`); upstream added `popfd`. `docs/reviews/a4s-r6-hunk5-provenance.md`
- **Hunk 8:** both forms are behaviourally equivalent (measured by executing the pinned
  `load_function_bodies`). `docs/reviews/a4s-r6-hunk8-equivalence.md`
- **Hunk 9:** the union is syntactically valid and lossless; its *correctness* depends on the merged
  snapshot machinery. `docs/reviews/a4s-r6-hunk9-refinement.md`
- **H1/H2 repair, tested against all 9 real hunks:** `docs/reviews/a4s-r6-h1-repair-proposal.md`
- **Evidence index:** `docs/reviews/a4s-r6-evidence-index.md`

## The H1/H2 rule requirement (owner's §5, verbatim intent)

`A4s-r6` must close the measured rule defect **structurally**. The rule may not treat *"this side added
zero lines"* as proof that all of that side's work is contained by the other side. Deletions and
replacements must be represented in the comparison so a deletion-only contribution cannot be silently
discarded. The repaired rule must remain:

- mechanical;
- bounded;
- safe for a literal executor;
- capable of PASS / FAIL / UNKNOWN;
- **not dependent on unbounded semantic analysis during execution.**

If the only honest way to classify some future hunk is `UNDECIDED`, that is **preferable** to a
mechanically wrong merge.

**Measured scope of the defect:** it is **not** H1-only. H2 has the same class and it bites on hunk 5
itself — hunk 5 satisfies H2's precondition while H2's action (*"keep both sides' edits"*) is undefined
for a base line one side deletes and the other keeps. Repairing H1 alone would move the failure one
step down the rule chain.

## Workflow requirements for this revision (owner's §4, §6)

- **Mandatory early Opus shape preflight.** Write the **first viable sketch (≤ ~15 lines)** — bounded
  claim, packet class, material unknowns, proposed merge procedure/experiment, outcome rows — and send
  it to the persistent Advisor **as soon as it exists and no later than 20 Planner tool calls**.
  Ask only the four shape questions and require exactly:
  `SHAPE: PROCEED | REDIRECT | DISCOVERY_FIRST` / `REASON:` / `POLICY_ISSUE:` / `REVERSED_BY:`.
  `PROCEED` does **not** mean the Advisor then performs the full packet review.
- After `PROCEED`, finish the packet **without further investigation** unless a specifically named
  unresolved fact can still change its shape.
- **Do not expand adjacent cleanup or architecture work** into `A4s-r6` unless omitting it could
  plausibly cause a false PASS, false FAIL, wrong merge implementation, wrong evidence binding, or
  unsafe execution. Otherwise record it as a **follow-up lead**.
- **Checkpoints:** at 40 calls write sketch 2 + yield + new forecast and send to the Advisor; at 60
  calls **stop investigating** and send sketch 3 + yield + remaining unknowns. The Advisor alone may
  authorize an extension.
- **Adequacy review by a FRESH Kimi Planner** (the Opus shape preflight is **not** adequacy review).
  Return the §5.3 block. `ADEQUATE` exactly when `BLOCKING: NONE` and `PREMISE_FRESHNESS` is not `FAIL`.
  **`ADEQUATE` ends plan iteration** — freeze and promote that exact revision, no polishing.

## Packet-specific requirements to consider (Planner decides; these are the measured inputs)

1. A **structural post-resolution check** that is not conflict-driven — at minimum duplicate `case`
   labels within one switch, and duplicate function definitions at file scope — evaluated on the
   **resolved** tree **before the build**, so a clean-hunk structural defect selects a conflict-class
   row instead of being misattributed to `R-BUILD`.
2. The hunk-1 ruling must state **which `bridge_KeResetEvent` definition survives**, not merely which
   side's `KeSetEvent` text is taken.
3. The two `case 138` duplicates need an explicit disposition (keep exactly one per switch).
4. Consider the `AC-TEST` **set-G carve-out** the Advisor recommended (modelled on the KX carve-out,
   naming failures by **test identity**, re-derived per execution and never carried forward).

## Standing constraints

- Write scope, `Stop if` conditions, `HA`, the `SCOPE` boundary guard and the decision rows are the
  packet's own; nothing in this brief overrides them.
- **Never push to `upstream`.** Push toolkit `main` to `origin` only at closure, after ACCEPT.
- The game repository has no remote; do not add one.
- Preserve the owner's uncommitted `docs/agent-workflow.md`.
