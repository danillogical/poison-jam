# `A2h-rdata-call-target-r1` — execution evidence: **`O-OPEN`**, with the object identity CLOSED

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-rdata-call-target-r1`, frozen
**`31343C167746872A501A21C6D558F52B897A7CD571E43C9EB016D42341152593`**.
**Worker:** `4a2a58dc-defd-4aa2-bb9f-1f8986182c2b`, commit `eca26ed`.
**Finding:** `docs/reviews/a2h-rdata-call-target-execution-evidence.md`.

**Row: `O-OPEN`** — the call site is **unique and `loc_`-anchored**, the table is **validated as a table and
refuted as a code-target table**, **the object identity is CLOSED**, and **the writer edge does not close.**

---

## The Session verified every load-bearing claim independently

| Claim | Session verification |
|---|---|
| **call site `0x00193EB5` is `call eax`** | **bytes `8d 4c 24 18 51 ff d0`** at `0x00193EB0..0x00193EB6`; decode gives `lea ecx,[esp+0x18]` / `push ecx` / **`call eax`** — **exact** |
| **the table entry structure is `{u32 id, char* name}`** | **confirmed**: `0x001D4BC8` = `id=0x280, name=0x001D5088 'djv112_0.adx'`; `0x001D4BD0` = **`id=0x281, name=0x001D5078 'djv000_0.adx'`** |
| **`edx=0x293` is the entry id, entry at `0x001D4C60`** | **confirmed exactly**: index arithmetic gives `0x001D4C60`, reading **`id=0x293, name=0x001D5078, 'djv000_0.adx'`** |
| **the array ends at an `id=0xFFFFFFFF` sentinel at `0x001D4DA8`** | **confirmed**: `id=0xFFFFFFFF` |
| **`0x2268 + 0x1C4 = 0x242C`** | **arithmetic confirmed** |

**The Session's own table claim was WRONG and the Worker refuted it correctly:** `0x001D4BD4` is **the `name`
field of the entry at `0x001D4BD0`**, not a table start. **The Session had flagged it as a lead precisely
because the first hit might be interior — and it was.** **The packet's line-6 warning did its job.**

---

## The strongest result: the OBJECT IDENTITY is closed

**This is the part worth recording as established, because it was the packet's step 4 and it is done:**

| Step | Value |
|---|---|
| **Callback field** | **`eax = MEM32(esi + 0x1C4)`** at `loc_00193E62`, **NULL-tested before the call** |
| **`esi`** | the incoming `ecx` — **the context** |
| **Context identity** | **`device + 0x2268`** (`D3D 0x0018CB60 lea edx,[eax+0x2268]`; passed as `ecx` at `0x0018E1D0`/`0x0018E1F4`) |
| **⇒ field alias** | **`0x2268 + 0x1C4 = 0x242C`** — **the Session's installer field and the Worker's callback field are THE SAME FIELD** |

**That resolves the Session's own puzzle:** *"a search for a plain `MEM32(reg + 0x242C)` load finds nothing."*
**It finds nothing because the field is CALLED, not loaded — it is a function-pointer slot read indirectly.**

**And the Session's suggested falsification test came out AFFIRMATIVE:** `sub_0015F9E0` is called from
**exactly one site (`0x00123319`, in `sub_00012210`) with `ebx = 0`**, so the NULL-substitution path installs
**`0x15F9D0`** — **exactly the refcount thunk the ICALL trace shows at cycle position 3.**
**The installer hypothesis is not refuted; it is corroborated end-to-end.**

**The `ecx − esp = 0x20` signature is DERIVED, not matched** — it follows from the ABI (`sub esp,0x14`; 4
pushes; `push eax`; callee `ret 8`; 2 pushes; return address). **And the signature is UNIQUE across the whole
executable: 4 sites match "`ecx ← [esp+N]` then `call reg`", and exactly ONE has `ecx−esp = 0x20`.**
**That is what makes the call site provable rather than plausible.**

---

## Why `O-OPEN` is the correct row — and the Worker was right to refuse promotion

**The writer edge does not close.** `[reg+0x1C4]` has **exactly ONE access in the entire D3D section — the
read.** **The field's only static writer is `sub_0018CE30`, whose only static callers pass the constant
`0x15F9D0`.** **The constructor/populator of the `device+0x2268` context is ABSENT from the generated and
recovered translation** (`0x0018CB60` merely **zeroes** it), **so any write from that path is invisible to
static analysis.**

**And a second, independent gap:** `sub_0013AEB0` — the consumer of the `name` field of the very table in
question — **stores into `[esi+8]` and `[esi+0x14]`**, so **a non-static route for the filename pointer cannot
be excluded.**

**So `O-DATA-AS-CALL` is NOT selected**, and **the packet's own line 22 forbids it**: *"A broken call, empty
source search, unexercised branch, stale build, sampled loss or non-target cannot select
`O-DATA-AS-CALL`."* **The mechanism is strongly suggested — a filename pointer occupying a code-target slot —
and "strongly suggested" is exactly what the packet says must not be promoted.**

**`O-ALTERNATE-PATH` also correctly fails:** there is **one** call site, **one** field, **one** static writer;
**the packet requires *verified distinct* paths, and the alternates here are *unverified*, not distinct.**

---

## The first unclosed edge, stated precisely

> **The reaching definition of `MEM32(device + 0x242C)` on the fourth polling iteration is not statically
> determinable, because (a) the field's only static writer is `sub_0018CE30`, (b) its only static callers pass
> the constant `0x15F9D0`, and (c) the constructor/populator of the `device+0x2268` context is absent from the
> generated and recovered translation, so any write from that path is invisible to static analysis.**

**The Worker's own next-packet suggestion is sound:** **the smallest closable edge is the `device+0x2268`
context constructor/populator** — **recovering those functions into the translation would make the write
surface static.**

---

## What this line established, in order of durability

1. **The call site is UNIQUE and provable** — `0x00193EB5 call eax` in D3D `sub_00193D90`, anchored to a
   **declared recovered boundary**, with a **unique ABI signature** across the executable.
2. **The object identity is CLOSED** — the callback slot is `[device+0x2268+0x1C4]` = **`[device+0x242C]`**,
   the same field the Session's installer writes.
3. **The mechanism is strongly suggested and explicitly NOT promoted** — a **filename pointer occupying a
   code-target slot**, with the writer edge unclosable from the current translation.
4. **The Session's table hypothesis was refuted**, correctly, by the packet's own interior-boundary warning.
5. **The Session's installer hypothesis was CORROBORATED**, via a falsification test the Session proposed and
   the Worker ran.

## Prohibitions and status

**Static analysis only** — no run, no source edit, **no instrumentation** (none was authorized).
**No synthetic completion.** No generated-code edits; `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`
unchanged. **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**. **The retired NULL line was not reopened and no DR record was cited.** **Float-bit siblings
remain contrastive — no family claim.** **All eight guards pass.**
