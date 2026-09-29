# CRT 64-bit divide helpers replaced (owner instruction, 2026-09-28)

**Defect.** The committed generated tree came from a lifter that did not lift `rcl`/`rcr`
(upstream fixed it in `4dd267a`). It dropped eight `rcr` instructions, all inside the MSVC CRT
64-bit divide helpers' normalisation loop, which runs whenever the divisor needs more than 32
bits. Those quotients and remainders were wrong.

**Negative control (measured).** The old generated `__aulldiv` body, compiled on its own from
`git show HEAD:src/recomp/gen/recomp_0004.c` before this change:

| a / b | old body | correct |
|---|---|---|
| `0x2540BE4000 / 0x100000001` | `0x40BE3FFF` | `0x25` |
| `0xDEADBEEFCAFEF00D / 0x123456789` | `0x281BB0D7` | `0xC3B6B4D1` |
| `0xFFFFFFFFFFFFFFFF / 0x2540BE400` | `0xC2F09883` | `0x6DF37F67` |
| `1000 / 7` (32-bit divisor) | `0x8E` | `0x8E` |

**Identification** (disassembly of the original XBE):

| VA | Helper | Result | Preserves |
|---|---|---|---|
| `0x0017C9C0` | `__alldiv` | signed quotient `edx:eax` | `ebx esi edi` |
| `0x0017D4D0` | `__aulldiv` | unsigned quotient `edx:eax` | `ebx esi` |
| `0x0017D2C0` | `__aullrem` | unsigned remainder `edx:eax` | `ebx` |
| `0x001816B0` | `__aulldvrm` | quotient `edx:eax`, remainder `ebx:ecx` | `esi` |

All are stdcall, `ret 0x10`, arguments dividend low/high then divisor low/high. 24 call sites,
all through `RECOMP_ABI_CALL`.

**Change.** Hand-written bodies in `src/jsrf_crt.c`; the four generated bodies in
`recomp_0004.c` replaced by a pointer comment (declarations and dispatch entries kept);
the four VAs added to `config/manual-functions.json` so a regeneration keeps them manual.
`tests/test_crt_divide.c` (`jsrf_crt_divide`): 2116 cases across 23 values including
divisors above 32 bits, `INT64_MIN / -1`, stack adjustment and preserved registers — all pass.
Build and `ctest` 23/23 pass.

**Discovery result — the A2h allocation is not caused by this.** Strict run
`20260928-183145-568-crt-divide-fix-strict` (settings as `20260928-120818-632-a2h-attrib-inert-off`)
still requests 598,869,040 bytes from `0x00149E50` and dies at `[ICALL] invalid target 0x00000000
return=0014982E`; no ABI failure from the new helpers. The A2h successor proceeds as specified.

**Removal gate.** If a regeneration uses a lifter that lifts `rcl`/`rcr`, these may return to
generated code by deleting the four manual entries and the `jsrf_crt.c` bodies together.
