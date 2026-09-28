# `A2h-callback-slot-writer` — Session execution of the identity gate: **the alias FAILS to bind**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-callback-slot-writer-r1`, frozen
**`D2CA02E17E2BACC1DE3726B3247965EA3A3B3061B504C06C9601F7FB4B894E3A`**.
**This executes the packet's FIRST GATE (identity discriminators 1 and 3) offline.**

---

## First: a byte-order trap that nearly produced a FALSE finding

**`scripts/inspect-jsrf.py memory` prints LITTLE-ENDIAN DWORD VALUES, not memory-order bytes.** **The
Session initially read its output as memory order and concluded `MEM32(0x19DCE0) = 0x30766A64`, i.e. the ASCII
text `"djv0"` — which would have made the D3D device global garbage and collapsed the predecessor's entire
object identity.**

**The Session settled the byte order with a KNOWN STRING** — the `.rdata` table holds `"djv000_0.adx"` at
`0x001D5078`, whose bytes are `64 6A 76 30 30 30 5F 30 2E 61 64 78`:

```
tool printed:  001D5078: 30766A64 305F3030 7864612E
```

**`30766A64` is `64 6A 76 30` read as a little-endian dword.** **So the tool prints DWORD VALUES, and the
`"djv0"` reading was the Session's error, not a finding.**

> **Recorded because this is the third time in this session the Session drew a false conclusion from a
> correct artifact read the wrong way** — and because the correct discipline is now demonstrated: **settle a
> tool's byte order with a known string before interpreting its output.**

## Second: the dump-integrity gate, and why it mattered

**AGENTS.md requires `check-dump-mapping.py` before interpreting XBE-backed memory.** **The Session ran it:**

| Result | Count |
|---|---|
| **PASSES integrity** | **1 of 24 runs** (`20260928-035337-386-a2h-repeat-on-1`) |
| **`CONTENT_MISMATCH`** | **23 of 24 runs** |

**In the 23 mismatched runs, `MEM32(0x19DCE0)` reads `0x00B21900`; in the ONE passing run it reads
`0x0019B200`.**

**`0x0019B200` is EXACTLY the value the generated source initialises it to:**

```c
recomp_0004.c:40150:  MEM32(0x19DCE0) = 0x19B200;
```

> **So the device global IS `0x0019B200` at runtime, and the mismatched runs' reading is an ARTIFACT.**
> **The integrity gate is what separated them — and the Session ran it before interpreting, which is why the
> false finding did not reach a record.**

## The identity gate's answer

**From the ONE integrity-passing run, with the byte order settled:**

| Read | Value | Meaning |
|---|---|---|
| `MEM32(0x19DCE0)` | **`0x0019B200`** | **the device, matching the generated initializer** |
| **`MEM32(device + 0x2268)`** | **`0x000000FD`** | **a small INTEGER — not a pointer, and not the device** |
| `MEM32(device + 0x242C)` | **`0x0015F9D0`** | **THE REFCOUNT THUNK — the installer DID run** |
| `MEM32(device + 0x100)` | `0x00000000` | the poller's snapshot field |
| `MEM32(device + 0x400700)` | `0x00000000` | the poller's while-nonzero test |

> ## **THE PACKET'S IDENTITY TEST ANSWERS NO: `[device+0x2268] != device` (it is `0xFD`).**
>
> **So `edi` is NOT `device+0x2268`. The alias `0x2268 + 0x1C4 = 0x242C` does NOT bind this call, and the
> slot read at `0x00193EB5` is `edi+0x1C4` for SOME OTHER OBJECT.**

**`device+0x2268` holding a small integer means it is a COUNTER/FIELD, not a sub-object whose first dword
points back at the device** — which is precisely the condition the alias needed and does not have.

## What this establishes and what it does NOT

**Established:**

1. **The device global is `0x0019B200`**, matching the generated initializer. **Verified in the only run whose
   dump passes the integrity gate.**
2. **`[device+0x2268] == 0xFD`, not the device** — **so the alias cannot bind this call.**
3. **The installer chain is REAL and it RAN:** **`MEM32(device+0x242C) = 0x0015F9D0`, the refcount thunk.**
   **So `sub_0018CE30` did install the constant, exactly as the generated source says.** **The chain is
   correct — it simply does not feed this call.**
4. **Discriminator 1's answer, from the disassembly:** **NO call site passes a literal
   `lea ecx,[reg+0x2268]`.** All four decodable sites do **`mov ecx,edi`** or **`mov ecx,[esp+0xC]`**, and all
   four read **`[esi+0x100]`** testing the **same bit `0x01000000`** before the call. **So the context arrives
   as a PARAMETER at every site** — its identity must come from each caller one level further up.

**NOT established — stated plainly:**

- **The one integrity-passing run has NO TERMINAL** — it never reached the `0x00193D90` call. **So its device
  state is a real device state, but not the FAILING iteration's state.** **The two global facts
  (`device = 0x19B200`, `[device+0x2268] = 0xFD`) are layout facts that should hold across runs; the
  `device+0x242C` value is that run's.**
- **`edi`'s actual identity is still UNKNOWN.** **The Session has shown what it is NOT; it has not shown what
  it IS.** **That requires the packet's discriminator 1 followed one level up: the caller of each containing
  function.**

## The row this points to

**`O-ALTERNATE-PATH`** — *"Verified distinct context identity (`edi≠device+0x2268`)… defeat the assumed
device-slot path"*. **That is now the evidence-supported direction, and the packet anticipated it.**

**`O-OPEN` remains available and is not excluded** — **the Session has refuted the alias but has NOT
identified the real context object, and the packet requires a *verified* distinct identity rather than a
refuted one.** **The Session records the refutation as a strong step toward `O-ALTERNATE-PATH`, not as the
row itself.**

## The methodological finding, which outlives this packet

**23 of 24 archived A2h runs fail the dump-integrity gate.** **Any A2h conclusion drawn from guest memory in
those runs is uninterpretable**, and **the Session's own near-miss shows how easily a mismatched read becomes
a confident false finding.**

> **A standing recommendation: score a run's integrity status BEFORE reading guest memory from it, and record
> the status alongside the read.** **This packet's discriminator 3 depends on it, and the Session has now
> shown the failure mode is live.**

## Prohibitions and status

**Static and offline only** — **no game run, no source edit, no instrumentation.** **No synthetic completion.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited.**
