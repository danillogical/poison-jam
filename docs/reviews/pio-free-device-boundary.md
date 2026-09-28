# `PIO_FREE` device boundary — the trapped route, measured from source and the original XBE

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Why this record exists:** the `PIO_FREE-model` discovery asks what device is behind `0xFE820010` and
whether a non-synthetic model can be specified. **The Session found the address resolution and the current
stub's semantics by reading pinned source and disassembling the original XBE** — before the packet's
execution step. Recording it now so the discovery does not have to re-derive it, and so the Planner can
check its premises against it.

**Method:** pinned toolkit source at `c151d4e`, game source at `510da98`, and
`inspect-jsrf.py disasm` against the original XBE. **Read-only; no build, no guest run.**

---

## 1. The address resolves into the APU **VP** block, not a separate PIO port

| Step | Value |
|---|---|
| Guest address | `0xFE820010` |
| APU base | `0xFE800000` |
| Offset from base | **`0x20010`** |
| APU dispatch arm (`apu_core.c`) | `0x20000–0x2FFFF` → **VP block** |
| VP-relative offset | **`0x20010 - 0x20000 = 0x10`** |
| Toolkit name | **`NV1BA0_PIO_FREE` = `0x00000010`** (`apu_regs.h:128`) |

**So the "PIO_FREE" of `A4p`'s question is the VP frontend's method/register `0x10`, reached through the APU
aperture at `0xFE820010`.** `A4p`'s "PIO" naming and the VP route are the **same** address — not two
candidate devices. This is the identity the packet's Question asks about.

## 2. The current stub is a **pretence**, and the packet is right to call it that

`apu_vp.c:551-562` (pinned `c151d4e`):

```c
uint64_t mcpx_apu_vp_read(void *opaque, hwaddr addr, unsigned int size)
{
    (void)opaque; (void)size;
    switch (addr) {
    case NV1BA0_PIO_FREE:
        return 0x80; /* Always pretend queue is empty */
    default:
        break;
    }
    return 0;
}
```

**It returns a constant `0x80` and every other VP offset reads `0`.** The comment states the intent
outright: *"Always pretend queue is empty."* **This is a synthetic completion in the `A4b2` sense — a
fabricated value, not a model.** `A4p`'s own limits already concede it does not establish *"whether `0x80`
is the true device value."*

## 3. The guest's threshold forms — and why `0x80` "works"

The `A4p` site `0x001A2296`, disassembled from the original XBE:

```
001A2296 mov      eax, dword ptr [0xfe820010]
001A229B and      eax, 0xfffffffc
001A229E cmp      eax, 4
001A22A1 jb       0x1a2296          <-- loop WHILE (val & ~3) < 4
001A22A3 ...proceeds
```

`0x80 & 0xFFFFFFFC = 0x80 = 128 ≥ 4`, so **the constant makes this loop exit immediately.** The second form
at `0x001A2A7F` is a **variable** threshold:

```
001A2A7F mov      edx, dword ptr [0xfe820010]
001A2A85 shr      edx, 2             <-- free count = val >> 2
001A2A88 cmp      edx, ecx
001A2A8A jb       0x1a2a7f           <-- loop WHILE (val >> 2) < ecx
```

**Two readings of the same word:** one masks (`val & ~3`) and compares to a constant `4`; the other shifts
(`val >> 2`) and compares to a **register**. `A4p`'s Planner flagged exactly this — *"the variable
`(v>>2)<reg` thresholds must remain variable, never replaced by `0x80`."* **The shift-by-2 reading is the
informative one: it says the register carries a count in units of 4, i.e. a number of free 4-byte slots.**

**Consequence for any model:** `0x80` is not a neutral "empty" — under `val >> 2` it asserts **32 free
units**, and under `val & ~3` it asserts a large free count. **A truthful model must produce a value whose
two readings are both defensible**, which the constant cannot be shown to be.

## 4. Writes dispatch **synchronously** — the crux the packet must resolve

`apu_vp.c:564-572`:

```c
void mcpx_apu_vp_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    MCPXAPUState *d = (MCPXAPUState *)opaque;
    (void)size;
    /* Dispatch known methods through fe_method */
    fe_method(d, (uint32_t)addr, (uint32_t)val);
}
```

**Every VP write is dispatched inline through `fe_method` and completes before the call returns.** There is
**no queue and no asynchronous drain.** So the current model's implicit claim is *"free space never
decreases, because work is never pending."*

**The false-model test the packet demands is therefore available and concrete:** a guest that writes a
voice command and then polls `0x10` expecting the count to *drop* would observe `0x80` throughout — the
model would report an unchanged free count across a write that must have consumed space. **The packet's
step 3 already names exactly this test** (*"a queued write that cannot truthfully leave the alleged free
count unchanged"*), and this record confirms it is well-posed against this code.

**And the neighbouring write target corroborates the queue reading.** The site writes the value to
**`0xFE820280`** = VP offset **`0x280`** = **`NV1BA0_PIO_SET_HRTF_HEADROOM`** (`apu_regs.h:158`), which
**`fe_method` handles**. So the guest's shape is **poll `0x10` for space, then push a method to a `0x2xx`
offset** — a **producer** pattern against a free-space counter. That is strong structural evidence the
register really is a **free-slot count**, and that the "PIO" name is apt.

> **A Session error, recorded because it is the same class as the ones that have bitten this project.**
> I first tried to decode that store by hand — reading the `.byte 0xa3` as the `A3` moffs opcode and the
> following bytes as the address — and got **`0xFE820282`, one byte off**. The **disassembler** gives
> `0xFE820280`, and `0x280` is the defined `NV1BA0_PIO_SET_HRTF_HEADROOM` whereas `0x282` is not a method at
> all. **The hand-decode was wrong; the tool was right.** The lesson is the one `AGENTS.md` already states:
> **derive accesses from the original XBE with the tooling, never by ad-hoc byte arithmetic.**

## 5. The untrapped route is a **different** thing at the same address

`main.c:82-86` states that the APU's 512K *"only faults when `RECOMP_APU_TRAP` unmapped it. Nothing else
routes these: the aperture is otherwise plain memory."* And the packet notes the untrapped label is
**`MCPX_COUNTERS[0x020010]`**, a tick counter whose tick is **gated off under the trap**.

**So the same VA has two distinct behaviours** — a trapped VP register read, and an untrapped memory
counter — and **they must not be merged.** The packet already says so; this record confirms the source
basis for it.

## 6. What this establishes, and what it does not

**Establishes (from pinned source and the original XBE):**

- the identity and route: `0xFE820010` = APU `+0x20010` = **VP `+0x10`** = **`NV1BA0_PIO_FREE`**;
- the current model is a **constant `0x80` pretence** with every other VP offset reading `0`;
- VP writes are **synchronous** (`fe_method` inline), so the model has **no queue**;
- the guest's two threshold forms, one constant (`& ~3` vs `4`) and one **variable** (`>> 2` vs a
  register), and that the shift form reads the word as a **count in units of 4**;
- the guest's shape at the first site is **poll-then-push** to `NV1BA0_PIO_SET_HRTF_HEADROOM`;
- the untrapped route is a **separate** counter at the same VA.

**Does not establish:**

- **what the true hardware value or unit is** — no primary source has been consulted, and `A4p`'s
  *"whether `0x80` is the true device value"* remains open;
- **the capacity, reset state, drain/completion rule, overflow or ordering semantics** of a real queue —
  these are the packet's ledger leaves and remain **`UNKNOWN`**;
- **whether any poll actually exits in a strict run** beyond the observed prefix, or anything about guest
  liveness or boot;
- **that the constant is wrong** — only that it is a **pretence** whose truth is unestablished, and that a
  truthful model must satisfy the variable-threshold form.

**This record satisfies no criterion and claims nothing works** (§5.8). It is a source-grounded input to
the discovery, not a substitute for it.
