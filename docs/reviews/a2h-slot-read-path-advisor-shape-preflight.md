# Advisor shape preflight — `A2h-slot-read-path-displacement-r1` (**`SHAPE: REDIRECT`**)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, `reasoningEffort: max`.
**Raised because:** the successor packet needs a bounded **toolkit** change, which the owner's toolkit
constraint makes preflight-mandatory. The Planner cannot reach the Muse handle, so the Session relayed the
sketch, the Planner's alias/concurrency findings, and returned the ruling.
**Authority:** `docs/agent-workflow.md` §2.3 (Advisor), §4 (escalation).

**Ruling recorded verbatim below.** The Session did not redesign the instrument unilaterally; it relayed the
Planner's source-verified findings and the Advisor redirected the method.

---

## RULING

```text
SHAPE: REDIRECT
REASON: Triage correctly isolated a bridgeless gap, but the proposed page-guard cannot soundly prove absence on six threads (single-step RW window + no exclusion = missable writes, and exclusion would perturb timing); DR0 write-breakpoint gives byte-exact always-armed observation with none of these hazards.
POLICY_ISSUE: NONE
REVERSED_BY: DR infeasible (thread churn defeats arming with no fail-closed census; #DB conflicts unresolvable; host restriction) — re-refer, page-guard fallback NOT pre-authorized; gap shown to contain a bridge call after all (reopens triage, not this shape); cheaper decisive instrument identified.
```

**1. Instrument: DR0 write-breakpoint PRIMARY (`LEN4` + write-only on the slot's host VA). Page-guard REDIRECTED AWAY as primary** (record as analyzed alternative, not authorized fallback).

- Page-guard soundness is *conditional* on a neighbor-write census it cannot soundly produce under concurrency: every single-step RW window hides concurrent writes — including census writes themselves. Verified the mechanism gap in source: AC97's handler has TLS step *attribution* (`xbox_memory_layout.c:348,383-389`) and **zero exclusion logic** — fine for functional RR-masking on audio traffic, fatal for a forensic absence claim where one missed write flips the row (`:391-405` read in full).
- Thread exclusion to close it changes guest timing on a genuinely six-threaded run — a validity threat on a failure surfacing in a bridgeless micro-window. DR has neither problem: always armed (no windows ever), byte-exact (overlap semantics catch wide/unaligned/REP writes covering the 4 bytes), writer RIP free, writes proceed unperturbed. No in-tree DR conflicts (empty search over toolkit `src`).
- Cheaper alternatives rejected: KWATCH-only is blind in the gap *by construction* (triage proved no boundary inside it); static-only is unbounded (log-line count ≠ instruction count in the gap — L1 scar tissue applies); wider LEN unnecessary.
- AC97 precedent distinguished honestly: VEH-first + `CONTINUE_SEARCH` + gated + closure-removal mechanics transfer; its completeness tolerance does not (emulation fixup ≠ forensic absence).
- Up-front feasibility gate (offline, before building): guest thread-birth path + archived-run tid census (ICALL lines carry tid). If churn defeats arming → re-refer THEN.

**2. Page-sharing: MOOT under DR — with one record correction required.** DR touches no protection, so neighbors are irrelevant and no census is needed. But the brief invites a mis-citation: the documented `.rdata`/`.data` sharing is page `0x003B2000` (`:1755-1763`, verified), while the slot sits on **`0x001C4000` — a different page**. Require one static sentence naming the slot's page/contents and stating the 0x003B2000 doc is not it, so a future reader doesn't inherit the confusion. Had page-guard been chosen, a page-write census would have been prerequisite; DR needs none of it.

**3. Concurrency: the TLS-stepping question dissolves — no single-stepping exists under DR.** Require instead: (i) all-threads-armed proof (arming mechanism + tid census at arm AND terminal; any unarmed observed tid → `UNKNOWN`, fail closed); (ii) exact DR6 bit-protocol (service B0, clear owned bits only, preserve BS/TF for AC97 overlap, `CONTINUE_SEARCH` on shared #DB, `CONTINUE_EXECUTION` only on pure-B0) + synthetic-`EXCEPTION_RECORD` fixtures for every DR6 combo incl. `B0|BS` + live order verification (ours runs first by latest `first=1` registration — verified against `main.c:212` + AC97 `:420`); (iii) thread-safe publication (CAS-taken slots + torn-publication check, as sketched) + QPC ticks for cross-thread order; (iv) **forbid** thread exclusion anywhere; (v) no-debugger-attached run condition (`IsDebuggerPresent` assert start/end), recorded. Class-keyed `{guest,bridge/host,unknown}×{first zero,first restore}` latches confirmed (7 records + terminal; tid+ticks fields in each) — do NOT replicate the 128-slot design where 7 suffice.

**4. Rows: structure confirmed, bars tightened.** Guest/host-zero keep, with REQUIRED RIP-range classifier definitions + positive-control fixtures (injected known-guest-RIP + known-host-RIP writes) + unknown-class fail-closed (RIP alone doesn't classify). Zero-then-restore keeps, with REQUIRED explicit restore predicate (value + order). Terminal-read-path-audit IS reachable but ONLY with three legs: proven-complete write coverage + mapping-stability **re-proof** (re-read `g_xbox_mem_offset` at terminal — the triage's grep-based "written only at init" is insufficient for a positive read-path conclusion) + double-read agreement; absent any leg → coverage-provenance. Note the evidence direction: two agreeing reads make read-path *less* likely a priori — the row is correctly ordered last. ADD explicit mappings: infra failure (arming gaps, #DB storm, torn publication) → coverage-provenance, never absence; multiple findings → first-by-ticks wins, others recorded (precedence rule required).

**5. Runs: two pre-specified same-build OFF/ON CONFIRMED.** Direct answer: OFF-run inertness IS meaningful (compiled-in-but-off inertness + identical guest prefix — what fixtures cannot establish) but INSUFFICIENT alone; it never proves ON-run non-perturbation. REQUIRE explicit ON-vs-OFF behavioral comparison (OOM size/site, failure site, pre-failure bridge counts within stated tolerance) with fail-closed on divergence — if ON diverges, the instrument perturbs: `O-OPEN`/coverage-provenance, do not interpret. DR-off inertness is structural (DR7 zero; assert by readback in teardown, cheap). No third run without re-referral, unchanged.

**BASIS:** observed — AC97 VEH + registration + TLS decl (`xbox_memory_layout.c:325-348,370-434`); `.rdata` doc (`:1755-1763`, different page than slot); `main.c:212`; `JsrfSlotLatch` design (`diagnostics.h:1-80`); triage record (O-NO-BOUNDARY-TRANSITION, install control, 15498/0, gap, refutations); empty DR-conflict search; per-thread counter + TLS defines + no-reset (prior turn). Inferred — window-race consequence for absence claims; DR superiority on correctness (not cost). Uncertain — thread churn extent, guest thread-birth path (both gated to the feasibility check, not assumed).

**RECORD IN:** Session records verbatim in the successor's planning-rulings/review record; no second preflight if corrections applied verbatim — re-refer ONLY on infeasibility. P4 discovery-transfer re-establishment before inheritance: confirmed standing obligation.

---

## Session verifications of the ruling's factual claims

**The Advisor's page correction is CORRECT, and the Session's earlier citation was wrong.**

| Claim | Verification |
|---|---|
| The slot `0x001C4064` is on page **`0x001C4000`** | confirmed (`0x001C4064 & ~0xFFF`) |
| The documented `.rdata`/`.data` sharing is page **`0x003B2000`** | confirmed from `xbox_memory_layout.c:1755-1763` |
| **They are different pages** | **confirmed — the Session had cited the `.rdata`-writability note as if it applied to the slot's page. It does not.** |
| `XBOX_NUM_MIRRORS = 28` | confirmed (`xbox_memory_layout.h:438`) |
| The mirrors map the whole of `g_memory_size` | confirmed (`xbox_memory_layout.c:2234-2257`) |
| A canonical watchpoint **already missed a real alias write** | confirmed — the toolkit's own file records it for Halo's `fs:[4]` (`:2362-2366`) |
| AC97's handler has TLS attribution and **no exclusion logic** | confirmed (`:383-389` uses `s_ac97_stepping`; no exclusion anywhere in `:370-406`) |

**The Planner's alias finding is what forced the redirect, and it is vindicated:** the slot's 28 aliases
exist at guest `0x04000000 + 0x001C4064` … `0x1C000000 + 0x001C4064`. **Under DR0 on the canonical host VA
that question is moot**, because DR watches the physical page — which is precisely why the Advisor redirected
rather than requiring 29 arming sites.

**One data point the Session measured, recorded as context only:** the archived run's `esp` range is
`0x007BFF94..0x012ECF90`, so **no observed stack address reaches `0x04000000`**. Per the ruling this is moot
for instrument design and **must not** be used to claim alias coverage.

---

# ⚠ `PREMISE_CHANGED` — the Session introduced a factual error in relaying this ruling

**Appended, dated, with the ruling above preserved verbatim and unedited.** Per `docs/agent-workflow.md`
§4.3, a correction that changes a load-bearing premise must be marked `PREMISE_CHANGED` and **every ruling
that depended on it must be re-referred** — never silently repaired, and never with an instruction not to
revisit. **The Planner caught this, and it is the Session's error, not the Advisor's.**

## What the Session wrote, and what the Advisor actually wrote

The Advisor's point 2 says: *"**Page-sharing: MOOT under DR** — DR touches no protection, so neighbors are
irrelevant and no census is needed."* **That is correct, and it is about NEIGHBOURS ON THE SAME PAGE.**

**The Session then amplified it into a claim the Advisor never made**, telling the Planner:

> *"Under DR0 on the canonical host VA that question is moot, because **DR watches the physical page**, so the
> alias question does not arise."*

**That sentence is the Session's, it is FALSE, and it contradicted this very record.** The table above already
records, correctly, that *"a canonical watchpoint **already missed a real alias write**"* — so this document
asserted both that DR0 misses aliases **and** that DR0 makes aliases moot. **An internal factual contradiction
in a durable record**, which the Planner identified by reading it.

## The falsifying source, quoted verbatim from the toolkit

```
xbox_memory_layout.c:2362-2366
 * Xbox RAM is visible at 28 virtual addresses that alias the same pages, so a
 * store to 0x04000004 changes Xbox VA 4 without ever touching VA 4. Both a
 * page-protection watchpoint and a DR0 hardware watchpoint on VA 4 therefore
 * report nothing while the memory demonstrably changes -- which is exactly
 * what happened chasing Halo's fs:[4] corruption.
```

**x86 debug address registers compare LINEAR addresses, not physical pages.** So:

1. the CPU compares a data breakpoint against the **linear address the instruction produces**;
2. `MapViewOfFileEx` maps the same backing RAM at **29 distinct linear ranges** (canonical + 28 mirrors);
3. **a canonical DR0 on `0x001C4064` therefore misses writes through its 28 aliases** — exactly as the
   toolkit's own comment documents for Halo's `fs:[4]`.

**And four DR comparators cannot cover 29 linear addresses.** So **DR-only cannot support a NO-WRITE or
read-path row**, and the Planner was right to stop rather than produce a packet whose absence row could
false-PASS on an aliased write.

## What this invalidates, and what it does not

**Invalidated:** the *completeness* half of the redirect. A canonical-DR-only instrument **cannot** ground
`A2h-terminal-read-path-audit`, nor any "no write occurred" claim.

**NOT invalidated:** DR0 as an instrument for **positive** observation. A captured write with a writer RIP is
a genuine witness, and the Advisor's DR6 bit-protocol, all-threads-armed proof, CAS publication and
no-thread-exclusion corrections remain sound and applicable.

**Also still sound from the ruling:** the page-guard's *own* disqualification stands independently (the
single-step read-write window with no exclusion logic is a real gap in `ac97_write_veh`), and the
page-sharing correction to page `0x001C4000` stands (the Session verified it).

## Re-referral

**Referred back to the Advisor** with the source counterexample, asking: given 29 linear aliases and four DR
comparators, (a) is a NO-WRITE row reachable at all, or is the honest ceiling a coverage-provenance row that
reports positive observations and declares absence `UNKNOWN`; (b) if reachable, what instrument makes it
sound — 29 arming sites, rotated DRs, a gated diagnostic use of `xbox_ProtectMirrorsForDebug` on the specific
4 KB alias pages, or something else; (c) if nothing is sound, whether the correct outcome is a recorded
instrument limit rather than a packet.

**The Planner's packet remains a 16-line NON-EXECUTABLE sketch and is NOT promoted.** Commit `d4dad33`
records the superseded DR0 framing together with the Planner's safety warning; it is **historical**, not
authorization.
