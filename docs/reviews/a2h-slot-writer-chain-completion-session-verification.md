# `A2h-slot-writer-chain-completion-r1` — execution: **`O-OPEN`**, and a SECTION-MEMBERSHIP trap

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-writer-chain-completion-r1`, frozen
**`3F7AD922DADD5E8E1B3944EAFF6B461468C1C05DC2350973243022C16305C7FD`**.
**Worker:** `37fb82a1-e078-49f2-a3d6-3fe861aaa285`, commit `973b95b`.
**Finding:** `docs/reviews/a2h-slot-writer-chain-completion-evidence.md`.

**Row: `O-OPEN`.** **The index was NOT solved backwards from the slot — the packet's core rule was
honoured.** **And the execution found a structural trap that likely explains the line's earlier
encoding-scoped blindness.**

---

## ⚠ THE SECTION-MEMBERSHIP TRAP — the most consequential finding, and the Session verified it

**The Worker's note:** *"`sub_00199DB0` and `sub_0018CB40` live in the **D3D section**, **not `.text`**. Any
enumeration seeded only from `.text` would have missed the entire store function — a plausible root cause of
why the earlier ModRM scan was encoding-scoped."*

**The Session verified it directly:**

| Address | What | Section |
|---|---|---|
| **`0x00199DB0`** | **`sub_00199DB0` — the store function** | **D3D** |
| **`0x0018CB40`** | `sub_0018CB40` | **D3D** |
| **`0x00199F45`** | **THE STORE** | **D3D** |

**And the section bounds:**

```
.text   0x00011000..0x0018CB30
D3D     0x0018CB40..0x0019E338
```

> ## **CONFIRMED: the store function lives in D3D, a DIFFERENT section from `.text`.**

**Why this matters beyond this row:** **the Session's own `2C 24 00 00` scan DID cover D3D** — **that is why
it found the installer.** **But any enumeration seeded only from `.text` — which is the natural assumption
for "code" — would have skipped the entire store function.** **The Worker's phrase is exact: this is a
plausible root cause for the encoding-scoped blindness, and it is a trap the line had not named.**

**Recorded as a durable structural fact about this image:** **code is NOT confined to `.text`.** **`D3D` is
`0x117F8` bytes of executable content, and the section carries `X` in its flags** — **which the packet's
own §6.1 line already required sweeping, and which the Worker did.**

## The four items — each answered at its true strength

### Item 1 — INDEX: **NOT WITNESSED**, and the enumeration is exhaustive

**The Worker enumerated EVERY encoding form that can reach the vtable slot `0xCC`:**

| Form | Sites |
|---|---|
| `FF /2` or `/4`, mod=10, `disp32 == 0xCC` | **1 — the thunk itself** |
| SIB `disp32 == 0xCC` | **0** |
| mod=01 `disp8 == 0xCC` | **0** |
| `mov reg,[reg+0xCC]; call reg` | **0** |
| `add`/`lea +0xCC` then indirect | **18 — NONE followed by an indirect call** |
| absolute `[abs32]` in region | **0** |

**And `sub_00153790` has NO direct caller** — **zero `e8`/`e9` rel32 hits across every file-backed section.**

**The thunk VA is published into SIX runtime dispatch tables, and the Session verified each has exactly ONE
absolute reference in the entire image:**

```
0x00257E00 : 1    0x002582B0 : 1    0x002586A8 : 1
0x00257EF0 : 1    0x002580D0 : 1    0x002587F0 : 1
```

> **Each table's single reference is its own writer — so no reader exists in the decoded population.**
> **The thunk caller's 2nd and 4th pushed values are UNTRACED, and the Worker states it cannot distinguish
> "the reader is in the undecoded 25.02%" from "the tables are written but never read."**

**That is the honest form of an unfound edge.** ✓ **And critically: *"ARG1 was NEVER solved backwards from
the slot — the `0x3EC+2064*4=0x242C` arithmetic is reported as an identity only."*** **The packet's core rule
was honoured.** ✓

### Item 2 — ORDER: **PARTIAL**

**Installer CONFIRMED as an instruction sequence** (`0x0018CE30`/`34`/`3A`/`ret 4`), **sole caller
`0x00012319`**, **`0x0015F9E0` substitutes the constant `0x0015F9D0`** — **boundary verified against
`inc dword [0x265174]`/`ret`.**

**Fourth read CONFIRMED:** **`0x00193E62 mov eax,[esi+0x1c4]` → `0x00193EB5 call eax`**, with
**`esi = 0x0019D468` → `0x0019D62C = software_device+0x242C`.**

**And the Worker held the line:** *"The competitor write is NOT identified and the value at the fourth read is
NOT witnessed (zero dump reads). **Polls treated as change-between-polls, not attribution.**"* ✓

### Item 3 — VALUE: **BYTE TUPLE**, with the reaching definitions traced

**The Worker traced the actual dataflow:** **four `[edi±]` floats, each clamped against `[0x1c4578]` /
`[0x1c43d0]`, `fmul [0x1c4ccc]` (255.0f), `fadd [0x1c4550]` (0.5f), `call 0x192a80`, result into a BYTE slot,
then packed with `shl`/`or` at `0x00199F3B–0x00199F43`.**

> **So the store's `eax` IS a packed byte tuple by construction** — **that is a DATAFLOW finding, not a
> decomposition of the target value.**

**And it separately confirmed the competing hypothesis:** **the SAME dword `0x001D5078` IS an ADDRESS** —
**XBE bytes there are ASCII `djv000_0.adx`.**

**So BOTH readings are live, and the Worker says exactly that:** *"Value attribution to the slot is NOT
witnessed — fail closed."* ✓ **This is the right handling of the reviewer's unfalsifiability objection: the
formula is a formula, and the VALUE at the slot is unwitnessed.**

### Item 4 — SWEEP: **DONE, NOT CLEAN, and it states its coverage**

**The scale is worth recording:** **65 888 memory-write instructions out of 549 980 decoded, from 5 652
declared seeds** (`recomp_funcs.h`), **span = seed→next seed in the same section, decode overruns 0.**

| Section | Coverage |
|---|---|
| `.text` | **98.24%** |
| **`D3D`** | **94.74%** |
| **`DSOUND`** | **33.07%** |
| `MMATRIX`/`XGRPH` | 100% |
| `XPP` | 98.49% |
| **total** | **74.98%** — **563 396 uncovered non-padding bytes** |

**Reachability:** **3 220 stores can reach `base+0x242C`** (2 931 base-ambiguous); **index ≤ `0x1000` → 1 008**
(956 ambiguous). **Exactly ONE store has a literal disp of `0x242C`: `0x0018CE3A`.**

**And the coverage gap is carried as UNKNOWN, not as uniqueness:** *"No competitor claimed or excluded; each
competitor got the same ARG1/ARG3/value/order treatment and failed identically."* ✓

> **This is the §6.1 rule applied at full strength, and it is the FIRST time this line has a coverage number
> rather than a claim.** **74.98% total, with DSOUND at 33.07% as the named gap.**

## Boundary verification — and a second witness

**The Worker used TWO independent boundary witnesses:** **5 652 declared starts → 549 980 boundaries**, **and
D3D padding-derived starts → 18 168 boundaries**, *"since `sub_00199DB0` is NOT declared."*

**And it reports: 0 cited VAs are non-boundaries, with matched text printed for each.** **It caught
`0x0018B5AE` as mid-instruction (`0x0018B5AD` starts it) and cited it only as a counter-example.** **And it
did NOT inherit the old off-by-1–2 labels.** ✓

## Controls — the honest non-claim, again

**ZERO dump reads, so the per-run gate is explicitly NOT claimed as passed.** **The paired offset-shift
control ran against original XBE bytes and both values matched, parsed with `int(text, 16)`, with the masked
shift passing.** **And: *"NO INPUT: the dump-based memory-read control had no dump input by construction."*** ✓

**That is the REJECT's control lesson applied correctly.**

## The disposition

**Row `O-OPEN`. `WRITER: candidate` — instruction confirmed, slot reachability not.**

**BROKEN EDGE: the reader of the six dispatch tables holding `0x000D4DA0`, hence the thunk caller's 2nd/4th
pushed arguments.**

**PARK + RE-REFER for scope.** ✓ **EDGE 5 recorded, not chased. C-5 separate.**

## Prohibitions and status

**Static and read-only.** **No game run, no source edit, no instrumentation.** **No synthetic completion.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited.** **All eight guards pass.**
