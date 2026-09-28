# `A2h-rdata-call-target-r1` — stage-1 acceptance: **`ACCEPT-WITH-CORRECTIONS`** (all three applied)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Reviewer:** `908c33c6-73a4-47d2-9f36-4e8f3d3d9153` (`workbuddy-ai/hy4-preview-f` @ `high`), **independent** —
it neither authored the packet nor executed it.
**Review:** `docs/reviews/a2h-rdata-call-target-acceptance-review.md`, committed `59caab3`.
**Disposition: `ACCEPT-WITH-CORRECTIONS`. `BLOCKING: NONE`. `EVIDENCE OUTRUNNING CLAIMS: NONE.`**

**The row `O-OPEN` is correct, and the reviewer independently reproduced the load-bearing claims.**

---

## What the reviewer verified independently

| Claim | Reviewer's verification |
|---|---|
| **call site `0x00193EB5` + declared boundary** | **CONFIRMED three ways** — `recovered-functions.json` `{start 0x00193D90, end 0x00193EE2, D3D, test_unit 11c1}`, `recovered.c:347329` header, `recomp_dispatch.c:8006` |
| **uniqueness: 4 sites, one with `ecx−esp = 0x20`** | **CONFIRMED by its own byte sweep** for `8D 4C 24 N 51 FF Dx` over seven sections: `0x13E269`/`0x13E350`/`0x140A62`/`0x193EB0`. **And the `disp32` variant `8D 8C 24` returns ZERO, so the sweep is complete.** It also checked `ecx−esp` **against the recorded registers** and got `0x7BFF9C` exactly |
| **table structure + `edx=0x293`** | **CONFIRMED** — base `0x001D37C8`, sentinel at `0x001D4DA8`; **`(0x1D4C60−0x1D37C8)/8 = 0x293` EXACTLY** |
| **object identity `0x242C` alias** | **CONFIRMED, with a scoping caveat** — `0x0018CE3A` is the **only** `+0x242C` access in all executable sections; **but the poller passes `ecx = edi`, not a literal `lea ecx,[ebx+0x2268]`** |
| **installer corroboration** | **CONFIRMED** — `0x0015F9E0` has **exactly one caller** (`0x00123319`) |
| **the missing edge is REAL** | **YES** — *"real, not a stop-short… The gap is a property of the code."* **`sub_0018CE30` has no `call` anywhere, only two `jmp` sites with constant args, so the only static writer cannot produce `0x001D5078`.** |

**The reviewer's uniqueness reproduction is the most valuable part:** it **re-ran the sweep with a completed
pattern set** (`disp32` variant included) and **confirmed zero additional sites**, which is what makes the
call-site claim **provable rather than plausible**.

## Three corrections — all applied

**Two of them the Session had found independently, which is worth recording as convergence.**

**(a) `0x0018CB60` is `rep movsd`, NOT `rep stosd` — it COPIES, it does not zero.**
**The reviewer verified it, and the Session had found the same thing independently** (`0x2268+0x214 = 0x247C`,
and the `0x300`-byte copy spans `0x247C..0x277C`, **excluding** the callback field at `0x242C`).
**So the evidence's *"it zeroes the sub-object"* and its entire *"a zeroing constructor does not explain the
value"* argument were VOID.**

**The replacement reason is STRONGER:** the context is **populated from caller-supplied data**, i.e. **an
indirect write surface into the same object** — *"which is precisely why the chain cannot close statically."*

**(b) *"the constructor/populator … is absent from the translation"* is FALSE.** `sub_0018CB60` **IS present**
(`recomp_0004.c:36665`, declared `recovered.c:5336`, called `recovered.c:254405`). **What is genuinely absent is
a CALLER of `sub_0018CE30`.** **Stating the populator is absent OVER-CLAIMED the gap.**

**(c) *"Within D3D, `[reg+0x1C4]` has exactly ONE access"* omitted two `[esp+0x1C4]` STACK-LOCAL hits**
(`0x18D97D`, `0x18D98F`, possibly linear-disassembly artefacts). **The operative claim — no write to the OBJECT
field — still holds.**

**All three are applied in place, with the superseded text retained so the correction is legible.**

## One over-claim the reviewer flagged and the Session accepts

**The evidence called the object identity *"closed."*** **The reviewer's scoping caveat is correct:** *"the
immediate poller `0x00196C38` passes `mov ecx, edi`, NOT a literal `lea ecx,[ebx+0x2268]` — so the final hop
(that `esi` at `0x00193D90` IS `device+0x2268`) is inferred, not proved at that site."*

**So the alias arithmetic is right and the binding at this call site is INFERRED.** **The Session's own
verification record also called it *"CLOSED"*, and that word is now too strong** — **recorded here as a
correction to the Session's record as well as the Worker's.**

**Net effect on the row: none.** **The gap is real and independently reproduced; only the stated justification
was partly unsound.**

## The reviewer's own residual uncertainties — carried, not smoothed

1. **The final identity hop** — `edi → device` not traced.
2. **`ebx = 0` at `0x00123319`** — lines `3674-3742` not exhaustively read.
3. **`0x15F9D0` as "the refcount thunk"** — not disassembled by the reviewer. *(The Session did disassemble
   it: `inc dword ptr [0x265174]; ret`, which is a refcount increment.)*
4. **The negative claim about code-pointer loads** — the cited consumers were not all re-derived.
5. **The two `[esp+0x1C4]` hits** — possibly artefacts.

## Scope, source and completion — all clean

**Scope respected:** only `0x001D5078` analysed; **float-bit siblings contrastive with no family claim**; **the
retired NULL line not reopened and no DR record cited**; no `PIO_FREE`/`A4b2-*`/`0xFFFFB3` work; **no
instrumentation** (none was authorized).

**Source unchanged:** `git show --stat eca26ed` = **1 file, the evidence record only.** **No source, generated
file or config touched.**

**No synthetic completion.** **All eight guards pass.**
