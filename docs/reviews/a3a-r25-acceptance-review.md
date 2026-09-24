# A3a-r25 acceptance review — OVERALL: ACCEPT

**Packet:** `A3a`, `docs/packets/a3a-ac97-codec-model.md`
**Revision:** `A3a-r25`, frozen SHA-256 `6F907A42EAC6FD02392E11EADEF127AE840D97A245E771CD15B63A387F0E5BDC`
**Run reviewed:** `logs/runs/20260924-100502-623-a3a-codec-model`
**Reviewer role:** Acceptance reviewer (`docs/agent-workflow.md` §1)
**Reviewer child:** `27433454-8a09-4813-8f6a-7ab9564ee8bb`
**Route (MEASURED from the child's own `request/header`):** `claude` / `claude-opus-5-5` @ `low`
**Verdict:** **ACCEPT.** `AC1`–`AC6` all **`AGREED`**. **No disagreements.**
**Files changed by the reviewer:** none in either repository.

## Per-criterion results, in the reviewer's own words

| Criterion | Result | Reviewer's evidence |
|---|---|---|
| `AC1` | **AGREED** | `git -C ..\xboxrecomp diff` changes only `src/kernel/xbox_memory_layout.c`; removes the `getenv("RECOMP_AC97_READY")` gate; adds `InterlockedOr`/`InterlockedAnd`; comment states "OUTSIDE the `g_apu_mmio_trapped` gate"; `Select-String` finds no `RECOMP_AC97_READY` left |
| `AC2` | **AGREED** | `check` prints `PASS: AC2 satisfied -- …` exit 0; `pins` prints `PASS: pins match the packet` exit 0 |
| `AC3` | **AGREED** | `check-run-profile.py` prints `STRICT` (strict 1, missing 0); `max([KERNEL] #N) = 13473` < `RECOMP_KERNEL_LOG_BUDGET = 100000` (`metadata.json:42-43`); `ret=0x001A7432` at `jsrf_run.log:2932` |
| `AC4` | **AGREED** | `jsrf_run.log:2941` reads `[A3A] ac97 witness: gc=0x00000002 gs=0x00000100` — GC bit 1 and GS bit 8 both set; `jsrf_run.log:2937` has `vector 6 -> routine 0x001A72E0`; the only push of that address is `PUSH32(esp, 0x1A72E0)` at `recomp_0005.c:23655`; p5 control shows vector 6 at its L2839 |
| `AC5` | **AGREED** | neither `launch data page` nor `HalReturnToFirmware` appears in the run log; **the same search finds both in the baseline at L786 and L989** |
| `AC6` | **AGREED** | `result.json` has `outcome=diagnostic_deadline`, `exit_code=3`; `stacks.txt:21101-21102` show a live frame at `sub_001A1769+0xB20`; the only `FAULT` matches in the log are the substring inside `DEFAULT` path lines (L4610, L4619), **not** real faults |

**`DECISION ROW: R-PASS` — matches the Session.** The reviewer walked the ordered rows first-match-wins
itself: *"none of the earlier FAIL or UNKNOWN rows applies. The witness is present, there is 1
`ret=0x001A6CD2` stall line (not 1000), `AC5` holds, and the `AC3` budget is not hit."*

**`POSITIVE CONTROLS: Yes.`** The `AC5` search finds both strings in
`20260923-013448-357-p0-strict-baseline` (L786, L989). The p5 control shows vector 6 (L2839) and a
`ret=0x001A7432` kernel call (L2834). **Absence checks therefore had working positive controls.**

**`SCOPE: In scope.`** Toolkit changes only `xbox_memory_layout.c`; the game-side classifier edit is
limited to line 391 per `AC2` clause D; nothing under `src/recomp/gen/` changed. Build and archived
`jsrf_recomp.exe` both `879716046B99CECF…87EF`.

## The reviewer's one caveat — accepted, and it led to a real finding

> *"packet L725 says the DSP-spin class was not expected and its appearance contradicts the
> exploratory inference; **record it that way**."*

**The Session's evidence record said the opposite** — it called the DSP spin "the expected *successor*
stop". The reviewer was right and the packet was explicit: L725 says this class *"is not expected under
this packet … record it as such rather than as the expected outcome."* **The record has been corrected.**

**Investigating the caveat produced a finding that matters beyond this packet.** The packet's expected
outcome was `div@0x001A2BFC` (L743), inferred from the exploratory control
`20260922-055208-364-p5-ac97-only`. That run reached the `div` fault only because it had
**`RECOMP_GPU_ACK` enabled** — synthetic GPU acknowledgement cleared the DSP pending word that the
strict run now spins on:

| Run | Profile | `RECOMP_GPU_ACK` | Outcome | Stop |
|---|---|---|---|---|
| p5 | exploratory | **enabled** | `unhandled_exception`/`3221225620` | `div` fault |
| strict baseline | strict | disabled | `normal_exit`/`0` | self-relaunch |
| **A3a** | **strict** | **`0`** | `diagnostic_deadline`/`3` | **DSP spin** |

Mechanism, from the run's own `source.zip` (`recomp_0005.c:6739-6751`): the guest posts a word at
`+0x810` and spins at `loc_001A18D0` until the **GP clears it by executing the uploaded program**.
With `RECOMP_GPU_ACK=0` nothing executes it.

**So the packet's "expected next stop" was an artifact of synthetic completion in its control run, not
a prediction about strict behaviour.** That is exactly the confusion the strict/exploratory distinction
exists to catch. It does **not** affect any criterion — `R-PASS` is decided by `AC1`–`AC5` plus the
named-class assignment, and the DSP spin is a named class in the packet's own table — and it does not
implicate the model, which is what let the guest reach DSP initialisation at all.

## What this does NOT establish (reviewer's list)

- Real AC'97 codec behaviour or audio output. This is **one register-bit model**.
- Anything after the DSP spin at `0x001A18D0`. The guest is not shown to be live.
- Whether results repeat across runs — there is a **single** run.
- That `AC1` holds line by line: the reviewer *"did not read the full model body"*.
- That the other documentation edits in the working tree were declared by this packet. (The reviewer
  noted it *"did not attribute each doc change to this packet"*. The Session records that the
  unrelated modifications are pre-existing and were preserved deliberately; `AC2` reports no
  compiled-input drift, and `AC2` clause B/D/E cover the declared edits.)

## Session disposition

**ACCEPT.** No disagreements were raised, so the Planner/Advisor escalation path
(`docs/agent-workflow.md` §5, steps 5–6) was **not** invoked — it is required only when Session and
reviewer disagree.

The reviewer's caveat was accepted and acted on, and it produced a correction to the packet's
next-stop inference that is recorded in `docs/reviews/a3a-execution-evidence.md` as a **contradiction
of the exploratory inference**, not a confirmation of it.
