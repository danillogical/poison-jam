# A4s-r6: hunk 9 — the union IS mechanically well-formed (correction to the inventory note)

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured refinement of `docs/reviews/a4s-r5-conflict-inventory.md`'s hunk-9 entry.
**Script:** `logs/a4s/hunk9-union.py`.

## What the inventory said, and what is now measured

`docs/reviews/a4s-r5-conflict-inventory.md` recorded hunk 9 (`translator.py:987-994`) as
**UNDECIDED**, reasoning that *"The mnemonics are mutually exclusive alternatives, so a union is not a
union of intent"*. That was a judgment about **intent**, and it was reached **before** the mechanical
check below. The mechanical check shows the union is in fact **syntactically valid and lossless**, so
the classification deserves to be stated more precisely. The row selection (`R-CONFLICT`) is
**unchanged** — four other hunks remain UNDECIDED — but the reason recorded for hunk 9 should be
accurate.

## Measured

Candidate merged tuple = local's `("cmp", "test", "bsf", "bsr", "cmpxchg", "lock cmpxchg", "xadd",
"lock xadd")` plus upstream's `"inc", "dec"`, with upstream's added clause
`or insn.mnemonic in _RESULT_SNAPSHOT_SETTERS` retained:

| Check | Result |
|---|---|
| Syntax of LOCAL-only form | **valid Python** |
| Syntax of UPSTREAM-only form | **valid Python** |
| Syntax of UNION form | **valid Python** |
| Union drops anything LOCAL had | **no** — local members missing from union: `[]` |
| Union drops anything UPSTREAM had | **no** — upstream members missing from union: `[]` |
| Union adds vs LOCAL | `inc`, `dec` |
| Union adds vs UPSTREAM | `xadd`, `lock xadd` |
| Overlap with `_RESULT_SNAPSHOT_SETTERS` | **none** — that set is `{adc, add, and, neg, or, sal, sar, sbb, shl, shld, shr, shrd, sub, xor}`, and **no** candidate mnemonic (`cmp, test, bsf, bsr, cmpxchg, lock cmpxchg, xadd, lock xadd, inc, dec`) is a member |

**So the union is not mechanically contradictory.** `_RESULT_SNAPSHOT_SETTERS` does **not** already
cover `inc`/`dec` (or `xadd`/`lock xadd`), which is why upstream had to list `inc`/`dec` explicitly in
the tuple rather than relying on the new clause.

## Why it is still UNDECIDED (and still a real semantic question)

The union is **well-formed** but it is not obviously **correct**, because the two sides' additions
encode different behaviours:

- local added `xadd`/`lock xadd` because local's lifter routes those through `RECOMP_ATOMIC_*` and the
  snapshot is needed to test the compare the instruction actually performed;
- upstream added `inc`/`dec` as part of a broader result-snapshot rule change (its
  `_RESULT_SNAPSHOT_SETTERS` and `ZF_FROM_DEST` machinery), and `inc`/`dec` are precisely the pair
  upstream's own comment says *"the rule inc/dec has followed since 'Result at the flag-setting
  instruction, before later MOVs'"*.

Whether declaring snapshot temporaries for **all ten** mnemonics is correct depends on whether both
sides' downstream machinery (`_result_snapshot`, `_RESULT_SRC_SETTERS`, `ZF_FROM_DEST`) is also
merged — i.e. it is a question about the **merged whole**, not about this line in isolation. A union
here is plausible but not decidable from the hunk alone, which is exactly what `UNDECIDED` means.

## Consequence for `A4s-r6`

The Planner should not treat hunk 9 as "a union is impossible". The accurate statement is: **the union
is syntactically valid and lossless, but its correctness depends on the merged state of the snapshot
machinery**, so it needs either an Advisor ruling grounded in that machinery or an explicit packet
step that establishes it. The distinction matters because the two lead to different procedures: a
"union is invalid" hunk needs a side choice, whereas this one needs a **consistency** argument.
