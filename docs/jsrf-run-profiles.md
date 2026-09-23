# JSRF run profiles: strict, exploratory and fixture

Milestone packet **A1**. This document defines what a run is allowed to be
evidence *for*. It exists because the project has been reading `diagnostic_deadline`
and `normal_exit` as progress when neither establishes liveness, and because
several environment overrides answer a hardware poll without doing the work the
poll is asking about.

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
| `RECOMP_AC97_READY` | Sets the AC'97 codec-ready bit. | The codec-ready poll succeeds with no codec. Measured effect: the run gets ~200 kernel calls further and then faults at `0x001A2BFC` on a zero `WAVEFORMATEX`. |
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
