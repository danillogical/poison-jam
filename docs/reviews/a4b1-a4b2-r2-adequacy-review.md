# A4b1-r2 / A4b2-r2 adequacy review — INADEQUATE (both), from two independent reviewers

**Reviewers:** two **independent** fresh Planners that wrote neither packet (§5.1.5) —
`220ed309-29fb-4e31-8429-7540820a0e57` and `ab6cb9f8-3c94-479d-b1cb-36601797c66d` (the latter
recovered after a transient provider rate limit and completed). Both verified the hashes:
`A4b1-r2` `839E9BEC…D2BB7`, `A4b2-r2` `F8348C5C…914EB`.

**Both verdicts: `INADEQUATE`, each for one small blocking gap — and they found *different*
gaps.** Both independently confirm **the ledger redesign is sound** and that **all seven r1
blocking defects are closed**.

## What both reviewers agreed on (the redesign works)

- The ledger is **faithful to the ruling**: atomic `seq`; uncapped counters; per-class
  write-once latches via `InterlockedCompareExchange`; the five GP classes at the CAS point;
  `apu_watch_cpu_store` checking its own target; the 16-site `CPU_ZERO` table plus
  `CPU_ZERO_OVERFLOW`; `CPU_OTHER` counted only; latch lines once per firing plus counts lines;
  snapshot/reset accessors; the anchor site set by an exported setter, not hard-coded.
- **r1 B1 CLOSED** (every case resets and decides on the snapshot); **r1 B2 CLOSED** (the
  unmapped line is unconditional, required in trace-off, and excluded from its count);
  **r1 B3 CLOSED by the redesign** (`GP_CLEAR` latches only on a successful 3→0 CAS, so a
  pre-command write-back cannot consume it; exempt/cap rules deleted); **r1 B4 CLOSED**
  (`* -text` committed first, checked with `ls-files --eol`, hash taken byte-exact from
  `cat-file blob`).
- `A4b2`: **r1 B1/B2/B3 all CLOSED.** `AC-CLEAR` **cannot PASS without the GP's own DMA CAS
  exchanging a 3**; `R2-CPU` now requires a **positive** non-control `CPU_ZERO` latch after the
  anchor; absence routes to `R2-UNATTRIBUTED` or `UNKNOWN`.
- Rows are ordered and exhaustive with a single match, and every row names its next packet.

## BLOCKING — `A4b1` B1 (reviewer A, `220ed309`): latch-line fields undefined and unchecked

Device semantics 6 lists the latch fields `seq, va, observed, payload, site, frame, insns,
dsp_addr` but **defines only `frame`**. It never states that a GP latch's `va` is `W_va`, though
the ruling (a)2 requires `W_va` "recorded in every latch"; `insns` and `dsp_addr` are undefined
too. `AC-FIX` checks the snapshot and only two pieces of log text (the `GP_CLEAR` line in (i);
`boots=1 gp_frames=1` in (vi)) — but **`A4b2` decides entirely from the line text** (`va =
B+0x810`, `insns > 0`, `seq` ordering, `site ≠ 001A18CE`).

**Scenario:** an executor naturally records a GP latch's `va` as the DMA write's *destination
start*. A write-back of `B..B+0xN` with `N > 0x810` then gives `GP_CLEAR va=B` even though the
GP really cleared the word. Every fixture case still passes. In `A4b2`, `AC-CLEAR` step 3 gives
UNKNOWN and the rerun repeats it → **false `R2-UNKNOWN`**. This also breaks the §6.1 seam rule.

**Required:** DS6 defines each field (GP `va = W_va` at the event; CPU `va = target_va`; `seq` =
that event's own `InterlockedIncrement` value; `insns = gp_insns` at the event; `dsp_addr` = the
DSP-side address of the dword); `AC-FIX` asserts each latch line's `class`/`seq`/`va`/`observed`/
`payload`/`site` equal the snapshot (with `va = W_va` in (i), `site = S_1` in (iv)); and the
counts line's `boots`/`gp_frames`/`gp_insns` equal the snapshot in (vi).

## BLOCKING — `A4b1` B1 / `A4b2` B1 (reviewer B, `ab6cb9f8`): `[GPIN]` is still a lossy log that decides a criterion

`[GPIN]` is **once per distinct (kind, addr)** and its dedup set and "until `GP_CLEAR` latches"
cut-off **survive `apu_watch_reset()`**, which clears only counters, latches, CPU sites and run
counters. And `A4b2`'s **`AC-INPUTS` PASSes on the *absence* of a stub-class `[GPIN]` line.**

**Scenario 1 (false FAIL):** in the trace-on process, (a) bootstraps from the scratch page and
emits `(DMA_READ, page)`; (e)/(i) latch `GP_CLEAR`; (vi) bootstraps from the same page, so **no
`[GPIN]` line appears** in (vi)'s output → false FAIL → `R1-PORT-FAIL`. The packet's own line 171
("no case depends on the order of other cases") is false for this state.

**Scenario 2 (false PASS):** the dedup table's size and overflow behaviour are unspecified. A
fixed table that drops silently fills during the many pre-command frames; a later `PERIPH` read
of a shim constant then produces no line → **`AC-INPUTS` PASS → false `R2-PASS`** while a stub
constant shaped the GP's result. Also, (vi) exercises only `DMA_READ`, so a missing `PERIPH`,
`FIFO_READ` or `MIXBUF` hook passes **vacuously**.

**Required:** make `[GPIN]` lossless (an unbounded set, or a bounded one whose overflow is an
uncapped counter plus a write-once `GPIN_OVERFLOW` latch shown in the counts line); clear the
dedup set/cut-off in the reset (or a named trace-reset accessor every case calls); add `AC-FIX`
cases for **every** kind and for overflow; and make `A4b2`'s `AC-INPUTS` **UNKNOWN on overflow**
or when no `[GPIN]` accounting is present. `A4b2`'s P2 must map each `[GPIN]` kind it classifies
to the `AC-FIX` case exercising it — its current "[GPIN]: case (vi)" covers one kind of four.

## `A4b2` B1 (reviewer A): the P2 seam pin

`A4b2`'s P2 pins `A4b1-r2` and claims `AC-FIX` exercises every toolkit behaviour it decides on.
Closing `A4b1` B1 produces `A4b1-r3`, so **P2 can never hold as written**. Required: repin P2 to
the revision that closes `A4b1` B1 and map each decision field (`GP_CLEAR.va/.observed/.insns/
.seq`; `CPU_ANCHOR.seq`; `CPU_ZERO.site/.seq`; counts `boots`/`gp_frames`/`gp_insns`) to the
`AC-FIX` case asserting it. **`AC-CLEAR`'s logic needs no change.**

## DEFERRED (advisories, recorded not fixed)

- **`A4b1` D1 — the DS5 CAS race.** During the pre-command zero write-backs the guest's store of
  3 can land between a failed CAS that observed 0 and the following ordinary write of 0; the real
  clear is then classified `GP_ZERO_OVER_ZERO` → `R2-UNATTRIBUTED` (or `R2-CPU` if a later
  stop-path zero is latched). Nanoseconds wide, judged implausible. Cheap fix: when payload and
  observed are both 0, **skip the write**; otherwise retry the CAS with the observed value.
- `A4b1` D2 `GP_NONZERO_OVER` is never fixture-exercised (nothing decides on it). D3 the r1
  follow-ups stand (fixture infeasibility → `R1-PORT-FAIL`; `add_test` may run before
  `include(CTest)`). D4 (vi)/(vii) need GP state reset first — `apu_watch_reset` does not reset GP
  state. D5 the `CPU_ZERO_OVERFLOW` counter value in (v) is unstated. D6 Readiness says "select no
  row" but also "(→ R1-PORT-FAIL)" — contradictory wording. D7 line 107 says "one per other
  class" but `CPU_OTHER` is never latched.
- `A4b2` D1 `AC-BOOT` reads the 400-line-capped `[APUMMIO]` log; absence gives UNKNOWN so it
  fails closed, but it is in literal tension with §6.1 — consider deciding from `[GPBOOT]`
  `gprst`/`prev` plus the counts line's `boots`. D2 step 1 should cite sites by content/VA, not
  baseline line numbers. D3 a site's first-zero latch hides a later zero from the same site
  (→ `R2-UNATTRIBUTED`, safe). D4 `AC-CLEAR` step 4 is checked before `R2-CPU`, so a pre-command
  `ZERO_OVER_OTHER` can hide a real CPU witness — conservative, routes to the Advisor. D5
  `MIXBUF` records only the first value. D6 `PASS` has no claim limit for a second command that
  stops at the spin again, or a clear by a periodic write-back.

## Session note — the pattern

Both blocking defects are the **same defect class the ruling removed for the watched word**,
applied to *other* artifacts: a **first-N / first-per-key log deciding a criterion**, and
**decision-input semantics neither defined nor fixture-tested by the packet that owns them**.
`[GPIN]` deciding `AC-INPUTS` is a direct §6.1 violation ("no row may depend on the presence or
absence of such a line"), so it is fixed by **applying the already-ruled rule**, not by a new
Advisor consult — no third §5.5 trigger. The fix is local in both cases and is **not a redesign**.
