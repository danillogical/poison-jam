# `A2h-dr0-delivery-gate-r1` — Phase 1 trial: **`P1-UNKNOWN`**, gate defect found, **STOP → Advisor**

> ## ⚠ ADVISOR RULING APPLIED — two corrections to this record
>
> **Ruling:** `docs/reviews/a2h-dr0-gate-repair-advisor-ruling.md` (turn `01a0e7d4`). **One bounded repair
> packet authorized, reshaped; the ceiling applies after it.**
>
> **Correction A — the post-continue ARRAY IS VOID IN BOTH DIRECTIONS.** The Session wrote that
> `post_continue_reverted=13198` *"very likely measures the measurement."* **The Advisor voids the whole array,
> *"including any no-revert reading,"*** because **`post_continue_ok=1216` does not establish that those 1216
> were meaningful either.** **A race does not produce trustworthy positives just because it sometimes returns
> the expected value.** **So no conclusion in this record may rest on `post_continue_*` at all — including the
> `CONTEXT_LOSS` discriminator and the `seq=97` watch-set observation below.**
>
> **Correction B — the CARRY RULE.** This record proposes *"one ON trial"* for a repair. **The Advisor adds:
> a new code change breaks the OFF carry rule, so the repair needs a FRESH OFF *and* one ON.** **The Session
> had missed this.**
>
> **And the Advisor REFUSED the Session's own suggestion** that the underlying evidence might suffice for
> `NON_FIRING` without the broken `complete` flag: ***"Strong ≠ decidable."*** **A zero-`#DB` observation needs
> the delivery-completeness premise, and there is no evidence `#DB`s enter this debugger's stream at all —
> which is the untested positive control.** **Declaring `NON_FIRING` by fiat would substitute judgment for the
> packet's own decision rule.** **The Session accepts this without reservation.**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-dr0-delivery-gate-r1`, frozen
**`12A0B68EB550A96DCD4D2393DBC81262AC0CEE2838CC403A9FD99ED18D3BE537`**.
**Runs:** one **fresh OFF** (`…043810-117-a2h-dr0-gate-inert-off`) + **one ON** (`…043836-929-a2h-dr0-gate-on-1`),
both strict, same build.

**The packet's own stop rule:** *"failure means no verified install `#DB` by closure, unrepairable handling
within declared scope, or **irreconcilable/lossy evidence** — STOP → Q3/Advisor, no serial speculative
repairs or attribution."* **The gate returned `UNKNOWN_NOT_RECORDED` because of a defect IN THE GATE, not
because the channel is dead.** **That is irreconcilable evidence and the Session is stopping rather than
patching and re-running.**

---

## Fresh OFF — **record-level inert, and it was required**

The collector changed, so the carried OFF was invalid. The fresh OFF shows **zero** `[A2HSLOT]`, **zero**
`GUEST_DR_*`, **zero** `install_exec`, and a **frozen registry all-zero** (`install_present=False`,
`install_ok=0`, `watch_armed=0`).

**The 8 `GUEST_SLOT_*` lines are the PRE-EXISTING family**, all-zero and **byte-identical to the previously
accepted OFF** (`…030751-407`), so they are not new instrumentation. **Recorded because a naive prefix scan
flags them.**

## Phase 1 verdict: **`P1-UNKNOWN`** — and the reason is a GATE DEFECT

```
GUEST_DR_DELIVERY_TERMINAL raw_events=14471 raw_exceptions=14408 raw_single_step=0
  ss_first_chance=0 ss_second_chance=0 ss_routed_generic_first=14405 ss_routed_terminal_second=1
  other_code_exceptions=14408 post_continue_reads=14414 post_continue_ok=1216
  post_continue_reverted=13198 post_continue_overflow=1 install_hit=0
  watch_armed_value=00000000001D4064 canonical=00000000001D4064 complete=0
  decision=UNKNOWN_NOT_RECORDED
```

**`complete` is computed as:**

```c
int complete = (dr_raw_event_total > 0) && !dr_raw_ss_overflow && !dr_post_continue_overflow;
```

**`raw_events=14471 > 0` ✓ · `ss_overflow=0` ✓ · `post_continue_overflow=1` ✗ ⇒ `complete=0`.**

**`A2H_POST_CONTINUE_CAPACITY` is 16. The readback runs after EVERY `ContinueDebugEvent`, so it fired
14 414 times.** **The overflow flag is therefore set on ANY real run, `complete` is structurally 0, and
`NON_FIRING` IS UNREACHABLE BY CONSTRUCTION.** **The gate cannot return the answer it exists to produce.**

**And the 16-entry array fed by 14 414 events is a RUN-LENGTH-SIZED HISTORY, which the packet explicitly
forbade** — *"bounded arrays with explicit overflow flags … no run-length-sized history."* **Its overflow flag
then poisons the decision.** **So the defect is not merely a small capacity: the design conflates a
per-event history with a decision input.**

## A second defect: the post-continue readback measures a RUNNING thread

> **⚠ EVERYTHING FROM HERE TO THE END OF THIS SECTION IS VOID** per the Advisor's Correction A: the whole
> `post_continue_*` array is void **in both directions**, so the readings below are **not evidence of
> anything**, including the "two candidates" framing. **The Session's reading was too generous — it treated the
> `seq=97` non-zero reading as surviving, and the Advisor explicitly voided *"any no-revert reading"* too.**
> **Retained below only to show what was observed, not as findings.**

**The install thread (`tid 63568`) has 13 212 post-continue readbacks: 2 with `DR0` SET and 13 210 with
`DR0` ZERO.** **Last non-zero at `seq=97` (`dr0=00000000001D4064 dr7=00000000000D0001`); first zero at
`seq=110`.**

**Two candidates, and the evidence CANNOT separate them:**

- **(a)** something genuinely cleared `DR0`/`DR7`;
- **(b)** **`GetThreadContext` on a thread that `ContinueDebugEvent` has RESUMED returns unreliable values**,
  so the "revert" is an **artifact of measuring a running thread.**

**(b) is more likely, because the readback runs immediately after the continue BY CONSTRUCTION.** **Windows
documents thread context as valid only while the thread is suspended**, and the packet's own premise — *"a
context that reverts across `ContinueDebugEvent`"* — assumed a post-continue read is meaningful. **It is not,
without suspending first.**

**So `post_continue_reverted=13198` very likely measures the measurement, not the watch.** **And
`post_continue_ok=1216` shows the read does sometimes succeed, which is exactly the signature of a race.**

---

## What the trial DOES establish — real, and worth keeping

**These facts are independent of the two defects and are the strongest DR evidence this line has produced.**

| Fact | Value | Significance |
|---|---|---|
| **Debug stream demonstrably read** | `raw_events=14471`, `raw_exceptions=14408` | the zero is **not** an unread stream |
| **Zero `#DB`, either chance** | `raw_single_step=0`, `ss_first_chance=0`, `ss_second_chance=0` | **not second-chance delivery** |
| **The store executed, with correct values** | `install_exec … tid=63568 … raw=80000115 before=80000115 installed=FE000104` | the **latch is tied to the store itself** |
| **The store wrote the EXACT watched address** | `host=00000000001D4064` **==** `canonical=00000000001D4064` | **the Worker's flagged third cause (`host != canonical`) is RULED OUT** |
| ~~The watch was SET on the install thread~~ | ~~`seq=97` → `dr0=00000000001D4064 dr7=00000000000D0001`~~ | **VOID per Correction A** — it is a `post_continue_*` reading, and the whole array is void in both directions |

**The Advisor's scoping of what survives is precise:** *"Surviving independently: store executed, exact address
match, zero-`#DB` as **observed series** (not as completeness)."* **So the `seq=97` watch-set observation does
NOT survive** — it was the Session's own addition and it rests on the void array. **The Session withdraws it.**

**So the picture is now: a proven store, at the proven watched address, on the proven armed thread, with the
debug stream proven read, and zero `#DB` of either chance.** **That is much stronger than the previous
line's evidence — it eliminates the address-mismatch and stream-unread explanations — and it still does not
reach `NON_FIRING`, because the gate's `complete` flag is broken.**

**`ss_routed_generic_first=14405` and `other_code_exceptions=14408` confirm the Worker's defect fix worked:
non-single-step exceptions are counted separately and did NOT inflate the single-step counter.**

## Why the Session is stopping rather than fixing and re-running

**The packet says *"one packet only"* and *"no serial speculative repairs."*** **Fixing the capacity and the
suspend-before-read is a CODE CHANGE to a frozen instrument, and the packet's bound exists precisely to
prevent an unbounded repair loop.** **The correct move is the packet's own stop: `STOP → Q3/Advisor`.**

**The Session is NOT concluding that the watch cannot fire.** **It is reporting that the gate as built
cannot decide, that the trial nonetheless produced strong evidence, and that the decision on whether to
repair is the Advisor's.**

## Prohibitions and status

**No synthetic completion.** No guest semantics changed; the APU trap and `0x80` untouched; no allocation
faked; arena not widened; NULL call not bypassed; guest error handling not edited. **No `src/recomp/gen/*.c`
edit.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**. **No second ON run.**
