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

---

# Corrected ruling (`SHAPE: REDIRECT`) — NO-WRITE **is** reachable; the Planner's falsification accepted

**The Advisor reconsidered on the `PREMISE_CHANGED` referral and ruled again. Recorded verbatim. The first
ruling above stands as history; this supersedes its instrument.**

```text
SHAPE: REDIRECT
REASON: Canonical-DR0 cannot support absence (29 linear aliases, 4 comparators — Planner's falsification verified in source); page-guard shares the defect and adds single-step races. Sound design exists: DR0-canonical complete record + 28-alias first-touch census, same run, fail-closed throughout.
POLICY_ISSUE: NONE
REVERSED_BY: DR infeasible (host restriction; thread churn defeating census; #DB conflicts unresolvable) — re-refer, page-guard NOT pre-authorized as fallback; cheaper complete instrument identified.
```

**Plain admission first: my instrument is wrong for absence claims as specified, and the Planner is right.** Canonical-DR0 misses alias writes — verified at `xbox_memory_layout.c:2251-2257` (`MapViewOfFileEx`, separate PTEs per mirror) and stated verbatim in the toolkit's own Halo comment (`:2362-2366`). The Session's relay error ("DR watches the physical page") compounded it, but is not its source: **neither my ruling nor the sketch considered mirrors at all.** The correction is recorded here. (Relay discipline going forward: quote load-bearing sentences verbatim in briefs — paraphrase drift on a load-bearing claim is what turned my true neighbor-point into a false physics claim. The Planner catching it pre-packet is the system working.)

**1. NO-WRITE is reachable — via DR-canonical + alias census, same run. The packet proceeds; `DONE:no` is wrong.** Soundness recipe, each leg required:

- **DR0** on canonical slot bytes (write-only, `LEN4`), armed **before install** so the install write is the positive control (install untrapped → infra-failure, do not interpret); all-threads-armed proof + new-thread handling + fail-closed tid census (any unarmed observed tid → `UNKNOWN`); DR6 bit-protocol + synthetic fixtures + live order verification (carried from prior ruling); no-debugger run condition.
- **Census:** the 28 alias 4KB pages guarded RO + write-filter (AC97 pattern), recording **first touch per page** (VA/RIP/value/tick). First-touch is complete *despite* single-step windows, because a page's first touch necessarily precedes its first window — windows hide only subsequent touches, which the census doesn't need. Fixed 28 records + per-page uncapped counters (§6.1.6).
- **Completeness closure:** alias universe = {canonical} ∪ {mapped mirrors}, with mapped-set recorded at arm time; unmapped mirrors cannot sink writes silently (any use AV-crashes loudly — observable, not silent); tiled aperture excluded by separate storage **plus** a contiguity-pool-vs-slot-page non-overlap assert (not by citation alone).
- **Claim rule:** zero alias touches + DR-canonical silent + all controls green ⇒ NO-WRITE sound for that run. ANY alias touch ⇒ coverage-provenance `UNKNOWN` with touched aliases named (a touch detects even what a window hides — first-touch can't miss).

**2. Sub-candidates:** all-29-DR impossible — agreed. **Rotated-4 FORBIDDEN for absence** (time-division is sampling; §6.1.6 bars it from carrying a row — permit rotation only for existence-hunting, explicitly non-covering, which this packet doesn't need). Page-guard-with-explicit-race: NO — an explicit-but-unquantified race plus forbidden exclusion is not a design. Unmapped-mirrors equivalence run: legitimate idea, **deferred as recorded fallback lead** for a dirty-census followup only — not this packet.

**3. `xbox_ProtectMirrorsForDebug`: NO as a function, YES as a technique, implemented FRESH.** Verified (`:2372-2385`): whole 64MB views + count-only log + **no handler installed** — calling it is a tripwire that crashes on first mirror write, not a trap. **Forbid whole-view protection in this packet.** Implement selective 28-page protection + per-fault records fresh; never call the whole-view function.

**4. Not `DONE:no`.** No fundamental limit was found — only an incomplete instrument, now corrected. Proceed per above. (If DR proves infeasible → re-refer; *that* is the only path to `DONE:no`.)

**Notes, binding where stated:** alias VAs derived LIVE from `g_mirror_views` + slot offset with formula cross-check — the brief's hand range (`0x04000000…0x1C000000`) is arithmetically inconsistent under either memory size and must not propagate (eighth instance of the hand-arithmetic pattern; live-derivation is the fix). The archived esp range stays compatibility-only, never coverage. Row bars from prior ruling carry (RIP-range classifier + fixtures; explicit restore predicate; read-path-audit needs the full three legs; infra failure → coverage-provenance; first-by-ticks precedence). Two-run OFF/ON shape unchanged. P4 bridge obligation stands.

**The durable lesson** (candidate for `AGENTS.md` if it recurs — fourth instance after spelling, width, offset): *completeness claims enumerate by bytes affected — all spellings, widths, offsets, linear aliases — never by one form.*

**BASIS:** observed — mirror `MapViewOfFileEx` + tiled exclusion (`:2220-2299`); Halo comment verbatim (`:2358-2366`); `ProtectMirrorsForDebug` body (`:2372-2385`); AC97 no-exclusion handler (prior turn); per-thread counter + no-reset + TLS defines (prior turn); empty DR-conflict search. Inferred — first-touch completeness despite windows; unmapped-loud soundness; rotation/sample bars. Uncertain — thread churn extent, guest thread-birth path (both gated to feasibility checks, not assumed).

**RECORD IN:** Session records verbatim in the successor's planning-rulings/review record; no second preflight if applied verbatim — re-refer ONLY on infeasibility.

## Session verification of the corrected ruling's new factual claims

| Claim | Verification |
|---|---|
| Mirrors use `MapViewOfFileEx`, giving **separate PTEs** per mirror | **confirmed** (`xbox_memory_layout.c:2251-2257`) |
| `xbox_ProtectMirrorsForDebug` protects **whole 64 MB views**, logs **only a count**, and installs **no handler** | **confirmed** (`:2372-2385`) — so it is a tripwire, not a trap |
| The brief's hand-derived alias range `0x04000000…0x1C000000` is **arithmetically inconsistent** | **confirmed, and the Session accepts the correction.** The Session derived it from a 64 MB assumption while the toolkit's own mirror loop uses `(m+1) * g_memory_size` with `g_memory_size` set at runtime — so a hand range is unsound. **Alias VAs must be derived live from `g_mirror_views`.** |

**Session error #2 in this exchange, recorded — and the Advisor's arithmetic correction is exact.** The
Session hand-derived the alias range as `0x04000000 … 0x1C000000`. **Verified against the run's own log and
the toolkit's loop:**

| Quantity | Value |
|---|---|
| `g_memory_size` (run log) | *"mapped 65536 KB"* = **64 MB** = `0x04000000` |
| Mirror count (run log) | *"RAM mirror: 28/28 views mapped (covers 1856 MB)"* — and `29 × 64 MB = 1856 MB` ✓ |
| Mirror `m` (0-based) base | `(m+1) × g_memory_size` (`:2234-2237`) |
| **First alias base** | `0x04000000` |
| **Last alias base (m=27)** | **`0x70000000`** |
| The Session's stated last base | `0x1C000000` — **only 7 mirror steps, not 28** |

**So the Session under-counted the alias space by a factor of 4**, and the 28 alias bases run
`0x04000000, 0x08000000, … 0x70000000` — i.e. **`0x041C4064 … 0x701C4064`** for this slot. **This is the
eighth instance of the hand-arithmetic pattern in this line**, and the fix is the same one the project keeps
re-learning: **derive from the source of truth (`g_mirror_views`), do not compute by hand.**

**It also makes the Session's `esp`-range observation even weaker than it appeared:** the observed `esp`
range is `0x007BFF94..0x012ECF90`, and the aliases extend to `0x70000000` — far above it. **The Advisor's
instruction that the `esp` range is compatibility-only and never coverage is therefore doubly right**, and
the Session withdraws any suggestion that it bounds alias reachability.

## Session correction #3 — the "no-debugger-attached" condition is unsatisfiable as literally written

**The Planner caught this while expanding the packet, and it is a real contradiction in the ruling's
correction 3(v).** The ruling requires a *"no-debugger-attached run condition (`IsDebuggerPresent` assert
start/end)"*.

**Verified: every archived guest run satisfies `IsDebuggerPresent() == TRUE` by construction.** The
collector launches the target as its own debuggee:

```c
tools/harness/collect.c:294-295
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE,
            DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED, NULL, NULL, &startup, &child)) {
```

`DEBUG_ONLY_THIS_PROCESS` makes the calling process the debugger of the target, so **`IsDebuggerPresent()`
inside the guest is necessarily true**. A literal assertion would **fail every prescribed OFF/ON run** — and
the runner always goes through the collector (`scripts/run-jsrf.py:4,45`).

**Corrected requirement, which the Planner proposed and the Session adopts:**

> **No ADDITIONAL or EXTERNAL debugger may be attached; the collector's own expected attachment must be
> verified rather than asserted absent.** Concretely: assert the debugger is **exactly** the expected
> collector (by process identity), and assert **DR ownership** — that the four debug registers are owned by
> the instrument and not by a third-party debugger — at start and at teardown.

**This preserves the ruling's actual intent** (no *foreign* agent perturbing the run or stealing the DR
registers) **while being satisfiable.** Recorded because a literal reading would have made the packet
unrunnable, and because the Planner caught it before it reached a frozen contract.

**Also noted from the Planner, and correct:** the alias guard's first-fault record is **coverage-only** —
**any alias write hit ⇒ `UNKNOWN`** — so a racing *subsequent* write cannot false-pass the row. That is the
first-touch insight applied correctly, and it is why the single-step race that disqualified the page-guard as
a *write history* does not disqualify it as a *touch census*.

---

## Session execution of the Advisor's **up-front DR feasibility gate** — answered offline

The ruling required: *"Up-front feasibility gate (offline, before building): guest thread-birth path +
archived-run tid census (ICALL lines carry tid). If churn defeats arming → re-refer THEN."* **Executed
offline from both archived runs and the toolkit source. The gate PASSES with one binding design consequence.**

### The arming universe is SIX threads, not five — and the guest registry is an incomplete census

| Run | Guest registry records | Dispatching tids (log) | **Dispatched but NOT registered** |
|---|---|---|---|
| Run 1 (OFF) | 5 | 6 | **tid 43308** (180 dispatches) |
| Run 2 (ON) | 5 | 6 | **tid 65292** (328 dispatches) |

**Both runs show exactly one tid that made guest kernel dispatches yet never appears in the diagnostics
thread registry the collector archives.** The registry is populated from the `PsCreateSystemThreadEx` path
(`kernel_bridge.c:440`), so **a DR arming strategy keyed on the guest registry would miss this thread
entirely** — precisely the *"unarmed observed tid"* case the ruling says must fail closed to `UNKNOWN`.

### What that thread is — identified, and it is a HOST thread

| Evidence | Value |
|---|---|
| Its dispatch return PCs | `0x00193CFD` and `0x00193E62` |
| **Section containing them** | **`D3D`** (`0x0018CB40..0x0019E338`) |
| Its ordinals | **119 and 145**, in **identical counts** across both runs (90/90 and 164/164) |
| Its `esp` | `0x007BFFCC` / `0x007BFF94` — **the same value in both runs** |
| Its dispatch pattern | a strict 119↔145 alternation, 180 and 328 times |

**A repeating two-ordinal alternation at a fixed `esp`, from the D3D/DirectSound section, identical across two
independent runs, is a toolkit-spawned host worker** (the toolkit creates host threads at
`kernel_bridge.c:464`, `:2455`, and `apu_shim.h:148`), **not a guest thread**. It legitimately never appears
in the *guest* thread registry.

### The binding consequence for the packet

**The arming universe must be enumerated from SOURCE — every thread-creating path in the toolkit — not from
the guest registry.** Concretely, the packet must:

1. enumerate **all** toolkit thread-creation sites (`kernel_bridge.c:464`, `:2455`, `apu_shim.h:148`, and any
   others) plus every guest `PsCreateSystemThreadEx` thread, and state the finite total;
2. arm each, and **fail closed to `UNKNOWN` if any observed tid is unarmed** — which is exactly the ruling's
   own rule, now shown to be **load-bearing rather than theoretical**, because a real dispatching thread is
   absent from the registry today;
3. record the **tid census at arm and at terminal**, so an unarmed tid is detected rather than assumed away.

**This does NOT defeat DR feasibility** — the toolkit knows its own host threads and can arm them
explicitly — **but it does mean the "all-threads-armed" leg cannot be satisfied by trusting the guest
registry**, and a packet that assumed it could would have produced a silent coverage hole. **The gate is
therefore recorded as PASSED WITH A REQUIRED DESIGN CHANGE, not as a re-referral trigger.**
