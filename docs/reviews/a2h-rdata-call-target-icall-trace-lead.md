# `A2h-rdata-call-target` — Session offline lead: the ICALL trace localises the failure to a 4-call cycle

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Status:** **offline lead for the executing Worker — NOT a row, NOT a conclusion.**
**Why recorded:** the packet requires the reaching definition to come from `loc_`-anchored code.
**This narrows where to look; it does not substitute for the chain.**

---

## The finding: the failure sits at a FIXED POSITION in a repeating 4-call cycle

**`recomp_icall_fail_log` (`src/recomp_manual.c:19-34`) prints the last 16 indirect-call targets**, and in
every run that fails at `0x001D5078` the trace is a **strict 4-cycle**:

| position mod 4 | cycle 1 | cycle 2 | cycle 3 | cycle 4 |
|---|---|---|---|---|
| **0** | `0xFE000190` | `0xFE000190` | `0xFE000190` | `0xFE000190` |
| **1** | `0x00193D90` | `0x00193D90` | `0x00193D90` | `0x00193D90` |
| **2** | `0xFE0000B4` | `0xFE0000B4` | `0xFE0000B4` | `0xFE0000B4` |
| **3** | `0x0015F9D0` | `0x0015F9D0` | `0x0015F9D0` | **`0x001D5078`** ← **BREAKS** |

**The pattern is constant at all four positions except the last, where position 3 changes from
`0x0015F9D0` to the `.rdata` filename pointer.** **So the call target is NOT random — it is loaded from
something that held `0x0015F9D0` and now holds a filename pointer.**

## What each target is

| Target | Section | What it is |
|---|---|---|
| `0xFE000190` | synthetic | thunk **index 100** → table VA `0x001C40F0` → **ordinal 119** |
| `0x00193D90` | **D3D** | real code: `sub esp,0x14 / push ebx / … / mov esi,ecx / mov ebx,[esi]` — a **method taking `this` in `ecx`** |
| `0xFE0000B4` | synthetic | thunk **index 45** → table VA `0x001C4014` → **ordinal 145** |
| **`0x0015F9D0`** | **`.text`** | **`inc dword ptr [0x265174]; ret`** — a **reference-count increment thunk**, immediately followed by a second function at `0x0015F9E0` (`mov eax,[esp+4] / test eax,eax`) |
| **`0x001D5078`** | **`.rdata`** | **`b'djv000_0.adx\x00\x00\x00\x00'`** — a **filename** |

## Why this is a large narrowing

**The loop alternates two kernel calls with two guest calls, and the guest call at cycle position 3 is
supposed to reach a refcount-increment thunk.** **On the 4th iteration it reaches a filename instead.**

**That is the signature of a POINTER FIELD or TABLE ENTRY that changed value** — not of a random jump and not
of a stack smash. **And it localises the search to the single call site at cycle position 3.**

**Two candidate shapes, both statically decidable:**
- **a pointer field** that held `0x0015F9D0` and now holds the `.rdata` address — i.e. **something wrote a
  filename pointer where a function pointer belongs**;
- **a table whose index went out of range** and landed in the filename table.

**The `.rdata` table lead (59 stride-8 entries at `0x001D4BD4`) fits the second shape**, and the Worker should
test whether the position-3 call indexes it.

## What this is NOT

- **Not a row.** The packet requires a **complete reaching-definition chain** for `O-DATA-AS-CALL`, and this
  establishes only **where** to look.
- **Not `loc_`-anchored.** The trace is a **runtime observation**, so it **narrows the search** but **cannot
  itself certify the chain**.
- **Not evidence about the object** — `ecx`/`esp` bound the frame, and the object identification is still the
  Worker's step 4.

**Recorded so the Worker starts from the cycle position rather than re-deriving the trace, and so a reviewer
can see the narrowing was available and was not treated as a conclusion.**
