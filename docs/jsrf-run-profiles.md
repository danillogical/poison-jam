# JSRF run profiles: strict, exploratory and fixture

This document is the **permanent evidence-profile policy** for JSRF runs; it is not
tied to a current packet. It defines what a run is allowed to be evidence *for*.
`diagnostic_deadline` and `normal_exit` do not establish liveness, and several
environment overrides answer a hardware poll without doing the work the poll requests.

The runner records one of three profiles. A claim is only as strong as the
profile that produced it.

## Runner selection and archive checks

`scripts/run-jsrf.py` defaults an ordinary guest launch to `strict` and a named
`--probe` launch to `fixture`. Pass `--profile exploratory` explicitly to run an
ordinary guest with a synthetic-completion or bypass setting. A fixture requires
a named probe; an ordinary guest cannot be labeled fixture. The runner rejects a
strict request before starting any child unless the caller supplied
`RECOMP_GPU_ACK=0` exactly. It never inserts that value for strict runs.

The shared rules live in `scripts/jsrf_run_profile.py`. A new run records the
requested profile, inherited and effective relevant environment, resolved
runtime defaults, classifier/schema versions, exact collector command, source
XBE hash, copied executable/PDB/map/collector/build-source hashes, both
repository identities, and the isolated disposable save-root identity.
`scripts/check-run-profile.py` compares those hashes with the available archived
bytes, validates build-source entries and repository patch/status files, and
compares the XBE hash with the current `game/default.xbe` source. The XBE is not
duplicated in each run archive; if that source file is missing or has changed,
the run is `UNKNOWN`. The checker hashes `jsrf_run.log` and re-parses the actual
root witnesses rather than trusting metadata values alone. It recomputes the classification
from that archive and returns `UNKNOWN` for missing, contradictory, malformed,
duplicate-key or unsupported versioned metadata. It recognizes only the tested
legacy `settings` list and `settings.environment` / `settings.env` list shapes.
Legacy settings may still establish that an override made a run exploratory,
but a legacy environment that looks strict is `UNKNOWN` without a verified
requested profile, command and provenance.
An unavailable metadata file is `MISSING`. Only a reclassified `strict` run can
support strict acceptance claims; `fixture` is not a guest run.

For P0.S/P0.1 live evidence, the runner compares its recorded path with both the
game's resolved option and the post-initialization path-layer translation for
`\\Device\\Harddisk0\\Partition0`. A missing or mismatched path-layer witness
makes the run nonzero and leaves its archive `UNKNOWN`; argv alone is not proof
that the game used the selected root.

The classifier follows each runtime's actual semantics:

| Setting | Effective behavior | Strict classification |
|---|---|---|
| `RECOMP_GPU_ACK` | Enabled when absent; disabled only by exact string `0`. | Must be present as `0`. Empty, `false`, or any other value is exploratory. |
| `RECOMP_AC97_READY` | Presence forces codec-ready, including empty or `0`. | Must be absent. |
| `JSRF_ALLOW_UNRESOLVED` | Presence continues after unresolved calls, including empty or `0`. | Must be absent. |
| `JSRF_ABI_CONTINUE` | Presence continues after ABI failures, including empty or `0`. | Must be absent. |
| `RECOMP_APU_DSP_ACK` | C `strtoul(..., 0)` parses addresses; each nonzero address is cleared on APU ticks, up to eight nonzero entries. | Must be absent or parse to zero addresses. Unsupported/malformed inputs are `UNKNOWN`, never clean. |

Environment names are compared case-insensitively, as on Windows. Duplicate
spellings of one setting are `UNKNOWN`; do not convert a list of settings into a
dictionary before duplicate validation.

## Strict

No override that supplies an answer the emulated hardware did not produce. Tracing
and logging are free — they observe, they do not answer.

A strict run is the only run that can establish:

- boot, CRT and initialiser completion;
- that a device path works;
- that a wait was satisfied by a modelled cause;
- that a guest command had a semantic effect.

**A strict run may stop earlier than an exploratory one. That is the point.** A
strict run that stops sooner is not a regression against an exploratory run; the
two are not comparable.

## Exploratory

Any of the overrides below. Useful for reaching code that is otherwise
unreachable, and for measuring what a subsystem asks for. **Never sufficient for
boot, audio, GPU or liveness acceptance**, and a result obtained this way must say
so in the same sentence that states it.

## Override classification

### Synthetic completion — answers a poll without doing the work

| Override | What it does | Why it cannot satisfy acceptance |
|---|---|---|
| `RECOMP_APU_DSP_ACK=<addr>[,<addr>...]` | Clears those guest dwords once per APU tick. | The title's DSP pending word goes to 0 without the GP DSP having run anything. This is the override the audit names explicitly: it makes the audio path *look* complete. |
| `RECOMP_AC97_READY` | Deleted; replaced by the always-on modeled cause listed in §"Unconditional modeled hardware causes". | Historical: it set the bit with no codec modelled behind it. The replacement models codec presence as device state, which is admitted under the always-on section; this name no longer exists in the runtime. Measured effect of the old override: the run got ~200 kernel calls further and then faulted at `0x001A2BFC` on a zero `WAVEFORMATEX`. |
| `RECOMP_GPU_ACK` | The busy-bit ack table: clears busy bits and mirrors `USER_DMA_PUT` into `USER_DMA_GET`. It is enabled by default unless its value is exactly `0`. | Register handshakes complete with no engine behind them. Strict launches must explicitly set `0`; absence is exploratory. Note this also gates the memory mirrors — see the register-owner gate below. |
| ~~`RECOMP_VBLANK`~~ | **Removed 2026-09-22 (A2).** It used to assert vblank by OR-ing into `NV_PCRTC_INTR_0` and `NV_PMC_INTR_0`, both of which are write-1-to-clear — so it cleared pending bits instead of setting them and could never assert anything. The vblank source is now part of the model (`nv2a_vblank_pulse` on the display clock), the guest's own W1C is the only acknowledgment, and both of the guest's enables gate delivery. There is nothing left to override. |

### Retired overrides — a third category, and it is revision-relative

`RECOMP_VBLANK` is not merely absent from the current binary; it is **retired**, and
that is a stronger and more specific claim than "unknown". A name qualifies only when
**all** of these hold: it is documented as removed here; active policy still names it
as synthetic completion; it is verified inert in the current binary (source grep
returns nothing and the string is absent from the built executable); and it is
verified to have been *honored* in an archived binary. `RECOMP_VBLANK` is currently
the only such name. The registry lives in `scripts/jsrf_run_profile.py`
(`RETIRED_OVERRIDES`), one dated row with the boundary commits.

**This document is the single authority for override classification — not
`AGENTS.md`.** `AGENTS.md` is loaded automatically with a 65,536-byte budget, so it
carries a pointer here rather than a second copy of this table. A duplicated
enumeration is how `AGENTS.md` came to assert a removed override as active.

Because a retired override's effect depends on *which binary* ran, it is handled
differently at the two ends:

- **At launch — fail closed.** A strict launch carrying a retired name is refused
  before any child starts. The name cannot be evaluated against the binary being
  launched, so treating it as inert would be an assumption, not a measurement.
  Unknown-but-unread names are **not** refused: most documented overrides are
  observation or feature enablement, and a name the runtime does not read cannot
  make a run exploratory. Only the retired conjunction is rejected.
- **In an archive — resolved by recorded revision.** A retired name contributes
  `EXPLORATORY` when the archive's recorded toolkit revision could still honor it
  (descendant of the add commit, not of the removal commit), and is annotated as
  inert when that revision had already lost it. If the revision is missing or
  unreadable the run is `UNKNOWN`, never silently clean. The resolution is one
  `git merge-base --is-ancestor` test against the removal commit.

The classifier's own five-variable enumeration is unchanged by this: it encodes the
*current* binary's semantics, and a name with no runtime semantics cannot make a run
exploratory.

### Bypass — continues past a failure rather than completing work

| Override | What it does | Why it cannot satisfy acceptance |
|---|---|---|
| `JSRF_ALLOW_UNRESOLVED=1` | Continues past unresolved calls. | The plan's original text is right: do not use as acceptance evidence. Useful only to see *what lies beyond* a stop, and the `0x00700010` case showed the answer can be garbage. |
| `JSRF_ABI_CONTINUE` | Continues past an ABI contract failure. | An ABI failure means the translated body ran with a wrong stack or register contract; continuing measures the wreckage. |

### Feature enablement — real capability, not a bypass

| Override | Effect |
|---|---|
| `RECOMP_APU_TRAP` | Routes `0xFE800000..0xFE880000` to the emulated APU. Real capability; the APU still needs a GP SGE engine. |
| `RECOMP_PB_EXEC` | Runs the pushbuffer executor. |
| `RECOMP_RASTER_TEST` | Draws one known triangle through the executor. |
| `RECOMP_USB`, `RECOMP_FMV_HOST`, `RECOMP_FB_WINDOW` | USB, FMV host decode, window. |

**Enabling a feature does not turn stub answers into modelled ones.** A claim is only
as strict as the source of each value it relies on, so the run's label is necessary but
not sufficient. Enabling a capability can put a *stub* in the path of a read the guest
depends on — a constant returned by an unimplemented block is not device behaviour, and
progress the guest makes because of it is at most exploratory however the run
classifies. Two consequences:

- A claim must be scoped to what actually answered each value. "The guest wrote X then
  read Y then stopped at Z" stays strict; "the device path works" and "the wait was
  satisfied by a modelled cause" do not, if a stub produced the value that let the guest
  continue.
- A capability that changes several things at once is not a single-variable comparison.
  `RECOMP_APU_TRAP` instantiates the APU, replaces the `0xFE820010` tick-loop counter
  with a constant served by the VP register map, and answers GP/EP reads with zero. A
  difference between a trapped and an untrapped run must not be attributed to any one of
  those without a measurement that separates them.

Where a capability's stub answers are load-bearing for a packet, the packet states them
in its claim limits. This rule is general and prospective; it changes no classification
the classifier computes.

**Stub dependence is judged one wait at a time.** A stub that only gates *whether* the
guest reaches a point limits reachability and liveness claims, not the provenance of a
later value produced by a model. Concretely: if a polled stub value is used only to decide
when a wait-for-space loop exits, and is dead once the loop exits, then the stub determines
*when* the guest arrives — it does not determine the data the guest later acts on. A claim
about a value the guest itself supplies, or that a model produces, stays strict for that
value even though the guest passed through stub gates to get there. The claims it does
limit are stated in the packet's own claim limits: the guest's arrival, and everything
after it, is exploratory-grade, and no boot-progress or liveness claim follows from it.

Where a stub gates reachability of a wait whose *own* value is later relied on, the stub
still has to be resolved before any strict claim about reaching beyond that point. Such a
model is its own packet, placed before the first criterion that needs it.

### Observation only — no semantic effect

`RECOMP_MMIO_TRACE`, `RECOMP_PFIFO_TRACE`, `RECOMP_NV2A_TRACE`, `RECOMP_PB_SCAN`,
`RECOMP_PB_UNHANDLED_ALL`, `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_KERNEL_WATCH*`,
`RECOMP_TRACE_*`, `RECOMP_PEEK*`, `RECOMP_FB_DUMP`, `RECOMP_FB_VA`, `RECOMP_TEX_*`,
`RECOMP_FMV_DUMP`, `RECOMP_CS_*`, `RECOMP_USB_TRACE`, `RECOMP_APU_TRACE`,
`RECOMP_FIND_NAN`, `RECOMP_FIND_QUAD`, `RECOMP_TRAP_NULL`, `RECOMP_CMDLINE`.

`RECOMP_KERNEL_LOG_BUDGET` is observation but changes conclusions anyway: the
default truncates the log, and a truncated log reads as a hang. Use 100000.

## Two traps that have already cost sessions

**A gate is not a bypass.** `xbox_Nv2aClaimRegisterOwner()` clears
`g_nv2a_ack_enabled`, and the whole ack-thread body sat inside that flag — so
taking the MMIO aperture silently stopped the ticks that write *guest memory*,
which have nothing to do with register ownership. The gate was correct for the
register mutations and wrong for the mirrors. When adding a gate, name which
class of writes it is for.

**`diagnostic_deadline` means the capture was bounded; it does not mean the guest
was live.** It says the collector reached its deadline and dumped. Liveness
requires a separate semantic criterion — a value that advanced for a modelled
reason, or a named checkpoint reached. `normal_exit` is the mirror image: the
guest returned from its entry point, which is not the same as the guest being
satisfied. Both are read as progress far too easily.

## Evidence revision

A claim must link to an artifact produced by the source it describes. The archived
run `logs/runs/20260922-110738-253-nv2a-1bcc/` records toolkit `cf03f46`, which is
**not** the reviewed source — it is not a validation of the current tree. Where a
claim borrows a newer revision than its artifact, say so or re-run.

## Unconditional modeled hardware causes

Strict evidence needs an answer the emulated hardware really produces. Some of that
behaviour is **autonomous**: real hardware produces it by itself, with no guest action and
no host shortcut. Modelling it is not an override, because there is no poll the model
answers *instead of* the hardware — the model **is** the hardware's own behaviour.

An **unconditional modeled cause** may support strict evidence when it represents
behaviour that real hardware produces autonomously and is supported by the evidence rule
below.

### Evidence rule

A modelled cause requires **either**:

- one credible **primary** hardware source — a datasheet or vendor specification for the
  relevant device; **or**
- at least **two independent corroborating secondary sources of meaningfully different
  provenance** — for example a reference emulator's device model plus an operating-system
  driver for hardware implementing the same register interface.

Under the secondary-source path, all of these are required:

- at least one source is specific to **Xbox/MCPX**, or to a demonstrably relevant
  implementation of the same interface;
- the sources are **independent of our own `xboxrecomp` implementation**. Our own code,
  comments, prior decisions and accepted packets are not sources — an implementation
  cannot corroborate itself;
- the exact **source, version, commit and line** evidence is recorded durably;
- **observed guest behaviour may corroborate an interpretation but does not count as one
  of the two independent sources.** Guest code is evidence about the title, not about the
  hardware.

Where no usable primary source is publicly available for the relevant device, the
secondary-source path is the operative one. That is a **recorded limitation, not a
waiver**: the corroboration requirement is what stands in for the datasheet, so a model
admitted this way must say so in its own record.

### Admission criteria

All four are required.

1. **Unconditional.** Always active. Not enabled by an environment variable, a diagnostic
   switch, a build option or a command-line flag. A model behind any switch is an override
   and is classified as one under the tables below.
2. **Grounded in cited hardware/device behaviour**, per the evidence rule above.
3. **Limited to the modeled state or event itself.** It writes only the register fields it
   models and touches nothing else.
4. **Incapable of standing in for actual work** whose resulting data or side effects the
   guest later consumes. If the guest reads back a *result* of computation, the model must
   not supply that result.

### Allowed classes

1. **Device state resulting truthfully from modeled prior state** — a register field whose
   value follows from state the model already tracks, including state the guest itself
   wrote.
2. **Autonomous clocks and counters** — a value that advances on its own on real hardware.
3. **Periodic modeled device events** — an event the device generates on its own schedule.

### This is not synthetic completion

The distinction is what the model is standing in for. **Synthetic completion answers a poll
without doing the work the poll requests** — `RECOMP_APU_DSP_ACK` clears the title's DSP
pending word although no DSP ran, so the guest proceeds on a result that does not exist.
An admitted modeled cause supplies **state the hardware presents on its own**, and there is
no computation behind it whose output the guest later consumes. If a candidate model
supplies a *result of work*, it is synthetic completion however it is written.

### Listed models

| Model | Class | Basis |
|---|---|---|
| `KeTickCount` advance (kernel/APU clock worker) | autonomous clock | pre-existing accepted practice; reasoning in its source comment |
| MCPX APU GP sample counter `0xFE820010` (`MCPX_COUNTERS`) | autonomous counter | pre-existing accepted practice; reasoning in its source comment |
| NV2A vblank pulse (`nv2a_vblank_pulse`, display clock) | periodic device event | pre-existing accepted practice; the guest's own W1C is the only acknowledgment |
| AC'97 primary-codec-ready, `GLOB_STA` bit 8 (`0xFEC00130`) | device state from modeled prior state | `docs/reviews/ac97-codec-ready-evidence.md` — secondary-source path, two independent sources |

The first three rows are recorded for **classification continuity**: they predate this
section and were already accepted as always-on models. Their listed basis is their existing
status and source comments, **not** evidence gathered under the rule above, and **they have
not been re-adjudicated against it**. Two consequences follow, and are stated rather than
left implicit:

- **Their eligibility for strict evidence is unchanged by this section.** They were accepted
  before it and remain accepted; this section neither re-grants nor withdraws it. Formally
  re-adjudicating them is an owner decision and is **not** done here.
- **At least one would not satisfy criterion 1 as written.** The APU GP sample counter's tick
  loop sits inside `if (g_mcpx_regs && !g_apu_mmio_trapped)` (`xbox_memory_layout.c:652`), so
  `RECOMP_APU_TRAP` switches it off — it is not unconditional in that configuration. That is a
  **known discrepancy between a grandfathered row and the criteria**, recorded so it is not
  mistaken for a rule the row satisfies. It does not affect the AC'97 row below, which is
  unconditional.

The AC'97 row is the first model **admitted under this section**. It is admitted by owner
decision on the recorded secondary-source evidence, and it is **prospectively listed**: the
admission is a statement that the model is admissible as strict evidence, not a claim that
it is implemented. Its implementation is the subject of a packet, which must still satisfy
its own adequacy and acceptance rules.

**Any model added from now on must meet the evidence rule and record its sources.** A packet
may **cite** this section; it must not amend its criteria or this table to suit itself. An
admission recorded here is a policy determination about *evidence eligibility* only — it
does not accept any packet, and it does not establish that any wait has actually been
satisfied.

### Claim limits

A wait satisfied by an admitted modeled cause proves **only** that the specific wait was
satisfied by that modeled cause. It does **not** by itself prove downstream device
fidelity, liveness, audio output, DSP execution, DirectSound success, or general boot
correctness. Those remain separate obligations, and a packet that relies on a listed model
must state in its own claim limits which of them it is **not** establishing.
