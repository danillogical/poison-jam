# A4s-r6: the step-3 guard is decidable — the recorded inventory is a faithful capture

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** Session measurement supporting execution. Not a ruling, and not a packet revision.

## Why this was checked

`A4s-r6` step 3 carries a guard:

> **Guard:** the conflicted paths must be exactly the 5 recorded files …, **and** the conflict hunks must
> be exactly the 9 recorded in `logs/a4s/conflict-hunk-inventory.txt` (same files, same count, matching
> base/ours/theirs text). **Any difference → stop, roll back, `R-CONFLICT`**.

`R-CONFLICT` is a **fail-closed** row. If the inventory were *not* actually a faithful capture of the
merge, a literal executor would select `R-CONFLICT` on a **correct** merge — a **false FAIL**. That is a
§3.1-class risk, so the guard's decidability was measured before execution rather than discovered during
it.

## Result

**The inventory is a faithful capture.** Comparing all 9 hunks against the saved diff3 conflicted files
(`logs/a4s/conflicted-*`):

| File | Hunks | Sections match | Line spans match |
|---|---|---|---|
| `src/kernel/kernel_bridge.c` | 3 | **yes** | **yes** |
| `src/kernel/xbox_memory_layout.c` | 1 | **yes** | **yes** |
| `tools/recomp/lifter.py` | 3 | **yes** | **yes** |
| `tools/recomp/test_icall_feedback.py` | 1 | **yes** | **yes** |
| `tools/recomp/translator.py` | 1 | **yes** | **yes** |

**27 sections compared (9 hunks × ours/base/theirs), 0 mismatches.** Every recorded line span equals the
saved marker block exactly — `1365–1404`, `1425–1445`, `8981–8988`, `2123–2145`, `400–412`, `426–437`,
`1276–1297`, `116–124`, `987–994`.

## The comparison the guard requires

The inventory is a **rendered** document, not raw conflict text: it labels sections
`[OURS / 0d7929c]`, `[BASE / 051a128]`, `[THEIRS / 766ecef]`, contains **no** `<<<<<<<` markers, and
indents every content line. A **raw byte comparison against the real conflicted files would therefore
fail on indentation and blank lines alone** — and that is *not* what the guard means. The correct
comparison is:

- **non-blank lines only**;
- **per-line leading/trailing whitespace stripped**;
- sections terminated at the next `[SECTION]`, `--- HUNK`, `# FILE`, or `####…` boundary.

Under that comparison all 27 sections match.

## Two Session errors found and corrected here (recorded because the first two answers were wrong)

My first two aggregate scripts both reported **"`theirs` 0/9 match"**, which would have meant the guard
could not be evaluated. **Both were bugs in my parser, not defects in the record:**

1. **The `####…` file-separator line was absorbed into the preceding hunk's `theirs` section.** Since
   `theirs` is always the last section, only it was affected — which is exactly why the failure pattern
   was "`ours` 9/9, `base` 9/9, `theirs` 0/9" instead of something uniform.
2. **`norm()` kept trailing blank lines.** The inventory has blank lines after each section; the saved
   conflict files do not. Every hunk therefore "differed" on blanks alone.

What broke the deadlock was **printing one hunk verbatim from both sources** (`compare-one-hunk.py`)
instead of refining the aggregate. That direct comparison showed `translator.py` hunk 1 matching
perfectly in all three sections — which proved the record was sound and the aggregate was wrong.

This is the **third** time in this work that an aggregate measurement was wrong while the underlying
record was fine, and each time the fix came from looking at one case directly rather than from another
pass over the aggregate. Noted as a working lesson, not as a policy.

## What this does NOT establish

- It does not verify the **9-hunk classification** (1 HA, 1 H1, 2 H2, 5 UNDECIDED) — that is
  `docs/reviews/a4s-r5-conflict-inventory.md`'s claim, checked separately by the adequacy review.
- It does not prove the **real** merge will reproduce these hunks; it establishes that **if** it does,
  the guard can be evaluated and will pass. The guard itself still catches a genuine divergence.
- It says nothing about hunks **outside** the recorded 9 — the guard handles those by `R-CONFLICT`, and
  the packet forbids resolving them by analogy.
