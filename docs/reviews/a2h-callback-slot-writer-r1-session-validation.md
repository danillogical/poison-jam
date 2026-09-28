# `A2h-callback-slot-writer-r1` — Session validation, freeze, and a **concurrent-write incident**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-callback-slot-writer.md`, authored by Planner
`db69694f-7ba9-42c0-87b1-1e20d33ba893` (`codex/gpt-6-sol` @ `high`), committed `537ab9a`.
**Authority:** the Session's redirect `docs/reviews/a2h-callback-slot-identity-redirect.md`.

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-callback-slot-writer-r1`** |
| Lines | **19** (20 with the trailing newline) |
| Bytes | **7488** |
| **SHA-256 (frozen)** | **`D2CA02E17E2BACC1DE3726B3247965EA3A3B3061B504C06C9601F7FB4B894E3A`** |

---

## ⚠ A concurrent-write incident — the Session's `git add -A` swept an in-progress packet

**The Planner detected this and reported it, which is exactly the right behaviour:**

> *"concurrent workspace write detected: before my commit `git status` showed packet as `M` rather than `??`,
> and commit `4699204` records only 1 insertion/1 deletion (not 18 new lines), suggesting your session or
> another agent committed the same packet while I was writing."*

**What happened, verified from history:**

| Commit | What it is | Who |
|---|---|---|
| **`17a0d9e`** | the Session's redirect record — **and it swept in the Planner's IN-PROGRESS 18-line packet** via `git add -A` | **Session** |
| **`4699204`** | the Planner's intended packet commit, landing as a **1-line delta** because the file was already tracked | Planner |
| **`537ab9a`** | the Planner's **redirect-applied revision** — the current, correct content | Planner |

**The Session's error:** it ran `git add -A` while **another agent was mid-write on a file in the same tree.**
**That is the second time this session a broad `git add -A` has captured work the Session did not author** (the
first was a scratch file). **The Planner caught both the symptom and the likely cause, and explicitly asked the
Session to inspect history before promoting — which the Session has now done.**

**No content was lost:** the final packet is the Planner's **redirect-applied** revision, and it is coherent.

## Validation — 13 of 13 checks pass

**The packet implements the Session's redirect faithfully, and the Session verified each element:**

| Check | Result |
|---|---|
| **identity gate FIRST** | **OK** — the question is *"What object does poller `edi` denote… and is `edi+0x1C4` actually `device+0x242C`?"*, with the writer question **conditional** |
| **all five call sites named** | **OK** — `0x194419`, `0x1944D3`, `0x194AB8`, `0x194F71`, `0x196C38` |
| **`edi=ecx`, `esi=[edi]`** | **OK** |
| **`[device+0x2268]==device` test** | **OK**, statically **and** in an integrity-checked archived dump |
| **conditional section sweep** | **OK** — *"CONDITIONAL ON IDENTITY"* |
| **selector search conditional** | **OK** — *"ONLY IF same object"* |
| **the `+0x247C` boundary respected** | **OK** — the packet explicitly refuses to claim the copy initialises `+0x2268` |
| **no instrumentation** | **OK** — `NONE pre-authorized` |
| **`loc_` anchoring / no inferred byte-scan** | **OK** |
| **`uint32` normalization** | **OK** |
| **DR records excluded; NULL line not reopened** | **OK** |
| **float-bit siblings contrastive** | **OK** |
| **`O-ALTERNATE-PATH` covers the redirect** | **OK** — *"Verified distinct context identity (`edi≠device+0x2268`)"* |

## One honest caveat on the pinned baseline

**The packet pins *"game `5e3dc20`"*.** **The tree is now at `537ab9a`**, which includes the Session's redirect
record and the Planner's two packet commits. **The Session's validation record, this file, and the redirect are
all documentation-only**, so **no source or binary identity has moved** — **but the executor should re-pin at
execution time as the packet itself requires** (*"re-pin both trees, original XBE/section map, generated/recovered
source and archived terminal evidence before execution"*). **Recorded so a stale pin is not treated as a
contract violation.**

## Freeze decision

**The packet is identity-first, correctly scoped, disciplined about its own hypotheses, and every validation
check passes.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class (`discovery`), its rows, or its prohibitions.** **No synthetic completion.**
**No toolkit change is required or authorized.** The producer line stays **PARKED**; `PIO_FREE` stays
**DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.

## A process note the Session records against itself

**When multiple agents share a working tree, `git add -A` is unsafe.** **The Session should scope its adds to
the files it authored**, and it did not. **Two incidents in one session is a pattern, not bad luck.** **Recorded
so it is not repeated.**
