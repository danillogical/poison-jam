# JSRF run profiles: strict and exploratory

Milestone packet **A1**. This document defines what a run is allowed to be
evidence *for*. It exists because the project has been reading `diagnostic_deadline`
and `normal_exit` as progress when neither establishes liveness, and because
several environment overrides answer a hardware poll without doing the work the
poll is asking about.

Two profiles, and a claim is only as strong as the profile that produced it.

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
| `RECOMP_GPU_ACK` | The busy-bit ack table: clears busy bits and mirrors `USER_DMA_PUT` into `USER_DMA_GET`. | Register handshakes complete with no engine behind them. Note this also gates the memory mirrors — see the register-owner gate below. |
| `RECOMP_VBLANK` | Enables the vblank source. | Assertion is not delivery; enabling it does not establish that a guest callback runs. |

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
