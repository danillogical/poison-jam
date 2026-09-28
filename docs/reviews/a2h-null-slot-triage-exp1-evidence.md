# `A2h` NULL-slot triage — Exp1 offline mining results (zero new runtime code)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Authority:** Advisor critical-path ruling (`docs/reviews/a2h-critical-path-advisor-ruling.md`), Q2 Exp1 —
*"offline log mining, zero new code"*.
**Instrument:** `scripts/a2h-null-slot-triage.py` + `scripts/test_a2h_null_slot_triage.py` (**17 tests OK**),
adopted as the packet's accounting instrument by the Advisor's shape preflight.

**Every quantity below is TOOL-COMPUTED from a named artifact with a recorded hash.** Per the Advisor's
binding count discipline, counts are citable only as **(artifact, query, value)** triples — the two hand
counts this replaced (1177 and 1909) were withdrawn precisely because they were not.

---

## Artifacts

| Run | Log SHA-256 (first 16) | Role |
|---|---|---|
| `20260927-160330-655-a4b2-gp-trap-trace` | `C52D71655C48C752…` | **R1** — the failing run (raw-zero terminal) |
| `20260927-130036-879-a4b2-nr-baseline` | `5358A9F957C5F5A4…` | **baseline** — the contrast run |

**Budget, grounded from `metadata.json` and NOT from log text** (Advisor correction — an env var is not log
text, so searching the log for it is unsound):

| Run | Budget | Kernel lines | Headroom |
|---|---|---|---|
| R1 | **100000** | 6471 | **93529** |
| baseline | **100000** | 6608 | **93392** |

**Neither run is near its cap, so neither log is truncated by the budget.** *(A budget-truncation notice was
searched for and not found in either.)*

### The two runs share a deterministic prefix — checked, not assumed

Both runs report **exactly 5555 distinct indices**, `min=1`, `max=5555`, with the **same repetition
signature** (`#1` ×6, `#2` ×6, `#3` ×5) and **exactly one** line at the maximum index. That coincidence was
**investigated rather than accepted**, because an identical structure across two builds could equally mean
the index is not what the source says.

**It is consistent with a deterministic shared prefix:** both runs perform the same early call sequence, and
the main thread's counter reaches **5555** in both before the runs diverge. That matches the independently
observed fact that both logs carry the same numbered kernel prefix through the OOM and diverge only
afterwards. **The mechanism remains the source's `RECOMP_TLS` per-thread counter** (`kernel_bridge.c:316`);
the shared maximum is a property of the workload, not evidence against the mechanism.

**It does NOT establish which thread reached 5555** — the kernel lines carry no `tid`, which is the
unattributed-series problem recorded below.

---

## Finding 1 — the call index is **PER-THREAD**, and this is source-proven

| Quantity (R1) | Value |
|---|---|
| parsed `[KERNEL]` lines | **6471** |
| distinct indices | **5555** |
| **indices reused for different events** | **916** |
| index **decreases** | **357** |
| duplicate indices with *identical* content | **0** |
| printed `summary:` thread total | **4274** (printed **once**) |

**`kernel_bridge.c:316`: `static RECOMP_TLS int g_kernel_call_count = 0;`** — `RECOMP_TLS` is **one counter
per thread**, with no reset path. **That is the mechanism**, and it explains every anomaly at once: `#N`
restarts per thread, so indices repeat and decrease, and 6471 logged lines can sit under a maximum index of
5555. **The reused indices carry *different* content, so these are real distinct events, not log duplication.**

**This answers the Advisor's own `#5551`/`#5553` concern:** the "gap" is **per-thread restart**, not loss.
The R1 sequence is `#5551` ordinal 277, `#5552` ordinal 294, `#5553` ordinal 277, `#5554` ordinal 184 (the
OOM), `#5555` ordinal 294, then the ICALL — a **contiguous per-thread run**, so no index is missing there.

**Consequence, and it is binding on the packet:** a raw parsed-line total is a **lower bound on LOGGED
dispatches, not a dispatch total**. Any claim of "N dispatches before the transition" must **name the
thread**, and **the printed "4274 total" is ONE thread's count — never cite it as a total.** The tool reports
`complete: false` for both runs and says why.

## Finding 2 — the two runs have **different terminal events on different threads**

| | R1 (failing) | baseline (contrast) |
|---|---|---|
| ICALL form | `invalid target 0x00000000 … return=0014982E` | `Failed to resolve VA 0xFFFFFFFF (thread calls: 953, …)` |
| tid | **65356** | **57592** |
| target | `0x00000000` | `0xFFFFFFFF` |

**These are different failure forms.** A parser knowing only R1's form reported **zero** ICALL lines for the
baseline — which would have silently mischaracterised its terminal event. Both forms now parse and are
**tagged** by `form`.

**So "the baseline run without the NULL call" is a statement about an EARLIER TERMINAL BOUNDARY, not about
survival:** the baseline also ends in `unhandled_exception`; it simply reaches a *different* terminal event
first, on a *different* thread. **It is not a controlled causal contrast** and must not be presented as one.

## Finding 3 — the ordering, stated as observation only

| Event | R1 log line |
|---|---|
| OOM (`requested 598869040`) | **17110** |
| ICALL (raw-zero terminal) | **17119** |
| EXCEPTION `0xE0424943` | **17120** |

**The OOM and the ICALL are nine lines apart**, so they are not adjacent — consistent with the
**different-passes** reading, and inconsistent with "the OOM immediately causes the NULL call."

**Per the Advisor's terminal-event rule, this is an OBSERVATION of line order and NOT an attribution.**
The attribution comes from the **bytes**: `00149E56 test eax,eax` / `00149E58 jl 0x149eec` — the guest
**checks** the result, `0xC0000017` is negative, so the branch is **taken** and the OOM is **handled**. The
tool records that fact separately and labels its evidence class **`static structural, not this log`**.

## Finding 4 — threads, and what the archive can and cannot support

**R1's ICALL line carries `tid=65356`, one distinct tid.** But **the `[KERNEL]` dispatch lines carry NO
`tid`** (verified: the toolkit's kernel prints have zero `GetCurrentThreadId`). **So the archive's kernel
lines are UNATTRIBUTED**, and a single ICALL tid does **not** bind the surrounding `#5551`–`#5555` lines to
that thread.

**Therefore:**
- **per-thread windows are NOT reconstructable from the archived logs**, and the tool reports `complete:
  false` for exactly this reason;
- **archived kernel-thread attribution is CONDITIONAL and fails closed**;
- this is precisely why the Advisor's preflight requires **`tid` augmentation on `[KERNEL]`/`[KWATCH]`
  prints** in the new run — *"unattributed multiplexed series cannot support per-thread windows or
  negatives."*

## Finding 5 — the install positive control

From the R1 log's own install line, the toolkit reports **base `0x001C3F60`, 120 entries**, so
**slot `0x001C4064` = base + `0x104` = index 65.** The patch loop writes a **synthetic** VA
(`kernel_bridge.c:9252-9254`), so the installed value should be **`0xFE000000 + 65*4 = 0xFE000104`**, whereas
the **original XBE** holds **`0x80000115`** (ordinal **277**).

**That original→installed pair is an independent positive control** the packet must require: it is a value
the image predicts, an event the log reports, and a transformation the source performs — three independent
statements that must agree. **The terminal `return=0014982E` and the slot's ordinal-277 identity corroborate
each other**, which is what makes "the slot's history is the ordinal-277 path" a supported reading.

---

## What Exp1 establishes, and what it does NOT

**Establishes:** the index is per-thread and source-proven; neither log is budget-truncated; the two runs
have different terminal events on different threads; the OOM and the ICALL are nine lines apart; the
archived kernel lines are **unattributed** so per-thread windows are **not** reconstructable from them; and
the install positive control is available and predicted.

**Does NOT establish:** what zeroed, or what read as zero at, `0x001C4064`; whether a bridge or guest code
accounted for it; whether the OOM and the NULL slot are related at all; any writer identity; or any strict
criterion. **No count here decides a row on its own** — the rows require Exp2's live transition record.

**Prohibitions honoured:** no guest run, no build, no toolkit change, no generated-code edit, **no synthetic
completion**, and no rerun. **`PIO_FREE` stays DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened;
`0xFFFFB3` stays `UNRESOLVED`.
