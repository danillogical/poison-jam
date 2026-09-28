# `A2h-named-producer-frame-r1` execution evidence — **`O-OPEN`**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a2h-named-producer-frame.md`, revision **`A2h-named-producer-frame-r1`**, frozen
SHA-256 **`2333B6523B39866F1AEB9C1BEFFE1CCE42176E35BA3ADB86F8965A265BF88887`** — verified before execution,
**not edited**. **Class:** discovery (§5.8).

**Selected row: `O-OPEN`** — *"No uniquely bound failing invocation, no write provenance, ambiguous
frame/return, a competing or indirect entry, coverage gap, unsafe seam, helper frame ambiguity, cap/stack
overflow, or failure to reach the event in one attempt."*

**No guest run was performed.** The packet permits one **only if** the offline audit cannot identify the frame
**and** a safe non-generated seam exists. The audit **did** identify the frame and the caller, but produced
**no invocation-bound read value**, and the only seam observing the failing call is **toolkit-side**, which
this packet's write scope does not authorize. **So the run is not reached.**

---

## Identity

| Item | Value |
|---|---|
| Game revision | `c914448923e9b8e6065c76de7d3f999c3231b411`, clean |
| Toolkit revision | `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`, clean — **unchanged** |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| R1 profile | **STRICT** |
| R1 dump mapping | `matches: 0 content-mismatch: 1` — **not usable for an image-content claim** |
| Failing invocation | **index 93**, `ret=0x00149E50`, `esp=0x00F7FCF0`, `size=598869040`, `type=0x801000` |
| Capture outcome | `unhandled_exception`, `code=0xE0424943` — **frozen at the crash** |

**Toolkit / runtime changes: NONE. Instrumentation added: NONE.**

---

## Experiment 1 — the frame question is DECISIVELY answered

### The packet's explicit requirement, discharged

> *"The entry frame may change through the early helper `sub_0017D1F8`: trace/measure `g_seh_ebp` across that
> call rather than assuming preservation."*

**It changes it.** From the **original XBE**, decoded from verified boundaries and byte-verified by the new
tool (`scripts/a2h-frame-audit.py verify`, **ALL VERIFIED**):

```
0017D1F8  push  0x1804a0          ; SEH scope table
0017D1FD  mov   eax, fs:[0]
0017D203  push  eax               ; old SEH record
0017D204  mov   fs:[0], esp
0017D20B  mov   eax, [esp+0x10]   ; the local size the callee pushed
0017D20F  mov   [esp+0x10], ebp   ; SAVES the inherited ebp      (bytes 89 6c 24 10)
0017D213  lea   ebp, [esp+0x10]   ; <-- RE-ESTABLISHES ebp        (bytes 8d 6c 24 10)
0017D217  sub   esp, eax
...
0017D230  ret
```

**The recompiled helper publishes that frame** (`recomp_0004.c:1578,1589`): `ebp = esp + 0x10;` …
`g_seh_ebp = ebp; esp += 4; return;`. **And the callee reads it back** (`recomp_0003.c:20122, 20132`):
`ebp = g_seh_ebp;` … `eax = MEM32(ebp + 0x10);`.

### The frame arithmetic

With **E** = `esp` at the callee's entry: `push 0x178` → `E−4`; `push 0x1E0BE8` → `E−8`; the `call` → `E−12`;
helper `push 0x1804A0` → `E−16`; helper `push eax` → `E−20`; **`lea ebp,[esp+0x10]` → `ebp = E−4`.**

**So `[ebp+8] = [E+4]` = arg0, `[ebp+0xC] = [E+8]` = arg1, `[ebp+0x10] = [E+0xC]` = arg2.** The helper's
frame **aliases the standard argument positions** — exactly what MSVC's `__SEH_prolog` is for — and the
recompilation reproduces it faithfully.

### E is fixed by three independent facts, and the third was not used to derive it

| # | Source | Result |
|---|---|---|
| 1 | ordinal-277 `esp=0x00F7FD00` + `0x1A0` | **E = `0x00F7FEA0`** |
| 2 | ordinal-184 `esp=0x00F7FCF0` + `0x1B0` | **E = `0x00F7FEA0`** |
| 3 | post-prologue `esp = E−0x198 = 0x00F7FD08`; the ICALL is preceded by one push + the call | predicts `0x00F7FD00`; **log says `0x00F7FD00`** |

**Facts 1 and 2 are two `esp` readings taken at different points inside the same activation; fact 3 is
independent of both.** All three agree, so `ebp = 0x00F7FE9C` and **`[ebp+0x10] = 0x00F7FEAC`**.

## The caller IS bound — and the argument-count question is settled

**`[0x00F7FEA0]` holds `0x0017C926`, and a real `call 0x1497dc` ends exactly there** (`call@0x0017C921`).
So the caller is identified **by a return address in the frame**, not by inference.

> ### ⚠ Two Session errors in this step, both caught by measurement
>
> **My hand-count said the bound caller passes ONE argument**, which would have made `[ebp+0x10]` stale
> caller stack. **That was wrong.** The call site is:
>
> ```
> 0017C918  push  eax          ; arg2 -- align16 of the caller's own argument
> 0017C919  push  0            ; arg1
> 0017C91B  call  0x14a838     ; an INTERVENING call
> 0017C920  push  eax          ; arg0 -- the getter's result
> 0017C921  call  0x1497dc
> ```
>
> `0x14a838` is `mov eax,[0x27dcd4]; ret` — **a plain `ret` that consumes no arguments**, so the two pushes
> below it **survive as argument material** for `0x1497dc`. **The caller passes THREE.**
>
> **I then over-corrected a second time**, reading `[E+0xC]` as the `push 0`; that double-counted the getter's
> popped return address. **Both errors came from hand-arithmetic on a stack, which is exactly why the tool
> now tracks ESP symbolically instead of counting pushes.** The tool was then validated against the
> recompiler's own call list: **13 sites, exact set equality with the generated source.**

**Result: 12 of the 13 direct call sites pass three arguments; one passes two.** The packet's premise that the
two three-push sites were the candidate set is **superseded** — but see below, because that did **not** produce
a positive row.

## Experiment 1 — the archive does **not** yield an invocation-bound read value

**The callee never writes `[ebp+0x10]`** — verified across its whole generated body (841 lines): **0 writes,
5 reads**, the earliest at `0x001497EB`. So its only writer is the caller's push, and there is **no competing
writer**.

**But the frozen frame is NOT the failing allocation's activation.** The evidence:

| Fact | Value |
|---|---|
| Dump `[ebp+0x10]` (the live frame) | `0x4C000010` |
| `align16(that) + 0x20` | `0x4C000040` = 1275068480 — **never requested** in the log |
| The failing size | `0x23B20430` = 598869040, requested **exactly once** |
| **Crash registers** | **`eax=0x4C000020`**, `edi=0x04C00002` |

**`align16(0x4C000010) = 0x4C000010`, and `0x4C000010 + 0x20 = 0x4C000020` — exactly the `eax` in the crash
registers.** So the dump's frame **is** the crash activation's, and it is **self-consistent with the crash**,
not with the failing allocation.

**And control left the callee between the two:** the bridge call after the OOM returns to `0x00149F5D`, which
is **outside** `sub_001497DC`'s range `0x001497DC..0x00149F48`. **So the failing activation returned, and a
later activation of the same function reused the same stack addresses at the same depth.** Its `arg2` was
overwritten and **is not in the artifact**.

### The decisive arithmetic — the slot matches **neither** activation

The producer's value survives in the dump at `[ebp−0x24]` (`0x00F7FE78` = **`0x4C000020`**), and `edi` was set
from it (`shr edi,4`). Checking the crash registers against the verified instruction chain:

| Quantity | Dump / register | Consistent? |
|---|---|---|
| `[ebp−0x28]` = `align16(arg2)>>4` | dump `0x00F7FE74` = `0x04C00002`; **crash `edi=0x04C00002`** | **exact match** |
| `[ebp−0x24]` = `align16(arg2)` | dump `0x00F7FE78` = `0x4C000020`; **crash `eax=0x4C000020`** | **exact match** |
| ⇒ the crashing activation's `arg2` | must lie in **`[0x4C000011, 0x4C000020]`** | — |
| the dump's `[ebp+0x10]` slot | **`0x4C000010`** | **below that interval — matches neither** |

**So the dump's `arg2` slot holds `0x4C000010`, while the crashing activation must have read a value in
`[0x4C000011, 0x4C000020]`, and the failing allocation implies `0x23B20410`.** The slot agrees with **neither**.
**The `arg2` slot is dead stack by capture time** — the callee's `ret 12` raises `esp` above it — **so it is not
a witness for any activation's `arg2`.**

*(Note the producer's own two stores **do** survive and **do** match the crash registers exactly. That is what
makes the `arg2` slot's disagreement conclusive rather than a mapping error: the frame address is right, the
surrounding slots are right, and only the argument slot has been recycled.)*

**Also checked per `AGENTS.md`'s spelling rule:** the callee's body contains **5** `ebp + 0x10` occurrences and
**0** decimal-spelled `ebp + 16` equivalents, and **0 writes** in either spelling. **The no-writer conclusion is
not a spelling artifact.**

**`[TRACE]` does not help:** `sub_001497DC` is **not traced** (0 lines), and `0x0017C926`/`0x0017C921` appear
**0** times as a `from=`. **The caller identity therefore rests on the crash activation's return address**,
which is strong evidence the call site is the same one — but **not a witness for the failing allocation.**

## Experiment 3 — the seam is toolkit-side and outside this packet's write scope

| Candidate | Where | Assessment |
|---|---|---|
| `src/diagnostics.c` | game, non-generated | the A4b store-observer; observes six fixed store sites, **not** the callee's entry or its read |
| `src/main.c`, `recomp_manual.c`, `jsrf_crt.c` | game, non-generated | no entry/read hook |
| **`kernel_thunk_dispatch()`** (`kernel_bridge.c:8944`) | **toolkit** | **the one seam that sees every bridge call with `esp`** — it owns `g_kernel_call_count` and logs `g_esp` |

**The only seam that observes the failing call is in the toolkit, and this packet authorizes "at most the
identified diagnostic-only game hook" — not a toolkit edit.** The packet also states that if seams cannot
observe the transitions without changing guest behaviour, **stop at `O-OPEN`** and that *"if this is
impossible, `O-OPEN` is the output, not permission to improvise."*

## Selected row: `O-OPEN`

| Row | Applicable? | Why |
|---|---|---|
| `O-IDENTITY` | **No** | XBE matches, toolkit clean and unchanged, R1 STRICT, failing invocation rebound uniquely |
| `O-FRAME-ARG` | **No** | Requires **a proved reaching call-site write to that exact address** *and* the actual read/value — the frame is bound, but the captured activation is the **crash's**, not the failing allocation's, so **the read value is absent** |
| `O-FRAME-OTHER` | **No** | Same missing witness; and there is **no competing writer** to attribute to |
| **`O-OPEN`** | **YES** | **A coverage gap: no invocation-bound read value for the failing allocation**, and no safe non-generated seam |

**The one smallest missing witness, as the row requires:** a **recorded value of `[ebp+0x10]` at `0x00149800`
on the OOM activation** — i.e. the read at guest address `0x00F7FEAC` **at the moment of the failing
allocation**, before the activation returned. **The address is known exactly; only the moment is missing.**

## What this establishes, and what it does not

**Establishes** (verified bytes + the toolkit's own generated code, **not** the dump): the SEH helper
**replaces** `ebp` with a frame that **aliases the argument positions**; `[ebp+0x10]` at `0x00149800` is
therefore **arg2 of the caller**; **E = `0x00F7FEA0`** by three independent facts; the caller is
**`call@0x0017C921`** by a return address in the frame; the callee has **no write** to the slot, so there is
**no competing writer**; **12 of 13 call sites pass three arguments**; and the archived frame is the **crash**
activation's, self-consistent with the crash registers.

**Does not establish:** the failing allocation's `arg2` **value**; the source of that value; that the caller's
`align16` input is title data rather than a modelled input; a guest-vs-toolkit defect; trap necessity; a
repair; survival past ≈4.77 s; or any boot/audio/GPU/device/liveness claim. **No synthetic completion** — the
trap and `0x80` were not suppressed, the allocation was not faked, the arena was not widened, the NULL call
was not bypassed, and **no guest error-handling change is proposed**. **`PIO_FREE` remains DEFERRED**;
`A4b2-r7`, accepted/closed `A4b2-r8` and `A4b1-r4` were **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.

## Tooling produced

**`scripts/a2h-frame-audit.py`** + **`scripts/test_a2h_frame_audit.py`** (**21 tests, OK**), because three
hand-analyses of the same stack produced three different answers. It **caught two of its own author's
errors**: a mis-transcribed byte string (`89442410` vs the real `896c2410`), and a linear section sweep that
returned **zero** call sites for a target with thirteen. Both are now pinned by tests.
