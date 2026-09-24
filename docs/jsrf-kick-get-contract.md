# JSRF pushbuffer kick / DMA GET contract

**Scope:** technical pushbuffer kick/DMA-GET contract derived from the original
XBE; not a current packet/status authority. It was established before historical
recovery work on `0x001918E0`. Current recovery/acceptance state lives in the plan and
review records. The instruction basis is `python -X utf8 scripts/inspect-jsrf.py disasm`
on the retail XBE plus the PRAMIN/RAMHT toolkit work used for this contract.

Source ranges (end exclusive): `0x00191530..0x001916A3`,
`0x001916B0..0x001916BF`, `0x001917F0..0x001918D3`,
`0x001918E0..0x001919BE`, `0x00190240..0x0019049C`.
Already-accepted neighbours used by the contract: `0x00191270..0x0019129D`,
`0x001912A0..0x00191384`, `0x00191390..0x0019143F`,
`0x00191440..0x001914FE`, `0x00196C0B`, `0x001945D6`.

## Facts

### Ring control block

`MEM32[0x0019DCE0]` is the ring/GPU context pointer used by the whole chain.
Fields referenced below, all off that pointer:

| Offset | Meaning | Evidence |
|---|---|---|
| `+0x00` | current write cursor (CPU PUT), guest pointer | `0x001918E2`, `0x0019153E` |
| `+0x04` | ring end limit for the cursor | `0x001918EA` |
| `+0x08` | flags word | `0x0019153A`, `0x0019024D` |
| `+0x24` | ring start, guest pointer | `0x00191583` |
| `+0x28` | ring end, guest pointer | `0x001915AC` |
| `+0x2C` | kernel read pointer, **guest physical** | `0x00191305`, `0x001915CB` |
| `+0x30` | kernel write pointer, **guest physical** | `0x001913AD`, `0x00191451` |
| `+0x34` | pointer to a dword holding the GPU-side GET | `0x00191574` |
| `+0x38`/`+0x3C` | submission-slot counter / mask | `0x001913FF`, `0x00191400` |
| `+0x40` | current DMA PUT value published to hardware | `0x001913DF`, `0x001912FE` |
| `+0x44` | pending wrap byte count | `0x001915CB` |
| `+0x4C` | submission record array | `0x00191402`, `0x0019160D` |
| `+0x2264` | pointer to the guest memory descriptor | `0x00191576` |
| `+0x2268` | mapped NV2A register base | `0x00191271` |

`MEM32[0x0019E328]` is the full ring size in bytes; `MEM32[0x0019E32C]` is
the same value used halved (`shr ebx,1` at `0x00191610`).

### CRITICAL — the kick is a real register write, already partially modeled

`0x00191270..0x0019129D` is the kick primitive, and it is **not** an unknown
MMIO address:

```
00191270  mov  eax, MEM32[0x19dce0]      ; ring context
00191275  mov  eax, MEM32[eax+0x2268]    ; mapped register base
0019127B  sfence
0019127E  mov  ecx, MEM32[eax+0x100410]
00191284  or   ecx, 0x10000
0019128A  mov  MEM32[eax+0x100410], ecx  ; <-- KICK
00191290  test MEM32[eax+0x100410], 0x10000
0019129A  jne  0x00191290                 ; <-- waits for the bit to clear
0019129C  ret
```

`+0x100410` is `NV_PFIFO_CACHE1_DMA_PUT` (0x100410 is exactly the offset
already named in the toolkit). So the guest kicks by **setting bit 16 of the
DMA PUT register and then polling until the hardware clears it**.

Two consequences that change the shape of the work:

1. `0x00191270` is an **uncalled helper**: `0x001912A0` inlines the same
   sequence at `0x001912C6..0x001912EA`. The `call 0x00191270` sites have not
   been located; treat `0x00191270` as an internal body unless a caller is
   found. Do not seed it as an independent function on speculation.
2. The poll at `0x00191290` is only satisfiable if the *model* clears bit 16.
   `nv2a_core.c` currently stores the raw value in
   `pfifo_write` `NV_PFIFO_CACHE1_DMA_PUT` and never clears bit 16, so this
   loop would spin forever once it is reached. **Clearing bit 16 to
   acknowledge a kick is the first required model change.** That is a real
   hardware behaviour, not a convenience stub: bit 16 is the kick request
   latch, and the engine clearing it is what the guest is waiting for.

### The GET wait helper `0x00191530` (`RET 8`)

Arguments: `[esp+4]`, `[esp+8]`. It does **not** push ESI/EDI/EBP across the
early exit but does pop what it pushes; EAX returns the new cursor. Two
exits:

- **Fast path** `0x0019153A..0x00191566`: when `flags & 4` is set, the
  function reads the dword at `[ring+0x350]+4` as the already-published GET,
  retires `cursor - get` bytes into `ring+0x354`, sets `ring+0x00 = new GET`,
  and returns. Nothing is written to hardware on this path.
- **Slow path** `0x00191569..0x001916A2`: computes `threshold` from the chip
  (getter object at `ring+0x2264`, field `+0x44`, with `0x80000000` forced
  on), wraps the cursor at `ring+0x28`, writes `ring+0x44`, writes the kick
  value at the cursor (`and edx,0xfffffff / inc edx` then store — the
  wrap marker), then walks the submission array at `ring+0x4C` looking for
  an entry whose second dword is below the cursor. On a match it calls
  `0x00191440` with `[esp+4]=1` (force).
- Both exits converge on `0x0019165C`: `ring+0x04 = cursor - 0x204`,
  then if `flags & 0x800` (test `ah,8`) it sets `flags |= 0x1000` and calls
  `0x001912A0` — **the kick** — before returning the cursor. Otherwise it
  calls `0x00191390` instead.

So the wait helper *is* the kicker: it decides whether the ring wrapped and
then either publishes through `0x001912A0` or through the submission-record
path `0x00191390`.

### `0x001916B0` — the two-argument wrap adjuster

```
001916B0  mov  eax, MEM32[0x19e328]   ; ring size
001916B5  push eax                    ; arg 2 = size
001916B6  shr  eax, 1
001916B8  push eax                    ; arg 1 = size/2
001916B9  call 0x191530
001916BE  ret
```

Plain `cdecl`-style call with no local cleanup (`ret`, not `ret 8`) — it
consumes its own pushes. Returns `0x00191530`'s EAX, i.e. the new cursor.
This is the "we ran out of room, skip to the halfway point" path.

### `0x001916C0` — the bounded reserve (`RET 8`)

`0x001916C0..0x00191707` is a separate entry with its own ABI: it reads
`ECX`/`EDX` from the stack, and when the requested `count*4 + 0x204` bytes do
not fit before `ring+0x30 + 0x200`, it **rewrites its own two stack arguments**
(`0x001916F8`, `0x001916FC`) to the grown bound and tail-jumps into
`0x00191530`. Otherwise it returns 8. That argument-rewriting-by-offset form
is unusual and must be reproduced exactly; it relies on `[esp+4]`/`[esp+8]`
being the return-address followers at that point.

### Submission record path `0x00191390`

Writes a 0x18-byte record at the cursor, records it in the ring's slot table
at `ring + 0x54 + slot*12` (`0x001913DB`, `0x001913E2`, `0x001913E5`,
`0x001913F3`), and when `flags & 2` is clear calls `0x001912A0` (kick).
`ring+0x30` advances by 2 per submission. The flag bit 1 suppresses the kick
so a batch can be published once.

### `0x00191440` — back-pressure wait (`RET 8`)

`[esp+8]` is the target PUT. It compares against `ring+0x30`. When
`ring+0x30` is *behind* the target by more than the outstanding window it
calls `0x00191150` / `0x001910E0` to fetch a submission record and spins in
`0x001914F0..0x001914F8` while `ring+0x30 - MEM32[ring+0x34]` is still below
the target. `[esp+0x18]` nonzero selects the "forced" record form that resets
`ring+0x2444` and emits the `0x40100/0x40110` marker pair.

This is the loop that will spin hardest if GET is never advanced by the
model. It is the direct reason the plan said not to recover the chain before
the completion contract existed.

### `0x001918E0` — the first method block (`0x001918E0..0x001919BE`)

- `ESI = ring`, first thing it does is the standard
  `if (cursor >= ring+4) call 0x001916B0` space check at
  `0x001918EA..0x001918F0`.
- Emits a 0x2C-byte method run at the cursor: NV097-class method pairs
  `0x42000/0x0E`, `0x44000/0x10`, `0x46000/0x11`, `0x40000/0x0D`,
  `0x42180/7`, `0x442FC/3`. Those are the NV097 surface/format method
  numbers already named in the toolkit's NV097 tables; the parameters are
  16-bit field pairs, consistent with the accepted `SET_SURFACE_*` shape.
- Zeroes `ring+0x201C` (8 dwords) and `ring+0x203C` (8 dwords), sets
  `ring+0x2020 = 3`, then emits a second run at `edx = cursor+0x3C`:
  `0x86184/3`, then a repeated `0x19` for seven dwords, then `0x11`.
- `ring+0x00 = edx` after the second run, pops, and **tail-jumps to
  `0x001917F0`** (`jmp`, not `call`).

### `0x001917F0` — the second method block, which kicks

- Same space check, then a 0x3C-byte run: `0xC0180/2`, `3/3`,
  `0x180190/4`, `9/0xA`, `3/3`, `8/0x401A8`, `0xC/0x41D6C`, `0`.
- Checks space **again** at `0x00191865` and may call the wrap adjuster.
- Second 0x3C-byte run: `0x409FC/1`, `0x100A50/0/0/0`, `0x3F800000/0x416BC`,
  `1/0x41E78`, `0x210000/0x41D80`, `1/0x41E68`, `0x7F800000`.
- `ring+0x00` updated, then `push edi` (`edi` is 0, zeroed at `0x0019185A`)
  and **`call 0x00190240`**. `ret` with no cleanup.

### `0x00190240` — the actual submit

`[esp+4]` is a flags word; only bit 4 (`test al,0x10`) and bit 4 after
masking are meaningful.

- Sets or clears `flags |= 0x200` on `ring+0x08` from that bit
  (`0x00190252`, `0x0019025A` — the clear is `and ecx,0xfffffdff`, so **bit
  9** is the one being toggled).
- Stores `flags & ~0x10` at `ring+0x2018`.
- When the masked flags are nonzero it jumps to `0x00190331` and does not
  submit. When zero (the `push edi` case, `edi==0`) it falls through and:
  - `or MEM32[0x19DED8], 0x1600` — sets the "engine busy" bits.
  - Space check again, then emits `0x41EA4/0x3C/0x300B80` plus a 12-dword
    block copied from `0x19A228` (`rep movsd`).
  - Three `0x00190FB0` calls building `0x400840/0x400880/0x4008C0` entries.
- `0x00190240` also has a second real caller at `0x0014D9D0..0x0014D9EC`,
  which pushes exactly `0` or `1` and then `xor eax,eax; ret 8`. That is the
  closest thing to an external "submit now / don't submit" control the game
  exposes, and its `RET 8` confirms `0x00190240` is a **one-argument
  (`RET 4`-style) cdecl** — the caller cleans one dword.

## Required model behaviour before the chain may be recovered

1. **Kick acknowledgement.** A write to `NV_PFIFO_CACHE1_DMA_PUT` with bit
   16 set must latch the kick, run the pending submission, clear bit 16, and
   leave the stored PUT readable. The `0x00191290` poll must terminate
   without any guest-side help.
2. **GET advances as a consequence of work, never on write.** GET may only
   move because a packet was executed to completion. A write by the guest to
   GET may be rejected or recorded, but it must not synthesize completion.
3. **`submit_pending` must not partial-commit on an unsupported method.**
   The existing rollback satisfies this; it must stay satisfied when the two
   `0x001918E0`/`0x001917F0` streams are executed, because the first real
   stream legitimately contains methods outside the current accepted subset
   (`0x42000`, `0x44000`, `0x401A8`, `0x41D6C`, `0x416BC`, `0x41E78`,
   `0x210000`, `0x41D80`, `0x41E68`, `0x180190`, `0x86184`, `0x409FC`,
   `0xC0180`, `0x3F800000`, `0x7F800000`, `0x100A50`).
4. **A blocked stream must stay blocked and diagnosable.** Widening the
   accepted method set is how this packet is expected to *stop*: the first
   stream that contains a method with no proven class contract must report
   its subchannel/method/param and leave GET, PUT, bindings and clip state
   untouched. That explicit stop is the success condition for the next
   packet, not a regression.

Point 3 is the reason this contract is being written before recovery rather
than after: the very first stream the game pushes includes roughly sixteen
distinct method numbers, of which only `SET_OBJECT` and the two clip methods
are currently accepted. Recovering `0x001918E0` will therefore surface an
`unsupported_method` stop almost immediately. That is expected and useful,
and it must be treated as evidence rather than worked around.

## Open items

- `0x0019DCE0`'s ring context is created where `0x00192090`'s pushbuffer
  allocation publishes it; confirm the exact field initialisation order so
  `ring+0x34`, `ring+0x4C` and `ring+0x2264` are non-null before the first
  kick. The `0x00191530` slow path dereferences `ring+0x2264` immediately.
- Bit 9 of `ring+0x08` (`0x00190250`) and bit 2 of the same word
  (`0x0019153A`) both gate behaviour and are not yet given names in the
  toolkit model.
- `ring+0x2018`, `ring+0x201C`, `ring+0x2020`, `ring+0x203C` are written by
  `0x001918E0` and never read by the chain; they are almost certainly
  debug/instrumentation counters. Do not model them as functional state.
- The `call 0x00191270` sites are still unfound. If a later recovery needs
  `0x00191270` as an entry, search for the callers first rather than seeding
  it from the function database.
