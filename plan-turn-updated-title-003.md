# Turn plan (updated) — `title-003`

Living execution plan. Owner: the Orchestrator (`workbuddy-ai/deepseek-v4.1-flash` @ high).
Baseline: `plan-turn-start-title-003.md` (immutable). This file records how execution actually
evolved and why.

## Status at last update

**Both objectives landed.** The primary objective — a generalized alias-shim / hidden-entry detector
— is built, gated with no baseline, and it repaired 50 live defects. The secondary objective — resume
the runtime chain around `0x00154540` — was attempted and **succeeded**: run g07 produced an
ABI-verified return through the repaired thunk, so stop 20 is now **runtime-confirmed**, and g07 then
exposed a new stop of a **mirror-image** class that is repaired.

**The title screen is NOT reached and M15 is NOT claimed.** Presents still freeze at exactly 1000 with
the disclaimer hash `5bdaea576b8509f5` unchanged.

| # | Blocker | Fix | Run evidence |
|---|---|---|---|
| 20 | `ABI FAILURE 0x00154540 expected +24` (observed delta 16) | two this-adjusting thunks corrected 20→12; swallowed `0x154560` recovered (`stack_args 20`) | **g05 found it; g06 did NOT exercise it; g07 CONFIRMED it** — `[RECOVERED] 0x00154540 returned; ABI verified` |
| 21 | `[ICALL] Failed to resolve VA 0x000B5F82` | `0xB5EB0` restored to its real end `0xB6732`; false split `0xB5F3A` removed; generated patch `remove-b5f3a-dispatch` (L02) | **g07 found it**; byte-derived repair, **NOT yet runtime-confirmed** |
| — | 50 over-wide spans consuming 63 separately evidenced entries | spans tightened; 48 new reviewed entries; 5 already owned an entry; 2 already had generated bodies | byte-derived + the new gate; no run needed |

## PLAN_CHANGE

- **Changed:** the detector was implemented against a **stronger** invariant than the starting plan
  proposed, and the gating class was split three ways rather than one.
- **Evidence:** the plan proposed reusing owner reachability as the discriminator, which is correct but
  insufficient on its own — it is a *negative* test and produced 250 raw candidates, dominated by
  `.rdata` word-array noise and bogus micro-spans. Adding the "body provably ends early" half (a
  **fully enumerated** walk with no fall-off and no opaque exit) cut that to 50, and every one is a
  `tail_jump_alias` container. Splitting the verdict into `HIDDEN_ENTRY` / `OVERLAP` / `SHADOWED`
  became necessary when a real `LNK2005` proved that two consumed addresses already had generated
  bodies and must **not** receive a new entry.
- **Why:** the plan's framing would have produced a noisy classifier; the measured invariant produces
  a zero-baseline gate. The change is toward the objective, not away from it.

## PLAN_CHANGE

- **Changed:** the repair of the new stop was extended beyond the manifest into
  `config/generated-patches.json`.
- **Evidence:** restoring `0xB5EB0`'s span alone did not link. `recomp_dispatch.c` is
  translation-owned and still carried `{ 0x000B5F3Au, (recomp_func_t)sub_000B5F3A }`, so the build
  failed with `LNK2001: unresolved external symbol sub_000B5F3A`. The patch
  `remove-b5f3a-dispatch` (L02) follows the existing `remove-54750-stub` precedent.
- **Why:** a manifest-only repair was incomplete; the generated dispatch had to be corrected too, and
  the sanctioned mechanism for a translation-owned file is a recorded patch.

## Primary objective — the detector

`scripts/check-hidden-entries.py`, with `tests/test_hidden_entries.py` (17 tests), CTest
`jsrf_hidden_entries`, and a `just hidden-entries` recipe. Wired into `just check`. **No baseline.**

The invariant, both halves load-bearing:

> A manifest entry's declared span must not extend past the end of its own reachable body into an
> address that has independent evidence of being a separate executable entry.

Half 1 reuses `Analyzer.walk` from `check-stack-depth.py` and requires the walk to be **fully
enumerated** — no truncated decode, **no fall-off**, no `indirect`/`terminal` exit, last reachable
instruction a terminator. That is what makes "the walk never visited this address" a proof. Half 2
requires an aligned `.data`/`.rdata` dword into `.text`, or another manifest start, plus the §12 noise
filter (aligned, not a padding byte, decodable, preceded by an alignment boundary).

**The `0x80BD0` lesson is honoured explicitly.** `0x80BD0` has zero rel32 callers and is a real SEH
function, so caller absence proves nothing; the test used here is that *this entry's own control flow
was completely enumerated and did not arrive at the address*. A test asserts the rejected test appears
nowhere in the decision procedure.

Verdicts: `HIDDEN_ENTRY`, `OVERLAP`, `SHADOWED` gate; `MISDISPATCH`, `OVER_RUN`, `UNQUALIFIED` are
reported only. `UNQUALIFIED` is never silently clean — `0x96F60` ends in `jmp [eax+8]` and a control
asserts it stays undecided.

**Two independent reproductions** support "not overfitted to `0x7DBD0`": a prototype written before the
checker agreed with `check-table-targets.py`'s existing in-span rule on **50 of 52** candidates (the two
differences being exactly the two opaque-exit containers), and it re-found `0x96F80` unprompted, which
§12 had already recorded as a known open instance.

## Secondary objective — the runtime chain

Run **g07** (`20261005-211627-927-g07-thunk`, exploratory, 900 s budget, ended `unhandled_exception` at
241 s, 546 ABI-verified returns, zero ABI failures) cleared stop 20 and produced stop 21.

**The new stop is the mirror image of the primary objective's class.** `0xB5EB0` was tightened to
`[0xB5EB0, 0xB5F3A)` by an earlier pass, which cut the function's **own** jump table at `0xB5F0F`: 8 of
its 9 arms lie beyond the declared end, so the lifter emitted `RECOMP_ITAIL` instead of a resolved
switch and the guest trapped on the `0xB5F82` arm. `0xB5F3A` is not an entry — it is reached by
fallthrough from the table arm `0xB5F16`, its only "pointer" `0x228214` is inside a packed `.data` run,
and its walk exits at the same `0xB672F ret 4` as `0xB5EB0`.

**A discriminator was tried and discarded.** The "reads a register before writing it" test that proved
`sub_000BBA04` false does **not** generalise: 684 genuine manifest entries trip it (330 read `ecx`
first, which a thiscall entry legitimately does, and 321 read `ebp`). The repair rests on
owner-reachability and jump-table-arm evidence instead.

**The under-wide class is measured but NOT gated.** 71 adjacent-entry pairs exist where the owner's walk
reaches the next entry's start, but the population is dominated by legitimate splits (the
`0x200A5`/`0x200A8`/`0x200AD` run is a chain of real continuations). The discriminator that separates
"the owner's own table arm reaches this address" from "this is simply the next function" is **not
established**, so it is recorded as an open measurement rather than frozen into a gate.

## Three real defects caught by tests rather than by reasoning

Two were **silent**, which is why they are recorded:

1. `apply()` keyed additions to the *container's* start, so all 50 new entries were dropped while the
   script printed `wrote 3110 entries` and the checker still passed — removing the additions removes
   the finding. Caught by counting `set(after) − set(before)`.
2. Two additions collided with generated bodies (`0x556D0`, `0xC42E0`) — a real
   `LNK2005 … already defined in recovered.obj`. The checker's first version decided "resolves at
   runtime" from a set that defaults to empty.
3. `0xB3C30` was given an extent one tail-jump short, so the generated body called a fatal stub for its
   own continuation — `tests/test_recovery_span_ownership.py` caught it.

## Advisor status — the route did not run

The Persistent Advisor was spawned **three times** on the detector-design consultation (invariant
soundness, gate-versus-census, the trap/misdispatch split) and each child **failed before finishing
with no closing message**. Per `docs/agent-workflow.md` §1 an unavailable route is reported, never
silently replaced, so **no Advisor ruling is claimed for any decision in this turn**. The design rests
on the measurements above, on the two independent reproductions, and on the four real defects the tests
and the run caught.

## Verification and closure

- `just check` green; CTest **38/38** (was 37); stack-depth `--selfcheck` **10/10**; hidden-entry
  `--selfcheck` 5/5 controls + 17 unit tests, including a deciding negative control that re-injects the
  motivating defect into a temporary manifest and requires the real gate to exit nonzero.
- `recovered.c` regenerated (3157 functions); provenance manifest and preservation baseline
  re-recorded with their hand-maintained history preserved (41 amendments, 3 regenerations).
- Commits `6f2e4e2`, `b2179d2`, `64945a3`, `d8632ce`; pushed to `origin` (`b065050..d8632ce`).
- Toolkit `6e6e056` clean and **intentionally unchanged**.

**A known tool defect was hit four more times and worked around, not fixed.**
`check-generation-provenance.py --write` records only the measured axes and erases the hand-maintained
`amendments` and `regenerations` history. Each time they were re-attached from the pre-write copy. The
tool is still wrong; the workaround is not a fix.
