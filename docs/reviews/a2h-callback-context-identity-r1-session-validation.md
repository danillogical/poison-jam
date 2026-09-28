# `A2h-callback-context-identity-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-callback-context-identity.md`, authored by Planner
`6df57264-9168-48a5-9642-f06b981a0c24` (`codex/gpt-6-sol` @ `high`), committed `4f75f9b`.
**Authority:** the `O-OPEN` row selection (`a2h-callback-slot-writer-r1-row-selection.md`) and the Advisor's
input restrictions (`a2h-integrity-audit-remediation-advisor-ruling.md`).

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-callback-context-identity-r1`** |
| Lines | **17** (18 with the trailing newline) |
| Bytes | **6053** |
| **SHA-256 (frozen)** | **`983581437B8F131E1A6D2A59261C4C3279627EB8DEC7A2A635EC09EC3C919654`** |

## Validation — 16 of 16 checks pass

**Every Advisor input restriction is encoded verbatim, and the packet preserves the disciplines this line
paid for:**

| Check | Result |
|---|---|
| **mapping-clean + XBE + log lines ONLY** | **OK** |
| **per-run dump gate, never the census** | **OK** — *"for THAT run"*, and the **23/24 census explicitly barred** |
| **known-answer control for EVERY extraction** | **OK** |
| **the worked `djv000_0.adx` control** | **OK** — with the exact expected output |
| **the `.text` control prefix** | **OK** |
| **no tainted caller bindings** | **OK** |
| **pending acceptance is NOT clearance** | **OK** — *"not cleared by its silence"* |
| **declared boundaries, never inferred** | **OK** |
| **the alias is PERMANENTLY REFUTED** | **OK** |
| **no re-litigation** | **OK** |
| **`device+0x242C` kept separate** | **OK** — *"it ran but does not feed this call"* |
| **`O-ALTERNATE-PATH` needs VERIFIED identity** | **OK** |
| **no positive row from a refutation alone** | **OK** |
| **no run/build/test/instrumentation** | **OK** |
| **DR excluded; NULL line not reopened** | **OK** |
| **siblings contrastive** | **OK** |

## ⚠ The packet's premise is now PARTLY SUPERSEDED — and the Session must say so

**The Session executed static-first steps 1, 3 and the negative half of 4 BEFORE this packet was frozen**, and
recorded them at **`docs/reviews/a2h-callback-context-identity-identified.md`** (`0889ae9`). **The Planner had
already finished when the Session's redirect arrived, so the packet's line 4 still lists those as open work.**

**What is DONE and should not be redone:**

- **Static-first 1:** the five containing functions are found — **`sub_00194300`, `sub_00194480`,
  `sub_00194A72`, `sub_00194EEF`, `sub_00196C0B`** — from **declared** `Original:` ranges.
- **Static-first 3:** the field layout is **consistent across four of the five**, and the shape is
  **`edi = ecx` (context), `esi = [edi]` (device), device fields through `esi`.**
- **Static-first 4, negative half:** **no site's `ecx` IS the device** — **the context HOLDS the device at
  `+0x00`.**

**What REMAINS, and it is the packet's real edge:**

> **Static-first 2 — the context's ALLOCATION SITE.** **The object is characterised; its producer is not
> found.** **So `O-ALTERNATE-PATH`'s *"producer and field-layout evidence"* is HALF met: the field-layout half
> is verified, the producer half is not.**

**Per the packet's own line 14, the row stays `O-OPEN` until the producer binds** — **and the packet is right
about that.** **The Session is NOT upgrading the row on its own finding.**

**The executor must read `a2h-callback-context-identity-identified.md` FIRST** so it starts from the
identified shape rather than re-deriving it, **and must still satisfy the packet's controls independently.**

## Freeze decision

**The packet is correctly scoped, its input restrictions are exactly the Advisor's, and every validation check
passes.** **It is `ADEQUATE`, validated, and frozen.**

**Its premise being partly superseded does NOT change its class, rows or prohibitions** — **it means the
executor starts further along, and the Session has recorded the delta rather than silently editing a frozen
packet.**

**No synthetic completion.** **No toolkit change required or authorized.** The producer line stays **PARKED**;
`PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays
**`UNRESOLVED`**.
