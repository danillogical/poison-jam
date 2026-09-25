# A4b — `AC-PIO` methodology ruling (§5.5): redesign and move to a discovery packet `A4p`

**Advisor:** Persistent advisor child `407c54a3-6ca4-4a65-835b-faf5355195cd`, route
`claude` / `claude-opus-5-5` @ `high`, marker `DSH-JSRF-ADVISOR-7Q4M2XK9`.
**Trigger:** `docs/agent-workflow.md` §5.5 — two consecutive adequacy verdicts on `A4b`
(`a4b-r2`, `a4b-r3`) found blocking defects in the **same mechanism**, `AC-PIO`'s
exit/callee handling. §5.5 forbids a third patch of the same shape.
**Consulted by:** Session `session-4a79ce06-0bd5-46f1-a760-c800ce8b62f9`.

## Ruling (verbatim)

> RULING, in brief:
> - Change both the shape and the home of AC-PIO. The r3 exit-trace rule is sound but asks for a whole-program dataflow proof, and that is the wrong shape.
> - Measure what Q1 condition 3 actually needs: does the stub value reach data inside the code that uses it? Use an explicit calling-convention premise that is checked at every boundary where it is used.
> - Take the analysis out of the A4b change packet and put it in a small static discovery packet (call it A4p) that runs first. A4b then cites A4p's result as a precondition.
> - A4b-r3 is not patched a third time.
>
> (a) How the criterion changes: a boundary convention, and what the criterion measures.
> The r3 rule treats eax/ecx/edx as possibly live across every call and ret. That is the whole-program problem, and why dead edx scratch climbs callers until the bound returns UNKNOWN. Replace it with the x86 convention as a stated premise (inferred), checked locally wherever it is relied on:
>
> C1. Volatile registers after a call. On return from any call (direct, import or indirect), remove ecx and edx from T. Remove eax too, unless the call was traced and the callee's final T contains eax.
>   Check: in the caller, on every path from the return address to the next boundary (call, ret, or jmp out), ecx and edx must not be read before they are written. A read before write means the convention is broken at that site: UNKNOWN.
>
> C2. Register arguments at a call, when ecx or edx is in T.
>   - Import: resolve the prototype as in r3.
>   - Direct call: disassemble the callee from its entry to its first boundary. If a tracked register is read before it is written, trace into the callee (one level; deeper is UNKNOWN). Otherwise it is not an argument.
>   - Indirect call with ecx or edx in T, inside the site's own function: UNKNOWN.
>
> C3. Leaving a function (ret) with T live:
>   - ecx: dead. No x86 convention returns a value in ecx.
>   - edx: dead unless it carries the high half of a 64-bit return. Check every direct caller's return address as in C1. If no caller reads edx before writing it or reaching its next boundary, edx is dead. If one does, trace into that caller.
>     If the caller then reaches its own ret with edx AND eax both unwritten since the call (a possible 64-bit pass-through), go up one more level under the same rule.
>     If the function is address-taken (its entry VA appears as an immediate or data) and edx or eax is in T: UNKNOWN.
>   - eax: it is the return value, so trace into every caller. Keep r3's caller enumeration and the 3-level bound.
>   - ebx, esi, edi or ebp in T at ret, not restored by a pop: an ABI violation, so UNKNOWN.
>
> C4. Back-edges: keep a visited set of (VA, T). Revisiting the same pair with the same T ends that path. This is r3 D3, made operative.
>
> Uses (FAIL) are the same as r3: the tracked value flows into a store (its value or its address), an MMIO store, an address computation, a branch other than the loop's own test, or a resolved import argument.
>
> Why this terminates: each site's trace is now bounded by its own function, plus one-level callee checks, plus a caller trace only for eax (or edx in the pass-through case). I checked the reviewer's first case myself:
> - Site 0x1A38F4 (edx) reaches `call 1A1BAF`. sub_001A1BAF runs push esi / mov esi,ecx / cmp / je / mov cl,[esi] / call [0x1C4004] and never reads edx. By C2, edx is not an argument. By C1, edx is dead after the call, so T becomes empty and the site PASSes.
> The reviewer's second case:
> - Site 0x1A34EA (edx) exits via ret. Its caller's return address is 1A363E, where every path reaches a boundary (call 1A0F09, or call [ecx+8]) without reading edx. By C3, edx is dead.
> - The vtable call is in the caller, where T is already empty, so it no longer forces UNKNOWN.
>
> (b) What the narrower criterion establishes, and its limits.
> It establishes: at each of the 28 direct reads of 0xFE820010 (frozen population, the r3 steps 1, 2 and 5 unchanged), the loop is a threshold re-poll, and the polled value reaches no use under C1 to C4. That discharges Q1 condition 3 as ruled: the stub gates reachability and does not shape data the guest writes.
> Claim limits, written into the criterion:
> - It depends on the premise (inferred) that DSOUND follows the standard x86 register convention: eax/ecx/edx volatile, values returned only in eax or edx:eax. C1 and C3 check this at every boundary the analysis relies on, but not everywhere.
> - Still unwitnessed (uncertain): a whole-program custom convention that returns a value in edx and passes it straight on to a later callee with no instruction touching edx in between.
> - It does not cover register-indirect or computed access beyond step 5, timing, or whether 0x80 is the true device value.
>
> (c) Where the analysis lives: a separate discovery packet, A4p, executed before A4b is promoted.
> Reasons:
> - Two consecutive reviewers had to effectively execute AC-PIO to review it. Multi-step static analysis of a binary is execution (§5.1.3). "Show all 28 terminate" cannot be known at planning time. Under §5.1.2 that makes it discovery.
> - Its result decides whether A4b's claim is even the right one (R-PIO-DATA means a PIO_FREE model is needed first). Answering that after a GPL port is the wrong order.
> A4p outline, about one page:
> - Question: are the 28 sites gate-only under C1 to C4?
> - Experiment: the r3 steps 1, 2 and 5, then C1 to C4 applied to all 28 sites, with a per-site trace table: instruction VAs, T at each boundary, and each C1/C3 check.
> - Outcomes:
>   - O-GATE: all 28 PASS, step 5 clean. A4b proceeds.
>   - O-DATA: some site has a use. A PIO_FREE-model packet comes before A4b, and the matter goes to the Advisor.
>   - O-UNKNOWN: some sites are unresolved. List them. Planner and Advisor decide whether to close each by targeted reads or to treat it as O-DATA.
> - Self-reviewed per §5.3. The acceptance reviewer re-checks row selection.
> A4b then:
> - deletes AC-PIO and R-PIO-DATA;
> - adds a precondition: A4p ACCEPTED with O-GATE, on the same XBE SHA-256 (FD190557…EF9C);
> - keeps the claim limit that cites it.
> The A4b adequacy review of the other criteria may go ahead now; promotion waits for A4p's O-GATE. The removed text and this rationale go in a4b-revision-history.md, not in the packet.
> Optional, non-deciding, not required: an exploratory run varying the stub value (0x80 against a larger value that still passes every threshold) to compare the guest's writes. It is corroboration only and can never stand in for O-GATE.
>
> (d) The B2 watch cap: confirmed.
> - The first `[GPDMA] watch` line with payload=00000000 is always emitted, is exempt from the 16-line cap (separate counter), and is fflush'd.
> - Also emit a single `[GPDMA] watch cap reached` line when the cap is hit.
> - Belt and braces: "no deciding line AND W=0" selects R-CPU only if jsrf_run.log shows the watch path was alive (at least one watch line, or the cap line) and the log is not truncated. Otherwise R-UNKNOWN.
> - It folds into the same A4b revision (§5.4 cheap fix; a different mechanism from the §5.5 one). Fold r3 deferred items D1, D2, D4 and D5 into it as well. D3 is superseded by C4, which moves to A4p.
>
> BASIS:
> - Observed: I read §5.1.2 and §5.1.3, §5.3, §5.5 and §5.8; a4b-r3-adequacy-review.md; A4b-r3 lines 181-225.
> - Observed: XBE disassembly of sub_001A1BAF at 0x1A1BAF-0x1A1BC5 (edx never read; its import slot 0x1C4004 = 0x800000A1).
> - Observed: XBE disassembly of 0x1A3630-0x1A3689 (no edx read on any path from 1A363E to its boundaries).
> - Inferred: that DSOUND (an XDK library) follows the standard MSVC x86 convention. Supported by the thiscall/fastcall/stdcall shapes seen at every boundary read so far; not proven.
> - Uncertain: whether all 28 sites terminate under C1 to C4. I checked the two cases the reviewer cited, not all 28. That is exactly A4p's job.
> - Not re-read by me this turn: a4b-r2-adequacy-review.md, and the reviewer's claims about the 19E90D and 19EE50 chain. They are no longer load-bearing, because C1 ends that path at 1A1BAF.
>
> REVERSED BY:
> - A DSOUND boundary where C1 or C3's check finds a volatile register read before it is written. The premise is then false for that code. Those sites become UNKNOWN, and if that happens widely the convention must be replaced by a trace for those sites.
> - Evidence of whole-program custom calling conventions in DSOUND. The C3 edx/eax rule then needs an explicit per-function return-register witness.
> - An owner decision to keep A4b as a single packet. The C1 to C4 criterion would then go inside A4b, but it must still be executed before the port steps.
>
> RECORD IN:
> - This ruling, verbatim with this child's ID and route, in a new docs/reviews/a4b-pio-methodology-ruling.md, cross-referenced from a4b-r3-adequacy-review.md "Next step" and from the Q1 ruling's addendum. Q1 condition 3 is now discharged by A4p.
> - The plan: A4p inserted before A4b promotion; the A4b gate updated.
> - General rule, as a separate policy edit to docs/agent-workflow.md §5.5 (or §5.1.2): "A criterion whose evaluation is itself a multi-step static or dynamic analysis that the Planner cannot complete by reading belongs in a discovery packet that runs first; the change packet cites its accepted outcome as a precondition."

## Correction appended by the Advisor (2026-09-24), after A4p executed

> **The `001A34EA` worked example's route was incomplete** (it missed `call 0x1A1BAF` at
> `001A3566`); C2 and C1 empty `T` there; **the conclusion is unchanged** (PASS).

The ruling's worked case above says C3 finds `edx` dead at the return address `001A363E`.
Executing the packet's rule literally, `T={edx}` first reaches `call 0x1A1BAF` at
`001A3566`, where C2 finds `edx` is not an argument and C1 removes it, so `T` is empty
before the `ret` and C3 is never reached. The Advisor re-disassembled the path and confirmed
the executor's route is the correct one; its own hand-trace had skipped an instruction. The
site is PASS either way, and the fall-through exit also passes.

**Related interpretation, recorded with that ruling:** in `A4p`'s E4 cross-check clause, "a
disagreement" means a different **conclusion** for the site, not a different **route** to the
same conclusion. Full ruling in `docs/reviews/a4p-execution-evidence.md`.

## Session verification of the ruling's two worked cases

The ruling's claim that C1–C4 resolves the sites that defeated r3 depends on two
disassemblies. Reproduced by the Session:

| Advisor claim | Session check | Result |
|---|---|---|
| `sub_001A1BAF` never reads `edx` | `inspect-jsrf.py disasm 0x001A1BAF 0x001A1BC6` | **Confirmed.** Body is `push esi` / `mov esi,ecx` / `cmp [esi+4],0` / `je` / `mov cl,[esi]` / `call [0x1C4004]` / `and [esi+4],0` / `pop esi` / `ret`. `edx` is never read, so by C2 it is not an argument and by C1 it is dead after the call. |
| `0x1A3630`–`0x1A3689` reads no `edx` before a boundary | `inspect-jsrf.py disasm 0x001A3630 0x001A3690` | **Confirmed.** The range's boundaries are `call 0x1A0F09` (×2), `call [ecx+8]` (×2) and `ret`; no `edx` read appears before any of them. So by C3 `edx` is dead at that return address, and the vtable call no longer forces UNKNOWN. |

Both cases the r3 reviewer cited as deterministic false-UNKNOWNs are therefore resolved by
the convention, which is what makes the redesign viable rather than a fourth patch.

## What this changes for the project

- **`A4b` loses `AC-PIO` and `R-PIO-DATA`** and gains a precondition: `A4p` ACCEPTED with
  `O-GATE` on the same XBE SHA-256 (`FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`).
- **A new discovery packet `A4p` runs first.** About one page, §6.3 form, self-reviewed per
  §5.3. It is a discovery packet, so it never satisfies a strict criterion.
- **A4b's other criteria may be adequacy-reviewed now**; promotion waits for `A4p`'s `O-GATE`.
- **Q1 condition 3 is now discharged by `A4p`**, not by `A4b`. Cross-referenced from the Q1
  ruling's addendum.
- **B2 and r3's D1/D2/D4/D5 fold into the same A4b revision** — different mechanism from the
  §5.5 one, so they are ordinary §5.4 repairs. **D3 is superseded by C4** and moves to `A4p`.
- **General rule to record** in `docs/agent-workflow.md` §5.5 (or §5.1.2): a criterion whose
  evaluation is itself a multi-step static or dynamic analysis that the Planner cannot
  complete by reading belongs in a discovery packet that runs first; the change packet cites
  its accepted outcome as a precondition.
