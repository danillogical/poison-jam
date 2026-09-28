# `PIO_FREE` sufficiency — the demand at the 15 variable sites is **finite and byte-bounded**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Why this record exists:** `PIO_FREE-model-r2` returned `O-UNKNOWN` because **no admissible external source
documents the register's hardware semantics**, and its named successor — *"a focused source/queue-interface
discovery on the exact listed leaves"* — **may therefore be infeasible as specified**. This record develops
the **alternative question** the plan flags, and shows it is **finite, offline and mechanically decidable**.

**Method:** read-only disassembly of the original XBE (`inspect-jsrf.py disasm`), over the **15 VARIABLE
sites** already censused. **Nothing was run.**

---

## The distinction that makes this admissible

**`docs/jsrf-run-profiles.md:245-267`** requires a *modelled cause* to rest on external hardware
documentation, and states at **`:260-262`**:

> *"observed guest behaviour may corroborate an interpretation but does not count as one of the two
> independent sources. **Guest code is evidence about the title, not about the hardware.**"*

**So there are two different questions, and only one of them is blocked:**

| Question | Evidence class | Status |
|---|---|---|
| *What does the hardware's free-space register truly do?* | **hardware** | **BLOCKED** — no admissible source exists |
| *Is the existing stub **sufficient** for the title to proceed?* | **the title** | **DECIDABLE** — guest code **is** admissible about the title |

**This record answers only the second.** It **does not** model the hardware and **cannot** be read as doing
so. It establishes **sufficiency for this title under this stub**, never *what the device does*.

## The mechanism, from the guest's own code

Every VARIABLE site has the shape:

```
mov  <r>, [0xFE820010]
shr  <r>, 2                 ; available units = val >> 2
cmp  <r>, <demand>
jb   <poll>                 ; SPIN while (val >> 2) < demand
```

**So a site exits iff `(val >> 2) >= demand`.** With the stub's constant `0x80`, that is **`32 >= demand`**.
**If any site's demand can exceed 32, the guest spins forever there** — which is exactly the failure mode
`main.c:82-86` describes for the untrapped case.

## The finding: the demand is `k × <byte field>`, with `k` in {2, 3, 6, 7, 8, 9, 10}

Each site's demand register is computed by a **small integer multiply of a byte-sized field**. The distinct
setup forms across the 15 sites:

| Setup | k | Sites |
|---|---|---|
| `lea ecx, [eax + eax*2]` | 3 | 2 |
| `lea ecx, [eax + eax*8]` | 9 | 2 |
| `mov ecx, edx ; imul ecx, ecx, 7` | 7 | 2 |
| `lea ecx, [eax + eax]` | 2 | 1 |
| `lea ecx, [eax + eax*2] ; shl ecx, 1` | 6 | 1 |
| `mov ecx, eax ; shl ecx, 3` | 8 | 1 |
| `lea eax, [ebp-0x2c] ; lea eax,[ecx+ecx*2] ; shl eax,1` | 6 | 1 |
| `mov ecx, esi ; lea ecx, [eax + eax]` | 2 | 1 |
| `mov eax, ecx ; imul eax, eax, 7` | 7 | 1 |
| `lea eax, [ecx + ecx*4] ; shl eax, 1` | 10 | 1 |
| `lea ecx, [ebp-0xc] ; lea ecx, [eax + eax]` | 2 | 1 |
| `add eax,[eax] ; or eax,0xffff ; mov eax,edi ; imul eax,eax,7` | 7 (see note) | 1 |

**And the source field is a BYTE at every site:**

| Source size | Sites |
|---|---|
| **byte-sized load** (`movzx`/`movsx` … `byte ptr […]`) | **15** |
| wider or other | **0** |
| unresolved | **0** |

**So the demand is bounded by `255 × k ≤ 255 × 10 = 2550`** — **finite**, and the bound is *derived*, not
assumed.

**The source field is also consistent:** the byte is at **`[esi+0x64]`** at **14** of the 15 sites and
**`[ebx+0x64]`** at the 15th — **the same structure field**, reached through a different base register.

> **One site needs care and is flagged rather than smoothed.** `001A3F24`'s setup window also contains
> `add eax,[eax]` and `or eax,0xffff`, which **are not part of the demand computation** — the window simply
> caught unrelated instructions before the `mov eax,edi ; imul eax,eax,7` that actually sets the demand.
> **A clean re-derivation must bind each demand to its own reaching definition**, which is exactly why the
> successor needs a **checked-in, tested extractor** rather than this window heuristic. The *bound* is
> unaffected (the `imul …,7` on a byte-sourced value still bounds it), but the *form* at that site is
> **not** established by this record.

## The CONSTANT sites: the literal is **not** always 4, and two sites have **zero margin**

The `PIO_FREE-model-r2` Planner corrected a simplification in the Session's brief: the 13 constant sites do
**not** all compare against `4`. **The Session verified this and it is right.** Reading each site's actual
literal:

| Literal | Sites | Stub `0x80 & ~3 = 128` passes? | Margin |
|---|---|---|---|
| `0x4` (4) | 4 | yes | 124 |
| `0x8` (8) | 3 | yes | 120 |
| `0xC` (12) | 1 | yes | 116 |
| `0x20` (32) | 1 | yes | 96 |
| `0x48` (72) | 1 | yes | 56 |
| `0x4C` (76) | 1 | yes | 52 |
| **`0x80` (128)** | **2** | **yes** | **0** |

**All 13 pass — but two sites compare against `0x80` exactly, so the stub's masked value equals the
threshold with ZERO margin.** A stub returning `0x7C` or less would **fail** those two sites. The constant
form exits iff `(val & 0xFFFFFFFC) >= literal`, so the correct statement is *"the stub passes all 13, with
zero margin at two"* — **not** *"any value ≥ 4 passes."* **The Planner's correction is adopted.**

## The VARIABLE sites: the demand register is `ecx` **or** `eax`

The Planner's second correction is also confirmed: a hardcoded-`ecx` assumption would be wrong.

| Demand register | Sites |
|---|---|
| `ecx` | **11** |
| `eax` | **4** |

**So any extractor must identify the demand operand from each site's own `cmp`, not assume a register.**

> **A Session method error, recorded.** My first pass at the literal table began disassembly at `va−2` for
> sites whose addresses were **already instruction starts**, landing **mid-instruction**. The disassembler
> **silently emitted garbage** (`.byte`/`jmp dword ptr [ecx - 0x17dfff0]`) rather than erroring, and five
> polls appeared "not found". **This is the fourth ad-hoc parsing failure in this analysis**, and it is the
> most dangerous kind — a tool that returns plausible output for a misaligned request. **Any successor must
> start disassembly at a verified instruction boundary.**

## What this makes decidable

**The sufficiency question reduces to a finite, offline property of the title's own data:**

> For each of the 15 variable sites, what is the **maximum value** the byte field can take?
> The stub is **sufficient** iff `max_byte × k ≤ 32` at every site — i.e. iff every site's byte stays at or
> below `32 / k`.

**Both outcomes are actionable:**

| Outcome | Meaning | Consequence |
|---|---|---|
| **every `max_byte × k ≤ 32`** | the constant `0x80` is **sufficient** at all 28 sites | the stub is **adequate for this title**, and the PIO_FREE boundary is *bounded* rather than unknown — **no hardware model needed for progress** |
| **some `max_byte × k > 32`** | the constant is **insufficient** at that site | the guest can spin forever there; a **truthful free-space model becomes necessary**, and *that* would require the hardware sourcing that is currently blocked — a genuine, evidence-backed escalation |

**This is a far stronger position than `O-UNKNOWN` left us in**: the unknown has been reduced from
*"what does the hardware do?"* (unanswerable with admissible sources) to *"how large do 15 byte fields get
in this title?"* (finite, mechanical, and admissible because it is a question about the title).

## What this does **not** establish

- **It does not model the hardware**, and cannot be cited as a hardware model. **The five `UNKNOWN` ledger
  leaves remain `UNKNOWN`** — units, capacity, drain, overflow and ordering are **untouched** by this record.
- **It does not establish that any poll actually exits in a strict run**, or anything about liveness, boot,
  or audio. It establishes a **bound on a demand**, not an observation.
- **It does not prove `0x80` is the true device value** — only whether it is *sufficient*, which is a
  different claim.
- **The `k` and byte-source extraction is a window heuristic and is not itself the deliverable.** One site's
  *form* is explicitly unresolved. **A successor must re-derive it with a checked-in, tested extractor**
  (the packet's own requirement, and now backed by three ad-hoc parsing failures in this session).
- **No strict criterion is satisfied and nothing is claimed to work** (§5.8).
