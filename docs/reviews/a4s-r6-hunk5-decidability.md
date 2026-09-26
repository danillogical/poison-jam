# A4s-r6: hunk 5 is decidable from source — the merge tree contradicts itself

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence for the Advisor's hunk-5 ruling. Not a ruling.
**Script:** `logs/a4s/hunk5-decidability.py`.

## The measurement

In the merge tree `75083476` — i.e. the tree the packet's rules would produce if hunks were resolved
textually — the mnemonic `"lock xadd"` appears in **both** flag-classification sets:

| Set | Contains `"lock xadd"`? | Source side |
|---|---|---|
| `_FLAGS_UNDEFINED` | **YES** | upstream's line, kept by the hunk |
| `_EFLAGS_SETTERS` | **YES** | local's line (added by commit `b3de85c`) |
| `translator.py` snapshot tuple | **YES** | local's line (`"lock cmpxchg", "xadd", "lock xadd"`) |

**Both revisions test `_FLAGS_UNDEFINED` before `_EFLAGS_SETTERS`** (local `3300` vs `3304`; upstream
`3612` vs `3616`), so the merge tree's behaviour would be: `lock xadd` is treated as
**flags-undefined** — while two other tables in the *same tree* classify it as a **tracked atomic
flags-setter**. That is a self-contradiction inside one file, produced by resolving hunk 5 textually
to upstream.

## Why this makes hunk 5 decidable from source

The question is not a matter of taste between two defensible models. Local did not merely delete a
line; it **moved** `lock xadd` from `_FLAGS_UNDEFINED` to `_EFLAGS_SETTERS` (measured: walking the 24
local-only commits, exactly one — `b3de85c` — changes the membership, and its diff is the two-line
move). Local's move is **corroborated inside the merge tree by two independent places**:
`_EFLAGS_SETTERS` and `translator.py`'s snapshot tuple. Upstream's kept line is corroborated by
**nothing** in the merge tree — it is the only place `lock xadd` is called flags-undefined.

So the correct merged form is a **per-line combination**, not a side choice:

- `_FLAGS_UNDEFINED`: **remove** `"lock xadd"` (local's reclassification wins) and **keep** `"popfd"`
  (upstream's addition; local never had it in that set);
- `_EFLAGS_SETTERS`: **keep** local's `"xadd", "lock xadd", "lock cmpxchg"` line;
- `translator.py`: unchanged in this respect (both sides' additions compose; see
  `a4s-r6-hunk9-refinement.md`).

The result is **consistent**: `lock xadd` appears only in `_EFLAGS_SETTERS`, `popfd` only in
`_FLAGS_UNDEFINED` (hunk 6's H2 union drops it from `_EFLAGS_PRESERVE`, measured in
`a4s-r6-hunk5-provenance.md`).

## The general form, for the rule repair

This is a **second, independent argument** for representing deletions in the containment test, and it
is stronger than "a deletion might be lost":

> When one side's contribution is a **reclassification** — it removes a token from one table and adds
> it to another — a textual side choice can leave the token in **both** tables, producing a
> self-contradictory tree. The contradiction is **detectable mechanically** (the same token in two
> mutually-exclusive sets), which makes it a candidate for the post-resolution check rather than for
> human judgment.

**Limits.** "Mutually-exclusive sets" is a *semantic* property of these particular tables (the code
tests them in a fixed order), not something a generic checker can infer. A mechanical check could
detect the *shape* — a token added to one set and removed from another by the same hunk — but
deciding that the two sets are mutually exclusive requires knowing the consumer's precedence. So this
is a **lead** for the Planner: a generic check should report the shape, and the packet should state
which sets are order-sensitive.
