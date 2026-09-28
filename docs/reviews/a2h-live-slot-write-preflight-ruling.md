# Advisor PREFLIGHT ruling — the live slot-write packet: page-protection primary, DR excluded entirely

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e8d7-2bad-7000-9850-c70ff9474acc`.
**Why:** the Advisor made a **preflight mandatory** for the new toolkit scope (turn `01a0e8d1`).
**Authority:** §2.3, §4.3.

---

## RULING

```text
SHAPE: PROCEED
REASON: Page-protection primary is sound (proven pattern, no delivery doubt); DR excluded entirely, not demoted; scope is one bounded trial with installer-gated rows.
POLICY_ISSUE: NONE
REVERSED_BY: page-guard delivery failure (faults not reliably delivered); thread churn defeating census; cheaper complete instrument identified.
```

**1. Mechanism: page-protection primary — your reading confirmed, one step stronger.** DR is not demoted to corroboration; it is **excluded**: a channel that failed its positive control contributes *nothing*, not even "support." **Corroboration from a broken oracle is how false confidence builds.** (The census machinery's write-filtering stands as measured — **RO pages never fault reads by construction**; `ExceptionInformation[0]==1` is belt-and-suspenders the packet's fixtures must **prove, not assume**.)

**2. Discrimination: RO protection IS the discriminator** (reads proceed silently; only writes fault — the poll loop's read volume is therefore free). **Single-step re-arm keeps the AC97 interlock obligation:** specify the **dual-mid-step `#DB` ownership** exactly (own-TF vs AC97-TF, bit-exact, fixture-tested with synthetic overlap) — same requirement as the DR6 protocol, adapted. **Windows get logged; rows conditional on no-window-overlap for absence; positive hits stand on their own records. No thread exclusion, ever.**

**3. Scope: fresh OFF + up-to-N ON (N≤5, pre-specified), stop at first qualifying run.** **Qualifying = writer-observed (RIP/value/order at the slot) plus terminal-match (`0x001D5078` failure) plus controls green (installer trap REQUIRED — else infra failure, fail closed).** **New code ⇒ fresh OFF (carry rule).** Non-matching runs are contrastive data, not waste. **Zero qualifying in N ⇒ report + re-refer, never extend.** **Carry forward, re-verified for the new page (never inherited):** tiled/contiguous non-overlap, `g_xbox_mem_offset` re-read at terminal, `MEM32(0x19DCE0)` base-stability at arm + terminal (**device move ⇒ re-scope, not silent**), no-debugger condition, VEH order.

**BASIS:** observed — census write-filter as measured; AC97 no-exclusion handler; empty DR-conflict search; `%04X` print formats. Inferred — DR exclusion (uncertified channels contribute nothing). Uncertain — slot-page traffic volume (packet sizes latches + overflow-fail-closed; volume does not change soundness).

**RECORD IN:** verbatim in the packet's planning record; **no second preflight if applied verbatim** — re-refer ONLY on infeasibility.

---

## The one step stronger, and why it matters

**The Session had proposed DR as *"inherits a non-firing history"* and page-protection as *"the
uncertified-but-unretired one."*** **The Advisor's correction is sharper:**

> **DR is EXCLUDED, not demoted.** *"A channel that failed its positive control contributes **nothing**, not
> even 'support.' **Corroboration from a broken oracle is how false confidence builds.**"*

**The Session's framing would have permitted DR as a *corroborating* channel** — **and the Advisor refuses
it.** **That is the right call:** **a channel that never fired live cannot corroborate a firing, because its
silence is uninformative and its agreement would be coincidence.**

## The discrimination answer, which the Session had over-thought

**The Session proposed three options for distinguishing a write from a read, including a
single-step-after-fault to decode the faulting instruction.**

**The Advisor's answer is simpler and correct:** ***"RO protection IS the discriminator (reads proceed
silently; only writes fault — the poll loop's read volume is therefore free)."***

> **A read-only page faults on WRITE and not on READ. That IS the discrimination.** **The Session's option
> (ii) was unnecessary machinery.**

**And `ExceptionInformation[0]==1` is *"belt-and-suspenders the packet's fixtures must prove, not assume."***
**So the fixtures must demonstrate that the filter is correct rather than trusting it.**

## The obligation the Session must not lose

> **"Single-step re-arm keeps the AC97 interlock obligation: specify the dual-mid-step `#DB` ownership
> exactly (own-TF vs AC97-TF, bit-exact, fixture-tested with synthetic overlap)."**

**This is a REAL constraint from the NULL line's history:** **the collector already has an AC97 interlock, and
a single-step re-arm interacts with it.** **The packet must specify the `#DB` ownership bit-exactly and
fixture-test a synthetic overlap.**

**And: *"No thread exclusion, ever."*** **The Session must not narrow the observation to one thread.**

## The scope bound

| Element | Value |
|---|---|
| **Runs** | **fresh OFF + up to N ON, N ≤ 5 pre-specified, STOP at first qualifying** |
| **Qualifying** | **writer observed (RIP/value/order) + terminal match (`0x001D5078`) + controls green** |
| **Positive control** | **the installer trap is REQUIRED — else INFRA FAILURE, fail closed** |
| **Carry rule** | **new code ⇒ fresh OFF** |
| **Zero qualifying in N** | **report + RE-REFER, NEVER extend** |
| **Non-matching runs** | **contrastive data, not waste** |

**And the carry-forward list must be RE-VERIFIED for the new page, never inherited:** **tiled/contiguous
non-overlap, `g_xbox_mem_offset` re-read at terminal, `MEM32(0x19DCE0)` base-stability at arm AND terminal
(device move ⇒ re-scope, not silent), the no-debugger condition, and the VEH order.**

## The Session's preflight request is now satisfied

**No second preflight is required if the packet applies this verbatim.** **Re-refer ONLY on infeasibility.**
