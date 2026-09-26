# A4s-r6: `AC-MERGE` verified on `M` — PASS

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Merge commit `M`** = `3f8bf67c450861aefcbc376698750bc1446bc9dd` (parents `0d7929c`, `766ecef`).
**Status:** **PASS.** Scripts: `logs/a4s/ac-merge-final.py` (targeted), `ac-merge-twin-raw.py` and
`ac-merge-twin-analysis.py` (the diagnostic trail), `settle-hunk8.py`.

## Result

| Sub-check | Result |
|---|---|
| (a) parents `M^1`, `M^2` | **PASS** — `0d7929c…`, `766ecef…` |
| (b) `theirs→M` ⊆ ours-changed (38 files) | **PASS** — excess empty |
| (c) `ours→M` ⊆ theirs-changed (153 files) | **PASS** — excess empty |
| (d) presence witness | **PASS** — every added line present, or its omission/replacement recorded |
| (d-twin) deletion witness | **PASS** — **16/16** targeted checks |

## The (d-twin) witness — the word *"credited"* is load-bearing

The criterion:

> **(d-twin) deletion witness (Advisor-ruled):** for each of the 10 files, every base line that a
> **credited** side **deleted** … is **ABSENT** from `git show M:<f>`, **unless the other side replaced
> it** … or the line is restored by an HA pre-ruling that is recorded.

It took **four attempts**, and **all three failures were mine, not the merge's**:

1. **Inverted the test** — I reported base lines that were *absent*, which is the **desired** state. The
   witness fires on a line that is **present** despite a credited deletion, i.e. a *resurrected* line.
2. **Stripped whitespace** — git reports a line as removed when its **indentation** changes, and after
   stripping, the "removed" text matched text legitimately present elsewhere. **261 phantom hits.**
3. **Credited both sides globally over whole files** — 22 hits. That flags every region where one side
   deleted a line and the **other** side's version of that region was taken, which is a normal, correct
   3-way outcome. Measured: of the 22, **14 had text occurring more than once in `M`** (so presence
   proved nothing — the same "union by line presence" flaw the Advisor ruled against for H2), and **all 8
   remaining unique-text candidates were KEPT by the other side**.
4. **Credited per resolution region** — the correct reading, and what the criterion means.

**A side is credited for a region only where its edit was applied.** Under the pre-ruled table that is
determinate: hunk 5 credits **local** (it deletes `"lock xadd"`), hunk 4 credits **local**, hunks 1–2 are
COMBINED, hunks 6/7 are disjoint H2, hunk 8 credits **local**, hunk 9 is a UNION.

### The 16 targeted checks (all PASS)

| # | Check |
|---|---|
| 1 | hunk 5: local's deleted `"lock xadd"` line is **absent** from `_FLAGS_UNDEFINED` |
| 2 | hunk 5: upstream's `"popfd",` is **present** |
| 3 | hunk 5: exactly **6** comment lines from upstream's insertion |
| 4 | hunk 6: upstream's edit applied (`"pushfd", "pushal",` without popfd) |
| 5 | hunk 6: local's edit applied (`"wbinvd"` added) |
| 6 | hunk 3: local's comment present |
| 7 | hunk 3: exactly **one** TLS dispatch-slot declaration |
| 8 | hunk 4: local's `VirtualProtect`-failure `else` present |
| 9 | hunk 4: upstream's `ac97_arm_write_trap();` **absent** (HA omission) |
| 10 | hunk 8: local's `--functions fns` form present **in the conflicted function** |
| 11 | hunk 8: upstream's `none.json` form **absent from the conflicted function** |
| 12 | hunk 9: local's members present (`"xadd", "lock xadd"`) |
| 13 | hunk 9: upstream's members present (`"inc", "dec"`) |
| 14 | hunk 9: upstream's clause kept (`_RESULT_SNAPSHOT_SETTERS`) |
| 15 | edit (a): exactly **one** `bridge_KeResetEvent` definition |
| 16 | edit (b): exactly **two** `case 138` sites |

## A second false alarm in my own check, settled by measurement

A file-wide search for upstream's `none.json` form found **one survivor** in the resolved
`test_icall_feedback.py`, which looked like a failed HA omission. It is not:

| Revision | `none.json` occurrences, by enclosing function |
|---|---|
| upstream `766ecef` | `test_seeds_drops_unaligned_targets`, `test_seeds_align_zero_keeps_everything` |
| `M` (resolved) | `test_seeds_align_zero_keeps_everything` |
| ours `0d7929c` | none |
| base `051a128` | none |

The **conflicted** function is `test_seeds_drops_unaligned_targets` (defined L96), and `M` has **no**
`none.json` in it — so HA-LOCAL was applied correctly. The survivor is upstream's **second** occurrence,
in `test_seeds_align_zero_keeps_everything`, a function that is **not part of any conflict**, where it is
ordinary merged content. Scoped correctly, the omission is exact. (Recorded because a file-wide text
search is the wrong instrument for a region-scoped criterion — the same lesson as the hunk-5/`none.json`
distinction in the `(d)` witness.)

> **Correction — acceptance-review Advisory 4.** An earlier version of this record named the conflicted
> function `test_seeds_align_16`. **That name does not exist in the file.** The stage-1 acceptance
> reviewer caught it; the Session re-verified (`logs/a4s/verify-advisory4.py`): the hunk-8 assertion at
> L116 sits in `test_seeds_drops_unaligned_targets`, whose body contains `--functions fns` and **no**
> `none.json`, so **HA-LOCAL is correct**. This was a **documentation error in this record**, not a
> resolution defect, and it changes no criterion result.

## What this establishes, and what it does not

- **Establishes:** both sides' work is represented in `M`; no credited deletion was resurrected; no
  content was invented; the resolutions match the pre-ruled table line for line.
- **Does not establish:** semantic correctness of the merged code — `AC-MERGE` is a textual
  presence/absence check. `AC-TEST` and the strict run carry that.
- **Claim limit:** (d)/(d-twin) are text-based, so a line that legitimately appears in several places
  cannot be attributed to a specific occurrence; the targeted per-region checks above are what make the
  result meaningful, and they are recorded individually rather than as a count.
