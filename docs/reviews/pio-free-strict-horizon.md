# The strict-observation horizon is **configuration-dependent**, not a universal ~4.8 s

> ## ⚠ CORRECTION (applied the same session, before this record was relied on)
>
> **An earlier version of this record drew the WRONG conclusion from the durations.** It said the untrapped
> run "reaches the full requested deadline" and therefore that a default strict run has a ~31.7 s budget.
> **The duration comparison is backwards.** The untrapped run's 31.7 s is a **HANG**, not progress: its
> guest is sitting **inside the `loc_001A18D0` spin**, and the trapped run is the one that **exits** it.
>
> **See §"What the durations actually mean" below.** The corrected finding is that the **trapped** run is
> the one making real progress, so **~4.8 s (A2h) is the binding horizon for any claim that needs the GP or
> APU path** — which is the opposite of what this record first claimed. The correction is kept in place
> rather than silently overwritten because the wrong version was already cited by the `PIO_FREE` packet's
> Planner.

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Why this record exists:** the owner's authorization states *"strict execution currently terminates in the
known A2h heap-OOM class at approximately 4.8 seconds despite a longer requested deadline; treat this as an
observed current horizon, not a guaranteed constant."* **The measurement below confirms it is not a
constant — and identifies what actually bounds it.**

**Method:** every strict run in `logs/runs/` (20 runs), reading each run's `metadata.json`
(`run_profile.classification == "strict"`, plus `effective_settings` for `RECOMP_APU_TRAP`), `result.json`,
and `jsrf_run.log`. **Archived artifacts only; nothing was run.**

---

## The correlation

**The heap-OOM class tracks the instrumented (trapped) configuration on recent builds.**

### One build, both configurations

exe **`bc8e288dd54d`** is the **accepted `A4b2-r8` build**. It has exactly two strict runs:

| Run | Configuration | Outcome | Duration | OOM |
|---|---|---|---|---|
| `20260927-160330-655-a4b2-gp-trap-trace` | `RECOMP_APU_TRAP=1` | `unhandled_exception` | **4.77 s** | **YES** |
| `20260927-160335-562-a4b2-default` | trap off | `diagnostic_deadline` | **31.71 s** | no |

### Across all strict runs

| Configuration | n | OOM | Duration range |
|---|---|---|---|
| trap **ON** | 10 | **6** | 4.76 – 32.61 s |
| trap **OFF** | 10 | **0** | 1.92 – 32.64 s |

**No untrapped strict run has ever hit this OOM.**

### And the trap alone is not sufficient — the build matters

| Era | trap ON, n | OOM |
|---|---|---|
| 2026-09-24 (older builds) | 4 | **0** |
| 2026-09-27 (a4b2 era) | 6 | **6** |

Older builds **with the trap on** reached the full deadline (`879716046b99`: 31.70 s;
`9597ff7c2a37`: 32.61 s; no OOM).

### The failure signature (unchanged across all six OOM runs)

```
xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)
[ICALL] invalid target 0x00000000
[EXCEPTION] code=0xE0424943
```

**`requested 598869040` ≈ 571 MB** against a 48.5 MB arena — a guest-side allocation request, not a log
buffer.

---

## What the durations actually mean — the correction

**Duration is not progress.** The two runs differ in *what the guest is doing*, and the discriminating
evidence is unambiguous:

| Witness | trapped (4.77 s) | untrapped (31.71 s) |
|---|---|---|
| `[GPBOOT]` blocks | **1** | **0** — *but see the caveat below: tracing was off* |
| `[GPWATCH]` latch lines | **5** | **0** — *same caveat* |
| `[APUMMIO] read 0x20010` (the `PIO_FREE` poll) | **58** | **0** — *same caveat* |
| `[GPDMA] watch va=803C0810` (the clear) | **2** — `cas=ok`, `3→0` then `2→0` | **0** — *same caveat* |
| `Wf` (pending word at end) | **NOT reportable** — this run's dump is `CONTENT_MISMATCH` | **still `3`** (its dump **is** mapping-valid) |
| **`F`** — `stacks.txt` lines in `sub_001A1769` | **0** | **2** |

> ### ⚠ Which of these are actually evidence
>
> **The untrapped run has `RECOMP_APU_TRACE` absent and `RECOMP_APU_TRAP` absent**, so it emits **zero
> `[GP*]` lines by design** — `AC-DEFAULT2` *requires* zero `[GP*]` lines in a default run. **So the four
> zero columns above are NOT evidence that the GP never booted; they are the expected consequence of
> tracing being off.** I initially read `GPBOOT=0` as evidence and it is not. Recorded because it is the
> same error class as the rest of this project's: reading an absent record as a negative measurement.
>
> **The evidence that actually carries the hang conclusion is the two rows that come from artifacts
> unaffected by tracing:**
>
> - **`F=2`** — `stacks.txt` places the untrapped guest inside the `loc_001A18D0` **pending-word** spin
>   (`recomp_0005.c:6755`, `L+1` with `L=6754`), still comparing against zero.
> - **`Wf0=3`** — read from the untrapped run's **own mapping-valid dump**
>   (`check-dump-mapping.py`: `matches: 1, content-mismatch: 0`), so the pending word it is spinning on
>   **never became zero**.
>
> **`F=2` and `Wf0=3` together are sufficient and tracing-independent**: the guest is in the pending-word
> spin at the end of a 31.7 s window, and the word it waits on is unchanged. **The trapped run's
> `GPBOOT=1`/clear witnesses are then corroboration that the trapped path is the one that resolves it** —
> and those *are* valid presence witnesses, because the trapped run had tracing on.

**`F=2` in the untrapped run means the guest is sitting inside the `loc_001A18D0` spin loop**
(`recomp_0005.c:6755`, i.e. `L+1` with `L=6754`), still comparing the pending word against zero. **The
trapped run has `F=0` — it is NOT in the spin**, because the GP cleared the word and the guest left.

**So the untrapped run's 31.7 s is a hang.** `F=2` places the guest at `loc_001A18D0`
(`recomp_0005.c:6755`), which is the **pending-word** spin — `if (CMP_NE(MEM32(ebx), 0)) goto loc_001A18D0` —
and `Wf0=3` confirms the word it is waiting on never became zero. That is exactly the condition
`main.c:82-86` describes: without the trap the APU aperture is plain memory, so the GPRST write *"is
accepted and discarded, which is how the DSP command block at 0x803C0800 gets a status word written to it
that nothing ever answers."* **The trapped run is the one that makes real progress**: its tracing was on and
it shows the GP booting, driving the clear twice, and leaving the spin — then dying at 4.77 s.

**Precision note:** `F` witnesses the **pending-word** spin, not the `PIO_FREE` poll. Whether the untrapped
guest also passes the `PIO_FREE` gate is a *separate* question — the untrapped route is labelled a
`MCPX_COUNTERS[0x020010]` tick counter whose tick is gated off under the trap, so the two routes may return
different things. **This record does not decide that**; it is the `PIO_FREE` packet's subject. The hang
conclusion rests on **`F=2` + `Wf0=3`** — the two witnesses that are independent of the tracing difference.

**The corrected reading:**

| If a claim needs… | Usable horizon |
|---|---|
| the **GP/APU path** (any trapped strict observation) | **~4.8 s** — the pre-OOM prefix |
| **no** GP/APU involvement | the untrapped window, but note that window may be a hang, so it is **not** a progress budget |

**So `A2h` IS the binding prerequisite for anything requiring trapped strict observation beyond ~4.8 s** —
which is the Planner's `O-SPEC`/`O-UNKNOWN` framing and the packet's stated rule. **The packet is correct;
this record's original conclusion was not.**

**This also means the `A4b2-r8` acceptance stands and is unaffected**: its decision inputs are all emitted
before 4.77 s, which is precisely why the Hy4 advisory called the early termination *"not a violation."*

## What this does **not** establish

- **It is correlational, from archived runs.** The archive varies build, era and configuration together, so
  the OOM's causal mechanism is **not** proven. The one-build/two-configuration pair is the strongest
  evidence and is still one pair.
- **It does not identify what the guest does differently under the trap.** A plausible reading is that the
  trap is what makes the APU aperture *route* rather than behave as plain memory, so the trapped run's
  guest gets real answers — but **that is a hypothesis**, and the `PIO_FREE` packet is the right place to
  settle it.
- **Both logs are capped**, so absence in them is not proof; **presence is**. The witnesses above are
  presence witnesses, which is why they carry the conclusion.
- **Neither duration is a guaranteed budget** in either direction.

