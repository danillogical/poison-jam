# A4s-r6: hunk 5 provenance — local MOVED `lock xadd`, it did not merely delete it

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence for the Advisor's hunk-5 ruling. Not a ruling.
**Script:** `logs/a4s/hunk5-provenance.py`.

## Membership of `_FLAGS_UNDEFINED` per revision (measured)

| Revision | `_FLAGS_UNDEFINED` members |
|---|---|
| `051a128` (base) | `cpuid, div, idiv, **lock xadd**, mul, rdtsc` |
| `0d7929c` (local) | `cpuid, div, idiv, mul, rdtsc` |
| `766ecef` (upstream) | `cpuid, div, idiv, **lock xadd**, mul, **popfd**, rdtsc` |

Local removed vs upstream: `lock xadd`, `popfd`. Upstream removed vs local: **none**.

## The removal is a MOVE, not a deletion

Walking the 24 local-only commits in `051a128..0d7929c` and diffing the set membership after each,
exactly **one** commit changes it:

```
CHANGE at b3de85c  Fix JSRT startup translation and runtime diagnostics
    removed: ['lock xadd']
```

Its `tools/recomp/lifter.py` diff is a two-line move:

```diff
+    "xadd", "lock xadd", "lock cmpxchg",  # Atomic arithmetic flags
-    "lock xadd",           # Lock prefix - complex flag behavior
```

The added line lands in **`_EFLAGS_SETTERS`** (local line 265); the removed line was the
`_FLAGS_UNDEFINED` entry (base line 272). The commit subject states the intent: *"Preserve atomic and
repeated-comparison flags…"*. So local reclassified `lock xadd` from *flags-undefined* to
*flags-setter with tracked atomic flags*, and separately handles it as a result-snapshot setter.

## Why this matters for the H1 defect

The conflicted hunk shows local's side as **adding no lines** (it only removed one). H1's literal test —
*"every non-blank line one side added is also present in the other side's version"* — is therefore
**vacuously true for local**, so literal H1 returns **upstream** and keeps `"lock xadd"` in
`_FLAGS_UNDEFINED`. Because both revisions test `_FLAGS_UNDEFINED` **before** `_EFLAGS_SETTERS`
(local `3300` vs `3304`; upstream `3612` vs `3616`), the resurrected entry would take precedence and
local's reclassification would be silently discarded — even though local's `_EFLAGS_SETTERS` still
lists `lock xadd` and `translator.py` still treats it as a result-snapshot setter. The merged lifter
would classify `lock xadd` as flags-undefined while two other tables in the same tree classify it as a
flags-setter.

**This is a deletion-only contribution being discarded by vacuous containment** — the exact defect the
`A4s-r6` rule repair must close, and it is a *real* instance in this merge, not hypothetical.

## Note on the other half of the hunk

`"popfd"` is upstream's addition and has **no** local counterpart: local's `_FLAGS_UNDEFINED` never
contained it. Upstream's comment explains it moved `popfd` out of `_EFLAGS_PRESERVE`. Local's
`_EFLAGS_PRESERVE` (line 285) still lists `"pushfd", "popfd", "pushal",`.

**Correction (measured after the first version of this record).** An earlier note here claimed the
merged tree would list `popfd` in **both** sets and that hunks 5 and 6 therefore interact in a way that
needed explicit coupling. **That was wrong**, and the mechanical H2 check shows why. Measured
membership:

| Revision | `popfd` in `_EFLAGS_PRESERVE` | `popfd` in `_FLAGS_UNDEFINED` | `wbinvd` in `_EFLAGS_PRESERVE` |
|---|---|---|---|
| `051a128` (base) | **True** | False | False |
| `0d7929c` (local) | **True** | False | **True** |
| `766ecef` (upstream) | **False** | **True** | False |

Per-base-line attribution for hunk 6's two base lines:

| Base line | local changed it | upstream changed it |
|---|---|---|
| `"pushfd", "popfd", "pushal",` | **False** | **True** (drops `popfd`) |
| `"sgdt", "ljmp", "sfence",` | **True** (appends `"wbinvd"`) | **False** |

So H2's precondition holds (each base line changed by at most one side) and H2's disjoint union
yields **upstream's line 1 + local's line 2**:

```python
    "pushfd", "pushal",
    "sgdt", "ljmp", "sfence", "wbinvd",
```

**`popfd` is therefore dropped from `_EFLAGS_PRESERVE` by hunk 6's own H2 union** — which agrees with
upstream's intent. Combined with hunk 5's resolution (keep upstream's `popfd` in `_FLAGS_UNDEFINED`,
keep local's removal of `lock xadd`), the result is **consistent**: `popfd` appears only in
`_FLAGS_UNDEFINED`, and `lock xadd` appears only in `_EFLAGS_SETTERS`. **No special coupling between
hunks 5 and 6 is required**, and hunk 6's existing `H2` classification stands unchanged. Script:
`logs/a4s/hunk56-interaction.py`.
