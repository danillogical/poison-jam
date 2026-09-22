---
name: recompiler-hang-triage
description: Diagnose a guest hang or spin in the JSRF Xbox recompiler. Use when a run reaches diagnostic_deadline instead of crashing, when a guest loop repeats forever, when a model "content gap" is suspected, or when an ABI/delta report fires and the offending instruction is not obvious. Covers the probe-first workflow, the checks that are already built, and the traps this project has already fallen into.
description_zh: "诊断 JSRF 重编译器的 guest 挂起/自旋"
description_en: "Triage a guest hang in the JSRF recompiler"
agent_created: true
---

# Recompiler hang triage

For the `my_xbox_game` / `xboxrecomp` pair. Order matters: each step below is
cheaper than the next, and the later ones are only worth running once the earlier
ones are clean.

## Step 0 — is it a hang, or just slow?

Run the same scenario at two durations and compare counters:

```
kernel calls, ABI-verified bodies, handled faults, distinct ICALL targets
```

Identical numbers at 33 s and 73 s means a hang. This has been worth doing every
time: "slow" and "hung" produce the same log tail.

## Step 1 — read what the model already reports

Before adding any probe, check the run's own artifacts:

- `jsrf_run.log` for `[ABI]`, `[DELTA]`, `[RECOVERED] ABI FAILURE`, `[PFIFO]`
- `gpu-report.md` for `PFIFO_DMA_GET` / `PFIFO_DMA_PUT` and the final queue decode
- `stacks.txt` for the guest thread's frames — the top frame names the spin site
  as `recovered.c:LINE`, which maps straight to a guest instruction

`gpu-report.md`'s "final queue decode" is a **post-mortem**, not a live stall.
`budget_exhausted` there does not mean the walk hit its budget during the run —
check the `[PFIFO] budget_exhausted` line, which is live, before believing it.

## Step 2 — checks that are already built

- **Exact-delta check** (`RECOMP_ABI_CHECK`, **off by default** — CMake *caches*
  `CMAKE_C_FLAGS`, so `-DCMAKE_C_FLAGS=-DRECOMP_ABI_CHECK` then clear it with
  `-DCMAKE_C_FLAGS=`). Reports `[DELTA] site=… callee=… actual=… expected=…`.
  The site is the pushed return address, so a chain of reports reads as a call
  chain with the innermost function last.
- **`jsrf_trace_seq(site, value)`** in `src/diagnostics.c` — a sequenced probe.
  Use it rather than a one-shot print: a single-shot probe cannot distinguish
  "the value was wrong" from "this ran twice".
- **`scripts/converge-manifest.py`** — manifest convergence, with the runtime ABI
  check as the discriminator between a missed entry and a mid-body fragment.

## Step 3 — probe placement rules

These have each cost a wasted round:

- **Probe the saved slot at one fixed address**, not `MEM32(esp+N)` at several
  sites — `esp` differs per site, so the readings are unrelated.
- **Scope probe insertion to one function body.** A whole-file pass interleaves
  probes from functions that call the same callee and produces a confident,
  wrong answer.
- **In `RECOMP_ICALL_SAFE`, put probes before the `IS_CODE` check.** After it,
  they silently never fire.
- **Never trust a static push/pop count.** `PUSH32` is also how arguments are
  passed, and `returns=0` can just mean the exit is a tail jump.
- **A negative result needs its own verification.** Grep for a string the logger
  is *known* to emit. "Zero hits" and "zero occurrences of the string I searched
  for" are different claims — this project has twice retracted a correct finding
  because of it.

## Step 4 — the two defect classes worth checking first

1. **A dispatch redirect to the wrong function.** The translation pass folds a
   real function entry into its neighbour when the disassembler classified it
   `tail_jump_alias`, so the dispatch tuple points at the wrong body. Compare the
   guest's `ret N` at the requested address against the tuple's target. Three
   instances found so far (`0x00168480`, `0x00151C40`, `0x0019E3B7`). **If the two
   `ret N` values coincide, no contract check can see it** — only executing the
   wrong code reveals it, so a same-delta redirect needs a runtime probe.
2. **A model content gap.** The guest waits on a value some other agent should
   publish and the model never does. Precedents: PFIFO low-mark status, and the
   ring-position word at `0x80000000`. Confirm the loop is a *faithful
   translation* (dump the raw bytes and compare) before blaming the recompiler —
   a spin that cannot change its own condition is a wait, not a bug.

## Step 5 — before "fixing" the model

The model's rejection behaviour is **deliberately tested**. `jsrf_nv2a_registers`
pins that an unsupported method, a budget overrun and a full sink must each reject
the stream and leave `PFIFO_DMA_GET` untouched. Relaxing a guard to stop a hang
will fail that test and is the wrong fix: the answer is capability (implement the
class, give the sink a consumer), not a weakened contract. Run `ctest` after any
model change — this mistake was made once and reverted.

## Invariants to keep green

- `ctest --test-dir build -C Release` → 11/11
- No `[ABI]` / `[DELTA]` reports, no `[RECOVERED] ABI FAILURE`
- No ad-hoc `TEMP` probes left in `src/recomp/gen/*.c` or `recovered.c`
  (`recovered.c` is regenerated by `scripts/recover-functions.py`, so regenerating
  reverts probes placed there)
- Record the state as a run path plus both commit ids
