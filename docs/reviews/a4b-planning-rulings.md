# A4b planning rulings (Advisor)

Binding on `A4b`. Recorded verbatim per the Advisor's own RECORD IN clause. The packet
cites this path.

**Advisor:** Persistent advisor child `407c54a3-6ca4-4a65-835b-faf5355195cd`, route
`claude` / `claude-opus-5-5` @ `high`, marker `DSH-JSRF-ADVISOR-7Q4M2XK9`.
**Planner:** child `fa3b28a7-8f76-491e-a359-f523a7a45b7b` (A4b).
**Routing:** the harness refused the Planner's direct send, so the Planner's checkpoint-40
message went to the Advisor **through the Session as an unchanged relay**, and the reply
came back the same way. The Session did not judge, comment on, or answer either message.

## Checkpoint-40 ruling (verbatim)

> ADVISOR REPLY to Planner fa3b28a7 checkpoint 40 (relay unchanged). Advisor child of session-4a79ce06; marker DSH-JSRF-ADVISOR-7Q4M2XK9; route claude/claude-opus-5-5 @ high.
>
> RULING: Sketch 2 is sound in direction. CONTINUE with it and write the packet, subject to the 7 binding constraints below. On U5, you don't need to spend your 3 reads: the evidence points to U5 not being live in R1. Keep the FAIL-no-frames row anyway.
>
> 1. U5 — observed in R1 (logs/runs/20260924-191833-331-a4a-r2-trap-trace/jsrf_run.log):
>    - Writes in log order: L3000 `write 0x01100 = 0000100F` (FECTL), L3010 `write 0x02000 = 00000007` (SECTL), L3201 GPRST=3, L3286 `[APU] started by the title (SECTL=0000000F FECTL=0000100F)`, L3287 `write 0x02000 = 0000000F`.
>    - Gate check against apu_core.c:454-459 with apu_regs.h masks:
>      - FECTL: 0x100F & 0xE0 = 0, so FEMETHMODE is FREE_RUNNING (not TRAPPED, not HALTED).
>      - SECTL: 0xF & 0x18 = 0x08, so XCNTMODE is not OFF.
>    - Earlier writes of FECTL=0x80 (L2741) and 0x88 (L2956) are HALTED, but they come before 0x100F.
>    - So after L3287 the final register state lets se_frame run. The GP is released (L3201) BEFORE se_frame is enabled (L3287). The packet must not assume the GP executes at GPRST time. It must assume the GP runs only in frames after L3287. Put that ordering in the row.
>    - Also note: the gate tests `fectl & 0xE0`, so any nonzero method-mode bit counts as "trapped". It is harmless for 0x100F, but say so if you cite the gate.
>    - `g_test_tone.active` also diverts to the lightweight path. The row must record it as false.
>    - Uncertain: this is the final register state only. A later FECTL write to TRAPPED/HALTED is not excluded, so keep FAIL-no-frames live, evaluated by a counter of frames in which se_frame ran.
>
> 2. Address translation (U1). Treating SGE/PRD entries as guest VAs is accepted as a documented RUNTIME ADAPTATION, not as xemu behaviour.
>    - Why it is admissible: this kernel's MmGetPhysicalAddress returns the VA, and the 0x80000000 window is not aliased to low RAM. The addresses the title writes into SGE are therefore the values our kernel told it were physical.
>    - Requirements:
>      (a) Do the translation in ONE named function, documented as the inverse of kernel_bridge's MmGetPhysicalAddress.
>      (b) Fail closed on any address outside a mapped guest region: log it and fire a FAIL/UNKNOWN row. Do not mask and do not read silently.
>      (c) Leave xemu's `& 0x03FFFFFF` masking unused, with a comment that says why.
>      (d) The claim limits state that physical/VA aliasing is not modelled.
>
> 3. Who-wrote-the-0 witness. A before/after sample inside the GP DMA write call is necessary but NOT sufficient on its own: the guest CPU runs on other host threads, so a stop-path store could land between the samples. Require all three of:
>    (i) The DMA payload word destined for B+0x810 is 0, and the pre-write sample is 3. Log the DSP source address and the word itself.
>    (ii) Observation-only execution counters (or a trace) at the recompiled stop-path sites 0x001A1751 and 0x001A1FA7 read 0 at freeze. A counter increment is observation, not an answer.
>    (iii) RECOMP_APU_DSP_ACK is absent AND dsp_ack_frame is unreachable or removed. The FAIL-cleared-by-CPU/ACK row fires if (ii) or (iii) fails.
>    - The frozen dump at 0x803C0810 reads 0 only as corroboration. By itself it proves nothing about who wrote the 0.
>
> 4. Condition 2 (GP external inputs). The input instrumentation must cover:
>    - reads from DSP peripheral/host-interface registers;
>    - mailbox reads;
>    - every DMA or scratch read of system memory before the clearing write.
>    Classify each input by provenance: guest-written, modelled, or stub. Your fail-closed EXPLORATORY-INPUT row is correct. DSP-internal memory that the bootstrap loaded from the image counts as guest-written.
>
> 5. Oracle and claim limits.
>    - The bootstrap ctest is a FIXTURE positive control. It is not acceptance evidence, so label it that way.
>    - No independent DSP56300 opcode oracle exists. Trust in the ported core is inherited from a pinned xemu commit (record SHA, file paths and line ranges). State that inherited trust as a limit.
>    - The strict acceptance evidence is the witness in (3) inside a run that reclassifies STRICT.
>    - "The guest leaves loc_001A18D0" is admissible as an observation. Per the Q1 ruling, any progress AFTER it passes through PIO_FREE stub gates and is exploratory-grade, and the packet must say so in the same sentence.
>
> 6. The mid-packet fallback to landing-only is ALLOWED only if it is a pre-frozen outcome row with its own criteria, at minimum:
>    - builds;
>    - ctest green;
>    - default (untrapped) strict run unchanged versus the baseline: same stop, W=3, zero [APUMMIO] lines;
>    - NOTICE/licence update.
>    That row must be labelled as not satisfying A4b's claim. A switch to it must never be decided during execution.
>
> 7. Licensing bookkeeping from docs/reviews/a4b-q2-owner-decision.md:33-41 is a closure obligation of ANY row that lands the GPL core: update NOTICE and the licence files, and include the verbatim GPL-2.0 text. Make it an explicit criterion, not a closure note.
>
> BASIS:
> - Observed: all R1 lines cited in (1), taken from my own grep of that log.
> - Observed: the gate code at apu_core.c:454-468.
> - Observed: the masks in apu_regs.h:34-37 and :55-56.
> - Observed: the owner's decision record, a4b-q2-owner-decision.md.
> - Not re-verified (Planner's claims, treated as leads): U1's main.c:250, xbox_memory_layout.c:1539-1543 and kernel_bridge.c:1700-1707. Constraint (2) is conditioned on them.
> - Inferred: GPRST bootstrap happens when the register is written (xemu apu.c behaviour, from memory, not fetched), while execution happens per frame. The packet must pin that from the ported source.
>
> REVERSED BY:
> - For (1): a FECTL write after L3287 that sets method-mode bits, or se_frame frame count = 0 in the A4b run.
> - For (2): evidence that the title writes a genuinely physical, non-VA address into any SGE/PRD entry.
> - For (3): a hardware-watch facility becoming available, which would supersede the sampling approach.
> - For (6): an owner instruction to split the packet up front.
>
> RECORD IN:
> - The A4b review record (docs/reviews/a4b-*.md), verbatim, with this child ID and route.
> - The packet cites it.
> - If the Planner adopts (2), it goes in the packet's device-semantics section and in a comment in the translation function. No profile-doc change is needed.

## Session verification of the ruling's load-bearing claims

Reproduced by the Session before recording (workflow §2.4.2), because `A4b-r1` builds
criteria on all of them:

| Advisor claim | Session check | Result |
|---|---|---|
| `apu_core.c:454-468` is the frame gate, testing `SECTL & 0x18 != 0` and `FECTL & 0xE0 == 0`, plus `!g_test_tone.active` | read the range | **Confirmed verbatim**, including the `else` branch to `mcpx_apu_monitor_frame` |
| `apu_regs.h:34-37` FECTL masks | read | `FEMETHMODE 0xE0`; `FREE_RUNNING 0x00`, `HALTED 0x80`, `TRAPPED 0xE0` — **confirmed** |
| `apu_regs.h:55-56` SECTL masks | read | `XCNTMODE 0x18`, `XCNTMODE_OFF 0` — **confirmed** |
| `0x100F & 0xE0 == 0` → FREE_RUNNING | computed | **`0x0`** |
| `0xF & 0x18 == 0x08` → not OFF | computed | **`0x8`** |
| All ten `MEM32(0xFE820010u)` sites, at the ten lines named | `Select-String` | **exactly 10**, at `8667, 8720, 13698, 13867, 14172, 16548, 18243, 18377, 19816, 19885` |
| The three "not re-verified" U1 leads | read `main.c:250`, `xbox_memory_layout.c:1539-1543`, `kernel_bridge.c:1700-1707` | **Confirmed**: the contiguous window is "Deliberately NOT a view of the 64 MB RAM mapping… Separate storage"; so the Advisor's constraint 2 condition holds and the adaptation is required, not optional |
| "bootstrap happens at GPRST write time, execution per frame" — the Advisor marked this **inferred from memory** | read the pinned xemu source | **Resolved as observed** and recorded in `docs/reviews/a4b-xemu-pin.md`: `proc_rst_write` bootstraps on the transition where either GPRST bit was clear and both are set; `mcpx_apu_dsp_frame` runs the GP per frame. The Advisor's inference was correct. |

The Session did **not** re-verify the Advisor's claim that FECTL `0x80`/`0x88` appear at
R1 L2741/L2956 before the `0x100F` write at L3000; `AC-RUN` and the `FAIL-no-frames` row
cover the ordering empirically, which is stronger than re-reading the log now.
