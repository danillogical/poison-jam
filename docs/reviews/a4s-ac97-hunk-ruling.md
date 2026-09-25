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

---

# Interpretation ruling 2: scope (Advisor, same child, 2026-09-24)

**Question:** does the zero-occurrence rule ("no `getenv("RECOMP_AC97_READY")` anywhere") and the
inventory rule ("a reintroduced name is a merge FAIL") reach an **unbuilt upstream scaffold** and a
**comment**, when `AC-MERGE` (b) simultaneously requires upstream-only files to stay byte-exact?
Raised after `A4s-r3` was ruled `INADEQUATE` (`docs/reviews/a4s-r3-adequacy-review.md` B1/B2).

## Ruling (verbatim)

> RULING, in brief: The conflict is mine. My ruling said "anywhere" and "a reintroduced name" without saying what counts as a reintroduction or where to look, and a faithful executor cannot satisfy it together with AC-MERGE (b). This interpretation is binding for A4s-r4 and for rule (d) in jsrf-run-profiles.md:
> - Scope is the source that can execute in the evidence binary.
> - A deleted variable name is matched as the thing that reads it, a quoted string literal, not as text.
> - Comments, documentation, tests and unbuilt scaffolds are recorded, never FAIL.
> - HA is untouched and correct, as the reviewer found.
>
> (a) Scope boundary, mechanical: SCOPE = the toolkit paths `src/` and `include/` at M. Nothing else.
> - Why: these are the only toolkit inputs to jsrf_recomp.exe, the executable every strict run uses.
>   - The root CMakeLists.txt at 766ecef adds only `src/*` subdirectories.
>   - The game CMakeLists.txt adds the toolkit as a subdirectory and uses `${XBOXRECOMP_DIR}/include`, `/src`, `/src/kernel`, `/src/nv2a` and single .c files there.
>   - Its one reference outside those, tests/kernel_inplace_event_test.c, is a test executable, not the game.
>   - templates/, tests/, tools/, docs/ and cmake/ are outside SCOPE.
> - Boundary guard (UNKNOWN, fail closed, back to the Advisor if it trips): any `+` line in `git diff 0d7929c M -- CMakeLists.txt cmake 'src/**/CMakeLists.txt'` that names `templates/` or `tools/`, or any `+` line in the game's CMakeLists.txt that does, means SCOPE is no longer the build boundary. Existing tests/ references (src/d3d → tests/d3d8_smoke, src/kernel → tests/kernel_timestamp_test.c) are test targets and do not trip it. Only added lines count.
> - Hits outside SCOPE are listed in the AC-INV inventory with the disposition "out of scope (not built into jsrf_recomp.exe)" and a path. They never FAIL.
>
> (b) templates/new-game/src/main.c is out of SCOPE.
> - AC-MERGE (b) stands: the file stays byte-exact upstream.
> - No exception and no edit is needed, because the rule as scoped does not reach it.
> - Its three occurrences (166, 203, 318) are listed in the inventory as out of scope.
> - Defence in depth that makes this safe: the run-profile classifier treats any presence of RECOMP_AC97_READY in the environment as non-strict (jsrf-run-profiles.md:51), so even code derived from the template could not produce strict evidence with it set.
>
> (c) Comment versus live reintroduction, mechanical: an environment variable can only be read through its name as a string literal. So:
> - Deleted names: FAIL if `git grep -n -F -f <file of quoted names> M -- src include` prints anything. The file holds `"RECOMP_AC97_READY"` and `"RECOMP_VBLANK"`, each with the double quotes, one per line.
>   - The comment at xbox_memory_layout.c:409 (` * rest of RECOMP_AC97_READY: …`) has no quoted literal, so it is not a reintroduction.
>   - Record it in the inventory as "comment mention". The executor leaves it, because HA's single authored line is unchanged.
> - Classifier-listed names: the same quoted-literal form. FAIL if M has a trimmed matching line in SCOPE that 0d7929c does not.
> - Code tokens (`|= MCPX_AC97_CODEC_READY`, `\bac97_arm_write_trap\s*\(`): a hit whose trimmed line starts with `*`, `/*` or `//` is a comment hit and is recorded. Every other hit counts as code, and the AC-KEEP (v) rules apply. This errs toward false-FAIL (a trailing comment on a code line counts as code), never false-PASS.
> - Controls, run by me this turn over `src include templates/runtime tools/recomp`:
>   - known-bad 766ecef: 2 hits (kernel_bridge.c:1983 `getenv("RECOMP_VBLANK")`, xbox_memory_layout.c:1960 `getenv("RECOMP_AC97_READY")`);
>   - known-bad 051a128: 2 hits;
>   - known-good 0d7929c: 0 hits;
>   - the conflict-free tree 75083476: 0 hits.
>   - The quoted classifier names give 2 trimmed lines at 0d7929c and 2 at 75083476, with no difference.
>   - The packet should record these as its controls.
> - Limit: a name built by macro pasting or runtime concatenation escapes the grep. This is stated as a claim limit, not closed.
>
> (d) The third AC-INV disposition, with a mechanical test. Replace the free triggers with:
> 1. Inventory triggers, all within SCOPE only:
>    - T1: any (d)1 name. These are the admitted-model tokens: MCPX_AC97_*, 0x0040012C, 0x00400130, g_apu_mmio_trapped, g_mcpx_regs, MCPX_COUNTERS, nv2a_vblank_pulse, KeTickCount. Plus the quoted classifier-listed and deleted names.
>    - T2: a `+` line containing `AddVectoredExceptionHandler(`, `AddVectoredContinueHandler(`, `SetUnhandledExceptionFilter(` or `VirtualProtect(`.
> 2. For each T2 hit, the "aperture test": does the call's text, on the hit line or its continuation up to the closing `)`, contain one of these device-aperture identifiers?
>    `g_mcpx_memory g_mcpx_regs g_ac97_page g_nv2a_regs XBOX_MCPX_BASE XBOX_NV2A_BASE XBOX_FLASH_BASE XBOX_OHCI0_BASE XBOX_OHCI1_BASE AC97_ 0xFE 0xFD 0xFEC`
>    For a handler registration, the test applies to the handler function's body instead.
> 3. Dispositions:
>    - "local admitted form kept": T1 hits in the HA region, as now.
>    - "dormant": the aperture test is true, and the enclosing function has zero call sites in SCOPE (the definition line is the only hit for `\b<fn>\s*\(`). This replaces "reachable from runtime init", which is not a grep. It is conservative: any call site at all counts as armed, so no call-graph analysis is needed.
>    - "host mechanism, not device-aperture" (new): a T2 hit whose aperture test is false. Record the protected expression. Worked examples, observed at 75083476:
>      - kernel_bridge.c bridge_NtProtectVirtualMemory: `VirtualProtect(XBOX_TO_NATIVE(base_va), …)`. A guest-requested kernel service, no aperture identifier.
>      - xbox_memory_layout.c: `VirtualProtect(g_memory_base, 0x1000, PAGE_NOACCESS, …)`. The RECOMP_TRAP_NULL guard, observation-only under jsrf-run-profiles.md:187, and already present at 0d7929c:1201.
>      - win32_compat.c/.h: the compat declaration and stub of AddVectoredExceptionHandler, already present at 0d7929c.
>    - "out of scope" / "comment mention": as in (b) and (c).
>    - FAIL: an aperture-test-true T2 hit with any call site; any T1 code hit outside the HA region that changes an admitted model's text.
>    Outcome for the known cases: ac97_write_veh and ac97_arm_write_trap are dormant; everything else the reviewer listed is host-mechanism or out of scope.
> 4. New environment names. The quoted-literal comparison also finds names read at M but not at 0d7929c. Observed at 75083476:
>    - RECOMP_APU_MIXDOWN_ALL: apu_dsp.c:91, default ON, sums all 32 mixbins into the host monitor buffer;
>    - RECOMP_USB_PORT: ohci.c:819, selects which port the virtual pad appears on.
>    Record each in the inventory as "new unclassified variable". This is not a merge FAIL, provided neither name is a classifier-listed or deleted name. The follow-up is a jsrf-run-profiles.md classification edit.
>    Uncertain: whether MIXDOWN_ALL's default-on path writes anything the guest reads back. I did not verify that monitor.frame_buf is guest-invisible. If it is guest-visible, it is new default-on device behaviour under (d)2, and A4b1, which rewrites apu_dsp.c, must classify it before any strict APU claim.
>
> (e) Was the trigger list implied by (d)1? No. VirtualProtect, AddVectoredExceptionHandler and PAGE_READONLY are not (d)1 content. The packet reached for them to implement (d)2 ("a new trap … enters dormant only") and (d)4 ("by content"). That goal is legitimate, but the packet added triggers with no path scope and no disposition for non-device hits.
> - The missing path scope and the missing disposition are packet defects: a trigger needs a verdict for everything it can match.
> - The undefined terms "anywhere", "reintroduced" and "device-relevant" are ruling ambiguities, mine, resolved above.
> - Both are fixed together in A4s-r4. This is an interpretation, so it does not count toward §5.5 churn on AC-INV.
>
> (f) General rule, recorded as a new rule 5 under jsrf-run-profiles.md §"Upstream merges never silently change admitted evidence semantics":
> "Evidence-semantic scope is what the evidence binary can execute. A merge check (1) searches only the build inputs of the executable whose runs are evidence, named as a path list with a guard that fails closed if the build graph starts including anything else; (2) matches each name in the form that carries its semantics: environment variables as quoted string literals, code tokens only on non-comment lines, arming as call sites of the arming function; (3) gives every trigger a disposition for every hit it can produce, including a 'not relevant' disposition decided by a stated mechanical test. Mentions outside that scope or form — comments, docs, tests, unbuilt templates — are inventoried, never failed. A check whose trigger can match something it has no verdict for is a defective check, not a strict one."
>
> BASIS:
> - Observed (run myself this turn in the toolkit):
>   - `git grep RECOMP_AC97_READY 75083476` gives exactly xbox_memory_layout.c:409 (a comment) and templates/new-game/src/main.c:166, 203, 318 (318 is `if (getenv("RECOMP_AC97_READY")) {`).
>   - The quoted-literal controls in (c).
>   - 75083476 conflict markers at 2123/2128/2138; `\bac97_arm_write_trap\s*\(` hits the definition at :411 and the HA-hunk call at :2134 only.
>   - The root CMakeLists.txt at 75083476 has only src/* add_subdirectory calls and no templates reference. src/d3d and src/kernel CMakeLists reference tests/ only.
>   - The game CMakeLists.txt uses `${XBOXRECOMP_DIR}` include/src/src/kernel/src/nv2a plus one tests/ file for a test target.
>   - VirtualProtect `+` lines in `git diff 0d7929c 75083476 -- src include`: kernel_bridge.c NtProtectVirtualMemory, the AC'97 trap (3 lines), and the TRAP_NULL guard (also present at 0d7929c:1201). AVEH registration in SCOPE at 75083476: only xbox_memory_layout.c:420 (ac97_write_veh), plus compat decl/stub also present at 0d7929c.
>   - New getenv literals at 75083476 vs 0d7929c: RECOMP_APU_MIXDOWN_ALL (apu_dsp.c:91, default 1) and RECOMP_USB_PORT (ohci.c:819). None removed.
> - Observed: the packet's AC-KEEP (v), AC-INV and AC-MERGE (b) text; the r3 review B1/B2; jsrf-run-profiles.md lines 51, 187 and 330–352.
> - Inferred: that no build path compiles templates/new-game into jsrf_recomp.exe. This rests on the CMake reads above, not on a configured build graph, which is why the boundary guard exists.
> - Uncertain: MIXDOWN_ALL guest-visibility; name construction by macro or concatenation (stated as a limit).
>
> REVERSED BY:
> - Evidence that any file outside toolkit src/ and include/ is compiled or linked into jsrf_recomp.exe, for example a configured build's compile_commands.json listing one. SCOPE then widens to include it, and the boundary guard should have tripped.
> - Evidence that the runtime reads environment variables other than through a quoted literal in SCOPE, for example a table of names assembled at run time. The quoted-literal test is then incomplete for that mechanism.
> - Evidence that RECOMP_APU_MIXDOWN_ALL's default path changes guest-readable state. It then falls under (d)2 as armed new device behaviour, and A4s must either leave it dormant (an owner or Advisor decision on defaulting it off) or route it to R-CONFLICT.
>
> RECORD IN:
> - This ruling, verbatim with this child's ID and route, appended to docs/reviews/a4s-ac97-hunk-ruling.md as "Interpretation ruling 2: scope". Cross-reference it from docs/reviews/a4s-r3-adequacy-review.md "Advisor question raised".
> - A4s-r4: AC-KEEP (v) and AC-INV rewritten per (a)–(d), with the (c) controls and the boundary guard; the deferred items D1–D4 folded in.
> - Rule (f) as rule 5 in docs/jsrf-run-profiles.md §"Upstream merges…". Clarify rule 3 there: "greps the merged tree" means "searches SCOPE for the semantic form".
> - The two new variables go in the plan as a jsrf-run-profiles classification follow-up, and in A4b1's premise check, since A4b1 rewrites apu_dsp.c from a pre-merge baseline.
