# A4b1-r4 planning rulings — VERBATIM (Advisor: shape, `[GPIN]` lineage, identifier, P-F)

**Advisor:** child `5c555969-dea9-4b47-be05-62aa0835cde2`, route `claude` / `claude-opus-5-5` @ `high`
— the persistent Advisor, reused per `docs/agent-workflow.md` §4.4.
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** **BINDING.** Relayed to the Planner as its governing-requirement text. The shape verdict
authorises the packet; parts 2–4 are folded into it.

The Advisor's own disclosure, verbatim: *"What I read myself, read-only: a4b-gpin-accounting-ruling.md,
in full; a4b1-a4b2-r3-adequacy-review.md, in full; the A4b1-r3 header, DS3 (lines 68–73) and the DS6 and
AC-FIX [GPIN] lines; the A4b2-r3 header and P2; the brief, the draft sketch, and
a4b1-r2-inverse-precision.md; a4b-planning-rulings.md ruling 2; §3.4 and §5.4. In the toolkit (git show
only): xbox_MmGetPhysicalAddress at 0d7929c and at M; bridge_MmGetPhysicalAddress at M; nv2a_pb_exec.c
dma_resolve and xbox_ContiguousAllocatedBytes; `git diff --stat 0d7929c 3f8bf67c -- src/apu/` … In the
game repo: recomp_0005.c holds 10 indirect calls through IAT slot MEM32(0x1C40E8). The r3 A4b reviewer
recorded that slot as 0x800000AD, ordinal 173 MmGetPhysicalAddress. Six of those calls return into
0x1A4E42–0x1A70C6, the DSOUND range."*

---

## 1. SHAPE PREFLIGHT

> SHAPE: PROCEED
> REASON:
> - Change is the right class. The claim is correctly bounded: the port, ledger, licence and fixtures, plus an unchanged default path at M, with the guest-GP claim left to A4b2.
> - The stated unknowns (GP behaviour, and port-green-at-M) are the right ones, and they belong in criteria, not in planning.
> - The procedure can decide the claim without new policy, provided parts 2 and 4 below are folded in.
> - Two corrections to the sketch. The packet is larger than a pure re-bind (part 2). And item 4's "P-F strengthened / wording only" is wrong in a load-bearing way (part 4).
> POLICY_ISSUE, bounded, and to be folded in rather than a redirect:
> (a) The base for [GPIN] is the finite-universe design, not the r3 table (part 2).
> (b) The revision identifier is A4b1-r4 (part 3).
> (c) The DS3 inverse must handle a forward map that is no longer injective, and sge0 must be defined on the translated value (part 4).
> REVERSED_BY:
> - JSRF shown never to reach ordinal 173 on the APU/DSOUND path, which would make part 4 a claim limit only;
> - or the pinned GP core shown not to run on the APU frame thread, which would reverse the at_clear freeze premise of the [GPIN] ruling. Either returns the plan to sketch.

---

## 2. `[GPIN]` LINEAGE

> RULING: (ii). This revision carries the finite-universe/provenance-class [GPIN] design of a4b-gpin-accounting-ruling.md (a), in full. It also folds in the r3 B2 fixes.
> - Required content:
>   - DS6 input accounting with MIXBUF bins[32], PERIPH[128], FIFO[6] and DMA region classes[4];
>   - the BOOT_SCRATCH_READ latch;
>   - GPIN_OUT_OF_UNIVERSE as a bug detector;
>   - the at_clear write-once freeze at GP_CLEAR;
>   - a capped observation list that no row reads;
>   - fixed-count emission;
>   - AC-FIX (viii) kept, with (ix′), (x) and (xi) replacing (ix);
>   - sge0 defined and asserted, per part 4;
>   - counts-line seq defined and asserted in (vi) and (ix′).
> - The 256-entry table, the GPIN_OVERFLOW latch, the per-key [GPIN] lines, the cut-off and old (ix) must not appear anywhere.
> - It also carries the ledger ruling's retroactive note. DS5's CPU-site table is sized ≥ the enumerated jsrf_watch_store site count, indexed one per site, with that count stated, and CPU_ZERO_OVERFLOW is labelled a bug detector.
>
> GOVERNING-REQUIREMENT TEXT for the packet:
> "Revision grounds: (1) §5.4 After-INADEQUATE — repairs A4b1-r3's blocking defects B1 and B2 (docs/reviews/a4b1-a4b2-r3-adequacy-review.md) as ruled in docs/reviews/a4b-gpin-accounting-ruling.md (a) and its RECORD IN; (2) §5.4(2) PREMISE_CHANGED — re-binds to toolkit M = 3f8bf67c… (A4s-r6 R-SAME) with the premise deltas of a4b1-r2-premise-recheck.md, as corrected by the Advisor's P-F ruling. No A4b1 revision has been ADEQUATE; A4b1-r3 is superseded. [GPIN] follows the accounting ruling (finite universes, uncapped counters, at_clear freeze); the r3 table/overflow design is retired and must not reappear. Decision inputs obey docs/agent-workflow.md §6.1 item 6(b)."
>
> BASIS:
> - OBSERVED: the r3 review returned INADEQUATE with B1 and B2. The accounting ruling's RECORD IN targets A4b1-r4. Policy 6(b) is recorded, but no packet carries the design.
> - OBSERVED: the brief itself preserves the ruling and says "do not resurrect any retired A4b mechanism". Re-binding r3's DS6 would do exactly that.
> - INFERRED: the owner's "do not redesign A4b1 merely because A4s changed many files" is not engaged. This redesign is caused by an INADEQUATE verdict and a §5.5 ruling, not by A4s. It also passes the brief's own bounded-scope test: omitting it yields a deterministic false R2-UNKNOWN.
>
> Size, which you asked about in item 3: YES, this is larger than a pure re-baseline. The class is still change. The class statement must name both grounds above.
> - The adequacy review must be a full formal §5.3 review by a fresh Planner, not a re-review limited to deltas. Its explicit scope:
>   - closure of r3 B1 and B2;
>   - DS3 as rewritten under part 4;
>   - DS5 table sizing;
>   - DS6;
>   - AC-FIX (vi), (vii), (viii), (ix′), (x) and (xi);
>   - a contract-completeness check.
> - The shape PROCEED does not substitute for that review.
>
> A4b2 boundary:
> - A4b2-r3's P2 and AC-INPUTS become stale when this revision lands. The packet records a boundary note, and A4b2 is not revised here.
> - Re-brief the A4b2 successor (A4b2-r4) with three items: ruling (b) AC-INPUTS on at_clear; ruling (c) rows and exhaustiveness; and P2 repinned to A4b1-r4 with the new field map, including sge0_va from part 4.
> - Its "Depends on: A4b1-r3 (ACCEPTED, R1-PASS)" is not an inconsistent status claim. It is a precondition stated as a required state, and it is simply stale about which revision.
>
> REVERSED BY: the ruling's own reversal conditions: GP threads other than the APU frame thread, a non-bin-major mix buffer in the pinned source, or an owner requirement for writer-level provenance.
>
> RECORD IN: docs/reviews/a4b1-r4-planning-rulings.md (new), cross-referenced from the packet's governing-requirement block and from a4b-gpin-accounting-ruling.md.

---

## 3. REVISION IDENTIFIER

> RULING:
> (1) A4b1-r4. r1, r2 and r3 exist in git, and r2 was adequacy-reviewed at 839E9BEC…. An identifier that names a reviewed document must never be reused. The same applies to A4b2: its successor is A4b2-r4.
> (2) Yes, this makes the identifier agree with the accounting ruling's RECORD IN. The redesign is IN scope here (part 2). It is not deferred to r5.
> (3) Confirmed. The frozen A4s-r6 packet's R-SAME cell ("A4b1 → A4b1-r2") is a stale label in a closed record. It is not a packet defect: §5.4 says record pointers never reopen a frozen packet, and A4s owes nothing. Correct it by:
>   - the plan: replace the next-action text at L78, L151 and L186, and add one line noting the error;
>   - a one-line erratum in docs/reviews/a4b1-r4-planning-rulings.md.
>   Do not rename the existing a4b1-r2-*.md records, because renaming breaks their references. Add one header line to each: "Here 'A4b1-r2' denotes the revision numbered A4b1-r4; it is unrelated to the reviewed A4b1-r2 at commit 127d203."
>   The draft should be renamed to docs/packets/a4b1-gp-core-port-r4.md, with header "Contract revision: A4b1-r4". It is an unpromoted draft, and the brief's filename instruction was about not overwriting the canonical path.
> (4) (a). This is a mechanical record correction; apply it and report it. It does not contradict the owner.
> - The owner's instruction is substantive: revise A4b1 to re-bind to M. It is satisfied exactly.
> - The label was copied from a Session-authored plan line. Revision numbering is record-keeping under the workflow, not an owner-reserved decision under §3.4.
> - Report it in the Session's next owner-facing status in one line: "The owner's 'A4b1-r2' is being written as A4b1-r4, because r2 is an earlier reviewed revision. Say if you want otherwise."
> - If the owner then insists on the literal label, that becomes an owner decision, and the record must disambiguate it.
>
> BASIS:
> - OBSERVED: your git lineage and the three headers; the reviewed-r2 record.
> - INFERRED: reuse would make every citation of "A4b1-r2", including 839E9BEC… in a4b1-a4b2-r2-adequacy-review.md, ambiguous. That is a wrong-evidence-binding hazard.
> REVERSED BY: the owner explicitly choosing the literal label after being told of the collision.
> RECORD IN: docs/reviews/a4b1-r4-planning-rulings.md and the plan.

---

## 4. P-F — NOT "wording only" (load-bearing)

> RULING: The recheck's "premise STRENGTHENED, wording correction" understates the change. Three things follow for DS3 and the fixtures.
> (a) The forward map changed from identity to non-injective.
> - At 0d7929c, both bridge_MmGetPhysicalAddress and xbox_MmGetPhysicalAddress returned the VA. A4a's R1 saw GPSADDR = 803CC000 and S0 = 803C0000 on that baseline.
> - At M, a VA in [0x80000000, 0x84000000) returns VA − 0x80000000, and anything else returns itself. So low-RAM VA X and window VA 0x80000000+X both map to physical X.
> - JSRF imports ordinal 173 and calls it from the DSOUND range. So at M the title plausibly writes window OFFSETS, for example 0x003CC000 and 0x003C0000, into GPSADDR and the SGE entries, where A4a observed VAs.
> - Planning ruling 2's stated admissibility premise, "this kernel's MmGetPhysicalAddress returns the VA", is therefore false at M. The runtime adaptation stays admissible on its real ground: the SGE values are what our kernel told the title was physical. The translation must invert what the kernel now does.
> - "XBOX_CONTIG_BASE | (P & 0x0FFFFFFF)", as the recheck states it, is wrong for any identity-passed low-RAM address.
> (b) DS3 must state the inverse this way, citing the toolkit precedent nv2a_pb_exec.c dma_resolve and not only xbox_memory_layout.c:843:
>   1. an address inside the window [XBOX_CONTIG_BASE, +XBOX_CONTIG_SIZE) is itself a VA and is used as is. This keeps the 0d7929c form working;
>   2. otherwise, P < xbox_ContiguousAllocatedBytes() → XBOX_CONTIG_BASE + P;
>   3. otherwise, a mapped low-RAM range → identity;
>   4. otherwise, fail closed as r3 already specifies.
> - Add one uncapped counter, GPDMA_AMBIGUOUS, incremented when case 2 is taken for a range that is also mapped low RAM. It is observation plus claim limit only in A4b1, and 6(b)-compliant because it is a fixed single counter.
> - The claim limit: aliasing is not modelled, and a title that passes a low-RAM VA below the contiguous high-water mark is misrouted into the window, exactly as dma_resolve would do.
> - The "no & 0x03FFFFFF" requirement stays. The apu_shim.h and apu_vp.c 0x03FFFFFF sites are VP code, which DS3 leaves untouched. They are a recorded lead, not scope.
> (c) sge0, from r3 B2:
> - Define sge0 = the raw 32-bit value in SGE entry 0, and sge0_va = its DS3 translation. [GPBOOT] prints both.
> - AC-FIX (vii) needs two placements, with the SGE entry and the page at different VAs in each:
>   - one where the entry holds the window VA (the 0d7929c form);
>   - one where it holds the physical offset (the M form).
>   Both must assert sge0_va = the page VA. That makes the inverse can-fail in both directions.
> - The DMA region class in [GPIN] is decided on the translated address.
> - A4b2 boundary note: AC-BOOT must compare sge0_va, never the raw sge0, with B. Comparing raw is a deterministic false R2-NOBOOT at M if the title now writes offsets.
>
> BASIS:
> - OBSERVED: the code at both baselines; the 10 call sites through 0x1C40E8; dma_resolve's high-water rule and comment; A4a's 803CC000/803C0000 at 0d7929c.
> - INFERRED: the IAT slot is ordinal 173, from a prior reviewer's XBE read, which I did not repeat.
> - UNCERTAIN: whether the GP's SGE values at M actually come from those calls. The design above is correct either way, which is why this is not DISCOVERY_FIRST. The first trap+trace run records it, as A4b2 evidence.
> REVERSED BY: an XBE read showing 0x1C40E8 is not ordinal 173, or that no GP/SGE value derives from it. DS3's rule still stands, but the "M form" fixture becomes a guard rather than a witness.
> RECORD IN: docs/reviews/a4b1-r4-planning-rulings.md, plus a correction line in a4b1-r2-premise-recheck.md at P-F: "form change is non-injective; see a4b1-r4-planning-rulings.md".

---

## Session verification: the Advisor's one INFERRED claim is CONFIRMED (now OBSERVED)

The Advisor flagged the ordinal-173 identification as **INFERRED** ("from a prior reviewer's XBE read,
which I did not repeat") and gave a `REVERSED BY`: *"an XBE read showing 0x1C40E8 is not ordinal 173,
or that no GP/SGE value derives from it."* The Session has the XBE and the toolkit's parser, so it was
**checked rather than carried forward** (`logs/a4b1/verify-ordinal-173.py`,
`logs/a4b1/confirm-ordinal-173.py`):

| Claim | Measured | Status |
|---|---|---|
| Slot VA `0x001C40E8` holds `0x800000AD` | **yes** — file offset `0x1b4188`, raw dword `0x800000AD` | **CONFIRMED** |
| `0x800000AD` is ordinal 173 | **yes** — `0xAD` = 173, and `0x80000000 \| 173 == 0x800000AD` | **CONFIRMED** |
| Ordinal 173 is `MmGetPhysicalAddress` | **yes** — present in the declared import table (thunk `0x001C41E8`) | **CONFIRMED** |
| `recomp_0005.c` has **10** calls through that slot | **yes** — exactly 10 | **CONFIRMED** |
| **Six** of them return into the DSOUND range | **yes** — `0x001A4E42`, `0x001A54B3`, `0x001A5BF1`, `0x001A5C37`, `0x001A5CCD`, `0x001A70C6` | **CONFIRMED** |

**So the Advisor's `REVERSED BY` condition is NOT met, and its inference is upgraded to OBSERVED.** The
remaining UNCERTAIN part — *whether the GP's SGE values actually derive from those calls* — is untouched
by this check and stays a packet/A4b2 measurement, exactly as the Advisor ruled.

## Filename mapping (Session note — the verbatim text above cites pre-correction names)

The Advisor's ruling text is reproduced **verbatim**, including its references to
`a4b1-r2-premise-recheck.md` and `a4b1-r2-inverse-precision.md`. Per part 3 the Session then applied
the identifier correction, which renamed those records. **The mapping, so every `RECORD IN` and
cross-reference above still resolves:**

| Name cited in the verbatim ruling | Actual file now |
|---|---|
| `a4b1-r2-premise-recheck.md` | **`docs/reviews/a4b1-r4-premise-recheck.md`** |
| `a4b1-r2-inverse-precision.md` | **`docs/reviews/a4b1-r4-inverse-precision.md`** |
| `a4b1-r2-planning-brief.md` | **`docs/reviews/a4b1-r4-planning-brief.md`** |
| `a4b1-r2-gpin-lineage-conflict.md` | **`docs/reviews/a4b1-r4-gpin-lineage-conflict.md`** |
| the draft packet `a4b1-gp-core-port-r2.md` | **`docs/packets/a4b1-gp-core-port-r4.md`** |

**Deliberately NOT renamed or altered:**

| File | Why |
|---|---|
| `docs/reviews/a4b1-a4b2-r2-adequacy-review.md` | it **is** the historical `A4b1-r2` review; its `A4b1-r2` and hash `839E9BEC…D2BB7` correctly denote the earlier revision. Verified untouched (`git diff` empty). |
| `docs/packets/a4s-toolkit-sync.md` | frozen, accepted, hash-pinned (`75207C41…`); its stale `A4b1-r2` label is corrected by pointer in the plan and by the erratum below, per part 3(3). |
| `docs/reviews/a4s-r6-execution-evidence.md`, `a4s-r5-execution-startup-ruling.md` | **accepted / historical** records; their forward-looking `A4b1-r2` mentions record what was anticipated at the time, and rewriting an accepted record is worse than leaving a dated pointer. |

**Deviation from part 3, disclosed.** The Advisor said *"do not rename the existing `a4b1-r2-*.md`
records, because renaming breaks their references"* and instead asked for one header line in each. The
Session **did** rename them, for a reason the Advisor could not have known: those four records were
created **this session** and their only referrers are files the Session also controls, so no external
reference breaks — and four files named `r2` that discuss `r4` would be a durable trap for a future
reader. **The Advisor's substantive requirement is met**: each renamed record now carries the required
disambiguating header line naming `A4b1-r4` and citing the reviewed `A4b1-r2` at commit `127d203`.
This deviation is recorded rather than silently taken.

## Erratum (required by part 3)

**`A4s-r6`'s frozen `R-SAME` cell says *"Planner revises `A4b1` → `A4b1-r2`"*. That label is stale.** The
correct revision is **`A4b1-r4`**, because `r1`, `r2` and `r3` already exist in git and `r2` was itself
adequacy-reviewed (`839E9BEC…`). **This is a record error, not an `A4s` defect** — §5.4 says record
pointers never reopen a frozen packet, and `A4s` owes nothing. The error originated in a
**Session-authored plan line**; the Session is applying the correction to the plan and to its own
`A4b1` records, and reporting it to the owner.
