# A4s AC'97 hunk — Advisor ruling

**Advisor:** child `407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude` / `claude-opus-5-5` @ `high`.
**Question:** how to resolve the merge hunk where upstream keeps the base's set-only codec-ready
write *and adds* a NABM write trap, against `c97ce2c` — the accepted `A3a-r25` model.
**Verdict:** resolve the hunk to the **LOCAL** side.

## Ruling (verbatim)

> RULING, in brief: Resolve the hunk to the LOCAL side. The accepted A3a-r25 model owns GS.bit8, and the RECOMP_APU_TRAP gate stays independent. Upstream's NABM "RR" write trap is merged as code but NOT armed. Arming it is a separate change packet that must admit it as a modelled device cause.
>
> (a) Upstream's trap complements the local model on the device. It contradicts it on gating and on the shared page.
> 1. Device content: they are different registers and different waits, so the trap neither supersedes nor replaces A3a-r25.
>    - A3a-r25 models GLOB_STA (0xFEC00130) bit 8, primary codec ready, from GLOB_CNT (0xFEC0012C) bit 1.
>    - Upstream's trap models the bus-master channel control byte (NABM +0x0B of each channel, 0xFEC0010B…) bit 1, RR "Reset Registers", self-clearing.
>    - Upstream still sets codec-ready with the base's one-shot `|=`. The trap does not touch 0x130.
> 2. Gating: this is where they contradict. Upstream places the codec-ready set, the APU 512K unmap (g_apu_mmio_trapped) and ac97_arm_write_trap() all under getenv("RECOMP_AC97_READY").
>    - Taking upstream here would bring back RECOMP_AC97_READY, which jsrf-run-profiles.md:90 records as deleted synthetic completion ("this name no longer exists in the runtime").
>    - It would reduce the admitted always-on model to an opt-in set-only constant.
>    - It would re-couple the APU unmap to the codec switch, the combination local deliberately split. Local's comment cites logs/runs/20260922-055053-100-p4-ac97, where the combined switch killed the process. That would break the RECOMP_APU_TRAP semantics A4a and A4b depend on.
>    - The upstream side of this block is therefore not admissible.
> 3. Shared page: arming the trap would also change the admitted model's runtime conditions. The trap sets 0xFEC00000–0xFEC00FFF PAGE_READONLY, and that page contains GLOB_CNT and GLOB_STA.
>
> (b) Merged behaviour for A4s:
> - GS.bit8 is owned by the local level-evaluated model, kept byte-for-byte as at 0d7929c: InterlockedOr/InterlockedAnd each tick, outside the g_apu_mmio_trapped gate, with the [A3A] witness.
> - The MCPX block is local's: RECOMP_APU_TRAP gates only the APU 512K unmap. There is no getenv("RECOMP_AC97_READY") and no `|= MCPX_AC97_CODEC_READY` anywhere.
> - Upstream's ac97_clear_reset_bits, ac97_write_veh and ac97_arm_write_trap definitions may come in. Nothing may call ac97_arm_write_trap. Add a one-line comment at the definition: "not armed; enabling is a separate packet (Advisor ruling a4s-ac97-hunk)".
> - If the build treats an unused static as an error, or adds a new warning, leave the uncalled definitions out rather than add a dummy reference. Either way the post-merge grep must show zero call sites.
> - Under the unarmed trap, the InterlockedOr/And implementation remains correct. Nothing else writes the page.
> - If a later packet arms the trap, the implementation must change. Every host InterlockedOr/And on a PAGE_READONLY page would then fault once per tick and open a process-wide read-write window in the VEH. In that window a guest RR write from another thread is not trapped, which reproduces the stale cl=2 hang the trap exists to prevent.
>   - The cleaner design for that packet: evaluate GS.bit8 := GC.bit1 inside the VEH's single-step half, right after each trapped guest write, since GC changes only through guest writes. The tick thread then no longer writes the page at all.
>   - That packet must also show the external harness debugger (tools/harness/collect.c) passes the single-step exception through to the VEH.
>   - This is design guidance for that packet, not part of A4s.
>
> (c) Effect on A3a-r25's acceptance.
> - With the resolution above, no A3a-r25 criterion is reopened. The accepted change survives unchanged: this is merge mechanics that preserves the accepted code.
> - A4s must still prove the preservation, as its own regression evidence:
>   - (i) `git diff 0d7929c <merge> -- src/kernel/xbox_memory_layout.c`, restricted to the AC'97 model region, shows zero changed lines;
>   - (ii) the merged tree has zero getenv("RECOMP_AC97_READY"), zero `|= MCPX_AC97_CODEC_READY` and zero ac97_arm_write_trap( call sites;
>   - (iii) the post-merge strict smoke run shows `[A3A] ac97 witness: gc=… gs=…` with GC bit1 and GS bit8 both set (A3a AC4's witness), and a strict profile.
>   If any of these fails, the merge did not preserve the accepted work, and A4s selects its conflict/fail row.
> - Any other resolution reopens criteria:
>   - taking upstream's block reopens A3a AC1 (the gate is removed and the Interlocked model is present) and AC3 (strict profile);
>   - arming the trap requires the arming packet to re-run A3a AC4 and AC3 as regression criteria under the new binary.
>
> (d) General rule, to record: "Upstream merges never silently change admitted evidence semantics."
> 1. For every hunk, conflicting or clean, that touches a mechanism in jsrf-run-profiles.md's admitted-models table, a classifier-listed variable, or a deleted variable: the local admitted form wins, and the hunk is listed in the merge packet's inventory with that disposition.
> 2. Upstream device behaviour that is new relative to the base (a new trap, a new ack, new register semantics) enters only dormant: not armed, not default-on, and not behind a variable that means synthetic completion. Enabling it is a change packet with admission evidence under §"Unconditional modeled hardware causes", or an exploratory-profile classification.
> 3. Every merge packet greps the merged tree for all deleted and classifier-listed variable names and for arming call sites. A reintroduced name is a merge FAIL, not a warning.
> 4. Clean hunks are not exempt. A mechanical merge that applies cleanly can arm behaviour as silently as a conflict can hide it. The inventory covers device and profile-relevant hunks by content, not by conflict status.
>
> A lead for planning, not part of this ruling:
> - JSRF has exactly upstream's RR pattern (observed): 0x1A6F6F `mov byte [eax+0xFEC0010B],2`, then 0x1A6F7F `mov cl,[eax+0xFEC0010B]` / `and cl,2` / 0x1A6F88 `test cl,cl` / `jne 0x1A6F88`, a hoisted single read.
> - The same RR write also occurs at 0x1A7406.
> - Uncertain: whether current runs reach 0x1A6F6F. The A3a/A4a runs stop at the DSP spin, which is consistent with not yet reaching it.
> - If a later run stalls at 0x1A6F88, the RR self-clear becomes the admission packet above. It is not a reason to arm the trap in A4s.
>
> BASIS:
> - Observed (read myself this turn): `git show` of all three revisions.
>   - Base 051a128:1572–1595: the codec `|=` and the APU unmap under RECOMP_AC97_READY.
>   - Local 0d7929c:261–267 (defines), 664–719 (the Interlocked model outside the gate, with the witness), 1716–1740 (RECOMP_APU_TRAP-only unmap; the comment citing the p4-ac97 crash); `git grep` finds no RECOMP_AC97_READY in local src.
>   - Upstream 766ecef:325–419 (the trap; PAGE_READONLY over 0x1000 bytes from NABM offset 0x400000; clear loop over 0x10B–0x17F) and 1960–1986 (everything under RECOMP_AC97_READY; the codec `|=`; ac97_arm_write_trap()); commit 35a74ea's message.
> - Observed: jsrf-run-profiles.md lines 51, 90, 215–222, 300. a3a-r25-acceptance-review.md AC1 (the gate removed; Interlocked present; no RECOMP_AC97_READY) and AC4 (witness gc=0x2 gs=0x100).
> - Observed: XBE `find 0xFEC0010B` gives 5 DSOUND hits; the disassembly at 1A6F6F–1A6F8A and 1A7406 is as quoted.
> - Inferred:
>   - the process-wide read-write race under an armed trap with host tick writes, from the VEH code (a TLS flag, but the page protection is global);
>   - that RR is self-clearing on hardware (the AC'97 bus-master convention, as upstream's comment says). Not admitted here; it needs cited evidence in its own packet.
> - Uncertain: whether JSRF reaches the RR wait in current runs; whether the unused static definitions build cleanly under the toolkit's warning flags.
>
> REVERSED BY:
> - Evidence that A3a-r25's model is wrong in a way upstream's set-only form is not. That would be an A3a reopening on its own evidence, not a merge choice.
> - An owner decision to follow upstream's opt-in convention (§3.4 scope/fidelity).
> - Evidence that the trap must be armed for JSRF to reach the DSP spin at all, i.e. a run stalling at 0x1A6F88 before loc_001A18D0. The arming packet then becomes a precondition of A4b rather than a follow-up.
>
> RECORD IN:
> - This ruling, verbatim with this child's ID and route: append to docs/reviews/a4s-ac97-hunk-question.md as "Advisor ruling", or a new docs/reviews/a4s-ac97-hunk-ruling.md cross-referenced from it.
> - The A4s packet revision: the hunk disposition, the checks in (c)(i)–(iii), the zero-call-site grep, and the rule-(d) inventory requirement.
> - Rule (d): docs/jsrf-run-profiles.md, beside the admitted-models table (it governs admissibility), with a pointer from docs/agent-workflow.md §6.1's drafting checklist for merge packets.
> - The RR-wait lead: the plan (plan-jsrf-bare-minimum.md) as a candidate future blocker. Not a packet yet.

## Session verification of the ruling's load-bearing claims

| Claim | Check | Result |
|---|---|---|
| Local has no `RECOMP_AC97_READY` in `src` | `git grep` at `0d7929c` | **Confirmed** — the name appears only in `docs/` and `scripts/`, never in `src/` |
| The codec-ready write differs by side | `git show` of base/local/upstream, grepping `CODEC_READY`/`InterlockedOr` | **Confirmed** — base `\|=`, local `InterlockedOr`+`InterlockedAnd`, upstream `\|=` |
| Upstream adds the NABM trap | `git show 766ecef:…` grepping `AC97_NABM_OFFSET`/`ac97_arm_write_trap` | **Confirmed** — `AC97_NABM_OFFSET 0x400000`, `AC97_TRAP_BYTES 0x1000`, `ac97_clear_reset_bits`, `ac97_write_veh`, `ac97_arm_write_trap` |
| The trap and the model share a page | register arithmetic | **Confirmed** — the trap covers `0xFEC00000`–`0xFEC00FFF`; the model's `GLOB_CNT`/`GLOB_STA` are `0xFEC0012C`/`0xFEC00130`, inside it |

## Consequence for `A4s-r2`

`A4s-r2` is **ADEQUATE** (re-review `ae48fcf5`, `BLOCKING: NONE`), but its `AC-KEEP` covers only
the *presence* of `c97ce2c`'s text and ancestry. The ruling adds three specific checks
((c)(i)–(iii), including the `[A3A]` witness in the strict run), a zero-call-site grep for
`ac97_arm_write_trap(`, the hunk disposition, and a rule-(d) inventory requirement covering
**clean** hunks too. Those are **criterion changes**, which are the Planner's authority, so the
Session returned the packet for a focused `A4s-r3` revision rather than editing it.

**Also recorded per the ruling:** rule (d) goes to `docs/jsrf-run-profiles.md` beside the
admitted-models table (policy edit by the Session), and the RR-wait lead goes to the plan as a
candidate future blocker.
