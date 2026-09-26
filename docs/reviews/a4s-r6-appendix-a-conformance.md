# A4s-r6: `M` conforms to Appendix A verbatim

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Merge commit `M`** = `3f8bf67c450861aefcbc376698750bc1446bc9dd`.
**Script:** `logs/a4s/verify-appendix-a.py` (provenance only).

## Why this check exists

The packet's Appendix A says: *"**Expected-5**, **Expected-6** and **Expected-9** are the ruled outputs
the executor's result must match"*, and hunks 1–2 must be replaced with **HA-COMBINED-1**/**-2**
*"verbatim"*. This compares `M` against the **packet's own bytes** — extracted from the fenced blocks —
rather than against my transcription of them, so a transcription error on my side could not hide.

## Result — PASS

| Appendix A block | Present verbatim in `M` | At line |
|---|---|---|
| **HA-COMBINED-1** (`bridge_KeSetEvent` + the single `bridge_KeResetEvent`) | **yes** | 1358 |
| **HA-COMBINED-2** (`bridge_KeWaitForSingleObject`) | **yes** | 1397 |
| **Expected-5** (`_FLAGS_UNDEFINED`) | **yes** | 397 |
| **Expected-6** (`_EFLAGS_PRESERVE`) | **yes** | 404 |
| **Expected-9** (translator `_fa` guard) | **yes** | 986 |

**Negative / structural checks (10/10 PASS):**

| Check | Result |
|---|---|
| `bridge_KeSetEvent`: upstream `bridge_resolve_handle` tier absent | PASS |
| `bridge_KeSetEvent`: upstream `SetEvent(h)` host call absent | PASS |
| `bridge_KeResetEvent`: upstream `ResetEvent(h)` host call absent | PASS |
| `bridge_KeWaitForSingleObject`: upstream `bridge_resolve_handle` tier absent | PASS |
| `bridge_KeSetEvent` uses the in-place tier **first** | PASS |
| `bridge_KeSetEvent` uses the shadow tier **second** | PASS |
| `bridge_KeResetEvent` has all **three** tiers | PASS |
| `bridge_KeWaitForSingleObject` has all **three** tiers | PASS |
| `lock xadd` absent from `_FLAGS_UNDEFINED` | PASS |
| exactly one `case 138` per switch (2 total) | PASS |

This is the strongest single piece of evidence that the resolution is exactly what the Advisor ruled:
the four-tier-free precedence order, the dropped `bridge_resolve_handle` tier, and the single surviving
`bridge_KeResetEvent` are all confirmed against the ruling's own text.

## Three false alarms in this check, all mine — recorded

Getting to PASS required fixing three defects in **my own** verification, each of which would have
produced a spurious FAIL:

1. **Label lines counted as code.** The appendix labels each block with a commentary line —
   `/* HA-COMBINED-1: … */` and `# Expected-5: …` — which is not part of the code to apply. Including
   them made a verbatim block comparison fail. (Same shape as the `none.json` false alarm in `AC-MERGE`.)
2. **File-wide search for a function-scoped omission.** `SetEvent(h)` at line 6943 is upstream's
   **clean-hunk** `bridge_KeSetEventBoostPriority` (ordinal 146) — a *different* function that the
   HA-COMBINED ruling does not govern. Scoping the check to `bridge_KeSetEvent`/`KeResetEvent`/
   `KeWaitForSingleObject` fixed it. **A file-wide text search is the wrong instrument for a
   function-scoped criterion** — the third time this lesson appeared in this execution.
3. **Substring false positive.** `'ResetEvent(h)' not in body` reported a FAIL because
   `ResetEvent(h)` is a **substring** of the legitimate runtime call `xbox_KeResetEvent(h)`. Fixed with a
   word-boundary match (`(?<![\w])ResetEvent\s*\(`), which matches only the standalone Win32 host call.

Recorded because the pattern is consistent and worth naming: **every one of my verification failures in
this execution has been an over-broad text match, never a defect in `M`.** The targeted, region-scoped
check is what makes the result meaningful, and each such check is recorded individually.
