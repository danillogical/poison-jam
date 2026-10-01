# T3 correction: the xemu gdbstub decoded the register block as x86-64

**Status:** defect found and fixed while re-running T3 against a live xemu. The T3
*acceptance* result does not change — it compares **memory bytes**, which were always
read correctly — but every **register** value the tool ever produced was wrong.

## The defect

`scripts/xemu-gdbstub.py` decoded the GDB `g` packet as x86-64: 24 registers in 8-byte
slots, `rax, rbx, rcx, …`. **The guest is 32-bit**, so the packet is the i386 layout:
16 registers in 4-byte slots, `eax, ecx, edx, ebx, esp, ebp, esi, edi, eip, eflags,
cs, ss, ds, es, fs, gs`.

Measured against a live xemu running JSRF, from the same 344 bytes:

| | read as x86-64 (wrong) | read as i386 (right) |
|---|---|---|
| instruction pointer | `rip = 0x0000000000000000` | `eip = 0x00193D67` |
| code segment | `cs = 0x0000BFFE88000000` | `cs = 0x00000008` |
| stack segment | `ss = 0x4007A00000000000` | `ss = 0x00000010` |
| data segment | `ds = 0xFFFFFF0000000000` | `ds = 0x00000010` |

The packet is **344 bytes**, which is not a multiple of 8 — so the wrong decode was
also misaligned from the first register onward.

## Why it survived

**The wrong decode was confident, self-consistent, and looked like a stopped guest.**
All four archived dumps reported `eip = 0x00000000`, which reads as "the emulator is
not executing anything" rather than "the decoder is reading the wrong bytes". Nothing
checked a register against an independent source, so nothing contradicted it.

`cs = 0x0000BFFE88000000` was the tell, and it was printed in every dump: a segment
selector is a small number, and a 16-hex-digit one cannot be a selector.

## The fix, and how it is verified

The layout is now i386 with a 4-byte width, and the packet's register names **are** the
guest's names — so there is no alias layer for a consumer to forget. The redundant
`registers_64` field is gone: it was the label that made a wrong decode look
authoritative. The tool now also reports `register_block_bytes` (344),
`register_undecoded_bytes` (280, the x87/SSE state it does not decode) and
`register_layout`, so a reader can tell a packet it understood from one it truncated.

**The verification that has teeth is a cross-check against the original XBE.** The live
guest reported `eip = 0x00193D67`; reading that address from the guest returned
`ff 86 f0 01 00 00`, and `inspect-jsrf.py data 0x00193D67` returns the **same bytes**
from the original image — `inc dword ptr [esi+0x1f0]`. A wrong decode cannot satisfy
that by accident, which is why it is a control rather than a note.

Measured after the fix, on the live guest:

```
eip=0x8001B030  esp=0x800395F0  ebp=0x80035C2C  eflags=0x00000246
eax=0x00000000  ebx=0x80035BDC  ecx=0xFFFFFFFF  edx=0x80010031
esi=0x80035A90  edi=0xD00725B8
cs=0x08  ss=0x10  ds=0x10  es=0x10  fs=0x20  gs=0x00
```

A real guest state: `eip` in kernel space, `esp` on a stack, and the selectors of a
32-bit protected-mode guest.

## Two errors in my own test fixture, found while fixing this

Recorded because they are the same failure mode one level down — a wrong constant that
makes every later run agree with the mistake:

- `ebx` was first written `00000000` where the probe returned `0xFD000000`;
- `eflags` was written so it decoded to `0x00020200` where the probe returned
  `0x00000202`.

**Both passed the controls I had written**, because those asserted only `cs`, `ss`,
`ds`, `es` and `eip`. The control now asserts **every** register against the probe's
own output, so no single field can be wrong and unnoticed.

## What this does and does not change

- **T3's acceptance is unaffected.** It compares memory bytes at a guest VA
  (`xemu-diff` MATCH, the `.text` control byte-for-byte), and memory reads were always
  correct — the defect was in the register decode only.
- **Any conclusion that rested on a *register* value from this tool is void.** No
  durable record cites one: the dumps in `logs/xemu-probe/` are scratch, and the
  launch record records asset paths and sizes only.
- **`xemu-diff.py` is unaffected** — it compares bytes and never decodes registers.


## T3's acceptance, re-run against a live guest

The owner had JSRF running in xemu, so the acceptance was re-run against a **live
guest** rather than an archived probe. All four criteria the plan names:

| Criterion | Result | Evidence |
|---|---|---|
| xemu's own config resolves all five assets | **PASS** | `xemu-oracle.py`: `result: CONFIGURED`, five `OK` rows, no missing assets |
| gdbstub reachable | **PASS** | `?` returned `T05thread:01;` |
| guest `0x00011000` reads the recorded `.text` control byte-for-byte | **PASS** | `8b512c85d28b4130c70190431c00741c`, identical to the control |
| `xemu-diff` MATCH against an archived recomp run | **PASS** | `MATCH (empty diff)`, both sides `98c60dd680442843` |
| self-vs-self empty, seeded byte found | **PASS** | `selftest: PASS` — 0 differences self-vs-self, 1 at `0x001C3F65` for the seed, 12 for truncation; current re-check, reproduced 2026-10-01 at `3be0adb` (script unchanged since `abd419c`): `logs/workers/t3-xemu-diff-selftest-2026-10-01.txt` |

**Asset paths are used in place from the owner's configuration.** Nothing was copied
into either repository, and only paths, sizes and existence were recorded — never asset
bytes.

## A hazard the re-run exposed: comparing against a `CONTENT_MISMATCH` dump

The first attempt at the `xemu-diff` criterion **reported `DIFFER, 16 byte(s)`**, which
looks like a real disagreement between the emulator and the recompilation. It was not.

**The run I passed was `20260930-053722-314-v3-repro-check`, whose dump is
`CONTENT_MISMATCH`.** Its XBE-backed content is displaced, so its bytes at `0x00011000`
are not the image at all — `check-dump-mapping.py` reports exactly the bytes `xemu-diff`
was comparing (`83c41456ff5010eb5d8b44240c85c06a`). Re-run against the `MATCH` dump
(`20260930-034938-427-ttd-exit-control`), the criterion passes with an empty diff.

**So `xemu-diff` can silently compare against displaced content**, and the failure looks
like a finding about the recompilation rather than about the dump. That is the same class
of error `AGENTS.md` already warns about for memory reads — *"a dump can be structurally
valid while XBE-backed content is displaced"* — but here it reaches a **comparison tool**
rather than a reader.

**The rule this session applies from now on:** run `check-dump-mapping.py` on the recomp
run **before** citing an `xemu-diff` result, and treat a `DIFFER` against a
`CONTENT_MISMATCH` run as a statement about the dump rather than about the code. The
acceptance above was re-run under that rule.

**Not fixed here, and deliberately:** `xemu-diff.py` could refuse a `CONTENT_MISMATCH`
run outright, or warn. That is a change to a tool with an accepted selftest, and the
choice between refusing and warning is a judgement about how the tool will be used — so
it is recorded as a follow-up rather than taken unilaterally.
