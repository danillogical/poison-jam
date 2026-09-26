# A4s-r6: AC-STRUCT caught a real defect in the Session's own resolution

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** defect **found, corrected, re-verified**. Recorded because it is direct evidence for the
Advisor's ruling that `AC-STRUCT` must run **before** the build, and because the failure mode is
instructive.

**Merge commit `M`** = `3f8bf67c450861aefcbc376698750bc1446bc9dd` (parents `0d7929c`, `766ecef`).

## What happened

`A4s-r6` edit (b) says:

> Keep exactly **one** `case 138` per dispatch switch — keep local's two positions (`75083476` 8040 and
> 8502); delete the upstream duplicates `case 138: return  4;  /* KeResetEvent (1) */` and
> `case 138: return bridge_KeResetEvent;` (`75083476` 8343 and 8868).

**My first implementation was a blanket text delete** of every occurrence of
`case 138: return bridge_KeResetEvent;` (count = 2). **That was wrong.** Measured from the raw preview
tree `75083476`, the two copies **within a given switch are the same form**:

| Switch | Sites in the raw tree | Forms |
|---|---|---|
| switch 4 (the args switch) | 8040, 8343 | **both** `case 138: return  4;` |
| switch 5 (the dispatch switch) | 8502, 8868 | **both** `case 138: return bridge_KeResetEvent;` |

So **text alone cannot separate "local's" from "upstream's"** — the packet's parenthetical naming of
`return  4;` and `return bridge_...` as "the upstream duplicates" does not match the tree, because each
form appears twice inside one switch. My blanket delete therefore:

- removed **both** copies from switch 5 — including the one the packet says to **keep**;
- removed **neither** copy from switch 4.

## The damage — and why the build would not have caught it

`AC-STRUCT` reported:

```
[dup-case] src/kernel/kernel_bridge.c:8008,8311  case 138 branch=()
```

That is one finding, but the fuller picture is worse:

| Switch | After my bad edit | Consequence |
|---|---|---|
| switch 4 | **TWO** `case 138` | duplicate case value — a **compile error** (C2196) |
| switch 5 | **ZERO** `case 138` | the dispatch entry is **gone** |

**The second is the dangerous one.** A switch with no `case 138` compiles **cleanly** and **silently drops
ordinal-138 dispatch**. The build would never have caught it, no test in `AC-TEST` would necessarily have
caught it, and the defect would have shipped as a behavioural regression. Only the structural check
surfaced it.

This is precisely the class the Advisor's §3 ruling exists for: a merge-introduced structural defect that
would otherwise be invisible, and that must not be attributed to the build.

## The correction

I **aborted the merge** (`git merge --abort`; verified the tree returned to exactly `0d7929c`, clean, with
`a4s-pre-sync` intact and no `MERGE_HEAD`), **re-ran the merge deterministically**, and replaced the rule
with a mechanical one that does not depend on telling the two sides apart by text:

> Within each switch that contains `case 138`, **keep the first occurrence and delete the rest.**

Because the duplicates within a switch are byte-identical, this is unambiguous, and on the raw tree it
yields exactly **8040 and 8502** — precisely the positions the packet names as "local's two positions".
The final tree is therefore exactly what the scripts produce; **no hand-repair was applied**.

## Verification after the correction

| Post-condition | Required | Measured |
|---|---|---|
| `bridge_KeResetEvent` definitions | 1 | **1** |
| total `case 138` sites | 2 | **2** |
| switches containing `case 138` | two switches, 1 each | **switch 4: 1, switch 5: 1** |

Then, in the packet's required order, **`AC-STRUCT` before the build**:

| Check | Result |
|---|---|
| resolved `M` | **0 findings, 0 UNKNOWN** (exit 0 with `--expect 0`) |
| positive control, raw `75083476` | **15** = 12 marker lines + the 3 structural findings |
| negative control `0d7929c` | **0 findings, 0 UNKNOWN** |
| negative control `766ecef` | **0 findings, 0 UNKNOWN** |
| fixtures | **14 tests, OK** |

## What this establishes, and what it does not

- **Establishes:** the structural check is load-bearing on this merge, not decorative. It caught a defect
  that the build would have passed and that would have silently removed a dispatch entry.
- **Establishes:** the packet's ordering (`AC-STRUCT` **before** the build) is the right order — had the
  build run first and succeeded, the natural next step would have been to trust it.
- **Does not establish:** that `AC-STRUCT` is sufficient. It is textual and non-preprocessing, with the
  limits already recorded; it certifies three structural properties over `SCOPE`, not correctness.
- **Session error, recorded as such:** my first edit (b) was a text-substitution shortcut applied without
  first checking whether the two sides are distinguishable by text in the merged tree. They are not. The
  packet's *intent* was clear and its *named positions* were correct; my *implementation* of it was wrong.
