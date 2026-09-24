# A3a execution evidence — the one authorized strict run

**Packet:** `A3a-r25`, SHA-256 `6F907A42EAC6FD02392E11EADEF127AE840D97A245E771CD15B63A387F0E5BDC`
**Run:** `logs/runs/20260924-100502-623-a3a-codec-model`
**Selected decision row:** **`R-PASS`**
**Date:** 2026-09-24

## Outcome summary

| Criterion | Result | Evidence |
|---|---|---|
| `AC1` | **PASS** | manual review of the diff, below |
| `AC2` | **PASS** | `PASS: AC2 satisfied — …` exit 0 |
| `AC3` | **PASS** | checker prints `STRICT`; `max #N = 13473 < 100000`; `ret=0x001A7432` present |
| `AC4` | **PASS** | witness `gc=0x00000002 gs=0x00000100`; `KeConnectInterrupt: vector 6 -> routine 0x001A72E0` |
| `AC5` | **PASS** | zero `launch data page`, zero `HalReturnToFirmware` |
| `AC6` | **PASS** | `R-PASS`; class **DSP pending-word spin at `0x001A18D0`** |

**The strict stop moved for the modelled reason.** The baseline relaunched before reaching the poll;
this run reached the poll, the model satisfied it, the code after it ran far enough to connect the
vector-6 ISR, and the guest proceeded into audio DSP initialisation, where it now spins on a pending
word that nothing clears.

## `AC4` — the witness, and why it is decisive

```
[A3A] ac97 witness: gc=0x00000002 gs=0x00000100
```

Both registers are read back **through the guest mapping**, not from locals, so the line shows what
the guest sees:

- `gc` (`0xFEC0012C`) bit 1 = **set** — the guest released `Cold Reset#`;
- `gs` (`0xFEC00130`) bit 8 = **set** — the primary codec reports ready.

That is `GS.bit8 := GC.bit1` holding on real guest state. The `AC4` FAIL branch — bit 8 clear while
bit 1 set — did **not** fire, so the model wrote the register the guest reads, not merely some
address.

The witness appears **once**, by design (one-shot flag), and is emitted with a bare `fprintf` +
`fflush` **outside** the `KERNEL_LOG_ON` budget gate, so it survives truncation.

**`AC4`-POLL:**

```
[KERNEL] KeConnectInterrupt: vector 6 -> routine 0x001A72E0 context 0x010DF6D0
```

`0x001A72E0` is pushed at exactly one site in the generated tree (`recomp_0005.c:23655`), and the
line is reachable **only** after the poll returned success (`0x001A73D9 test eax,eax` / `jne`; on zero
it sets `0x88780078` and exits). Its presence therefore proves the poll succeeded.

The poll's own return also appears: `[KERNEL] #1220: ordinal 44 (slot 92) esp=0x00F7FD94 ret=0x001A7432`.

## `AC5` — the self-relaunch is gone

| Run | `HalReturnToFirmware` | launch page `0x5345000A` | `dump_ok` |
|---|---|---|---|
| `20260923-013448-357-p0-strict-baseline` (known-bad control) | 1 | 1 | `false` |
| **`20260924-100502-623-a3a-codec-model`** | **0** | **0** | **`true`** |

The baseline's log stops at `#200` with zero stall lines and relaunches. This run does not relaunch
at all and reaches `#13473`.

## `AC6` — the next stop, classified

`result.json`: `outcome = diagnostic_deadline`, `exit_code = 3` — the **collector's** own deadline
exit, paired with that outcome in `tools/harness/collect.c:325,327`. It is **not** a guest exit code.

`stacks.txt` shows a **live** frame, not a `FAULT` (zero `FAULT` frames in the file):

```
00007FF79C6604B0 sub_001A1769+0xB20  src/recomp/gen/recomp_0005.c:6749
00007FF79C660F6D sub_001A19D6+0x38D  src/recomp/gen/recomp_0005.c:7051
00007FF79C64148D sub_0019E6D7+0x37D  src/recomp/gen/recomp_0004.c:60276
00007FF79C6472CC sub_0019F05C+0x13C  src/recomp/gen/recomp_0004.c:62166
```

The reported C line `6749` is inside `loc_001A18D0`, verified against the run's own archived
`source.zip`:

```c
6748 loc_001A18D0: ;
6749     _fa = (uint32_t)(MEM32(ebx)) & 0xFFFFFFFFu; _fb = (uint32_t)(0) & 0xFFFFFFFFu;
6750     _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp MEM32(ebx), 0 (32-bit) */
6751     if (CMP_NE(_fa, _fb)) goto loc_001A18D0; /* jne: not equal / not zero */
```

**This is the packet's `DSP pending-word spin at 0x001A18D0` class exactly**: `diagnostic_deadline`
with `exit_code = 3`, a live frame in `sub_001A1769` (span `0x001A1769`–`0x001A18DE`), and no fault.
The guest is polling a memory word for a DSP acknowledgement that nothing writes.

### This is NOT the expected outcome, and it contradicts the exploratory inference

**The packet says so itself** (L725): *"This class is not expected under this packet (no
`RECOMP_APU_TRAP`), and its appearance would contradict the exploratory inference in the
next-packet table; **record it as such rather than as the expected outcome**."*

The packet's expected outcome was **`div@0x001A2BFC`** (L743). It did not occur. **The record is
therefore: a contradiction of the exploratory inference, not a confirmation of it.**

**The contradiction is now explained, and the explanation is itself the finding.** The packet's
inference was drawn from `20260922-055208-364-p5-ac97-only`, which reached the div fault — but that
run had **`RECOMP_GPU_ACK` enabled**, and this run has it **disabled**:

| Run | Profile | `RECOMP_GPU_ACK` | Outcome | Stop |
|---|---|---|---|---|
| `20260922-055208-364-p5-ac97-only` | exploratory | **enabled** | `unhandled_exception` / `3221225620` | `div` fault |
| `20260923-013448-357-p0-strict-baseline` | strict | disabled | `normal_exit` / `0` | self-relaunch |
| **`20260924-100502-623-a3a-codec-model`** | **strict** | **`0`** | `diagnostic_deadline` / `3` | **DSP spin** |

The mechanism, read from the run's own archived `source.zip` (`recomp_0005.c:6739-6751`):

```c
6739     ebx = ebx + 0x810;
6746     MEM32(ebx) = eax;                      /* the guest posts the command word */
6748 loc_001A18D0: ;
6749     _fa = (uint32_t)(MEM32(ebx)) & 0xFFFFFFFFu; ...
6751     if (CMP_NE(_fa, _fb)) goto loc_001A18D0;   /* spin until something clears +0x810 */
```

The guest posts a word at `+0x810` and spins until the **GP clears it by executing the uploaded
program**. Under `RECOMP_GPU_ACK=0` nothing executes it, so the word is never cleared. In the p5
exploratory run, synthetic GPU acknowledgement stood in for that execution and cleared it — which is
why p5 proceeded to the `div` fault.

**So the packet's "expected next stop" was an artifact of synthetic completion in the control run,
not a prediction about strict behaviour.** This is precisely the class of confusion the
strict/exploratory distinction exists to catch, and it is recorded here as a correction to the
packet's inference. It does not affect any criterion: `R-PASS` is decided by `AC1`–`AC5` plus the
named-class assignment, and the DSP-spin class is a named class in the packet's own table (L725).

**It also does not implicate the model.** The codec-presence model is what let the guest reach DSP
initialisation at all; the spin is the next subsystem's absence, and the packet's own next-packet row
(L745) already names it: *"the GP SGE engine packet: make the GP actually execute the uploaded program
so it clears `+0x810` itself."*

## `AC2` — provenance

```
PASS: AC2 satisfied -- model compiled in; no other compiled input changed in either repository;
no untracked source; classifier changed only at line 391; policy unamended; tooling pins match the packet
```

Clause-by-clause:

| Clause | Result |
|---|---|
| A — model compiled in, content differs from step 0 | `48021b2cb82b444d` → `e0e151689d217a1c` |
| B — no other compiled input changed | **NONE** |
| C — no untracked source under either `src/` | **NONE** |
| D — classifier changed only at line 391 | changed lines: **`[391]`**, count 813 → 813 |
| E — policy section byte-identical | `62a63970f806f1c1` = captured |
| F — tooling pins | `PASS: pins match the packet` |

`AC2` was evaluated **in the tree the run was built from** (trust boundary 2); the run's own
`metadata.json` identities confirm it.

## `AC1` — manual review of the model

The change is exactly what the packet specifies, and nothing more:

- **The rule is the model.** `GS(0xFEC00130).bit8 := GC(0xFEC0012C).bit1`, evaluated
  level-triggered on every worker tick, via `InterlockedOr`/`InterlockedAnd`. Not a constant, not a
  latch: the guest's own reset write drives it.
- **Unconditional.** The `RECOMP_AC97_READY` override is **deleted**; no `getenv` gates the model and
  no new switch was added. `RECOMP_AC97_READY` no longer appears in the runtime.
- **Only bit 8 is written**, and **`GC` is never written by the host** — it is read only (one site),
  so the guest's value is what drives the model. This is the property that makes it a modelled cause
  rather than a synthetic one.
- **Outside the `g_apu_mmio_trapped` gate**, deliberately, so `RECOMP_APU_TRAP` cannot silently stop
  the model.
- **Admitted under the policy.** The comment cites `docs/jsrf-run-profiles.md` §"Unconditional modeled
  hardware causes" (commit `73eee970a2d3e22a4301879e00d0125d27ee0498`), class 1 — device state from
  modeled prior state — and states the evidence path, the named falsifier, and the W1C non-modelling
  explicitly.

**`AC1` claim limit, restated because it is the honest limit of this whole packet:** the manual review
establishes that the implementation *matches the stated rule*. It does **not** establish that the rule
is faithful hardware behaviour. The rule rests on secondary sources (xemu plus Linux `intel8x0`), no
public MCPX/ACI datasheet exists, and the falsifier — write `GC` bit 1 clear and read `GS` bit 8 —
cannot occur in this title. The model is a **stated modelling assumption with a named falsifier**, not
documented device behaviour.

## Build and tests

| Check | Result |
|---|---|
| Guard before build | `ok       : True` |
| `build-jsrf.py --parallel 1` | **Build succeeded** |
| `ctest` | **12/12 passed** |
| `tests/test_run_profiles.py` | **31 tests, OK** |
| `check-generation-provenance.py --check` | `ok       : True` |
| `build-identity.py verify` | exit 0 |
| `check-dump-mapping.py` | `matches: 1   content-mismatch: 0` |

Two compile errors were found and fixed during implementation, both recorded rather than hidden:
the register `#define`s were originally placed **after** their use in the worker (moved to file scope
beside `MCPX_COUNTERS`), and a comment quoting Linux source lines contained literal `#define` and
`/*` sequences that the preprocessor tried to interpret (reworded so no comment line can be mistaken
for a directive).

## What this PASS does not establish

- That the derivation rule is faithful hardware behaviour (see `AC1` claim limits).
- That audio works. No audible output was produced or claimed; no DSP command was answered; no
  DirectSound initialisation is proven to have succeeded.
- That the DSP handshake is complete. It is **not** — the guest now spins waiting for it, which is the
  next stop.
- Liveness, strict boot, or reproducibility beyond this one run.
- That the execution environment was trustworthy beyond the stated invocation: clause F pins two
  files' bytes, not the interpreter (trust boundary 5).
- That step 0's capture truly preceded the edit: that ordering is procedural (trust boundary 4).

## Next-state evidence (recorded, not a new packet)

The measured next blocker is the **DSP pending-word spin at `0x001A18D0`**: the guest waits for a
value in a DSP command block that no emulated DSP writes. This is the same stop the packet's
next-packet table anticipated, and `RECOMP_APU_TRAP` was explicitly moved to that packet. **Per the
owner's scope fence this is recorded as next-state evidence only — it is not turned into a new packet
in this session.**
