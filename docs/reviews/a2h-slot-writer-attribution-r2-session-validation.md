# `A2h-slot-writer-attribution-r2` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-writer-attribution.md`, **r2**, authored by Planner
`5b35d97d-11dd-41f6-8487-d3ca103b06cb` (`codex/gpt-6-sol` @ `high`), committed `b980f13`.
**Supersedes:** the draft `d3cd8ae` (`C2DD3ED8…AA27`).
**Authority:** `a2h-page-granularity-classifier-ruling.md` (turn `01a0e940`) — **apply verbatim; this ruling IS
the preflight.**

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-slot-writer-attribution-r2`** |
| Lines | **18** (19 with the trailing newline) |
| Bytes | **7922** |
| **SHA-256 (frozen)** | **`E209D1F4A4405F0B266E8D4DEFDA77A481A277D27615BFAE9ACB9F7A3A6C6378`** |

## Validation — 19 of 19 checks pass

### Both structural defects are encoded as the ruling directs

**DEFECT 1 — the page-granularity flaw:**

- **re-arm after EVERY write** ✓ (option (a), the accepted shape)
- **terminal-value coherence as a REQUIRED gate** ✓ — **last-recorded slot value vs terminal read**
- **mismatch ⇒ `UNKNOWN`** ✓ — **the worked example (`0x15F9D0` vs `0x1D5078`) is in the packet**

**DEFECT 2 — the classifier paradigm:**

- **range-based native-RIP classification** ✓
- **guest-byte expectations FORBIDDEN** ✓
- **the `0x7B3` residual is NOT to be verified** ✓
- **no fault-RIP encoding is cited as evidence** ✓ — **including ON-1/ON-2 and ON-3's `enc=3`**

### The packet structure is right

| Element | Packet |
|---|---|
| **ONE packet, fix + rerun together** | ✓ — *"splitting strands the fix unverified"* |
| **the order** | ✓ — **classifier fix → coherence gate → attribution read** |
| **Exp0** | ✓ — **offline range proof, gates everything** |
| **Exp1** | ✓ — **installer trap range-classified, MUST fire** |
| **Exp2** | ✓ — **N ≤ 5 pre-specified, early-stop, K ≥ 2-agree, zero qualifying ⇒ RE-REFER** |
| **AC97 + dual-`#DB` preserved** | ✓ |
| **DR excluded** | ✓ |
| **address never hardcoded** | ✓ — **`g_memory_offset` is the derivation** |

## ⚠ What this packet inherits, and what the Session verified

**The live phase PROVED the mechanism works** — **9 078 AVs, 9 035 steps, `rearm_failed=0`, `aliases=29/29`,
cross-validation clean, the trap firing at the corrected host page `0x001AD62C`, and the `0x001D5078`
terminal reproducing with the ICALL 4-cycle.**

**And it found two structural defects, BOTH now adjudicated and encoded.**

**The Session notes the honesty of the Planner's own record:** **it marked its first draft `INADEQUATE /
BLOCKING` and flagged stop and re-referral rather than silently redesigning an approved shape** — **and the
revised packet is `ADEQUATE` *"for the approved plan, not an execution result,"* which is the correct
scoping.** ✓

## The Session's own corrections, recorded

**Three of the Session's claims were WITHDRAWN across this phase** — **the "no competitor" inference, the
"control is green" inference, and the "chain is tied" inference** — **and a fourth correction removed the
`0x7B3` residual, which the Advisor then ruled dies with the paradigm entirely.**

**All four were the same error: promoting an observation into a conclusion the instrument did not support.**
**The Session records that pattern rather than the individual instances.**

## Freeze decision

**The packet applies the ruling verbatim, both defects are encoded, and every check passes.** **It is
`ADEQUATE`, validated, and frozen.**

**Nothing changes its class, rows or prohibitions.** **No synthetic completion.** **Page-protection only — DR
stays EXCLUDED.** The producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**;
`A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**; **the retired NULL line
is not reopened and no DR record may be cited.**

## Execution order

1. **Exp0** — offline range-based classifier proof.
2. **Exp1** — control run; **the installer trap must fire**.
3. **Exp2** — bounded attribution, N ≤ 5, early-stop, K ≥ 2.
