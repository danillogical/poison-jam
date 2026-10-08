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
| `RECOMP_DSP_ACK`, `RECOMP_POKE`, `RECOMP_FORCE_RETURN`, `RECOMP_PAD_PRESS` | Upstream v0.12 bring-up switches (toolkit `2925f0b`); see §"Synthetic completion". | Must be absent. Presence is exploratory whatever the value, including values the runtime would parse as nothing. |
| `RECOMP_KMEM_LEGACY`, `RECOMP_NV2A_ACTIONS` | Toolkit fork fixes `db96e30..2a349c8` (2026-09-28); see §"Legacy and unadmitted behaviour". | Must be absent. Presence is exploratory whatever the value. |
| `RECOMP_GUEST_SERIAL` | Serialised guest mode (toolkit `179439b`, 2026-09-30); see §"Legacy and unadmitted behaviour". | Must be absent. Presence is exploratory whatever the value. |
| `RECOMP_NV2A_ADMIT_UNKNOWN`, `RECOMP_WORKERS`, `RECOMP_FENCE_MIRROR_LIVE` | NV2A unknown-method admission (ledger L44), the worker model, and the pre-2026-10-06 live fence mirror (L17); see §"Legacy and unadmitted behaviour". | Must be absent. Presence is exploratory whatever the value. |

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
unreachable, and for measuring what a subsystem asks for. **Never sufficient for a
fidelity claim** (that the port behaves as the hardware does), and a result obtained
this way must say so in the same sentence that states it. Bare-minimum milestones are
the exception defined next.

## Pragmatic bare minimum (owner decision, 2026-09-30)

The bare minimum is pragmatic: the project takes the path of least resistance to the
title screen and then the rest of the slice. For **bare-minimum milestones**
(`plan-jsrf-bare-minimum.md`, "Definition of done" and "Milestones"):

- An **exploratory** run may satisfy the milestone, provided every path the result relies
  on that is not *emulated* or *translated* — every synthetic-completion switch, stub,
  patch or approximation — has an entry in `docs/jsrf-compatibility-ledger.md`, and the
  run's record lists those ledger IDs. A path missing from the ledger blocks acceptance
  until it is added.
- The switch classification below is unchanged and still labels every run; it now says
  what a run *used*, not whether the milestone may count it.
- **Strict** runs remain the only evidence for fidelity claims and stay useful as a
  diagnostic (a strict run isolates what the model does without shortcuts).
- The admitted-models section still governs what may run unconditionally in the default
  (switch-free) configuration.

## Override classification

### Synthetic completion — answers a poll without doing the work

| Override | What it does | Why it cannot satisfy acceptance |
|---|---|---|
| `RECOMP_APU_DSP_ACK=<addr>[,<addr>...]` | Clears those guest dwords once per APU tick. | The title's DSP pending word goes to 0 without the GP DSP having run anything. This is the override the audit names explicitly: it makes the audio path *look* complete. |
| `RECOMP_AC97_READY` | Deleted; replaced by the always-on modeled cause listed in §"Unconditional modeled hardware causes". | Historical: it set the bit with no codec modelled behind it. The replacement models codec presence as device state, which is admitted under the always-on section; this name no longer exists in the runtime. Measured effect of the old override: the run got ~200 kernel calls further and then faulted at `0x001A2BFC` on a zero `WAVEFORMATEX`. |
| `RECOMP_GPU_ACK` | The busy-bit ack table: clears busy bits and mirrors `USER_DMA_PUT` into `USER_DMA_GET`. It is enabled by default unless its value is exactly `0`. | Register handshakes complete with no engine behind them. Strict launches must explicitly set `0`; absence is exploratory. Note this also gates the memory mirrors — see the register-owner gate below. |
| `RECOMP_DSP_ACK=<va>[,<va>...]` | Upstream v0.12 (`3332005`): zeroes up to eight guest dwords whenever they are non-zero, every worker tick. | The same shape as `RECOMP_APU_DSP_ACK`: a DSP command or pending word completes with nothing having done the work. |
| `RECOMP_POKE=<va>:<value>[,...]` | Upstream v0.12 (`3332005`): holds guest globals at fixed values. | The guest reads a value no guest code or model produced. |
| `RECOMP_FORCE_RETURN` | Upstream v0.12 (`37a21c8`): functions translated with `--force-return` answer a constant. Inert unless the generated code carries it, which JSRF's does not today. | A function's answer is replaced with a chosen constant. |
| `RECOMP_PAD_PRESS=<mask>` | Upstream v0.12 (`8da86c1`): synthesises a pulsing button press on the emulated pad. | Input no player or host device produced. A bring-up probe by its own description. |
| ~~`RECOMP_VBLANK`~~ | **Removed 2026-09-22 (A2).** It used to assert vblank by OR-ing into `NV_PCRTC_INTR_0` and `NV_PMC_INTR_0`, both of which are write-1-to-clear — so it cleared pending bits instead of setting them and could never assert anything. The vblank source is now part of the model (`nv2a_vblank_pulse` on the display clock), the guest's own W1C is the only acknowledgment, and both of the guest's enables gate delivery. There is nothing left to override. | **Removed, so it cannot satisfy acceptance at all.** Recorded here because the audit named it; there is no override left to set, and vblank delivery is now modelled rather than asserted. |

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

### Legacy and unadmitted behaviour — exploratory by presence

Added with the owner-directed toolkit fixes `db96e30..2a349c8` (2026-09-28;
`docs/jsrf-technical-record.md` §7) and later switches that change guest-visible behaviour without
modelling hardware. Some let work complete that the model does not perform (`RECOMP_NV2A_ADMIT_UNKNOWN`
advances GET past methods it does not execute), and none can support a strict claim.

| Override | What it does | Why it cannot satisfy acceptance |
|---|---|---|
| `RECOMP_KMEM_LEGACY` | Restores the kernel memory semantics the fixes replaced: `NtFreeVirtualMemory` always failing, reservation base hints ignored, commit-only calls succeeding anywhere, contiguous frees and oversized heap reuse as before. | It exists only to A/B the fix. The old semantics are known wrong, so a run with them measures the defect, not the title. |
| `RECOMP_GUEST_SERIAL` | Lets one host thread run guest code at a time and runs the GPU ISR, DPCs and timer DPCs at tick checkpoints, deferred while the guest's IRQL blocks them (compatibility ledger L34). `RECOMP_GUEST_SERIAL_TIMEOUT_MS` sets the bounded wait. | It replaces the scheduling model, and a waiter that times out runs anyway, so a run mixes serialised and concurrent execution; `[GSERIAL]` counts the overruns. A bare-minimum milestone may rely on it with L34 listed. |
| `RECOMP_NV2A_ACTIONS` | Arms NV2A behaviour the strict model otherwise lacks: semaphore release (`0x1D70`), the software-method trap (NOP with a non-zero parameter), and the `FLIP_STALL` hold. | Modelled device behaviour that has **not been admitted** under §"Unconditional modeled hardware causes"; the evidence for admission is recorded in the toolkit's `docs/technical/nv2a-action-methods.md`. Until the owner admits it (it would then become unconditional and this switch would be retired), a run with it is exploratory. |
| `RECOMP_NV2A_ADMIT_UNKNOWN` | Discovery switch (ledger L44); the runtime acts only on the exact value `1`. A method the table does not list, on a class the table knows, is captured as state and handed to the consumer instead of rejecting the walk. It is **not executed**. | The walk no longer stops at a method the strict model rejects, so the guest proceeds on a stream the strict run would not have accepted. The run record must list the admitted methods from the `[PFIFO] admit-unknown` lines, and no claim about an admitted method's behaviour follows. |
| `RECOMP_FENCE_MIRROR_LIVE` | Restores the live fence mirror (L17): the submitted fence counter is published every tick whether or not the walk consumed the commands. An A/B escape hatch for comparing with runs before 2026-10-06. | After a rejected walk the guest keeps reusing ring space the walk never read, so the run diverges from what the hardware would do the moment a walk is rejected. |
| `RECOMP_WORKERS` | `inline` runs a title's worker routines on the calling thread instead of spawning one (toolkit `src/kernel/kernel_bridge.c:699`, `:8146`). | It changes the scheduling model, and a worker that blocks waiting for requests never returns, so the run may deadlock. A bisecting tool for separating a concurrency bug from everything else, never acceptance evidence. |
| `RECOMP_NV2A_PACKET_CAP` | Diagnostic-only: sets the submission walk's packet cap (default 1024). The default is deliberately a STOP, not a yield (compatibility ledger L40), so this exists to make the cap an **experimental variable** — varying it on ONE binary is the controlled test of benign resumable chunking versus artificial starvation. | Any value changes which submissions the walk rejects, so the run measures the cap rather than the title. A run with it set is exploratory however it is spelled. |
| `RECOMP_APU_GP_INPUT_PERTURB` | `zero`, `max` or `prng[:seed]`: substitutes the values the GP DSP reads from the mixbuffer and peripheral inputs (toolkit `src/apu/apu_watch.c:145`); the toolkit logs `[GPPERTURB]`. | Substituted inputs are synthesised data, so a run measures the DSP's response to a control, not the title. Discovery instrumentation only. |

### Feature enablement — real capability, not a bypass

| Override | Effect |
|---|---|
| `RECOMP_APU_TRAP` | Routes `0xFE800000..0xFE880000` to the emulated APU. Real capability; the APU still needs a GP SGE engine. |
| `RECOMP_PB_EXEC` | Runs the pushbuffer executor. |
| `RECOMP_RASTER_TEST` | Draws one known triangle through the executor. |
| `RECOMP_USB`, `RECOMP_FMV_HOST`, `RECOMP_FB_WINDOW` | USB, FMV host decode, window. |
| `RECOMP_USB_HC`, `RECOMP_USB_NDP` | Upstream v0.12: opt-in OHCI host-controller features and port count. |
| `RECOMP_ASYNC_IO` | Upstream v0.12 (`991ff12`, `517682e`): reads on handles opened asynchronous return pending and complete later, as the console does. Both settings are model behaviour; runs with and without it are not a single-variable comparison with each other. |
| `RECOMP_KEYBOARD` | Upstream v0.12: the host keyboard stands in for a pad. Real host input, not synthesised. |
| `RECOMP_USB_PORT` | Which OHCI root-hub port the emulated pad arrives on (`0` or `1`; toolkit `src/usb/ohci.c:928`), which decides the controller slot XAPI assigns. Selects where a real device appears; synthesises no input. |
| `RECOMP_APU_MIXDOWN_ALL` | Selects the host monitor mixdown of the APU mixbins (default wide, `2` for the earlier even/odd fold; toolkit `src/apu/apu_mixdown.c:62`). Writes only the host audio monitor buffer, no guest state. |
| `RECOMP_VP` | BearddOddity pushbuffer executor (toolkit merge `a253876`): vertex-program batches are interpreted unless the value is `0`. Read only by the executor under `RECOMP_PB_EXEC`. The legacy feed required the GPU-ack gate; Architecture A (toolkit `a71f937`) uses the owner commit consumer independently of that gate. Profile classification rules are unchanged. |

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
`RECOMP_FIND_NAN`, `RECOMP_FIND_QUAD`, `RECOMP_TRAP_NULL`, `RECOMP_CMDLINE`, and from upstream
v0.12: `RECOMP_IRQL_TRACE`, `RECOMP_KEY_TRACE`, `RECOMP_INPUT_DIAG`, `RECOMP_PB_WRAP_TRACE`,
`RECOMP_FB_WINDOW_DUMP_EVERY`, `RECOMP_FB_PRESENT_DUMP_EVERY`,
`RECOMP_FB_PRESENT_DUMP_AFTER_S` (window-thread sample of published flips: a log
every N flips and at least every 10 s, a BMP when the visible image changes and
once a minute while it does not, starting AFTER_S seconds after the window thread's
first sample; no guest write), `RECOMP_WATCH`, `RECOMP_WATCH_RAW`; from the 2026-09-28 fork fixes:
`RECOMP_FFP_TRACE` and `RECOMP_TRACE_FLIP` (executor tracing), and `RECOMP_GUEST_METER` (counts host
threads inside lifted guest code; changes no guest state or scheduling); from 2026-09-30:
`RECOMP_PB_EXEC_VERBOSE` (executor logging, `nv2a_pb_exec.c:61`); from 2026-10-07:
`RECOMP_FLIP_TRACE` (budget), `RECOMP_FLIP_TRACE_FROM` (first flip) and `RECOMP_FLIP_TRACE_CHANGE`
(print only when the decision key moves) — the same-flip draw/present trace at `NV097_FLIP_STALL`,
off by default. It reads the per-frame present flags before `present_track_flip` clears them,
records the surface every `SET_SURFACE_COLOR_OFFSET` names, hashes each candidate and the published
copy, and keeps a ring of the frame's last batches. It does not choose, redirect, force a resolve,
write guest memory or alter the present call; it is a pure read plus `fprintf`, and it is
observation only.
`RECOMP_WATCHDOG_SECS` (after N seconds a watchdog thread dumps registers and recent indirect calls and exits the process, `xbox_memory_layout.c:2170`; it only ends the run); `JSRF_TRACE_A2H_DR`, `JSRF_TRACE_A2H_SLOT` and `JSRF_TRACE_A2H_SLOTW` (A2h slot/alias write witnesses; off is inert and the handler changes only the faulting thread's own single-step state); `RECOMP_APU_DMA_DESC_TRACE`, `RECOMP_APU_GP_B9_TRACE`, `RECOMP_APU_GP_DECODE` and `RECOMP_APU_PWRITE_WATCH` (GP DSP trace files, each with an optional `..._FILE` path name: `RECOMP_APU_DMA_DESC_TRACE_FILE`, `RECOMP_APU_GP_B9_TRACE_FILE`, `RECOMP_APU_PWRITE_WATCH_FILE`); and the literal `JSRF_FATAL`, which is not an environment variable but a substring the kernel bridge looks for in a created path to dump the guest stack (ledger L41); `RECOMP_RDATA_GUARD` (reports stores into read-only XBE sections and lets each complete, ledger L32)
and `RECOMP_READ_DIRECT` (reads files straight into guest memory, as before the bounce buffer, L29). `RECOMP_PB_WRAP_TRACE` is no
longer read: the executor merge replaced the wrap scan it traced.

`RECOMP_UNIMPL_TRAP` (since the 2026-09-28 regeneration, `src/recomp_manual.c`): an untranslated
instruction is always a no-op and is reported as `[UNIMPL] … REACHED`; this switch only makes the first
one abort. It can end a run earlier, never let it get further, so it is observation (fail-closed).

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

**A decision input must be lossless by construction.** Where a criterion selects a row from
something a run recorded, that record must be one that cannot drop the deciding event — a
write-once latch or an uncapped counter, written at the event by the code that performs it.
A capped, sampled, rate-limited or first-N **log** is observation only: it may corroborate,
and no row may depend on the presence or absence of such a line. Absence of a witness is
never a positive attribution; it selects `UNKNOWN` or an explicit unattributed row, never a
row that blames a specific agent. See `xboxrecomp/src/apu/GP-INTEGRATION.md`.

**A decision input must also be bounded by construction.** Its size must be fixed by a finite
universe stated and derived from source, independent of run length and input volume; it is keyed
by the property the decision classifies (a provenance class, a bin, a region), not by the identity
of individual events (an address, a page, a value). A table whose key universe is not shown finite
is observation only, and an overflow counter is a bug detector — if a record can overflow because
the run was long or busy, the key is wrong. See `xboxrecomp/src/apu/GP-INTEGRATION.md`.

### TTD trace query (W11)

**Advisor ruling, 2026-09-30.** The full text, its attribution, its basis, its reversal
conditions and the findings it rests on are in
`docs/reviews/rulings/ttd-query-decision-input.md`. This section states the rule; the
ledger states the case.

A **full-mode** TTD trace is admitted as a **lossless record class**, because it emulates
every user-mode instruction of every thread in the process from launch and therefore
cannot drop an instruction-performed store. That is a class admission, not a licence to
use any query over a trace: a query's *projection* is a separate decision input and must
satisfy the two rules above on its own. The per-alias query in `tools/ttd/ttd-query.py`
qualifies only when all of the following hold, and **any failure selects `UNKNOWN`** — it
never names a writer, and it never selects an unattributed or read-path row:

- **S1.** Full mode only: no `-ring`, trace size **below** `-maxFile`, and the `.out`
  shows a **process-exit end** with its exit code recorded. A trace ended by `ttd -stop`
  is out of scope unless the terminal position precedes the stop.

  **`Recording stopped` is a FAIL, and a size at or above the cap is a FAIL.** Measured
  (Advisor ruling 2026-09-30): a recorder cut off by its own `-maxFile` cap leaves the
  process **running**, so the process's log can carry events the trace does not. A trace
  that fails S1 has **no coverage for its tail** and therefore supports **no absence at
  all** — not even for the write class it can see. Size the cap from the T14 disk gate.
- **S2.** The trace **contains** the event under investigation, and **P is that event's
  position** — not the end of the trace. The traced run's executable hash and profile
  settings match a strict ledger run or every difference is recorded, and the event of
  interest happens before P.

  **A terminal found only in the run LOG is a FAIL.** The log is written by the process,
  not by the recorder, so a log line does not put an event inside the trace. W-a reads
  "the trace contains the strict horizon event at position P", and
  `--terminal-in-trace` asserts it explicitly rather than defaulting it true — defaulting
  it true is exactly the defect this wording replaces, where S2 passed on
  `terminal.present` read from the log while P came from `!tt 100`.
- **S3.** Every alias summary is present, none truncated, counts uncapped,
  `mapped == expected == XBOX_NUM_MIRRORS`, and the debugger exits 0.
- **S4.** The deciding write is **the last write before P**, never whichever the
  enumeration returns first.
- **S5.** Every pass evaluates the install-write positive, a **mirror** positive, a known
  negative and a trace-live control, mechanically.
- **S6.** A value-consistency witness decides between the attributed, unattributed and
  read-path rows: the value at P must equal the last write found before P. If it does not,
  an unrecorded writer (kernel, external, or an overlapping wide store) is the finding.
- **S7.** The artifact binds the trace hash, the hash of the query code, and the debugger
  version, and is **rerun** from those rather than transcribed.
- **S8.** A TTD trace is **not** an archived strict run. It may decide an attribution
  criterion; it cannot satisfy a strict boot, liveness or horizon criterion, and it cannot
  add a line to `docs/reviews/strict-horizon-ledger.md` by itself.

Two losslessness limits are stated rather than left implicit, because a query over a trace
cannot see past them: **kernel-mode writes are not instruction stores** and are not
reported by the memory query (uncertain; a control would retire it), and **`--max-hits`
makes an enumeration a first-N log**, which is observation only. An admissible projection
reports, per alias, an uncapped count and the single last write before P with its full
value — 29 fixed-shape rows, keyed by alias index.


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
| AC'97 primary-codec-ready, `GLOB_STA` bit 8 (`0xFEC00130`) | device state from modeled prior state | `docs/jsrf-technical-record.md §3` — secondary-source path, two independent sources |

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

### Upstream merges never silently change admitted evidence semantics

A merge from upstream can change what counts as admissible evidence without anyone choosing
it — a hunk can restore a deleted override, re-gate an always-on model, or arm new device
behaviour, and a **clean** hunk can do it as silently as a conflicting one. Therefore:

1. For every hunk, conflicting or clean, that touches a mechanism in this document's
   admitted-models table, a classifier-listed variable, or a deleted variable: **the local
   admitted form wins**, and the hunk is listed in the merge packet's inventory with that
   disposition.
2. Upstream device behaviour that is new relative to the merge base (a new trap, a new ack,
   new register semantics) enters **dormant only** — not armed, not on by default, and not
   behind a variable that means synthetic completion. Enabling it is a change packet with
   admission evidence under §"Unconditional modeled hardware causes", or an exploratory
   classification.
3. Every merge packet **searches the scope for the semantic form**. "Grepping the merged tree"
   means searching **SCOPE** — the build inputs of the evidence binary, named as a path list —
   for each name **in the form that carries its semantics**: environment variables as quoted
   string literals, code tokens only on non-comment lines, arming as call sites of the arming
   function. A reintroduced name is a merge **FAIL**, not a warning.
4. The inventory covers device- and profile-relevant hunks **by content, not by conflict
   status**.
5. **Evidence-semantic scope is what the evidence binary can execute.** A merge check
   1. searches only the build inputs of the executable whose runs are evidence, named as a path
      list, with a **guard that fails closed** if the build graph starts including anything else;
   2. matches each name in the form that carries its semantics (see rule 3);
   3. gives **every trigger a disposition for every hit it can produce**, including a "not
      relevant" disposition decided by a stated mechanical test.

   Mentions outside that scope or form — comments, docs, tests, unbuilt templates — are
   **inventoried, never failed**. A check whose trigger can match something it has no verdict
   for is a **defective check, not a strict one**.

Measured cost of not doing this: a merge hunk would have restored `RECOMP_AC97_READY` and
re-gated the accepted always-on codec model, and the first scoped check then produced a
false FAIL on an unbuilt scaffold and a comment
(`docs/jsrf-technical-record.md` §1, the AC'97 hunk ruling and merge-check scope).

### Claim limits

A wait satisfied by an admitted modeled cause proves **only** that the specific wait was
satisfied by that modeled cause. It does **not** by itself prove downstream device
fidelity, liveness, audio output, DSP execution, DirectSound success, or general boot
correctness. Those remain separate obligations, and a packet that relies on a listed model
must state in its own claim limits which of them it is **not** establishing.
