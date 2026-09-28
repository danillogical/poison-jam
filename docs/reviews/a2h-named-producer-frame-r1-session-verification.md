# `A2h-named-producer-frame-r1` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a2h-named-producer-frame.md`.
**Class:** **discovery** (§5.8) — the **writing Planner** reviews its own packet against §5.3's two questions
and **no second Planner is spawned**. **No shape preflight** (only a new **change** packet triggers one).
**Authoring Planner:** child `6f40fcb1-4f3c-4beb-ade3-06ba0e38e6d3`, `codex/gpt-6-sol` @ `high`.
**Adequacy:** the writing Planner's own review — **`ADEQUATE`**, `BLOCKING: NONE`,
`PREMISE_FRESHNESS: BOUNDED`.

| Item | Value |
|---|---|
| Revision | **`A2h-named-producer-frame-r1`** |
| SHA-256 (frozen) | **`2333B6523B39866F1AEB9C1BEFFE1CCE42176E35BA3ADB86F8965A265BF88887`** |
| Lines | **37** |

**Hash verified 3× over ~12 s immediately before promotion**, all identical, and matching the Planner's own
reported value.

> **A stale-pin error I caught and corrected before promoting.** An earlier plan revision quoted a draft hash
> for this packet that I had read **while the Planner was still writing**; the file then changed underneath
> it. **The plan now pins no hash until freeze and says why.** This is the promotion discipline this project
> has had to learn more than once: **a file that looks finished is not evidence its author has stopped
> writing.** The freeze below was taken only after the Planner reported completion, the file read stable
> three times, and the hash matched the Planner's own report.

## Command validation (§5.1.5.2 — the Planner asked for this explicitly)

| Named command / path | Validation |
|---|---|
| `python -X utf8 scripts/check-run-profile.py logs/runs/20260927-160330-655-a4b2-gp-trap-trace` | **works** → `STRICT` |
| `python -X utf8 scripts/check-dump-mapping.py logs/runs/20260927-160330-655-a4b2-gp-trap-trace` | **works** → `matches: 0 content-mismatch: 1`, correctly reported *"Not usable for an IMAGE-CONTENT claim"* — so the packet's instruction **not to use a displaced dump as repaired memory** is the right one |
| `python -X utf8 scripts/run-jsrf.py --profile strict --seconds 8 --label …` | **valid** — `run-jsrf.py` accepts `--seconds`, `--label`, `--profile {strict,exploratory,fixture}` |
| `scripts/a2h-oom-slice.py` + `scripts/test_a2h_oom_slice.py` (the reuse targets) | **exist**, **31 tests OK** |
| `JSRF_TRACE_A2H_FRAME` | **does not exist** in the game **or** the toolkit — it is the packet's **deliverable**, correctly gated behind the step-3 seam check |

**No named command is broken, and the one missing identifier is the packet's own output.**

## The packet correctly does **not** inherit a refuted premise

The Planner's own adequacy review states it explicitly: `docs/packets/a2h-oom-causal-slice.md`'s **"no-trap
A2g" premise at lines 7, 9, 16 is refuted by acceptance and not inherited.** **The Session verified this is
so** — the successor does not carry the withdrawn claim anywhere. That is the previous packet's most
important correction being honoured rather than propagated, and it is the specific failure mode the earlier
five-round acceptance existed to find.

## What the packet adds over its predecessor

**It applies the Planner's verified refutation of the Session's narrowing.** The Session had written that
only two of `sub_001497DC`'s eight call sites could supply `arg2`; the Planner showed that because the
function is **`fpo_leaf`** and reads **inherited `g_seh_ebp`**, `[ebp+0x10]` belongs to the **caller's
frame** and **every call site has a frame**. The packet therefore:

- treats the two three-push sites as **leads, not attribution** (*"in a frameless callee `[ebp+0x10]` need
  not equal its explicit third stack argument"*);
- requires the **finite eight-site** set to be reconciled against original call instructions **and all
  indirect/other entries**, continuing with an **explicit unresolved entry class** if the eight are not
  exhaustive rather than closing a negative;
- requires **call modelling as transfer/return/cleanup**, not fall-through — the exact shortcut the
  stage-2 reviewer flagged as non-conservative for a call-graph walk;
- requires tracing **`g_seh_ebp` across the early helper `sub_0017D1F8`** rather than assuming frame
  preservation;
- and requires a **proved write to the exact `[ebp+0x10]` address** for any positive row, with
  *"a coincidentally equal value is not proof of a writer."*

**It also keeps the accepted producer's interval honest:** `0x23B20410` is the **aligned pre-add** value,
**not** automatically the exact unaligned input — the possible input interval is
**`0x23B203F1..0x23B20410`**.

## Prohibitions verified present

**No synthetic completion** — the packet forbids suppressing the APU trap or `0x80`, faking the allocation,
widening the arena, bypassing the NULL call, and proposing a guest error-handling change. **`PIO_FREE` stays
DEFERRED**; `A4b2-r7`, accepted/closed `A4b2-r8` and `A4b1-r4` are **not** reopened; `0xFFFFB3` stays
**`UNRESOLVED`**; any toolkit advance requires **re-establishing P4's discovery-transfer bridge** first.

## Promotion

`A2h-named-producer-frame-r1` at **`2333B652…F88887`** was **frozen and promoted into `CURRENT PACKET` in
the same step**, byte-identical with no revision (§5.3). **The Session did not edit the packet.**

**Next:** execute the packet — the offline bounded audit, extending the existing tested tool; a bounded
8-second diagnostic capture **only if** a safe non-generated seam exists; then §5.8 acceptance.
