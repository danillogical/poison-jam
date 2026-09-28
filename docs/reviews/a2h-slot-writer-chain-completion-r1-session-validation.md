# `A2h-slot-writer-chain-completion-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-writer-chain-completion.md`, authored by Planner
`d2da4ac0-a47a-4fcd-8557-7c9158af5bbf` (`codex/gpt-6-sol` @ `high`), committed `3f96ae2`.
**Authority:** the stage-1 `REJECT` (`a2h-slot-writer-terminal-acceptance-record.md`, `73facd4`) and the
Advisor's pre-stated handling (turn `01a0e8af`).

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-slot-writer-chain-completion-r1`** |
| Lines | **16** (17 with the trailing newline) |
| **SHA-256 (frozen)** | **`3F7AD922DADD5E8E1B3944EAFF6B461468C1C05DC2350973243022C16305C7FD`** |

## Validation — 22 of 22 checks pass

**⚠ One check reported MISSING on the first pass, and the fault was the Session's check string again: it
tested `int(text,16)` while the packet writes `int(text, 16)` — with a space.** **The Session read line 13 in
full and confirmed the rule IS present, verbatim.** **That is instance 13 of the extraction family, and the
response was the same as before: read the artifact rather than trust the check.**

### The four obligations are the entire scope, and each is correctly bounded

| # | Obligation | Packet |
|---|---|---|
| **1** | **INDEX provenance** | *"witness caller-side constants or a sound bounds argument over ARG1/ARG3, or report the precise untraced edge; **never solve ARG1 backwards from the slot**"* |
| **2** | **ORDER** | *"Log polls 1–3 show installed value and poll 4 shows `0x001D5078`, **proving change BETWEEN polls, not attribution**: bind installer → candidate/competitor write → fourth read … **absence of a log line is not negative attribution**"* |
| **3** | **ADDRESS VERSUS BYTES** | *"`Q(v)=(int)(v*255.0f+0.5f)` … is a **confirmed FORMULA, not proof this store produced `0x001D5078`**; four byte components in `0..255` fit ANY dword. **The SAME dword is an address pointing to the ADX filename string**"* |
| **4** | **FOLDED-IN SIB SWEEP** | *"the found writer does **NOT** close exclusivity: the earlier ModRM literal-displacement scan **was encoding-scoped**"* — **enumerate ModRM + SIB + computed/register-indirect/alias forms, state coverage, give each competitor the same treatment, no standalone sweep** |

**The index consequence is stated exactly:** *"`0x3EC+2064*4=0x242C` is **arithmetic ONLY**; indices `0..2063`
occupy `0x3EC..0x2428`, making 2064 **one-past** a 2064-element array. `ARG1=0` needs `ARG3≥2065`;
`ARG1=2064` starts at the slot … **neither is assumed.**"*

### The §6.1 rule and the instruction-boundary requirement are encoded

**Line 11 names the line's own failures as prohibitions:** *"old `0x0015F9E5/E8/EA/EF/F2` labels were off by
1–2 and `0x000D45D4` was mid-instruction (`0x000D45D2` starts it); **do not inherit them as anchors**."*

**And it requires:** *"Verify EVERY cited VA is an instruction boundary against original XBE bytes and
declared starts."* ✓

### The controls

**Line 13, verbatim:** *"paired guest VA `0x001D5078` and `0x001D5079` MUST yield printed dwords `30766A64`
then `3030766A`; parse `int(text, 16)`, NEVER `int.from_bytes(bytes.fromhex(text),"little")`; require
`(v1 & 0x00FFFFFF) == (v0 >> 8)` (PASSES, retaining `0x30`). **Mismatch invalidates the read, never silently
corrects guest state; XBE-byte control does not substitute for a dump gate.**"*

**That last clause is the REJECT's control lesson encoded** — **the previous execution used an XBE-byte
control while performing zero dump reads, and the packet now forbids presenting that as a dump gate.** ✓

## ⚠ The Session's thunk-mapping finding — the executor must have it

**`docs/reviews/a2h-thunk-argument-mapping.md`** (`e09534c`).

**The vcall thunk is a `JMP`, not a `CALL`** — **Session-verified from the bytes: `8b 01 ff a0 cc 00 00 00`,
and `FF A0` is the `/4` extension (`jmp`), not `FF 90` (`/2`, `call`).**

> **Because it is a `JMP`, `esp` is UNCHANGED from the original caller's frame.**

**So the mapping is:**

| Worker argument | Is the caller's |
|---|---|
| **ARG1** (the INDEX, `ebp`) | **SECOND pushed argument** |
| ARG2 (the float array) | THIRD |
| ARG3 (the TRIP COUNT) | FOURTH |

> **⚠ AND THE CALLER'S FIRST PUSHED ARGUMENT IS IGNORED BY THE THUNK ENTIRELY.**

**The packet's obligation 1 says to trace `[esp+8]`/`[esp+0xc]`/`[esp+0x10]` — which is correct — but it does
not state that this makes worker ARG1 the caller's SECOND argument and skips the first.** **The Session
records it so the executor does not count from the wrong end** — **the same class of error as the
ARG1/ARG3 displacement confusion that produced the REJECT.**

## Freeze decision

**The packet is four-obligation, correctly bounded, its controls are the Advisor's and the REJECT's, and it
carries the fail-closed index requirement.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class, rows or prohibitions.** **No synthetic completion.** **No toolkit change required
or authorized.** The producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**;
`A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.
