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

## Session verification of the ruling's load-bearing claims

Independently reproduced by the Session before recording (workflow §2.4.2):

| Claim | Check | Result |
|---|---|---|
| Ten `0xFE820010` read sites in `recomp_0005.c` | `Select-String 'MEM32\(0xFE820010u\)'` | **exactly 10**, at the ten lines the Advisor named |
| Each is a threshold re-poll loop | read the loop bodies | `L8667`: `eax &= 0xFFFFFFFC` → `cmp eax,4` → `jb loc_001A2296` (re-poll). `L14172`: `eax >>= 2` → `cmp eax,ecx` → `jb loc_001A4181`. `L19885`: same form as `L8667`. Confirmed. |
| The value is dead after the loop exits | read the exit targets | `L8667` exits to `loc_001A22A3: eax = MEM32(ebp + 8)` — `eax` immediately overwritten. `L14172` exits to `loc_001A418D: edi = 0` — `eax` unused. `L19885` exits to `loc_001A6129: MEM32(-25034484) = 1` — `eax` unused. Confirmed at the three sites sampled. |
| `apu_vp.c` returns the constant with no queue | read `mcpx_apu_vp_read` | `case NV1BA0_PIO_FREE: return 0x80; /* Always pretend queue is empty */`, and `mcpx_apu_vp_write` dispatches through `fe_method` synchronously. Confirmed. |

The Session did **not** re-verify the Advisor's full ten-site dead-after-exit audit (the
Advisor itself marks that uncertain at the three sites where a call follows the loop);
A4b's condition 3 closes it. That uncertainty is carried, not resolved.

## General clarification recorded in its owning document

Added to `docs/jsrf-run-profiles.md` §"Feature enablement — real capability, not a bypass"
as a separate policy edit, per the ruling's RECORD IN and §280-282 (a packet may cite that
section, it must not amend it). The added text is headed **"Stub dependence is judged one
wait at a time."** Documentation only; it changes no classifier behaviour.
