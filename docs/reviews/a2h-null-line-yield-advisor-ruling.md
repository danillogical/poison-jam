# Advisor ruling — `A2h` NULL line: K = 1 yield, the successor, and the `0x001D5078` pivot

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e793-2f72-7000-a186-80903e8fa109`.
**Raised because:** the accepted `A2h-arming-coverage-attribution-r2` returned **K = 1**, and its own K=1 rule
is *"preserve the run-local row, `UNKNOWN` generality, re-refer"* with **no `N` extension without referral.**
**Authority:** `docs/agent-workflow.md` §2.3, §4.3.

**Route note:** two attempts at this referral failed with model timeouts (`MODEL_UNAVAILABLE`, then
`TURN_FAILED` after 3 attempts). **Served on the third at `low` effort.** Recorded so a future reader does not
misdiagnose a route outage.

---

## RULING

**(a) once, bounded — then the decision tree forks. (c) queued behind it. (b) rejected. (d) not now.**

- **One more N=5 packet, r2 design repeated verbatim** (same instrument, same rows, same early-stop-at-K≥2-agreeing, hard cap, no further extension without referral). Rationale: the design didn't fail — the yield did (66% hit chance at p≈0.4); one bounded repeat is the cheapest path to a decision. Anti-fishing structure is already in the design (pre-specified N, early-stop only on agreeing K=2, agreement rule); keep all of it.
- **Efficiency notes:** no code changes ⇒ same build (`a7e3246` hash-verified) ⇒ existing OFF inertness evidence carries, no fresh OFF needed. The 40% figure is a *planning input only* (pooled same-build draws); K counts *only* r2-qualifying realizations — set-A targets stay leads, never K. Do not let "4/10" migrate into any K claim.
- **(b) rejected:** nothing predicts the terminal (anchor 5555 in every run of both classes — measured, not assumed), so yield cannot be raised honestly; pinning the failure would change the system under test. There is no better-yielding design — only the same design run again.
- **(c) queued, with triggers:** MISS (K<2 after this packet) → pivot to a `0x001D5078` packet; HIT → fix packet → reassess siblings after. The sibling line does not advance concurrently — one critical path, ordered.

## Q2 — Pivot condition as written does NOT fire; pivot is queued, not taken

"Target rare + siblings dominate" requires dominance; 6 non-targets across 5 distinct terminals is fragmentation, not dominance. But `0x001D5078` (3/10, coherent class: valid-`.rdata`-address-as-target, deterministic registers) is queued as above. It is not yet *more productive* — only *potentially* so — and displacing a one-packet-from-decision line for a potentiality would be the wrong trade.

## Q3 — Not now; queued second. Not a distraction — same critical path, ordered

The NULL line holds a proven instrument + a 66% near-term decision; context-switching now wastes that position. If authorized later, scope is tight: which object holds the filename pointer, who wrote it there, by what call path — static-first (address concrete, registers deterministic), instrument only on gaps. Note as lead (not finding): the float-bit terminals (`0x3E800000`, `0x41200000`) suggest a garbage-pointer *family* — a future sibling packet checks for shared mechanism, but classifies per terminal first, same discipline as here.

## Q4 — YES, name run 2 — as a run-local record with exact boundaries

Record: coverage-complete TARGET, zero DR hits, zero alias touches, terminal zero, mapping re-proved — **consistent with read-path, writer UNKNOWN, generality UNKNOWN**. Name it because the packet's own K=1 language ("preserves only its run-local row") demands preservation, and because the next packet needs it as the agreement comparator and template. It claims no writer and no read-path — those need K≥2 by the same rule.

## Stopping point, stated in advance

If the next packet also misses K≥2: re-refer for pivot-or-defer with 10+ qualifying runs on record — *that* is the natural stopping point for the NULL line as framed, not this turn.

**BASIS:** observed — r2 row/early-stop/no-extend text, `ACCEPT` with K=1 + arming confirmed three ways, run-2 facts, 4/10 terminal table, anchor-5555 non-predictive. Inferred — 66% hit chance at p≈0.4 (planning arithmetic, not evidence). Uncertain — true rate (N≈10, too few; hence no rate claims, only the bounded plan).

**REVERSED BY:** evidence the sibling terminals share one mechanism with the NULL slot (collapses the ordering — one packet instead of two); a sound yield-raising design (none seen).

**RECORD IN:** verbatim in the successor's review record; Session updates CURRENT PACKET (successor name, N=5 cap, K≥2/agreement rule carried, `0x001D5078` queued with triggers).

---

## Session note on the sibling characterisation the referral carried

**The Session characterised `0x001D5078` offline before referring, and the finding is what made question 3
answerable rather than speculative:**

```
0x001D5078: "djv000_0.adx"        (neighbours: "effect_006.adx", "effect_005.adx")
```

**It lies in `.rdata` and the bytes there are a FILENAME STRING**, so **the guest performs an indirect call
whose target is a DATA address holding an ADX audio filename.** The register set
`eax=0x001D5078 ecx=0x007BFFBC edx=0x00000293 esp=0x007BFF9C` is **byte-identical across two independent
realizations**, so the pointer source is deterministic.

**Recorded as a characterisation, not a cause** — it establishes **what the address is**, not why the pointer
holds it. **The Advisor accepted it as a coherent class and queued it**, which is the correct disposition:
**coherent enough to be worth a packet, not yet demonstrated to be more productive than the line it would
displace.**
