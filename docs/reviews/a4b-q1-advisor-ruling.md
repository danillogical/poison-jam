# A4b Q1 — Advisor ruling: `PIO_FREE` stub dependence does not contaminate A4b

**Question (Q1):** does the guest's dependence on the `PIO_FREE` stub (`0xFE820010` →
constant `0x80`) before it reaches the spin contaminate A4b's strict claim that "the GP
DSP56300 engine executes the uploaded program and thereby clears `0x803C0810` in a STRICT
run"?

**Advisor:** Persistent advisor child `407c54a3-6ca4-4a65-835b-faf5355195cd`, route
`claude` / `claude-opus-5-5` @ `high` (workflow §1), marker `DSH-JSRF-ADVISOR-7Q4M2XK9`.
**Asked by:** Session `session-4a79ce06-0bd5-46f1-a760-c800ce8b62f9` at the plan's Q1 gate.
**Verdict:** **It does NOT contaminate the claim**, provided the claim is scoped as below.
A sourced `PIO_FREE` model is **not** a prerequisite for A4b and **must not** be part of
A4b.

## Ruling (verbatim)

> **RULING:** It does NOT contaminate that claim, provided the claim is scoped as below. A
> sourced PIO_FREE model is NOT a prerequisite for A4b and must NOT be part of A4b.
>
> Why: stub contamination is judged one wait at a time and one value at a time
> (jsrf-run-profiles.md §Feature enablement: "a claim is only as strict as the source of
> each value it relies on").
> - The value that ends the +0x810 wait is the 0 that A4b's GP engine is required to
>   write. It does not come from a stub.
> - The PIO_FREE answers satisfy different waits. At every poll site I read, the polled
>   value is used only to decide whether a free-space loop exits. It is never used as
>   data. So those answers determine whether and when the guest reaches the spin. They do
>   not determine the command or image the GP later acts on, which the guest wrote itself.
>
> A4b's strict claim therefore holds only if A4b establishes ALL of the following:
> 1. **Who wrote the 0.** +0x810 goes from 3 to 0, and the store that wrote 0 comes from
>    the GP engine's memory-write path while it executes the uploaded program. The run
>    must also show that no guest-CPU store wrote it: neither the stop-path sites
>    0x001A1751 / 0x001A1F9B nor any other. RECOMP_APU_DSP_ACK must be absent. The
>    positive controls: GPRST goes 0→3 and bootstraps from G, the loaded words equal the I
>    image, and GP instructions retire.
> 2. **Where the GP's inputs came from.** List everything the GP program reads from outside
>    itself: peripheral/host registers, the mailbox, and system-memory transfers via SGE or
>    DMA. Show that each value was either written by the guest or produced by a modelled
>    mechanism. If any of those inputs comes from a stub (GP/EP register reads still
>    answered with 0, the constant PIO_FREE, VP state the stub VP never computed, or the
>    trap-disabled tick counter), the claim is exploratory for that dependency.
> 3. **That PIO_FREE is only used as a gate.** Record, from XBE disassembly for all ten
>    0xFE820010 read sites, that each is a threshold loop (`(v & ~3) < N` or
>    `(v >> 2) < ecx` → re-poll) and that the value is dead once the loop exits. This is
>    what limits the dependence to reachability.
> 4. **Claim limits written into the packet:**
>    - the guest's arrival at the spin, and everything after it, goes through PIO_FREE stub
>      gates and is exploratory-grade;
>    - A4b therefore does not establish boot progress, liveness, DirectSound init success,
>      audio output, or the title's behaviour under a modelled VP FIFO;
>    - A4b does not establish that the title reaches the spin in the same state or with the
>      same timing as real hardware.
>    A4b establishes exactly one thing: this wait was satisfied by modelled GP execution of
>    the guest's own command.
>
> **Clarifications:**
> - GP execution behind RECOMP_APU_TRAP is not an "unconditional modeled cause" and doesn't
>   need to be one. It is device work on the guest's command, which is exactly what
>   criterion 4 requires be done. Ruling D's criterion-1 bar applies to autonomous causes
>   only.
> - The classifier's STRICT label is still necessary. RECOMP_GPU_ACK=0 is required.
>
> **When PIO_FREE becomes a prerequisite:** before ANY later strict criterion about
> liveness or boot progress past the spin, e.g. "the title reaches <checkpoint> in a strict
> run". Plan it as its own packet, placed before the first such criterion, not inside A4b.
> - Note for that packet: xemu master hw/xbox/mcpx/apu/vp/vp.c vp_read returns 0x80 for
>   NV1BA0_PIO_FREE with the comment "we don't simulate the queue for now, pretend to
>   always be empty". So the obvious secondary source describes itself as a pretence and
>   cannot corroborate 0x80 as device state. Our apu_vp.c is xemu-derived, so it isn't
>   independent either. Condition (ii) of my earlier ruling most likely needs a real
>   FIFO/free-count model, or a primary source, not a citation.
>
> **BASIS:**
> - Observed: the per-value, per-wait scoping rule is in docs/jsrf-run-profiles.md
>   §Feature enablement (146-165). Its line 154-156 contaminates "the wait was satisfied by
>   a modelled cause" only "if a stub produced the value that let the guest continue".
> - Observed: in src/recomp/gen/recomp_0005.c, all ten `MEM32(0xFE820010u)` reads (lines
>   8667, 8720, 13698, 13867, 14172, 16548, 18243, 18377, 19816, 19885) are threshold
>   re-poll loops with N ∈ {4, 8, 0x20, 0x80} or `(v>>2) < ecx`.
> - Observed: at 0x1A22A3, 0x1A22DD, 0x1A3EC2, 0x1A3FEA, 0x1A418D, 0x1A567E, 0x1A60C5 and
>   0x1A6129, eax is overwritten or unused right after the loop. Disassembly of 0x1A4181,
>   0x1A4E4C, 0x1A57CD and 0x1A6129 confirms the pattern: the loop exits into VP method
>   stores (0xFE820804/0808, 0xFE821000/1004, 0xFE82010C).
> - Uncertain: I have not audited, in XBE disassembly, dead-after-exit for the value along
>   every path at the three sites where a call follows the loop. A4b condition 3 closes
>   this.
> - Observed: in A4a R1 (logs/runs/20260924-191833-331-a4a-r2-trap-trace/jsrf_run.log) all
>   58 reads of 0x20010 return 0x80. The first is at L3013, after SECTL=7 at L3010.
>   GPRST=3 is at L3201, and polls continue until L4470. So some PIO_FREE gates come after
>   the GP starts, and the trace does not say which thread made them.
> - Observed: toolkit apu_vp.c:551-571. PIO_FREE returns the constant 0x80, and VP writes
>   run fe_method synchronously, so the model has no queue.
> - Inferred: the title boots on real hardware, so these monotone gates eventually pass
>   there. The stub changes when they pass, not whether they pass or what data follows. It
>   could hide a real FE stall; that possibility is covered by claim limit 4.
> - Uncertain: whether the GP program reads any VP-produced state or timing-sensitive
>   state. That is exactly condition 2.
> - Observed: the xemu vp_read comment, from
>   raw.githubusercontent.com/xemu-project/xemu/master/hw/xbox/mcpx/apu/vp/vp.c, fetched
>   today, master (commit not pinned).
>
> **REVERSED BY:**
> (a) Any 0xFE820010 read site where the polled value flows into data the guest writes (for
> example, sizing a burst or selecting methods). The stub would then shape the GP's inputs,
> and A4b would need a modelled PIO_FREE first.
> (b) The condition-2 inventory showing that the GP program consumes a value produced by a
> stub, VP state from the stub VP, or the trap-disabled counter. The claim is then
> exploratory unless that input is modelled first.
> (c) The 0 at +0x810 traced to a guest-CPU store or a DSP_ACK path. That is a FAIL, not a
> narrowed claim.
> (d) An owner or profile-doc change that makes strictness apply to the whole run rather
> than to each value.
>
> **RECORD IN:**
> - Case ruling: verbatim in A4b's review record (docs/reviews/a4b-*.md), with this child
>   ID and route. The plan's Q1 gate marked answered, pointing to that record. A4b's claim
>   limits cite it.
> - General clarification: add as a separate policy edit to docs/jsrf-run-profiles.md
>   §"Feature enablement": "Stub dependence is judged one wait at a time: a stub that only
>   gates whether the guest reaches a point limits reachability and liveness claims, not the
>   provenance of a later value produced by a model." Never added by the packet itself.
> - PIO_FREE-model packet: add to the plan's follow-up list as a prerequisite for the first
>   strict liveness or boot-progress criterion past the spin. Include the xemu
>   self-described-pretence finding.

## PREMISE_CHANGED addendum (2026-09-24) — the site population is 28, not 10

**Added after the original ruling above. The original text is not overwritten.**

The A4b adequacy reviewer (a Planner that did not write the packet) ran out of context
before returning a verdict, but its recovered reasoning contained this finding; the Session
reproduced it and put it to the Advisor as a `PREMISE_CHANGED` under §4.3. **The Advisor
confirmed the error was its own**: it built the Q1 site list by grepping the hex spelling
of the address, so the list covered **10 of 28** sites.

**The measurement (observed, independently reproduced by the Session):**

- `0xFE820010 - 2^32 = -25034736` — the same guest VA, two spellings.
- `MEM32(0xFE820010u)` = **10** occurrences in `src/recomp/gen/recomp_*.c`.
- `MEM32(-25034736)` = **18** occurrences, at `recomp_0005.c:10365, 11307, 11449, 11883,
  12018, 12132, 12396, 12547, 12658, 12782, 12882, 13120, 13246, 13770, 14291, 14399,
  15755, 18358`.
- Overlap: **0**. No other spelling exists. Distinct total: **28**.

**Why the spellings differ (observed lifter behaviour, now recorded as operating
knowledge):** an `A1` moffs load — always into `eax` — is emitted as `MEM32(0xFE820010u)`,
while a ModRM `disp32` operand is emitted as signed decimal `MEM32(-25034736)`. That is why
all 10 hex sites load `eax` and all 18 decimal sites load other registers. **Every address
≥ `0x80000000` is exposed to this**, which covers all MMIO and the contiguous window.

### Advisor ruling on the premise change (verbatim)

> First, the error is mine. I built the Q1 site list by grepping the hex spelling of the address, so the list covered 10 of 28 sites.
>
> RULING:
> (a) Q1's conclusion STANDS. What changes is the work condition 3 requires and the evidence that backs it; it does not need re-deciding.
> - The ruling is about the address, not a list of sites: PIO_FREE is only a gate.
> - I checked all 18 additional sites myself. Every one has the same threshold re-poll shape: `(v & ~3) < N` with N ∈ {8, 0xC, 0x48, 0x4C}, or `(v >> 2) < reg`, followed by a jump back to the read on below.
> - So the corrected population does not trigger reversal (a) on the evidence available.
> - Still, Q1 is only as good as condition 3 executed over the COMPLETE population. Until AC-PIO passes over 28 sites whose completeness is itself witnessed, the Q1 conclusion is only provisional. A4b-r1 cannot be ADEQUATE as written.
>
> (b) What AC-PIO must require, so that a literal executor cannot pass it over a subset:
> 1. Build the population from the ORIGINAL XBE, not from the generated C text.
>    - Command: `python -X utf8 scripts\inspect-jsrf.py find 0xFE820010`. Record it verbatim, with its output and the XBE SHA-256.
>    - I ran it: it returns exactly 28 occurrences, all in section DSOUND, 0x001A2297 … 0x001A611D.
>    - Decode each hit as the 32-bit memory operand of an instruction starting 1 byte earlier (opcode A1, `mov eax,[moffs]`) or 2 bytes earlier (8B /r disp32). A hit that does not decode that way is UNKNOWN; it is never dropped.
> 2. The packet must freeze the population count (28) and the list of instruction VAs. The PASS predicate must be conditioned on: count found == frozen count, AND every listed VA is evaluated. Any difference makes the criterion UNKNOWN, not PASS.
> 3. Reconcile against the generated tree.
>    - Parse every integer literal inside MEM8/16/32 in `src/recomp/gen/recomp_*.c` and normalise it mod 2^32. Do not match text.
>    - Every XBE site must map to exactly one generated read (`loc_<insnVA>` label). The normalised count must be 28, with no leftovers in either direction.
> 4. Define "the value is not used after the loop" precisely:
>    - no flow of the polled register into a guest store, MMIO store, branch condition or address, on any path, until the register is overwritten;
>    - a callee-save push/pop pair is not a use;
>    - if a `call` happens while the register still holds the polled value, the criterion must either show the callee does not read it as input or mark the site UNKNOWN. Sites 0x001A3414 (esi) and 0x001A4325 (ebx) exit through `lea ecx,[ebp-8]; call 0x1A1BAF` with the register still live until `pop`.
> 5. State the static method's limit, in the same sentence as the claim: it cannot exclude register-indirect access to 0xFE820010.
>    - Required supporting evidence: the other MCPX-window base forms found in the XBE. I found exactly one: 0xFE800000 used as a displacement at 0x001A2E4A…0x001A2FA4, with base registers loaded from the table at 0x001B9ECC, whose entries read 0x2054…0x2074. That is inferred to be the voice-list registers, not PIO_FREE.
>    - One stray 0x00020010 at .rdata 0x001E4888 has no pointer references, which is inferred to be data.
>    - If any of this can reach PIO_FREE, the criterion is UNKNOWN.
>
> (c) General rule, worth recording. The trap is systematic, not a one-off.
> - Observed lifter behaviour: an `A1` moffs load (always into eax) is spelled `MEM32(0xFE820010u)`, while a ModRM disp32 operand is spelled as signed decimal `MEM32(-25034736)`. That is why all 10 hex sites load eax and all 18 decimal sites load other registers.
> - Every address ≥ 0x80000000 is exposed to this, which covers all MMIO and the contiguous window.
> - Rule: a claim that enumerates guest accesses to an address must:
>   (i) derive the population from the original XBE instruction stream, with operands normalised to uint32;
>   (ii) reconcile it against the generated code after normalising literals, never by spelling;
>   (iii) freeze the count and the generating command in the criterion;
>   (iv) state what the method cannot see: register-indirect, computed, and table-driven accesses.
>   A text grep is admissible as a lead, never as a completeness witness.
>
> Dependent-ruling reconsideration (§4.3). Checkpoint-40 constraint 3(ii) relies on an enumeration of guest CPU stores that write 0 to +0x810 (0x001A1751 and 0x001A1FA7). Those stores are register-indirect (B+0x810), so no literal enumeration can prove the set is complete. Amend 3(ii) as follows:
> - Demote the stop-path counters to corroboration.
> - Make the primary witness atomic: the GP DMA write to the single dword at B+0x810 is done as InterlockedCompareExchange(expected 3 → 0).
>   - A successful exchange proves the GP's own write made the 3→0 transition, and no CPU store could have come in between.
>   - On failure, do the ordinary store and log the observed value; that routes to UNKNOWN / FAIL-cleared-by-CPU.
>   - The final memory result is the same as an ordinary store in both cases, so no device semantics change.
> - 3(i) (payload word is 0) and 3(iii) (no RECOMP_APU_DSP_ACK) still apply.
>
> BASIS:
> - Observed: my own counts in src/recomp/gen/recomp_*.c: `MEM32(0xFE820010u)` = 10; `-25034736` = 18, at the 18 recomp_0005.c lines you listed; no other spelling.
> - Observed: `inspect-jsrf.py find 0xFE820010` = 28 occurrences, all in DSOUND. Each hit is at label+1 (A1 form) or label+2 (8B form) of the 28 generated `loc_` labels.
> - Observed: the generated loop bodies at all 18 lines.
> - Observed: XBE disassembly of the exits at 0x1A3414, 0x1A4325, 0x1A579E, 0x1A3F24, 0x1A38F4, 0x1A3B69 and 0x1A4242.
> - Observed: `find 0xFE800000` = 7 hits (six displacements at 0x1A2E4A–0x1A2FA4 plus .data 0x22DB72); table 0x1B9EC0 data; `find 0x00020010` = 1 hit at .rdata 0x1E4888, and 0 references to that address.
> - Inferred: the 0x2054–0x2074 table is voice-list registers, and 0x1E4888 is data.
> - Uncertain: whether sub_001A1BAF reads esi or ebx as input. I did not audit this; AC-PIO item 4 must.
> - Uncertain: runtime-computed accesses to 0xFE820010 outside the forms found. The dynamic trace cannot attribute a read to an instruction, so this cannot be settled at runtime yet.
>
> REVERSED BY:
> - Any of the 28 sites where the polled value flows into data, a store or a branch other than its own loop test. Q1 then flips, and a PIO_FREE model becomes a prerequisite for A4b.
> - A register-indirect or table-driven read of 0xFE820010 whose value is used as data.
> - Evidence that the XBE-find method misses an encoding form, for example the address split across an instruction boundary or built at runtime. The enumeration rule then needs another method.
>
> RECORD IN:
> - The case ruling, verbatim with this child ID and route, in docs/reviews/a4b-q1-advisor-ruling.md, as a dated PREMISE_CHANGED addendum. Do not overwrite the original text.
> - Cross-reference it from docs/reviews/a4b-planning-rulings.md for the constraint-3(ii) amendment.
> - A4b-r1 must be revised: AC-PIO per (b), and constraint 3 per the amendment. Then run a fresh adequacy review.
> - General rule (c): as a separate policy edit in docs/agent-workflow.md, in the section governing packet criteria and executor-literal predicates.
> - The lifter spelling fact (A1 moffs → hex, disp32 → signed decimal): one line in AGENTS.md under "Generated-source rules", as operating knowledge.

### Session verification of the addendum's claims

| Claim | Session check | Result |
|---|---|---|
| `find 0xFE820010` returns 28 | ran the command | **`28 occurrence(s) of 0xFE820010`**, all `DSOUND`, `001A2297` … `001A611D` — matches the Advisor exactly |
| Each hit maps to a generated `loc_` label at hit−1 or hit−2 | mapped all 28 against `recomp_0005.c` | **28/28 mapped**, none unmapped — the reconciliation procedure is mechanically sound |
| `find 0xFE800000` = 7 hits | ran it | **`7 occurrence(s)`**, last two `001A2FA4 DSOUND`, `0022DB72 .data` |
| `find 0x00020010` = 1 hit | ran it | **`1 occurrence(s)`** at `001E4888 .rdata` |
| The `0x1B9ECC` table holds `0x2054…0x2074` | read the XBE as data at that VA | **`00002054 00002058 0000205C 00002060 00002064 00002068 0000206C 00002070`** — voice-list registers (`NV_PAPU_VPVADDR` `0x202C`… family), **not** `PIO_FREE` (`0x20010`). The Advisor's inference is confirmed. |

The Advisor's escape-hatch analysis therefore holds on the evidence available: the only
other MCPX-window base form addresses voice-list registers, and the stray `0x00020010` is
data. The method's blind spot (register-indirect and computed access) is real and must be
stated with the claim.

**Status of Q1:** the conclusion stands, but it is **provisional** until `AC-PIO` passes
over a population whose completeness is itself witnessed. `A4b-r1` cannot be `ADEQUATE` as
written.

> **Condition 3 now moves to `A4p`.** After two consecutive `INADEQUATE` verdicts on
> `A4b`'s `AC-PIO` (§5.5), the Advisor ruled that condition 3's analysis is itself
> execution and belongs in a **discovery packet**, not in a change-packet criterion:
> `AC-PIO` is replaced by a locally-checked calling-convention rule (C1–C4) and moves to a
> new packet **`A4p`**, which runs before `A4b` is promoted. `A4b` deletes `AC-PIO` and
> cites `A4p`'s accepted `O-GATE` as a precondition. **Condition 3 is therefore discharged
> by `A4p`, not by `A4b`.** Full ruling: `docs/reviews/a4b-pio-methodology-ruling.md`.

## Session verification of the ORIGINAL ruling's claims (superseded in part — see the addendum)

Independently reproduced by the Session before the original ruling was recorded (workflow
§2.4.2). **Superseded in scope by the PREMISE_CHANGED addendum above:** the first row below
counted only the hex spelling and therefore covered 10 of the 28 sites. The other three rows
stand, and the addendum extends the loop-shape finding to all 28.

| Claim | Check | Result |
|---|---|---|
| ~~Ten `0xFE820010` read sites in `recomp_0005.c`~~ | `Select-String 'MEM32\(0xFE820010u\)'` | **exactly 10 of that spelling** — but the population is **28**. Corrected by the addendum; do not use this row as a completeness claim. |
| Each is a threshold re-poll loop | read the loop bodies | `L8667`: `eax &= 0xFFFFFFFC` → `cmp eax,4` → `jb loc_001A2296` (re-poll). `L14172`: `eax >>= 2` → `cmp eax,ecx` → `jb loc_001A4181`. `L19885`: same form as `L8667`. Confirmed. |
| The value is dead after the loop exits | read the exit targets | `L8667` exits to `loc_001A22A3: eax = MEM32(ebp + 8)` — `eax` immediately overwritten. `L14172` exits to `loc_001A418D: edi = 0` — `eax` unused. `L19885` exits to `loc_001A6129: MEM32(-25034484) = 1` — `eax` unused. Confirmed at the three sites sampled. |
| `apu_vp.c` returns the constant with no queue | read `mcpx_apu_vp_read` | `case NV1BA0_PIO_FREE: return 0x80; /* Always pretend queue is empty */`, and `mcpx_apu_vp_write` dispatches through `fe_method` synchronously. Confirmed. |

The Session did **not** re-verify the Advisor's full dead-after-exit audit (the Advisor
itself marks that uncertain at the sites where a call follows the loop). `A4b`'s `AC-PIO`
closes it, now over the complete 28-site population.

## General clarification recorded in its owning document

Added to `docs/jsrf-run-profiles.md` §"Feature enablement — real capability, not a bypass"
as a separate policy edit, per the ruling's RECORD IN and §280-282 (a packet may cite that
section, it must not amend it). The added text is headed **"Stub dependence is judged one
wait at a time."** Documentation only; it changes no classifier behaviour.
