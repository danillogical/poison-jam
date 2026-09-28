# `A2h-oom-causal-slice-r1` execution evidence — **`O-OPEN`**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a2h-oom-causal-slice.md`, revision **`A2h-oom-causal-slice-r1`**, frozen SHA-256
**`E9CDB1B39066CE5F5626FF74246E5EAD34C1612BC31BE622341B50FCC541108C`** — verified before execution, **not
edited**. **Class:** discovery (§5.8).

**Selected row: `O-OPEN`** — *"Any unbound edge/branch, incompatible witnesses, … or no reproduced failing
invocation in the one bounded attempt."* **No new guest run was needed or performed**, so the packet's step 3
and step 4 (instrumentation and the one bounded 8-second capture) were **not** entered.

---

## Identity and provenance

| Item | Value |
|---|---|
| Game revision | `b3bf2c1cf06e43a2f70dfae0be9ae74c53577ef3`, clean at execution |
| Toolkit revision | `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`, clean — **unchanged; no toolkit write** |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — **matches** |
| **New guest runs** | **NONE** — the answer was obtained offline from archived evidence |
| **Instrumentation** | **NONE added**, so nothing exists to enable or disable at closure |
| **Toolkit / runtime changes** | **NONE** |

### Deliverables (the packet's own, now checked in)

| Artifact | Role |
|---|---|
| `scripts/a2h-oom-slice.py` | The required checked-in parser/binder **and** the instruction-boundary verifier |
| `scripts/test_a2h_oom_slice.py` | Its tests — **31 tests, OK** |
| `docs/reviews/a2h-oom-causal-slice-binding.json` | The parser's deterministic output |

**Commands, exactly as run:**

```powershell
python -X utf8 -m unittest scripts.test_a2h_oom_slice
python -X utf8 scripts\a2h-oom-slice.py --log logs/runs/20260927-160330-655-a4b2-gp-trap-trace/jsrf_run.log --log logs/runs/20260922-224429-003-a2g-304f0-span/jsrf_run.log --verify 0x00149E24:8345dc20 --verify 0x00149E4A:ff15883f1c00 --verify 0x00149FB3:0fb707 --verify 0x0014980E:8945dc --out docs/reviews/a2h-oom-causal-slice-binding.json
```

> ### Acceptance stage 1 returned `NOT ACCEPTED` on two criteria — both are now fixed
>
> The reviewer reproduced **every** load-bearing measurement independently, including writing **its own
> capstone CFG** to test the dominance claim, and it **confirmed** the producer formula, the dominance
> result, the `movzx` non-reachability, the frameless function, the arena behaviour, the identity, and that
> **`O-OPEN` was correctly selected with an honest closure account**. It then found **two real defects**:
>
> **1. A required parser fixture was missing.** The packet names a *"mid-instruction disassembly start that
> is REJECTED"* fixture; it existed **only in a docstring**, because the tool had **no disassembly surface at
> all**. **Fixed:** `a2h-oom-slice.py` now exposes `verify_instruction` and a `--verify VA:BYTES` CLI, which
> rejects a misaligned start, wrong bytes, a length mismatch, malformed bytes and out-of-section VAs. **Nine
> new tests** cover it, against both a synthetic XBE and the **real** XBE. **31 tests, OK.**
>
> **2. The "no-trap" characterisation of A2g was FALSE.** Recorded in full in Experiment 1 above. **Fixed:**
> both records are corrected and the strong claim is **withdrawn**.
>
> **Both defects were mine, and both were caught by the acceptance stage doing exactly its job.** The second
> is the more instructive: I read `run_profile.effective_settings`, found it empty, and reported the setting
> as **"ABSENT"** — **reading an absent record as a negative measurement**, the same error class as the
> earlier `[GP*]`-zeros mistake and the byte-width writer census. **A census reading all plausible locations
> now shows all 35 archived runs with this request are trapped.**

## Experiment 1 — archive-first binding

| Check | R1 (trapped failing) | A2g (historical) |
|---|---|---|
| `check-run-profile.py` | **STRICT** | **EXPLORATORY** — so it is used **only as historical corroboration**, never as strict validation |
| `check-dump-mapping.py` | `CONTENT_MISMATCH` — structurally readable; **no shifted-offset substitution used** | `CONTENT_MISMATCH` — same |
| exe | `bc8e288dd54d…` | `2cd0472a256e9d…` — **a different build, five days earlier** |
| **`RECOMP_APU_TRAP`** | **`1`** | **`1`** — **BOTH RUNS ARE TRAPPED** (see the correction below) |
| outcome | `unhandled_exception`, 4.77 s | `unhandled_exception`, 4.95 s |
| log SHA-256 | `c52d71655c48c752ba275b3361d9abbafd7a8dea5de14cc70b26ad27c416c874` | `b9631ee5af44b40213f84c8645bdc6fb31f23a7462b41866ca9cfbe5f1176a04` |

> ### ⚠ CORRECTION — I wrongly reported A2g as "no-trap", and the strong claim does not survive
>
> **The acceptance reviewer caught this and it is a real error.** My binding script read
> `run_profile.effective_settings`, which is **empty** for the A2g run, and reported
> `RECOMP_APU_TRAP` as **"ABSENT"**. The value actually lives in `metadata.json`'s top-level
> **`settings`** dict:
>
> ```
> settings.RECOMP_APU_TRAP      = 1
> settings.RECOMP_APU_DSP_ACK   = 0x803C0810
> ```
>
> **and the A2g log's own line 26 says `APU: 0xFE800000..0xFE880000 trapped for MMIO`.**
>
> **So A2g IS trapped**, and my claim *"the trap is not a necessary cause"* is **NOT established by this
> archive pair — because both runs are trapped.** Re-censused across **every** archived run carrying the
> `598869040` request, reading the trap state from **all** plausible locations:
>
> | Quantity | Value |
> |---|---|
> | runs with the request | **35** |
> | log says the APU was **trapped** | **35** |
> | log does **not** say trapped | **0** |
>
> **There is no untrapped run with this request anywhere in the archive.**
>
> **The error class is the one this project keeps producing: reading an absent record as a negative
> measurement.** The value was in a different key, and I concluded "absent". It is the same shape as the
> earlier `[GP*]`-zeros mistake and the byte-width writer census. **The reviewer's independent check is what
> caught it, which is the acceptance stage working exactly as designed.**
>
> **What survives, in the narrower and still useful form:** the OOM **predates the A4b2 work** — it appears
> on a **different exe five days earlier** with the same size, same type, same OOM tuple and same terminal
> ICALL. **So it was not introduced by whatever A4b2 changed.** That is a real finding and it is what the
> evidence supports. **The strong trap-necessity claim is withdrawn.**

**The XBE hash matches in both archives and on disk.** *(A Session check reported a false mismatch by
comparing a lowercase computed digest against an uppercase expected string; case-normalised, it matches.)*

### The causal chain is **semantically identical** across two runs five days and one build apart

| Field | R1 | A2g | Same |
|---|---|---|---|
| invocation count | 94 | 94 | ✓ |
| failing index | **93** | **93** | ✓ |
| failing call site (`ret`) | `0x00149E50` | `0x00149E50` | ✓ |
| failing `esp` | `0x00F7FCF0` | `0x00F7FCF0` | ✓ |
| `base` / `size` / `type` | `0x0` / **598869040** / `0x801000` | `0x0` / **598869040** / `0x801000` | ✓ |
| OOM tuple | `(598869040, 12715008, 50855936)` | identical | ✓ |
| ICALL `(target, esp, return)` | `(0x0, 00F7FD00, 0014982E)` | identical | ✓ |

**This is the decisive answer to the packet's premise question, in its NARROWER form: the OOM predates the
A4b2 work.** A run on a **different build five days earlier** reproduces the chain **byte-for-byte in every
semantic field**. **So the failure was not introduced by whatever A4b2 changed** — it is older than the
trap-era work that made it reachable sooner.

> **The stronger claim — "the trap is not a necessary cause" — is WITHDRAWN** (see the correction in
> Experiment 1). **Both runs are trapped**, and **all 35 archived runs carrying this request are trapped**,
> so this archive pair cannot establish trap-necessity either way. **What the pair does establish is
> build-independence across a five-day gap**, which is what is claimed above.

> **Parser fix found by its own tests.** The first comparison compared whole invocation dicts, which include
> the **byte offset into each log file** — necessarily different between runs — and so reported a spurious
> difference. The tests caught it (`test_byte_offset_difference_does_not_report_a_semantic_difference`) and
> the comparison is now **semantic** over `(index, esp, ret, base, size, type)`. **This is exactly the class
> of error the packet's checked-in-parser requirement exists to prevent**, and the requirement earned its
> keep on the first execution.

## Experiment 2 — the backward demand slice **CLOSES**: the producer is the function's 3rd argument

> ### ⚠ This section SUPERSEDES the `O-OPEN` verdict below
>
> After the `O-OPEN` row was recorded, a **bounded dominance analysis** over the enclosing function's CFG
> closed the leaf. **The row for this packet was selected as `O-OPEN` on the evidence then available, and
> that selection stands as recorded** — but the missing witness has since been identified, so the
> *substantive* answer is now known. Both are reported, in order.

### The bounded question that closed it

Instead of forward enumeration, the Session asked a **dominance** question over one function's CFG:

> Does **every** path from the function entry to the call at `0x00149E4A` pass through a write to
> `[ebp-0x24]`?

**Method:** decode the enclosing region (`0x00149000`–`0x0014A400`, 1 609 instructions), build the CFG,
reverse it, and BFS from the call. Then re-run reachability with **all writer nodes removed**.

| Step | Result |
|---|---|
| Instructions that can reach the call | **46** |
| Writers to `[ebp-0x24]` among them | **2** — `0x0014980E` and `0x00149E24` |
| Is the `movzx` writer (`0x00149FB6`) among them? | **NO** — it does not reach the call at all |
| Candidate function entry reaching the call | **1** — `0x001497DC` |
| **Call reachable with all writers removed?** | **FALSE** |

**The dominance result was re-derived by a second, independent method** (a separate predecessor
construction rather than the linear-sweep edge build), because this project has twice been burned by
linear-disassembly assumptions:

| Quantity | Method A | Method B | Agree? |
|---|---|---|---|
| instructions reaching the call | **46** | **46** | ✓ |
| writers to `[ebp-0x24]` among them | **2** (`0x0014980E`, `0x00149E24`) | **2** (same) | ✓ |
| producer `0x0014980E` present | yes | yes | ✓ |
| candidate entries reaching the call | **1** (`0x001497DC`) | **1** (`0x001497DC`) | ✓ |
| call reachable with writers removed | **FALSE** | **FALSE** | ✓ |

**And an alignment check that matters here:** **zero** `.byte` instructions decode within `0x400` of the
call, so the sweep is instruction-aligned across the region the conclusion depends on. **The result is
robust to the failure mode that has produced plausible garbage before.**

> **Counting conventions, so nobody re-litigates them (reviewer advisory).** The reviewer's independent CFG
> reports **47** reaching instructions against my **46**, and **1 607** decoded against my **1 609**. Both
> differences are fully explained: one conditional node is counted differently, and two undecodable `.byte`
> entries at `0x0014A3FE`/`0x0014A3FF` are included by one sweep and not the other. **There is no
> disagreement about reachability, the writer set, or the entry.**
>
> **And one precision the reviewer supplied:** `0x00149E24` is a **read-modify-write**, not a pure writer —
> `add [ebp-0x24], 0x20` reads the slot as well as writing it. That is consistent with, and actually
> reinforces, the dominance result: the `add` **consumes** whatever the producer wrote, so a path reaching
> the `add` must have passed a value-producing write.

**The reviewer also noted its own CFG treats calls as fall-through, which is conservative** — it can only
*add* paths, never remove them. So the unreachability result holds *a fortiori* under a stricter
call-modelling. **A future caller-tracing packet should model calls explicitly.**

**So every path to the call passes a write, and the only *value-producing* writer on any reaching path is
`0x0014980E`.** The `movzx` writer I had been chasing **cannot reach the call at all** — which explains why
its 16-bit bound was irrelevant.

### The producer, and it reproduces both observed sizes exactly

```
00149800  mov   eax, dword ptr [ebp + 0x10]   ; <-- the function's 3rd stack argument
00149803  test  eax, eax
00149805  jne   0x149808
00149807  inc   eax                            ; if arg == 0 then arg := 1
00149808  add   eax, 0x1f                      ; round up ...
0014980B  and   eax, 0xfffffff0                ; ... to a multiple of 16
0014980E  mov   dword ptr [ebp - 0x24], eax    ; <-- THE PRODUCER
   ...
00149E24  add   dword ptr [ebp - 0x24], 0x20   ; + 0x20 immediately before the call
```

**So `RegionSize = align16(arg) + 0x20`, where `arg = [ebp+0x10]`.** Checked against **both** observations:

| Invocation | Observed `RegionSize` | Pre-add | Multiple of 16? | Implied `[ebp+0x10]` | Reproduces? |
|---|---|---|---|---|---|
| **index 89** (normal) | `2097200` = `0x00200030` | `0x00200010` | **yes** | `0x001FFFF1`–`0x00200010` (32 values) | **YES** |
| **index 93** (failing) | `598869040` = `0x23B20430` | `0x23B20410` | **yes** | `0x23B203F1`–`0x23B20410` (32 values) | **YES** |

**Both observations are reproduced by the same formula**, and the pre-add values are **exact multiples of
16** as the `and 0xfffffff0` requires. **That is a positive check, not a fit.**

*(The lower bounds are `…F1`, not `…F2`: `align16` maps any input in `(pre−0x20, pre]` to `pre`, which is 32
values. Corrected per the acceptance reviewer's arithmetic advisory; the upper bounds and the verdict are
unchanged.)*

**A stronger witness than the single failing row, also from the reviewer:** **all 94 invocation sizes are
identical index-for-index across both logs**, not merely the failing one. That makes the build-independence
argument strictly stronger than the one-row comparison originally recorded.

### What this establishes — and it is a different answer from the stale-slot hypothesis

**The producer is a guest argument, not a stale stack slot and not an APU read.** The stale-slot hypothesis
recorded below is **refuted**: every path passes the writer, so the slot is always initialised from
`[ebp+0x10]`.

**And the failing argument was `≈0x23B20410` — pointer-shaped, and far outside the 64 MB RAM window
(`0x04000000`).** So **the caller passed a pointer-like value where a size belongs.** That is a **guest
caller defect**, and the chain now extends to *which caller* supplied it.

**The function is `sub_001497DC`** (generated source `recomp_0003.c:20105`, original span
`0x001497DC`–`0x00149F48`), and the generated source lists its call sites with their pushed arguments —
so the next step is a **bounded** identification of the caller whose 3rd argument is pointer-shaped.

**Per the packet's own instruction the chain *"includ[es] the caller's passed inputs if reached"* — and it
is now reached.** Identifying the specific caller is the natural continuation, and it is finite: the
generated source names every call site and the argument expression each pushes.

## Experiment 2 (as originally recorded) — the `O-OPEN` slice

The packet directs the slice to begin at `0x00149E24` (`add dword ptr [ebp-0x24], 0x20`) and the call at
`0x00149E4A`, and to follow **only may-reaching writes** of `[ebp-0x24]`.

**The first pass found one in-region writer and could not close:**

```
00149FB3  movzx eax, word ptr [edi]          ; eax = ZERO-EXTENDED 16-bit -> 0 .. 65535
00149FB6  mov   dword ptr [ebp - 0x24], eax  ; [ebp-0x24] := that
```

**`movzx` of a word cannot produce `0x23B20410` (= 598869008)** — the maximum is **65535** — so the slice
did not close on it. **The dominance analysis above later showed this writer does not reach the call at
all**, which is why it was the wrong thread to pull.

**Two further structural facts:**

1. **The writer is at a HIGHER address than the call** (`0x00149FB6` > `0x00149E4A`), so on a straight-line
   first pass the **call executes before the writer**. Nothing jumps directly to the writer.
2. **The same call site is invoked twice, from different frames:**

| Index | Size | `esp` |
|---|---|---|
| **89** | `2097200` (`0x00200030`) — normal | `0x00F7FBD4` |
| **93** | `598869040` (`0x23B20430`) — **failing** | `0x00F7FCF0` |

**The ESP differs by `0x11C` (284 bytes)**, so these are **different stack frames** — consistent with two
different callers of `sub_001497DC`, which is exactly what the generated source shows.

**The stale-slot hypothesis recorded at this stage — now REFUTED — was:** the failing frame's
`[ebp-0x24]` was never written and held stale stack content. The dominance analysis **refutes** it: every
path passes the writer. **It is retained here only so the correction is visible.**

### The ledger (packet-required columns)

| Guest PC | Bytes | Arg | Operation / width | Witnessed value | Producer type | Edge proof | Missing witness |
|---|---|---|---|---|---|---|---|
| `00149E24` | `83 45 DC 20` | `RegionSize` | `add [ebp-0x24], 0x20` | pre-add `0x23B20410` | guest-written | direct observation of the call | — |
| `00149E4A` | `FF 15 88 3F 1C 00` | — | `call [0x1c3f88]` | ordinal 184 | **modelled register** (bridge) | log `ret=0x00149E50` | — |
| `00149FB6` | `89 45 DC` | — | `mov [ebp-0x24], eax` | max **65535** | guest-written | decode from a verified boundary | **cannot produce the observed value** |
| `00149FB3` | `0F B7 07` | — | `movzx eax, word ptr [edi]` | 16-bit | guest-read | decode | **`edi`'s provenance is untraced** |
| — | — | — | the failing frame's `[ebp-0x24]` writer | `0x23B20410` | **UNRESOLVED** | **not found** | **which instruction wrote that frame's slot** |

**The single missing witness: the instruction that wrote `[ebp-0x24]` in the frame at `esp=0x00F7FCF0`.**
It is not the `movzx` writer, and the packet's own scope provides no further offline edge.

## Row selection

> ### Two verdicts, reported in order
>
> **`O-OPEN` was the correct row on the evidence available when it was selected**, and it is recorded as
> such below. **The subsequent bounded dominance analysis then identified the missing witness**, so the
> substantive chain is now bound further than the row anticipated. **Both are stated plainly rather than
> retro-fitting the row.**

### The chain as now bound

| Link | Witness |
|---|---|
| failing invocation | index 93, `ret=0x00149E50`, `esp=0x00F7FCF0`, size `598869040`, type `0x801000` |
| call | `0x00149E4A  call dword ptr [0x1c3f88]` → ordinal 184 |
| pre-call adjustment | `0x00149E24  add dword ptr [ebp-0x24], 0x20` |
| **producer** | **`0x0014980E  mov dword ptr [ebp-0x24], eax`**, where `eax = align16([ebp+0x10])` via `0x00149800`–`0x0014980B` |
| **the value's origin** | **`[ebp+0x10]` — the function's 3rd stack argument** |
| function | `sub_001497DC`, original span `0x001497DC`–`0x00149F48` (`recomp_0003.c:20105`) |

**`RegionSize = align16([ebp+0x10]) + 0x20` reproduces both observed sizes exactly**, and both pre-add
values are exact multiples of 16 as the `and 0xfffffff0` requires. **No unresolved competing definition
remains within the function** — the dominance analysis proved every reaching path passes the writer, and
the `movzx` writer does not reach the call at all.

**So the chain is bound to a positively witnessed producer with its value, PC, arithmetic and
branch/ordering** — the shape `O-OTHER-INPUT` describes. **But the producer is an *argument*, and the
packet's chain instruction extends *"including the caller's passed inputs if reached."*** The generated
source names `sub_001497DC`'s call sites and the argument expression each pushes, so identifying the
specific caller that supplied the pointer-shaped argument is **the next bounded step**, not an open end.

**One caution that must accompany that step:** the generated prologue reads
`ebp = g_seh_ebp; /* fpo_leaf: inherit caller's frame */` — **`sub_001497DC` is frameless and inherits the
caller's frame**, so `[ebp+0x10]` is an offset in the *caller's* frame. Any caller identification must
respect that.

**The next packet is therefore an `A2h-named-producer` discovery** naming `[ebp+0x10]` of
`sub_001497DC` at `0x0014980E` as the producer, with the caller's identity as its single remaining
question. **No change packet is authorized by this row** — the packet says *"consider a change only after
cause is established,"* and the caller's identity is not yet established.

### A bounded lead for that packet: the caller chain is short, and one site passes a parameter through

**Recorded as a lead, not as a completed attribution.** The Session followed the caller chain only as far as
the generated source names it, and **stopped there** rather than expanding scope beyond the packet.

**`sub_001497DC` has 8 call sites** in the generated source. Only **two** push three arguments; the other
six push one, so they cannot supply `arg2` at all.

| Call site | guest `ret` | `arg2` expression |
|---|---|---|
| `recomp_0003.c:21992` | `0x0014A6D6` | `MEM32(ebp + 0x14)` |
| `recomp_0003.c:22281` | `0x0014A858` | `MEM32(esp + 8)` |

**The second is structurally the more interesting**, because it sits inside **`sub_0014A83E`**
(`recomp_0003.c:22271`), declared **`cdecl, 2 params`**, `Frame: fpo_leaf`:

```
loc_0014A83E:
  eax = MEM32(esp + 4);          ; arg0
  PUSH32(esp, MEM32(esp + 8));   ; 1st push  -> the callee's arg2
  eax = eax >> 3;
  eax = eax & 8;
  PUSH32(esp, eax);              ; 2nd push  -> the callee's arg1
  PUSH32(esp, MEM32(0x27DCD4));  ; 3rd push  -> the callee's arg0
  call sub_001497DC
```

**So `sub_0014A83E` passes its own `arg1` straight through as `sub_001497DC`'s `arg2`** — a
pass-through, not a local computation. **`sub_0014A83E` is itself called from several sites** (e.g.
`recomp_0003.c:23746` pushes `ebx` as its `arg1`; `:29959` pushes `edi`; `:31550` and `:31984` push `eax`).

**Two cautions before anyone builds on this:**

1. **Whether this is the failing path is NOT established.** The failing invocation ran from
   `esp=0x00F7FCF0`; matching that frame to a caller requires the frame identity, which the archived logs do
   not name. **The other candidate (`0x0014A6D6`, `arg2 = MEM32(ebp+0x14)`) is not excluded.**
2. ~~**The generated `PUSH32` macro's exact `esp` semantics were not read.**~~ **RESOLVED, and it CONFIRMS
   the derivation.** `src/recomp/gen/recomp_types.h:670-674`:

```c
#define PUSH32(sp, val) do { \
    uint32_t _pv = (uint32_t)(val); \
    (sp) -= 4; \
    MEM32(sp) = _pv; \
} while(0)
```

   **`val` is evaluated BEFORE `esp` is decremented** — so in `PUSH32(esp, MEM32(esp + 8))` the read uses
   the **pre-decrement** `esp`, which on entry is `arg1`. **The offset derivation above is therefore
   correct, not assumed**, and the header even documents the convention (*"where push [esp+N] reads the
   operand before adjusting ESP"*). **`sub_0014A83E` genuinely passes its own `arg1` through as the
   callee's `arg2`** — a real parameter of a `cdecl, 2 params` function, not a read past its parameters.

**This is exactly the shape the packet's `O-OTHER-INPUT` row describes** — *"distinguish title data,
translated ABI, or another device at this one producer"* — and the remaining question is small and bounded:
**which of the two 3-argument call sites was on the failing frame, and what supplied its `arg2`.**

### Why `O-OPEN` was selected on the then-available evidence

| Row | Applicable? | Why |
|---|---|---|
| `O-IDENTITY` | **No** | Provenance is clean: XBE matches, both logs bind, 94/94 invocations paired, the failing invocation is uniquely identified |
| `O-APU-INPUT` | **No** | **No chain binds the pre-add local to a trapped APU read.** The producer is a guest stack argument; the toolkit's ordinal-184 bridge is a *consumer* of the size, not its producer |
| `O-OTHER-INPUT` | **Not on the first pass** | At that point the producer was **not** positively witnessed — the only in-region writer found was a `movzx` that could not produce the value. **It is the better fit now that the dominance analysis closed the chain** |
| `O-SEMANTICS` | **No** | The invocation is a **`MEM_COMMIT`** (`0x801000`), **not** a reserve, and the same site passes a normal ~2 MB request on its first visit |
| **`O-OPEN`** | **YES, on the then-available evidence** | An unbound edge: the writer of the failing frame's slot was not identified, and the packet forbids inventing an attribution |

**The Session did not open another round after `O-OPEN`.** The dominance analysis above was performed
because it is a **bounded, offline, one-function** question — not forward enumeration — and the packet's
`O-OPEN` row names *"the smallest unresolved PC/value/edge"* as the thing to identify. It has now been
identified.

## The toolkit and the arena behaved correctly

`xbox_HeapAlloc(598869040, 4096)` fails; `alloc_type = 0x801000` has **no `MEM_RESERVE`**, so neither
reserve branch can run; the bridge returns **`0xC0000017` (`STATUS_NO_MEMORY`)**. **The guest then does not
check the result and calls through NULL**, raising `0xE0424943`. **No arena change is warranted, and
widening it would be synthetic completion** — it would not make a 571 MB *commit* correct.

## What this establishes, and what it does not

**Establishes:** the failure **predates the A4b2 trap work** — it reproduces on a **different exe five days
earlier**, with all 94 invocation sizes identical index-for-index, the same size, type, OOM tuple and
terminal ICALL; and it is **not trace-caused** (R1 has `RECOMP_APU_TRACE` with 401 `[APUMMIO]` lines; A2g has
none; trace is observation-only). **The trap is NOT established as unnecessary** — all 35 archived runs
carrying this request are trapped; the chain
is deterministic and semantically identical across builds; the failing invocation is uniquely bound
(index 93, `0x00149E50`, `esp=0x00F7FCF0`, size `598869040`, type `0x801000`); the same site passes a normal
`2097200` on its **first** visit from a **different frame**; the only in-region writer of `[ebp-0x24]` is a
`movzx` of a **16-bit** value that **cannot** produce the observed size; and the arena behaves correctly.

**Does not establish:** what wrote the failing frame's `[ebp-0x24]`; whether the size is a guest defect, a
stale stack slot, or something else; any repair; survival past 4.77 s; any boot/audio/liveness/device claim.
**No synthetic completion**, the trap was **not** suppressed, the allocation was **not** faked, and the arena
was **not** widened. **`PIO_FREE` remains deferred**; `A4b2-r7`, accepted/closed `A4b2-r8` and `A4b1-r4`
were **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.
