# A4s-r6: clean-hunk semantic changes to REACHABLE code (the timer path)

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** measured evidence for the Advisor's hunks 1–2 ruling and for the `A4s-r6` design. Not a ruling.
**Scripts:** `logs/a4s/timer-conflict-check.py`, `timer-provenance.py`, `timer-coherence.py`, `timer-semantics.py`.

## Why this matters

`docs/reviews/a4s-r6-structural-findings.md` established that the merge can change things in **clean
hunks** where the conflict-only H-rules never look. Those findings were structural (compile errors).
This record is the **semantic** counterpart: a clean-hunk change to **reachable** runtime behaviour.

## Measured

`bridge_KeInitializeTimerEx` serves **ordinal 113**, which JSRF **does** declare
(`docs/reviews/a4s-r6-ordinal-reachability.md`) — so it is **reachable**, unlike ordinals 108/110/138/146.

| Revision | `bridge_KeInitializeTimerEx` writes | shadow insert? |
|---|---|---|
| `0d7929c` (local) | `BRIDGE_MEM16(timer_va + 0) = 0x08 + (type & 1);` — **no `+2` write** | **no** |
| `766ecef` (upstream) | `BRIDGE_MEM8(timer_va + 0) = …`, `BRIDGE_MEM8(timer_va + 2) = 40;`, `MEM32(+4)=0`, `MEM32(+8)=0`, `MEM32(+12)=0` | **yes** — `ke_shadow_insert(timer_va, ev)` |

**The merge tree contains only upstream's form** (`75083476:2328`): `MEM16(timer_va + 0)` is **absent**,
while `MEM8(timer_va + 0)`, the `Size = 40` write and `ke_shadow_insert(timer_va, …)` are **present**.

**These lines are not inside any conflict hunk.** The merge tree's conflict ranges are
`1365-1402`, `1423-1439`, `8975-8978`; the timer initialiser sits at `2328`, outside all of them. So the
merge adopted upstream's timer model **silently**, in a clean hunk — precisely the case
`docs/jsrf-run-profiles.md` rule 4 warns about (*"Clean hunks are not exempt"*).

## Is the result coherent? Yes — and this is why it is a finding, not a defect

I checked whether the merged tree ended up with a **hybrid** that mixed the two models. It did **not**:

| Component | local `0d7929c` | upstream `766ecef` | merge `75083476` |
|---|---|---|---|
| `kernel_timer_thread` uses `g_timers` polling table | yes | yes | **yes** |
| `kernel_timer_thread` uses `ke_shadow_lookup(fired_va)` + guest `SignalState` write | **no** | **yes** | **yes** |
| `bridge_KeInitializeTimerEx` writes `Size = 40` and inserts a shadow handle | **no** | **yes** | **yes** |
| `g_timers` references | 14 | 15 | 15 |
| `kernel_set_timer` references | 3 | 3 | 3 |

Upstream **already had** the `g_timers` polling table — so the merged timer machinery is **upstream's
version wholesale**, not a mixture. The fire path (`75083476:2392-2455`) looks up the shadow handle for
`fired_va`, signals it, and writes the guest `SignalState` — all internally consistent with the
initialiser that inserted that handle. **No inconsistency found.**

## The bounded population: exactly three functions both sides changed outside any conflict

A sweep over `kernel_bridge.c` (`logs/a4s/both-changed-functions.py`) answers the "how many others
are there" question precisely. Population = functions that **both** sides changed relative to the base
**and** that lie **entirely outside every conflict hunk** (conflict ranges: `1365-1402`, `1423-1439`,
`8975-8978`):

| Function | local vs base | upstream vs base | merge vs upstream | merge vs local | Content |
|---|---|---|---|---|---|
| `bridge_for_ordinal` | +1 −0 | +208 −0 | **+1 −0** | +208 −0 | merge = upstream **plus** local's `case 138` line |
| `stdcall_args_for_ordinal` | +1 −1 | +188 −1 | **+1 −0** | +188 −1 | merge = upstream **plus** local's `case 138` line |
| `kernel_timer_thread` | +2 −2 | +12 −1 | **+2 −2** | +12 −1 | merge = upstream **plus** local's lines |

**Exactly three.** In all three the merge is a **union**, not a discarded change — which is why an
earlier "merge matches neither parent" verdict appeared: it is a *combination*. So the clean-hunk
exposure in this file is **bounded and now enumerated**:

- `bridge_for_ordinal` and `stdcall_args_for_ordinal` → the **duplicate `case 138`** defect (Finding 1
  of `a4s-r6-structural-findings.md`).
- `kernel_timer_thread` → the **timer semantic change** above.

**A near-miss worth recording.** `stdcall_args_for_ordinal` also shows a genuine **semantic
disagreement**: local has `case 207: return 36;  /* NtQueryDirectoryFile (9) */` and upstream has
`case 207: return 40;  /* NtQueryDirectoryFile (10) */` — the two sides disagree about the argument
byte count. Because those two lines sit in **different** hunks, the merge kept only **one** `case 207`
(the resolved tree has a single `case 207: return 40;`, verified in
`logs/a4s/case207-check.py`). **`case 207` is therefore NOT a duplicate** — the only duplicated label
in either switch is `138`. The merge silently preferred upstream's count; whether 36 or 40 is correct is
**not** established here, and it is a **follow-up lead**, not an `A4s-r6` obligation.

## The sweep extended to every both-side file

The three-function population above is for `kernel_bridge.c`. Extended to all **10 both-side files**
(`logs/a4s/clean-hunk-sweep-all.py`, `clean-hunk-narrow.py`):

| File | Conflict hunks | Conflict ranges |
|---|---|---|
| `CMakeLists.txt` | 0 | — |
| `src/kernel/kernel.h` | 0 | — |
| `src/kernel/kernel_bridge.c` | 3 | 1365-1402, 1423-1439, 8975-8978 |
| `src/kernel/xbox_memory_layout.c` | 1 | 2123-2138 |
| `src/kernel/xbox_memory_layout.h` | 0 | — |
| `templates/runtime/recomp_types.h` | 0 | — |
| `tools/disasm/functions.py` | 0 | — |
| `tools/recomp/lifter.py` | 3 | 400-410, 424-432, 1271-1291 |
| `tools/recomp/test_icall_feedback.py` | 1 | 116-120 |
| `tools/recomp/translator.py` | 1 | 987-992 |

Four further top-level Python blocks were changed by **both** sides and contain **no** conflict
(`FunctionDetector`, `_make_condition`, `lift_basic_block`, `BatchTranslator`). **All four are clean
unions — zero lost lines** (`logs/a4s/clean-hunk-loss-check.py`):

| Block | local added / missing | upstream added / missing |
|---|---|---|
| `tools/disasm/functions.py::FunctionDetector` | 87 / **0** | 9 / **0** |
| `tools/recomp/lifter.py::_make_condition` | 7 / **0** | 101 / **0** |
| `tools/recomp/lifter.py::lift_basic_block` | 11 / **0** | 3 / **0** |
| `tools/recomp/translator.py::BatchTranslator` | 205 / **0** | 9 / **0** |

**So the clean-hunk exposure is now bounded and enumerated across the whole merge:**

- **compile-error class:** duplicate `case 138` in both `kernel_bridge.c` dispatch switches; duplicate
  `bridge_KeResetEvent` definition.
- **semantic class:** the timer initialiser/thread (upstream's model adopted wholesale; coherent).
- **verified-clean:** the four Python blocks above, and every other both-side file has no conflict at
  all, so its content is one side's bytes by `AC-MERGE`'s rules.

**Limit, stated:** this checks that each side's **added** lines survive. It does not prove no side's
**deletion** was overridden in a clean hunk — that would need the same per-base-line attribution the
H2 repair uses, applied file-wide rather than per conflict hunk, and I have not run that. It is the
natural next step if the Planner wants the check to cover deletions too.

## What this establishes, and what it does not

**Establishes (observed):** a clean hunk silently replaced local's timer initialiser with upstream's,
in code reachable at ordinal 113, and the merge also brought in the matching reader
(`ke_shadow_lookup(timer_va)`). The result is self-consistent.

**Does NOT establish** that this is *correct* for JSRF — only that it is coherent. Whether the guest
observes a behavioural difference (e.g. a guest that reads the timer's `Size` field, or one that relied
on local's `MEM16` type write) has **not** been measured, and I make no claim about it.

**Does NOT establish** that the timer path is exercised in the current runs: ordinal 113 appears in the
**declared** table but **not** in the measured-called set (the run stops at the DSP spin first).

## Consequence for `A4s-r6`

1. This is the **second** measured instance of a clean-hunk change (after the duplicate `case 138` and
   the duplicate `bridge_KeResetEvent` definition), and the first with **runtime semantics** rather than
   a compile error. It strengthens the case for the **structural/semantic post-resolution check** the
   Planner is being asked to design: a check that compares the resolved tree against **both parents**
   and reports every reachable-code difference that no H-rule decided.
2. It is **not** evidence for a hunks 1–2 side choice: the timer path is decided by clean hunks, not by
   hunks 1–2. Whether upstream's timer model should be kept is a **separate** question the Planner may
   wish to raise — but per the owner's bounded-scope rule it enters `A4s-r6` only if leaving it out
   could cause a false PASS, false FAIL, wrong merge implementation, wrong evidence binding, or unsafe
   execution. My reading: it is a **follow-up lead**, not an `A4s-r6` obligation, because the merged
   form is coherent and no `A4s-r6` criterion depends on timer behaviour.
