# Route status — Persistent Advisor (Muse) unavailable for the terminal `L1` referral

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Advisor:** `muse_FkNhGaXtV9P5` (`muse-spark-1.3-contributor`), per `docs/agent-workflow.md` §2.3 and
`.muse-workers.md`.

---

## What happened

The `A4b2-NR-epoch-slice-followup-r1` packet executed and reached its **terminal referral** — the Advisor's
own ruling reserved the final `L1` scope/defer/retire decision to itself. The referral was attempted
**four** times:

| Attempt | Result |
|---|---|
| 1 (long prompt, pre-measurement) | `MODEL_UNAVAILABLE` — *"model stream idle timeout after 180000ms"*, `retryable: false` |
| 2 (same prompt, collected via `muse_session_read`) | `TURN_FAILED` — same cause, `retryable: false` |
| 3 (short prompt, post-measurement) | `MODEL_UNAVAILABLE` — same cause, `retryable: false` |
| 4 (shortest prompt, after `muse_session_open` recovered the handle) | **send timed out at 600 s; `muse_session_read` reports `in_flight`** |

Attempt 4 is **still running server-side**. Per `docs/agent-workflow.md` §4.4 the turn is collected with
`muse_session_read` and **no concurrent send is made on the handle**.

## Why no substitution was made

The owner's staffing authority is explicit: *"Do not substitute another model if any required route is
unavailable."* The Advisor's route is `muse-spark-1.3-contributor` via the persistent Muse handle, and no
fallback was substituted. The Planner route (`codex`/`gpt-6-sol`) was **not** used as a stand-in for an
Advisor ruling, and no acceptance route was repurposed.

## What was done instead, and why it is not a substitute

Rather than idle, the Session did work that **reduces what the ruling has to decide**, and recorded it as
Session evidence rather than as a ruling:

1. **Measured the consumer** instead of arguing it — the instrument the packet authorizes
   (`RECOMP_APU_DMA_DESC_TRACE`). Result: the exchange is produced by **block 24**, the `P 000E` call's
   descriptor, **all fields immediate**, `scratch_offset = 0x800` reproducing the observed `dsp_addr`.
2. **Independently re-derived** that claim from the raw artifacts with **fresh code reusing none of the
   earlier tooling** — six checks, all confirming, plus the sharp detail that block 24 is read **once, at
   ordinal 765**, the last read before the exchange fires at 766.
3. **Wrote the erratum** for the wrong descriptor identifier
   (`docs/reviews/a4b2-descriptor-identity-erratum.md`), so `A4b2-r8` cannot inherit it.
4. **Ran the final closure control** — zero diagnostic tags, no artifacts, tuple unchanged, ctest 18/18.

**None of this is an Advisor ruling**, and the row remains **referred**, not selected.

## Standing recommendation, recorded but NOT acted on

**`O-TWO-LEG`**, on these grounds: the exchange-producing descriptor's five fields are all immediates; the
leaf classification in the frozen packet closes immediate leaves; `L2 = INVARIANT` is carried; image-`I`
stability is proven by watch; the entry set, interrupt exclusion, alias-disjointness, and reader/writer
closures all hold; and **`O-REFUTED` is unavailable** because it requires a concrete feasible stub-derived
chain to a field or guard of the exchange-producing descriptor, and none exists.

**The Session did NOT open `A4b2-r8` on this recommendation.** The packet's terminal row for
`O-INCONCLUSIVE` — and the Advisor's own terminality ruling — route the decision to the Advisor, and the
Session will not convert an unavailable route into a self-granted one.

## What unblocks this

Either the Muse route recovering (the in-flight turn may yet complete, and `muse_session_read` will collect
it), or an owner decision on how to proceed while the Advisor is unreachable. **This is a genuine
route-availability boundary of the kind the owner's stop conditions name**, recorded rather than worked
around.
