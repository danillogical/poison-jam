# A4s-r5 step 3 — complete conflict-hunk classification (R-CONFLICT)

**Date:** 2026-09-25  **Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`
**Merge attempt:** `git -c merge.conflictStyle=diff3 merge --no-ff --no-commit 766ecef` from `0d7929c`.
**Result:** merge in progress, **5 conflicted files, all inside the packet's 10-file set**:
`src/kernel/kernel_bridge.c`, `src/kernel/xbox_memory_layout.c`, `tools/recomp/lifter.py`,
`tools/recomp/test_icall_feedback.py`, `tools/recomp/translator.py`. No conflict outside the 10.
**Selected row: `R-CONFLICT`** — UNDECIDED hunks exist. Toolkit `main` is rolled back to `0d7929c`.

Raw evidence: `logs/a4s/conflict-hunk-inventory.txt` (every hunk's base/ours/theirs text),
`logs/a4s/conflicted-*.c`/`.py` (the conflicted files verbatim, saved before rollback).

## Classification of all 9 hunks

Rules applied **in order** (packet step 3): `HA`, then `H1` containment, `H2` disjoint union,
`H3` version/comment only, otherwise `UNDECIDED`.

| # | File | Hunk lines | Rule | Why |
|---|---|---|---|---|
| 1 | `kernel_bridge.c` | 1365–1404 | **UNDECIDED** | One base line (`g_eax = xbox_KeSetEvent(...)`) changed differently by both sides in code: local adds the in-place-kevent branch and a whole `bridge_KeResetEvent` body; upstream replaces it with `ke_shadow_lookup`/`bridge_resolve_handle`/`SetEvent(h)`. Neither side contains the other, and the union is not valid — upstream's text references `guest_va` and `h`, which do not exist in local's version. |
| 2 | `kernel_bridge.c` | 1425–1445 | **UNDECIDED** | One base line (`g_eax = xbox_KeWaitForSingleObject(XBOX_TO_NATIVE(object), …)`) changed differently by both sides in code: local wraps it in the in-place-kevent branch plus `recomp_diag_record(9, …)`; upstream passes `h`. Same-line, different-code change. |
| 3 | `kernel_bridge.c` | 8981–8988 | **H1** | Base `static int g_kernel_dispatch_slot = -1;`. Both sides independently produce `static RECOMP_TLS int g_kernel_dispatch_slot = -1;`. Upstream's only added line is present verbatim in local's version, and local adds one further line (a comment) that upstream lacks → upstream is contained in local → **take the containing side: LOCAL** (keeps local's comment and `RECOMP_TLS`). |
| 4 | `xbox_memory_layout.c` | 2123–2145 | **HA** | Pre-ruled by the Advisor (`docs/reviews/a4s-ac97-hunk-ruling.md`, rule (b)): resolve to the **LOCAL** side. Local keeps the `VirtualProtect`-failure `else`; upstream's `\|= MCPX_AC97_CODEC_READY` and `ac97_arm_write_trap()` call are HA omissions. Not undecided. |
| 5 | `lifter.py` | 400–412 | **UNDECIDED** | Base line `"lock xadd",           # Lock prefix - complex flag behavior`. Local **deletes** it (local handles `lock xadd` as a result-snapshot setter instead); upstream **keeps** it and appends a comment block plus `"popfd",`. The two applicable rules disagree and neither is mechanical: **H1** read literally is *vacuously* satisfied — local added no lines, so "every line local added is in upstream's version" is true — which would take upstream and **silently discard local's deletion**, contradicting H1's own purpose ("take the containing side" so no work is lost); **H2** would instead union a deletion with an insertion. Resolving this needs a semantic judgment the packet forbids ("Do not choose a side, rewrite, or merge semantics by judgment"). Recorded as an **H-rule gap** for the Planner/Advisor. |
| 6 | `lifter.py` | 426–437 | **H2** | `_EFLAGS_PRESERVE`: base line 1 `"pushfd", "popfd", "pushal",` was changed by **upstream only** (drops `popfd`, adds a comment); base line 2 `"sgdt", "ljmp", "sfence",` was changed by **local only** (appends `"wbinvd"`). Each base line changed by at most one side → keep both edits; the union is syntactically valid. |
| 7 | `lifter.py` | 1276–1297 | **H2** | Base section is **empty**; both sides only **inserted** at the same point (local: `batch_spans`/`_batch_span_starts` plus comment; upstream: `imm_code_refs` plus comment). H2's "both only inserted at the same point" → keep both, **local lines first, then upstream**. |
| 8 | `test_icall_feedback.py` | 116–124 | **UNDECIDED** | One base line (`assert main(["--db", db, "seeds", …])`) changed differently by both sides in code: local passes `--functions fns`; upstream passes `--functions os.path.join(tmp, "none.json")`. Same line, two different arguments — a semantic choice, not a union. |
| 9 | `translator.py` | 987–994 | **UNDECIDED** | One base line (`"lock cmpxchg")`) changed differently by both sides in code: local appends `"xadd", "lock xadd"`; upstream appends `"inc", "dec"` **and** adds a further clause line `or insn.mnemonic in _RESULT_SNAPSHOT_SETTERS`. The mnemonics are mutually exclusive alternatives, so a union is not a union of intent; the added clause also depends on a name local introduces elsewhere. |

**Totals:** 1 × HA, 1 × H1, 2 × H2, **5 × UNDECIDED**.

## Why this selects `R-CONFLICT` (fail-closed, as the plan expected)

`R-CONFLICT`'s condition is: *"P0 passes and: a conflict outside the 10 files, an UNDECIDED hunk, or
`AC-MERGE`/`AC-KEEP` (incl. (iv)–(vi))/`AC-INV` FAIL"*. **Five UNDECIDED hunks** are recorded, so the
row matches on its own terms. Per step 3 the packet does **not** authorize choosing a side, rewriting,
or merging semantics by judgment for these; the inventory is the next brief.

The plan predicted this outcome — *"Expected first result: `R-CONFLICT`"*, with the Planner's
read-only preview having found *"at least six undecidable hunks"* and deliberately not pre-ruling
them. This attempt supplies the **complete** inventory from a single merge attempt, which is what
that prediction was for. (This Session's mechanical count is **5** UNDECIDED; the preview's "at least
six" is a Planner estimate, and the difference is not load-bearing — the row matches either way. The
candidate sixth is hunk 5, whose H1-vs-H2 ambiguity is recorded above.)

`R-CONFLICT` next action: *"Planner `A4s-r6` with the full hunk inventory as its brief, naming a
resolution per hunk; hunks that choose between local accepted runtime and an upstream model
(e.g. `kernel_bridge.c` event paths, `xbox_memory_layout.c` AC'97) go to the Advisor first."* Hunks 1,
2, 5, 8 and 9 are exactly such side-choosing hunks.

## Rollback (step 1 procedure, merge-in-progress branch)

The packet's step 1 rollback says: *"if a merge is in progress `git -C $T merge --abort`; else
`git -C $T branch -f a4s-merge-attempt HEAD` … then `git -C $T reset --hard 0d7929c`; then
`python -X utf8 scripts\build-jsrf.py` and record the exe SHA-256 (it must equal the step-2 pre-merge
SHA; if not, record — the restored tree is still `0d7929c`)."* A merge **was** in progress, so the
`merge --abort` branch applies. Because an aborted merge leaves no commit, there is no `HEAD` to keep
on `a4s-merge-attempt`; the attempt's evidence is preserved instead as the saved conflicted files and
the hunk inventory under `logs/a4s/`, which is what the `a4s-merge-attempt` branch exists to preserve.
This is recorded rather than silently worked around.
