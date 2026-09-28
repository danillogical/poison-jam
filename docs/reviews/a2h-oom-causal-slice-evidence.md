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
| `scripts/a2h-oom-slice.py` | The required checked-in parser/binder |
| `scripts/test_a2h_oom_slice.py` | Its tests — **22 tests, OK** |
| `docs/reviews/a2h-oom-causal-slice-binding.json` | The parser's deterministic output |

**Commands, exactly as run:**

```powershell
python -X utf8 -m unittest scripts.test_a2h_oom_slice
python -X utf8 scripts\a2h-oom-slice.py --log logs/runs/20260927-160330-655-a4b2-gp-trap-trace/jsrf_run.log --log logs/runs/20260922-224429-003-a2g-304f0-span/jsrf_run.log --out docs/reviews/a2h-oom-causal-slice-binding.json
```

## Experiment 1 — archive-first binding

| Check | R1 (trapped failing) | A2g (no-trap, historical) |
|---|---|---|
| `check-run-profile.py` | **STRICT** | **EXPLORATORY** — as the packet anticipated, so it is used **only to contradict trap necessity at that earlier build**, never as strict validation |
| `check-dump-mapping.py` | `CONTENT_MISMATCH` — structurally readable; **no shifted-offset substitution used** | `CONTENT_MISMATCH` — same |
| exe | `bc8e288dd54d…` | `2cd0472a256e9d…` — **a different build** |
| `RECOMP_APU_TRAP` | **1** | **ABSENT** |
| outcome | `unhandled_exception`, 4.77 s | `unhandled_exception`, 4.95 s |
| log SHA-256 | `c52d71655c48c752ba275b3361d9abbafd7a8dea5de14cc70b26ad27c416c874` | `b9631ee5af44b40213f84c8645bdc6fb31f23a7462b41866ca9cfbe5f1176a04` |

**The XBE hash matches in both archives and on disk.** *(A Session check reported a false mismatch by
comparing a lowercase computed digest against an uppercase expected string; case-normalised, it matches. The
error is recorded because it is the same class as the others — comparing the wrong representation.)*

### The causal chain is **semantically identical** across a trap run and a no-trap run

| Field | R1 | A2g | Same |
|---|---|---|---|
| invocation count | 94 | 94 | ✓ |
| failing index | **93** | **93** | ✓ |
| failing call site (`ret`) | `0x00149E50` | `0x00149E50` | ✓ |
| failing `esp` | `0x00F7FCF0` | `0x00F7FCF0` | ✓ |
| `base` / `size` / `type` | `0x0` / **598869040** / `0x801000` | `0x0` / **598869040** / `0x801000` | ✓ |
| OOM tuple | `(598869040, 12715008, 50855936)` | identical | ✓ |
| ICALL `(target, esp, return)` | `(0x0, 00F7FD00, 0014982E)` | identical | ✓ |

**This is the decisive answer to the packet's premise question: the trap is NOT a necessary cause.** A run
with **no trap at all**, on a **different build**, **five days earlier**, reproduces the chain
**byte-for-byte in every semantic field**. The trap makes the path reachable *sooner*; it does not create it.

> **Parser fix found by its own tests.** The first comparison compared whole invocation dicts, which include
> the **byte offset into each log file** — necessarily different between runs — and so reported a spurious
> difference. The tests caught it (`test_byte_offset_difference_does_not_report_a_semantic_difference`) and
> the comparison is now **semantic** over `(index, esp, ret, base, size, type)`. **This is exactly the class
> of error the packet's checked-in-parser requirement exists to prevent**, and the requirement earned its
> keep on the first execution.

## Experiment 2 — the backward demand slice

The packet directs the slice to begin at `0x00149E24` (`add dword ptr [ebp-0x24], 0x20`) and the call at
`0x00149E4A`, and to follow **only may-reaching writes** of `[ebp-0x24]`.

**Exactly one instruction writes `[ebp-0x24]` from a computed value inside the enclosing region:**

```
00149FB3  movzx eax, word ptr [edi]          ; eax = ZERO-EXTENDED 16-bit -> 0 .. 65535
00149FB6  mov   dword ptr [ebp - 0x24], eax  ; [ebp-0x24] := that
```

**`movzx` of a word cannot produce `0x23B20410` (= 598869008).** The maximum is **65535**. So the observed
pre-add value is **impossible for this writer**, and the slice does not close on it.

**Two further structural facts:**

1. **The writer is at a HIGHER address than the call** (`0x00149FB6` > `0x00149E4A`), so on a straight-line
   first pass the **call executes before the writer**. Nothing jumps directly to the writer (0 direct
   branches), so it is reached by fall-through from the loop body.
2. **The same call site is invoked twice, from different frames:**

| Index | Size | `esp` |
|---|---|---|
| **89** | `2097200` (`0x00200030`) — normal | `0x00F7FBD4` |
| **93** | `598869040` (`0x23B20430`) — **failing** | `0x00F7FCF0` |

**The ESP differs by `0x11C` (284 bytes)**, so these are **different stack frames**. `[ebp-0x24]` is
therefore a **different physical slot** in each, and the failing frame's slot held `0x23B20410`.

**The leading hypothesis, stated as a hypothesis:** the failing frame's `[ebp-0x24]` was **never written by
this function** — the call at `0x00149E4A` precedes the only writer, and the writer could not have produced
the value anyway. A stale stack slot holding a **pointer-shaped** value (`0x23B2xxxx`, far outside the 64 MB
RAM window) would look exactly like this. **But this is not proven**, and the packet forbids inventing an
attribution.

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

## Why `O-OPEN` and not the neighbours

| Row | Applicable? | Why |
|---|---|---|
| `O-IDENTITY` | **No** | Provenance is clean: XBE matches, both logs bind, 94/94 invocations paired, the failing invocation is uniquely identified (index 93, `ret=0x00149E50`, `esp=0x00F7FCF0`) |
| `O-APU-INPUT` | **No** | **No validated chain binds the pre-add local to a positively witnessed trapped APU read.** The only writer found is a `movzx` of a guest word that **cannot** produce the value, and the failing frame's writer is **unresolved**. The packet explicitly requires *"no unresolved competing definition"* |
| `O-OTHER-INPUT` | **No** | Same reason — no positively witnessed producer with a value, PC, arithmetic **and** branch/ordering |
| `O-SEMANTICS` | **No** | The invocation is a **`MEM_COMMIT`** (`0x801000`), **not** a reserve, and the same site passes a **normal 2 MB** request on its first visit. A deliberate large *virtual-region* request is not established |
| **`O-OPEN`** | **YES** | An **unbound edge**: the writer of the failing frame's slot is not identified, and the packet forbids inventing an attribution |

**Per the row: the next packet is an `A2h-one-missing-witness` discovery/tooling prerequisite, naming the
smallest unresolved PC/value/edge — which this record names precisely above.** The packet also says **"do not
invent an attribution or iterate forward,"** so the Session stops here rather than opening another round.

**No new run was performed.** Step 3's precondition is *"only if an actual size or producer edge cannot be
bound offline"* — it cannot — but step 3 also requires identifying **one exact guest-PC trace seam**, and the
honest position is that the missing witness is a **stack slot in a second frame**, which the archived logs do
not name. **The packet's own guidance for that situation is `O-OPEN` plus a named prerequisite, which is
what is recorded.** A future packet may specify the seam once the frame's identity is known.

## The toolkit and the arena behaved correctly

`xbox_HeapAlloc(598869040, 4096)` fails; `alloc_type = 0x801000` has **no `MEM_RESERVE`**, so neither
reserve branch can run; the bridge returns **`0xC0000017` (`STATUS_NO_MEMORY`)**. **The guest then does not
check the result and calls through NULL**, raising `0xE0424943`. **No arena change is warranted, and
widening it would be synthetic completion** — it would not make a 571 MB *commit* correct.

## What this establishes, and what it does not

**Establishes:** the trap is **not a necessary cause** (reproduced without it on an older build); the chain
is deterministic and semantically identical across builds; the failing invocation is uniquely bound
(index 93, `0x00149E50`, `esp=0x00F7FCF0`, size `598869040`, type `0x801000`); the same site passes a normal
`2097200` on its **first** visit from a **different frame**; the only in-region writer of `[ebp-0x24]` is a
`movzx` of a **16-bit** value that **cannot** produce the observed size; and the arena behaves correctly.

**Does not establish:** what wrote the failing frame's `[ebp-0x24]`; whether the size is a guest defect, a
stale stack slot, or something else; any repair; survival past 4.77 s; any boot/audio/liveness/device claim.
**No synthetic completion**, the trap was **not** suppressed, the allocation was **not** faked, and the arena
was **not** widened. **`PIO_FREE` remains deferred**; `A4b2-r7`, accepted/closed `A4b2-r8` and `A4b1-r4`
were **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.
