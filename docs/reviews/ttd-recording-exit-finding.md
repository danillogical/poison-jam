# TTD recording: the traced process exits `0xC0000409` before the horizon

**Question this answers.** Plan §7's C1 attributes the terminal slot write with a
TTD recording. The W11 ruling (`ttd-query-decision-input.md`) admitted TTD traces as
a lossless class but **not** the delivered artifact, because the traced run never
reaches the horizon. This record establishes *why*, so the next session starts from
the finding rather than re-deriving it.

**Status:** discovery finding, not a packet. It blocks C1 and hands the next Planner
a bounded subject.

## The measurement

| | |
|---|---|
| Traces | `logs/ttd/20260930-030513-184-c1-probe/`, `logs/ttd/20260930-035010-033-c1-repeat/` |
| Binary | `build/Release/jsrf_recomp.exe`, sha256 `134bda3a…` (identical in both, and to the ledger strict run) |
| Exit | `0xC0000409` (`STATUS_STACK_BUFFER_OVERRUN`) after 5765 ms / 5985 ms |
| Control | `logs/runs/20260930-034938-427-ttd-exit-control/` — **same environment, no TTD**, `outcome=diagnostic_deadline`, exit 3, `checkpoints_passed=true` |

The control is the discriminating one: the environment that dies under recording runs
to its deadline without it. **The failure is recording-induced or
recording-correlated, not a property of the build.**

## The death site (measured in both traces, identical)

Thread 0's final PC is `jsrf_recomp!body_00196967+0x40d`, guest PC **`0x00196A29`**,
with the same guest stack in both traces:

```
body_00196967+0x40d      <- the faulting PC
sub_00196967+0x6f
body_00194676+0x4cf  ->  sub_00194676+0x6f
body_00194ADD+0x8c7  ->  sub_00194ADD+0x78
body_00192090+0x3d1
```

Trace 1 stopped at `99FBB:0`, trace 2 at `99CF5:0`. Registers at death:
`edi = 0xFD000000` (the NV2A MMIO base), `esi = 0x00F7FC5C`.

The guest instruction at `0x00196A29` is, from the original XBE:

```
00196A1D mov dword ptr [edi + 0x2214], edx
00196A23 mov dword ptr [esi + 0x19c], eax
00196A29 lea ecx, [edi + 0x100214]     <- the final PC
00196A2F mov eax, dword ptr [ecx]      <- never reached
```

So the guest dies **entering an NV2A register access** (`edi + 0x100214` =
`0xFD100214`), immediately after two ordinary stores.

## Why the log stops at 577 lines, exactly

The TTD `jsrf_run.log` is substantively identical to the control's through line 577
(the only differences are paths, timestamps and thread ids). The control's **line
578** is `[RECOVERED] 0x00194520 returned; ABI verified (ESP/EBX/ESI/EDI)` — absent
from the TTD log because the trace ends first.

That is explained by the death site: `sub_00194520` is called ~190 bytes after
`0x00196A29`, so it was never entered. **Its ABI-failure assertion is therefore not
the cause**, and neither is any other abort path downstream of the death PC.

## Abort paths ruled out by measurement

TTD call queries returned **zero calls** for each of: `ucrtbase!abort`,
`kernelbase!RaiseFailFastException`, `ucrtbase!_invoke_watson`,
`kernelbase!RaiseException`, `ntdll!RtlRaiseException`,
`kernelbase!UnhandledExceptionFilter`, `ntdll!NtTerminateProcess`.

The stack cookie is intact: `__security_check_cookie` ran exactly three times and
returned normally each time; `__report_gsfailure` was never reached. The absence of
`RaiseException` also clears `recomp_icall_fail_log` and
`recomp_icall_not_code_log` (`src/recomp_manual.c`).

⇒ `0xC0000409` came from an in-process `int 29h` (`__fastfail`) that is **not a
symbolicated call**, which is why `TTD.Calls` cannot see it.

## Correction to the first reading

The first examination of the trace reported the stop "inside `nv2a_ack_thread`"
(`ntdll!NtDelayExecution` ← `jsrf_recomp!nv2a_ack_thread+0x693`). **That is a
reporting artifact.** `+0x693` is the `Sleep(0)` at `xbox_memory_layout.c:1256`, and
with `RECOMP_GPU_ACK=0` the whole acknowledgement block is skipped, so that thread
only spins. cdb showed it because **TTD reports the last-scheduled thread at the
trace tail, not the faulting one.** Recorded because the wrong reading was written
down first and would otherwise be inherited.

## The experiment, and its result: BOTH candidates refuted

The discriminating experiment was run against the existing trace. It refuted both
candidates this record originally ranked, and it moved the question.

### The mechanism is the static CRT's `abort()`, proven WITHOUT TTD

`logs/runs/20260922-190336-778-a2f-7e255-span/process.dmp` is a **non-TTD collector
minidump** carrying the identical exit code. `cdb -z <dmp>` -> `.exr -1`, `k`:

```
ExceptionAddress: 00007ffa6e78527e (ucrtbase!abort+0x4e)
   ExceptionCode: c0000409   Parameter[0]: 0000000000000007
                              Subcode: 0x7 FAST_FAIL_FATAL_APP_EXIT
ucrtbase!abort+0x4e:  cd29   int 29h
   ...  jsrf_recomp!sub_000304F0+0x19b     <- recovered.c ABI-check wrapper
   ...  jsrf_recomp!body_00025310+0x169
```

That run's log ends `[RECOVERED] ABI FAILURE 0x000304F0 ...`, which **this record
verified independently**. Two further non-TTD runs corroborate (`0x00048190`,
`0x00168480`). **The guest-emitted failfast is real and is proven without TTD**, so
the "TTD raises it on the guest's behalf" candidate is refuted as the mechanism.

### But it is NOT emitted at this trace's death PC

- `s -b jsrf_recomp!body_00196967 L600 cd 29` -> **no match**; the same for
  `sub_00196967 L200`. Only 16 `int 29h` sites exist in the image, all in static-CRT
  failfast stubs or unrelated recovered bodies; the nearest is `0x3DDDA` bytes away.
- The TTD run's `jsrf_run.log` contains **zero `ABI FAILURE` lines** and never
  mentions `0x00196967` -- **verified in this record**. Its last line is
  `[RECOVERED] 0x00196800 returned; ABI verified`.
- `dx -g @$cursession.TTD.Calls("ucrtbase!abort*")` -> **empty**: no `abort` call
  anywhere in this trace.

So the "the guest executes into a failfast in `body_00196967`" candidate is **also
refuted**. The `abort()` mechanism is real and repeatedly observed, but at *other*
PCs in older runs, not at `0x00196A29`.

### The guest instruction at `0x00196A29` never executed

Stepping the trace (`~0s`, `p`) reaches `TTD: End of trace reached` at position
`99CF6:0`. The last recorded event on thread 0 is the **not-yet-executed**
`mov eax,[r15+r10]` at `99CF5:0`. The guest PC `0x00196A29` is the lifter's *next*
guest instruction: **TTD stopped the guest before it ran.**

Stepping back from there lands in `ntdll!NtContinue` <- `ntdll!RtlCaptureContext2`
-- a **context-restore trampoline**, not a guest call chain. The "guest stack" this
record first reported is therefore not a call chain at all. That is the second
correction to the first reading.

### What remains unexplained, and it points at the recorder

Two facts are unexplained together:

1. Thread 0 is parked in a context-restore trampoline, never a normal guest path.
2. **Every** `int 29h` site is absent from the trace and no `abort` was recorded --
   yet `ucrtbase!abort+0x4e` was already fully resident in the trace process (same
   DLL base, same address as the non-TTD dump).

The natural explanation is a **memory fault on the instruction after `0x00196A29`**:
`mov eax,[ecx]` with `ecx = 0xFD100214` (`edi + 0x100214`), which is **outside the
NV2A window the game's own VEH handles** (`src/main.c:307`). The VEH would have
logged an `[EXCEPTION first-chance]` line, and **that line is absent too**.

Confirming it needs a **non-TTD capture at this stop**, which does not exist: the two
TTD traces are the only evidence for `0x00196A29`.

### Ruled out along the way

**CFG is inert**: `GuardFlags = 0x100` (CF_INSTRUMENTED only, no
`CF_FUNCTION_TABLE_PRESENT`), `GuardCFCheckFunctionPointer` -> `_guard_check_icall_nop`
(`ret 0`), dispatch -> `jmp rax`. **GS/range-check and CastGuard**: every query
empty; `__security_check_cookie` ran three times and returned normally.

## The next bounded step, and its RESULT

The step was: a **non-TTD strict run** long enough to reach the `0x00196A29` region,
with the collector's own minidump as the artifact. If the run stops at `0x00196A29`,
the minidump names the emitter directly. If it does **not** stop there, the TTD
recording is changing the guest's behaviour, and *that* becomes the finding.

### The non-TTD run passes straight through the region where TTD died

**MEASURED** on `logs/runs/20260930-053228-652-nonttd-196a29` -- same binary, same
environment (`RECOMP_GPU_ACK=0`), no TTD, 20 s bound:

| | TTD | non-TTD |
|---|---|---|
| log lines | 577 | 3226 |
| reached `0x00196967` | **no** (died before it) | **yes** -- `[RECOVERED] 0x00196967 returned; ABI verified` |
| reached `0x00194520` | **no** | **yes** -- `[RECOVERED] 0x00194520 returned; ABI verified` |
| reached `0x00196A65` | **no** | **yes** -- `[RECOVERED] 0x00196A65 returned; ABI verified` |
| `ABI FAILURE` lines | 0 | 0 |
| invalid ICALLs | 0 | 0 |
| outcome | `0xC0000409` at 5.8 s | `diagnostic_deadline`, exit 3, 25.0 s |

**This selects the finding's own second branch.** The traced run never reaches
`0x00196A29`; the untraced run passes `0x00196967`, `0x00194520` and `0x00196A65` and
runs to its deadline. The `0x00196A29` fault the first reading hypothesised **does not
occur without the recorder**, so **the TTD recording is changing the guest's
behaviour.**

That is exactly the condition the W11 ruling's reversal list names: *"Evidence that
`0xC0000409` is recording-induced and prevents any traced run from reaching the
horizon -> the class stays admitted, but **C1 via TTD is BLOCKED** and must be
re-planned."*

### Consequence for C1

**C1 cannot use TTD until a traced run reaches the horizon**, because witness W-a
(terminal-in-trace) cannot hold. The question is now *why recording changes the run*,
not *which code writes the slot*. That is a different and more tractable packet.

### What this does not establish

It does **not** name the mechanism by which recording changes behaviour. It does not
show the non-TTD run would have reached the horizon either: that run hit its own
deadline at 25 s with 0 invalid ICALLs, so it stopped for the collector's reason
rather than at a fault. A longer non-TTD run is the next bounded step.

## Two further results, and one correction to the analysis above

### The VEH-window hypothesis is REFUTED

The analysis above proposed that the guest faults on `mov eax,[ecx]` with
`ecx = 0xFD100214` because that address is **outside** the NV2A window the game's VEH
handles (`src/main.c:307`). **That is wrong**, and the arithmetic is trivial:

```
VEH window : 0xFD000000 .. 0xFE000000
address    : 0xFD100214
=> INSIDE
```

`0xFD100214` is comfortably inside the window, so the VEH would have handled the
fault rather than letting it propagate. The `[EXCEPTION first-chance]` line the
analysis predicted is therefore not expected either -- and indeed neither the TTD log
nor the non-TTD run contains **any** `[EXCEPTION` line (0 in both).

Recorded because the hypothesis was written down and would otherwise be inherited:
it was a plausible-looking address-range argument that one subtraction refutes. That
is the class of error this project's W2 premise gate exists to catch, and it is
exactly why a claim of the form "X is outside window Y" should carry the comparison
rather than the conclusion.

### A 90-second non-TTD run still does not reach the horizon

**MEASURED** on `logs/runs/20260930-053502-372-nonttd-90s` (same binary, same
environment, `RECOMP_GPU_ACK=0`, no TTD):

| | value |
|---|---|
| bound | 90 s (actual 93.9 s) |
| outcome | `diagnostic_deadline`, exit 3 |
| log lines | 4031 |
| invalid ICALLs | **0** |
| `ABI FAILURE` | 0 |
| `[EXCEPTION` | 0 |
| `[UNIMPL]` | 0 |
| kernel calls | 1002 total across 6 threads |

**This matters for C1's framing.** The strict horizon -- the thunk table being
overwritten, after which the next thunk call faults -- is **not reached in 90 seconds
of untraced execution**, whereas the earlier ledger runs reached it in roughly 5 to
8 seconds. The guest is now much further along, which is consistent with the fork
fixes (`db96e30..2a349c8`) and the V1 rebuild having moved real behaviour, and it
means the horizon framing itself deserves a fresh qualification rather than being
inherited from the 2026-09-29 runs.

**What this does not establish**: whether the horizon is *gone* or merely *later*.
Both 90 s and the 20 s control end at the collector's deadline, so they show the
guest was still running, not that it would never fault. A longer bound, or a run that
ends for a guest reason, would decide it. That is the successor packet's question and
it is bounded.


## SUPERSEDING RESULT: TTD reaches the horizon once the environment matches

**The two conclusions above about "TTD changes the guest's behaviour" are
SUPERSEDED.** They rested on a comparison against a control that was itself not on a
horizon-reachable path.

### What was wrong with the comparison

The ledger now records it (`docs/reviews/strict-horizon-ledger.md`, 2026-09-30): the
horizon reproduces **exactly** under V3's environment
(`RECOMP_GPU_ACK=0 RECOMP_APU_TRAP=1 RECOMP_KERNEL_LOG_BUDGET=100000`, 8 s) — same
return address `0x0014982E`, same slot 65, `exit_code=0xE0424943`, in 7.3 s. It does
**not** reproduce without `RECOMP_APU_TRAP=1`: 20 s and 90 s runs both reached the
collector's deadline with 0 invalid ICALLs.

`RECOMP_APU_TRAP` is **feature enablement, not a bypass** (`docs/jsrf-run-profiles.md`),
so a run with it is still STRICT, and `check-run-profile.py` agrees.

**Both TTD recordings were made with `RECOMP_GPU_ACK=0` only** — a configuration that
does not reach the horizon untraced either. So the earlier comparison was between two
runs that were both off the horizon path, and the difference between them (577 lines
versus 3226) was not evidence that recording changes guest behaviour.

### The corrected experiment

A TTD recording **with the horizon-reachable environment**
(`logs/ttd/20260930-053811-458-ttd-aputrap/`):

| | value |
|---|---|
| log lines | **23,484** (against 577 for the earlier TTD runs) |
| reached the horizon | **YES** — `[ICALL] invalid target 0x00000000 return=0014982E` |
| same site as the untraced run | **YES** — `return=0014982E`, slot 65 |
| kernel calls at the stop | 5175, ending `ordinal 294 (slot 64) ret=0x00149F5D` |
| `[EXCEPTION]` | `code=0xE0424943`, the project's own invalid-indirect-call code |
| trace size | 8192.0 MB against an 8192 MB cap — **AT the cap** |

**TTD recording does not prevent the horizon.** The traced run reaches the same
terminal site as the untraced one, with the same exit code. The two earlier TTD runs
died early because the launch lacked `RECOMP_APU_TRAP=1`, which is a launch
configuration, not a property of recording.

### What the W11 verdict says about this trace

Run through `tools/ttd/ttd-query.py` (S1–S8 evaluated mechanically):

```
W11 ADMISSION: NOT ADMITTED
  FAIL     S1_full_mode        trace is 8192.0 MB against a 8192 MB cap
  FAIL     S2_terminal_in_trace  no terminal position P supplied
  PASS     S3_coverage         29/29 aliases, none truncated, 28/28 views
  PASS     S4_last_before_P    29 alias(es) reported a last-write-before-P row
  FAIL     S5_controls         no mirror positive (W-c)
  UNKNOWN  S6_value_consistency  no P, so W-b cannot be evaluated
  PASS     S7_hash_binding
  PASS     S8_not_a_strict_run
```

The install positive is **FOUND** (`0xFE000104` written to slot 65) and the
trace-live and known-negative controls pass. **The two failures are now both
mechanical and both fixable**, which is the change this result makes:

  * **S1** — the trace hit its size cap, so the tail may be dropped. A larger
    `--max-file-mb` (or a shorter bound) removes it. The horizon is *inside* this
    trace, so the cap did not truncate the event here, but S1 cannot be *shown* to
    hold from the artifact alone.
  * **S2 / S6** — the query needs the terminal position P and the value at P, which
    `ttd-query.py` accepts as `--terminal-sequence` and `--value-at-p`. Supplying
    them turns W-a and W-b into mechanical evaluations.

**W-c (the mirror positive) remains a genuine missing control** and is the Advisor's
finding F5, unchanged: no write in this trace reached any mirror view, so a zero at a
mirror alias is still not evidence.

### Consequence for C1

**C1 is no longer blocked by "TTD cannot reach the horizon".** It is blocked by two
mechanical conditions that the next recording can satisfy:

1. record with `RECOMP_APU_TRAP=1` (so the horizon is reachable) **and** a larger
   `--max-file-mb` (so S1 holds);
2. supply the terminal position P and the value at P to the query, which requires
   reading them from the trace — the one piece of analysis the tool does not yet do
   for itself.

The C1 question itself is unchanged: which code writes the value the terminal read
sees at `[0x1C4064]`.
