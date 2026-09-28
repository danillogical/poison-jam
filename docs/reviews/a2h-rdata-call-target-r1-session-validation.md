# `A2h-rdata-call-target-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-rdata-call-target.md`, authored by Planner
`7c578633-30ae-474f-bced-1ddadadb5fbd` (`codex/gpt-6-sol` @ `high`), committed `8ffa531`.
**Authority:** `docs/reviews/a2h-null-line-closure.md` ruling 2 — *"`0x001D5078` next … static-first with
instrumentation only on gaps … Scope it to that terminal alone."*
**Adequacy:** the Planner's §5.8 review — **`ADEQUATE`**, pending the Session's source/identity validation.

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-rdata-call-target-r1`** |
| Lines | **22** (23 with the trailing newline) |
| Bytes | **7820** |
| **SHA-256 (frozen)** | **`31343C167746872A501A21C6D558F52B897A7CD571E43C9EB016D42341152593`** |

## Validation — every checkable claim reproduces

| Claim | Verified |
|---|---|
| **59 repetitions of `0x001D5078`, ALL stride 8** | **exactly 59, every stride equal to 8** |
| **candidate table beginning guest `0x001D4BD4`** | maps to `.rdata` (raw `0x1B4000`) **through the SECTION TABLE** — a validated boundary **for the section** |
| **the target is in `.rdata` and is DATA** | `0x001D5078` ∈ `.rdata` (`0x001C3F60..0x001EB760`); **bytes are `b'djv000_0.adx\x00\x00\x00\x00'`, all printable ASCII** |
| **the register set** | **CONFIRMED, and the Session found MORE than the packet claims** |

### The Session's correction — it is FOUR realizations, not two

**The packet says *"two have identical `eax=001D5078 ecx=007BFFBC edx=00000293 esp=007BFF9C`."*** **The
Session found four**, across **three separate run sets**:

```
run-on-3   eax=0x001D5078 ecx=0x007BFFBC edx=0x00000293 esp=0x007BFF9C
age-on-1   eax=0x001D5078 ecx=0x007BFFBC edx=0x00000293 esp=0x007BFF9C
age-on-5   eax=0x001D5078 ecx=0x007BFFBC edx=0x00000293 esp=0x007BFF9C
hot-on-1   eax=0x001D5078 ecx=0x007BFFBC edx=0x00000293 esp=0x007BFF9C
```

**Four independent realizations, byte-identical registers, including `esp` — which is remarkable**, since a
stack pointer depends on the entire call path. **That strengthens the determinism the packet relies on**, and
it is recorded as a correction in the safe direction.

**A minor terminology note:** the packet says *"3/10 same-build ON realizations"*, which is the count from the
`arming-coverage-repeat` set alone. **Across all same-build ON runs the Session has archived, the count is
higher.** **The packet's number is defensible as scoped, but a reader should not treat 3/10 as the total.**

## What the packet gets right, and it is the part that matters most

**The 59-entry stride-8 table is treated as a LEAD throughout, never as authority.** Line 6 says it in terms:

> *"this is a **LEAD**, neither validated table boundary nor proof all entries are distinct or callable …
> Boundary/index mismatch remains `O-OPEN`, **never reinterpret raw-scan alignment as authority**."*

**And the Session verified that caution is warranted:** mapping the first hit through the section table gives
`0x001D4BD4`, **but that does NOT establish the table STARTS there — the first hit may be an interior entry.**
**So the packet's framing is exactly right.**

**Line 5 states the discipline this project paid for:** *"decode its containing instructions from established
entry/labels, **NEVER byte-scan from an inferred code boundary or disassemble ASCII at `0x001D5078` as
code**."* **That is the recorded scar tissue — a byte-scan from an inferred boundary produced a wrong argument
count that reached an accepted record — stated in advance this time.**

**Line 7 adds the other half:** *"Confirm actual call site and target load rather than equating terminal `eax`
with an immediate load; **normalize addresses to `uint32`, never infer completeness from one generated-text
spelling**."* **That is the AGENTS.md two-spellings rule applied to this line.**

## Row set — designed fresh, and correctly

`O-IDENTITY` → `NON-TARGET` → **`O-DATA-AS-CALL`** → **`O-ALTERNATE-PATH`** → `O-OPEN`.

**The two positive rows are well distinguished:** `O-DATA-AS-CALL` requires a **unique labelled call AND a
complete reaching-definition chain** identifying the pointer field, writer and the erroneous interpretation,
**with independent XBE/generated agreement**; `O-ALTERNATE-PATH` covers **a unique labelled call but distinct
feasible paths or an expected-vs-observed target mismatch**. **Both require a complete chain, so neither can be
reached by partial evidence.**

**And line 18 keeps the discipline:** *"silence, capped logs, sampled history and another run's coverage never
prove absence. Per-realization findings precede any across-run generalization; disagreement leaves general
cause `UNKNOWN`. **Archived frequency 3/10 is descriptive, not a promise of recurrence or a license to retry
until target.**"*

## Scope discipline — the retired line is correctly sealed

**Line 19:** *"Retired NULL rows stand, its read path is dead, canonical-write channel uncertifiable, producer
parked; **never reopen them or cite DR records.**"* **And line 10:** *"no DR record or NULL-line instrumentation
may be a row input."* **The ceiling is respected rather than worked around.**

**Instrumentation: NONE pre-authorized**, and line 10 sets a **high bar** for any proposal — a **named static
gap**, a **non-generated seam**, an **event-owned write-once record**, **uncapped counts keyed to
source-derived finite classes**, **fixture positive/negative and overflow/loss gates**, an **OFF structural
control**, and **off by default at closure** — **and if no safe lossless seam exists, `O-OPEN`, not an invented
absence claim.**

## Freeze decision

**The packet is static-first, disciplined about its own leads, correctly scoped, and every checkable claim
reproduces.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class (`discovery`), its rows, or its prohibitions.** **No synthetic completion.**
**No toolkit change is required or authorized.** The producer line stays **PARKED**; `PIO_FREE` stays
**DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.

## Execution order

1. **Static-first steps 1–4** — locate the labelled caller, validate the table boundary and indexing, back-slice
   the `eax` definition, and bound the object from `ecx`/`esp`. **All from the XBE and generated source,
   anchored to `loc_` labels.**
2. **Rows offline.** Instrumentation only if a **named static gap** remains, with a separate scoped proposal.
3. **§5.8 acceptance.**
