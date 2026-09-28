# The strict-observation horizon is **configuration-dependent**, not a universal ~4.8 s

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Why this record exists:** the owner's authorization states *"strict execution currently terminates in the
known A2h heap-OOM class at approximately 4.8 seconds despite a longer requested deadline; treat this as an
observed current horizon, not a guaranteed constant."* **The measurement below confirms it is not a
constant — and identifies what actually bounds it.** The `PIO_FREE` packet's claim must be bounded against
the *right* horizon, so this had to be measured before planning proceeded.

**Method:** every strict run in `logs/runs/` (20 runs), reading each run's `metadata.json`
(`run_profile.classification == "strict"`, plus `effective_settings` for `RECOMP_APU_TRAP`), `result.json`,
and `jsrf_run.log`. **Archived artifacts only; nothing was run.**

---

## The finding

**The heap-OOM class is a property of the instrumented (trapped) configuration on recent builds — not of
strict execution as such.**

### The decisive controlled comparison — one build, both configurations

exe **`bc8e288dd54d`** is the **accepted `A4b2-r8` build**. It has exactly two strict runs:

| Run | Configuration | Outcome | Duration | OOM |
|---|---|---|---|---|
| `20260927-160330-655-a4b2-gp-trap-trace` | `RECOMP_APU_TRAP=1` | `unhandled_exception` | **4.77 s** | **YES** |
| `20260927-160335-562-a4b2-default` | trap off | `diagnostic_deadline` | **31.71 s** | no |

**Same executable, same XBE, same strict profile.** The trapped run dies at 4.77 s; the default run reaches
the **full requested deadline**. The same contrast repeats on `cacfb64c8190` (4.76 s vs 31.89 s) and
`d447749b9e60` (4.76–6.19 s vs 31.86 s).

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
`9597ff7c2a37`: 32.61 s; no OOM). So the OOM is specific to the **recent build + trap** combination.

### The failure signature (unchanged across all six OOM runs)

```
xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)
[ICALL] invalid target 0x00000000
[EXCEPTION] code=0xE0424943
```

**`requested 598869040` ≈ 571 MB**, against a 48.5 MB arena — a guest-side allocation request, not a log
buffer. So this is the guest taking a path that asks for ~571 MB, and the trap configuration is what sends
it down that path.

## What this means for `PIO_FREE`

| If the packet's claim needs… | Usable horizon on the current build |
|---|---|
| **default** (untrapped) strict runs | **~31.7 s — the full requested deadline** |
| **instrumented** (trapped/traced) strict runs | **~4.8 s** |

**So the owner's framing needs one refinement, and it is favourable:** A2h is **not** a universal
prerequisite for a strict claim. It is a prerequisite **only for claims that require instrumented strict
runs.** A claim decidable from default strict runs — or from static/source evidence plus the existing
archived prefix — is **not** bounded by 4.8 s at all.

**This also means the `A4b2-r8` acceptance is unaffected**, and the Hy4 advisory was right to call it *"not
a violation"*: every `A4b2-r8` decision input is log-bound and emitted before 4.77 s.

## What this does **not** establish

- **It is correlational, from archived runs.** The archive varies build, era and configuration together, so
  the exact causal mechanism is **not** proven. The **one-build/two-configuration** comparison on
  `bc8e288dd54d` is the strongest evidence, and it is still one pair.
- **It does not identify what the guest does differently under the trap.** A plausible reading is that the
  trap changes the value or timing the guest observes at the trapped read, sending it down a path that
  requests ~571 MB — but **that is a hypothesis, not a measurement**, and it is not needed for the bound.
- **It does not make the horizon a constant in either direction.** 31.7 s is what the default runs currently
  achieve; it is not guaranteed either.

**Recommended planning posture:** bound the `PIO_FREE` claim so it is decidable from **static/source
evidence plus the existing archived prefix** if at all possible — which is what the Planner has already
chosen. If a claim genuinely needs instrumented guest time, **name A2h as a prerequisite and state the
~4.8 s budget explicitly.**
