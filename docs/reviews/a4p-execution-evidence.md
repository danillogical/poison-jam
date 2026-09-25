# A4p-r1 execution evidence

**Packet:** `docs/packets/a4p-pio-gate-analysis.md`, `A4p-r1`, frozen SHA-256
`B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B`
**Executed:** 2026-09-24 by the Session, game `d52d23e`, toolkit `0d7929c`.
**Result:** all 28 sites **PASS**; 0 FAIL; 0 UNKNOWN. **Row selection deferred** — see
"Flags" below: two readings of the packet's own cross-check clause change the row, so the
Session escalated rather than choosing.

## E0 — identity

| | |
|---|---|
| `game\default.xbe` SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — matches the packet's baseline |
| game HEAD | `d52d23e14510ef4348d187b8bf102b5e68e478c4` |
| toolkit HEAD | `0d7929c86771dd0b971941592fd4f15436116e82` |

## E1 — frozen population

`python -X utf8 scripts\inspect-jsrf.py find 0xFE820010` → **`28 occurrence(s)`**,
`0x001A2297` … `0x001A611D`, all `DSOUND`. Decoded: 10 `A1` (`mov eax,[moffs]`) and 18
`8B /r disp32`, matching the packet's two lists exactly.

## E2 — reconciliation by value, not text

The packet's PowerShell command, run verbatim:

```text
28
0xFE820010 10
-25034736 18
001A2296 001A22D0 001A2A7F 001A303D 001A30E4 001A3414 001A34EA 001A35AA 001A3710 001A3847
001A38F4 001A39AE 001A3A48 001A3B69 001A3C21 001A3EB3 001A3F24 001A3FDB 001A4181 001A4242
001A4325 001A4A34 001A4E4C 001A5671 001A579E 001A57CD 001A60B8 001A611C
```

The 28 labels are an **identical set** to E1's 28, one-to-one, no extras either side.
Control (known-bad): a text search for `MEM32\(0xFE820010u\)` alone finds **10**, and
10 ≠ 28.

## E3 — indirect-access evidence

| Address | `find` | Resolution |
|---|---|---|
| `0xFE800000` | 7 hits: `001A2E4A 001A2E5F 001A2E65 001A2EDE 001A2F80 001A2FA4` (`.DSOUND`), `0022DB72` (`.data`) | The six `DSOUND` hits load `0xFE800000` as a base and take offsets from the table at `0x1B9ECC`, read as data: `2054 2058 205C 2060 2064 2068 206C 2070 2074` — which `apu_regs.h` names `NV_PAPU_TVL2D`…`NV_PAPU_NVLMP`, the **voice-list registers**. Not `PIO_FREE`. `0022DB72` holds the constant `0xFE800000` and **nothing references it** (`find 0x0022DB72` = 0). |
| `0xFE820000` | 0 hits | — |
| `0x00020010` | 1 hit, `.rdata 001E4888`, value `0x00020010` | A data constant; **nothing references it** (`find 0x001E4888` = 0). |

E3 is clean: no other path to `0xFE820010`.

## E4 — per-site analysis (all 28)

Every site: the loop is a threshold re-poll, contains **no store**, and on every exit path
`T` empties before any use. Where `T` reaches a boundary, the clause that empties it is
named. Shared callee `sub_001A1BAF` (`1BAF push esi` / `1BB0 mov esi,ecx` /
`1BB2 cmp [esi+4],0` / `1BB6 je` / `1BB8 mov cl,[esi]` / `1BBA call [0x1C4004]` / `1BC0 and
[esi+4],0` / `1BC4 pop esi` / `1BC5 ret`) never reads `eax`/`ebx`/`edx`/`esi` before its
first boundary, so it takes none of them as a C2 argument; `[0x1C4004]` = `0x800000A1` =
ordinal 161 = `xbox_KfLowerIrql`, `VOID __fastcall (KIRQL)`, reading only `cl`.

| Site | `r` | Function | Loop form, N | Exit path(s) and `T` | Clause that empties `T` | Result |
|---|---|---|---|---|---|---|
| `001A2296` | eax | `sub_001A2285` | `(v&~3)<4` | single exit `22A3 mov eax,[ebp+8]` | overwrite | **PASS** |
| `001A22D0` | eax | `sub_001A22BC` | `(v&~3)<4` | single exit `22DD`, `22E0 mov eax,[ebp+8]` | overwrite | **PASS** |
| `001A2A7F` | edx | `sub_001A29B4` | `(v>>2)<ecx` | single exit `2A8C xor edx,edx` | overwrite | **PASS** |
| `001A303D` | edx | `sub_001A3009` | `(v>>2)<ecx` | single exit `304A xor edx,edx` | overwrite | **PASS** |
| `001A30E4` | edx | `sub_001A309D` | `(v&~3)<0x4C` | exit `30F2`/`30F8`/`30FB` touch only `ecx`/`ebp`; `3101 xor edx,edx` | overwrite | **PASS** |
| `001A3414` | esi | `sub_001A332D` | `(v>>2)<ecx` | B: `342A movzx esi,[eax]`. A: `3469`→`346C call 1A1BAF` (`T={esi}`), `3471 pop edi`, `3472 pop esi` restores the pre-poll slot at `3338` | callee-save pop (C3) | **PASS** |
| `001A34EA` | edx | `sub_001A347A` | `(v>>2)<ecx` | B: `34FD lea edx`. A: `3562 pop edi`, `3563 lea ecx`, `3566 call 1A1BAF` (`T={edx}`) | **C1** at `3566` (see Flag 1) | **PASS** |
| `001A35AA` | ecx | `sub_001A3570` | `(v&~3)<0xC` | single exit `35B8 movzx ecx,[esi+0xC]` | overwrite | **PASS** |
| `001A3710` | ecx | `sub_001A368A` | `(v&~3)<0x48` | single exit `371E test eax,eax`, `3720 movzx ecx,[edi]` | overwrite | **PASS** |
| `001A3847` | edx | `sub_001A3804` | `(v>>2)<ecx` | B: `385A lea edx`. A: `38B7`, `38B8`, `38BB call 1A1BAF` (`T={edx}`) | C1 at `38BB` | **PASS** |
| `001A38F4` | edx | `sub_001A38C5` | `(v>>2)<eax` | B: `390A movzx edx`. A: `3950`, `3953 call 1A1BAF` (`T={edx}`) | C1 at `3953` | **PASS** |
| `001A39AE` | edx | `sub_001A395D` | `(v>>2)<ecx` | B: `39C4 movzx edx`. A: `39E1`, `39E4 call 1A1BAF` (`T={edx}`) | C1 at `39E4` | **PASS** |
| `001A3A48` | edx | `sub_001A39EF` | `(v>>2)<ecx` | single exit `3A55 xor edx,edx` | overwrite | **PASS** |
| `001A3B69` | edx | `sub_001A3B26` | `(v>>2)<eax` | single exit `3B76 xor edx,edx` | overwrite | **PASS** |
| `001A3C21` | edx | `sub_001A3BE0` | `(v>>2)<ecx` | B: `3C37 movzx edx`. A: `3C95`, `3C98 call 1A1BAF` (`T={edx}`) | C1 at `3C98` | **PASS** |
| `001A3EB3` | eax | `sub_001A3E58` | `(v&~3)<0x80` | single exit `3EC2 mov eax,[esi+8]` | overwrite | **PASS** |
| `001A3F24` | edx | `sub_001A3E58` | `(v>>2)<eax` | exit `3F31`→`3F34 jl 3FA9`. A: `3F3B movzx edx,word[ebx]`. B1: `3FB4 jmp 3FC4`→`3FC4 mov edx,[ecx]`. B2: `3FBA mov edx,[esi+8]` | overwrite (all three paths) | **PASS** |
| `001A3FDB` | eax | `sub_001A3E58` | `(v&~3)<0x80` | single exit `3FEA mov eax,[esi+8]` | overwrite | **PASS** |
| `001A4181` | eax | `sub_001A40F5` | `(v>>2)<ecx` | A: `4193 xor eax,eax`. B: `41F3`, `41F6 call 1A1BAF` (`T={eax}`) | overwrite / C1 at `41F6` | **PASS** |
| `001A4242` | edx | `sub_001A4212` | `(v>>2)<eax` | A: `4258 lea edx,[esi+0xC]`. B: `42C8`, `42CB call 1A1BAF` (`T={edx}`) | overwrite / C1 at `42CB` | **PASS** |
| `001A4325` | ebx | `sub_001A4212` | `(v>>2)<ecx` | A: `4382`, `4385 call 1A1BAF` (`T={ebx}`), `438C pop ebx`. B: `433B movzx ebx,word[edx]` | callee-save pop / overwrite | **PASS** |
| `001A4A34` | edx | `sub_001A49A7` | `(v>>2)<ecx` | A: `4A78`, `4A7B call 1A1BAF` (`T={edx}`). B: `4A4A movzx edx,word[eax]` | C1 at `4A7B` / overwrite | **PASS** |
| `001A4E4C` | eax | `sub_001A4E12` | `(v&~3)<8` | exit `4E59`, `4E5F`, `4E62` (store `edi`/`esi` only), `4E68 call 1A1BAF` (`T={eax}`) | C1 at `4E68` | **PASS** |
| `001A5671` | eax | `sub_001A5671` | `(v&~3)<0x20` | single exit `567E xor ecx,ecx`, `5680 xor eax,eax` | overwrite | **PASS** |
| `001A579E` | ebx | `sub_001A5742` | `(v&~3)<8` | A: `5793`, `5799 mov ebx,[ebp-4]`. B: `57CD`–`57D8` (C4 revisit of `(57CD,{ebx})`), `57DA mov ebx,[ebp-8]` | overwrite | **PASS** |
| `001A57CD` | eax | `sub_001A5742` | `(v&~3)<8` | A: `5817 mov eax,[ebp-8]`. B: `57F1`–`57F4`, `57F9 call 1A0EA1` (`T={eax}`) | overwrite / C1 at `57F9` | **PASS** |
| `001A60B8` | eax | `sub_001A6064` | `(v&~3)<4` | single exit `60C5`, `60CB mov eax,[0x1BA7D8]` | overwrite | **PASS** |
| `001A611C` | eax | `sub_001A6064` | `(v&~3)<4` | single exit `6129`, `6133`, `6136 call 1A1BAF` (`T={eax}`) | C1 at `6136` | **PASS** |

**C3 caller enumeration was never invoked at any of the 28 sites**: `T` was empty at every
`ret` and out-of-function `jmp`, either by overwrite, by a callee-save `pop`, or by C1 at a
preceding call. `ecx` sites terminate by C3's "`ecx` is dead" clause without enumeration,
and no `eax`/`edx` site reached a `ret` with `T` live.

## Flags — two readings of the packet's own text that change the row

**Flag 1 (affects the outcome row).** The packet's E4 cross-check clause says the ruling
resolved `001A34EA` by "C3 finds `edx` dead at the return address `001A363E`", and that "a
disagreement is recorded, and the site is `UNKNOWN`". Executing the packet's rule literally,
`T={edx}` first reaches `call 1A1BAF` at `001A3566` (verified: `3562 pop edi` /
`3563 lea ecx,[ebp-0x10]` / `3566 call 0x1a1baf` / `356B xor eax,eax` / `356F ret`), where
**C2 finds `edx` is not an argument and C1 removes it**, so `T` is already empty at the
`ret` and C3 is never reached. The site's *result* agrees with the ruling (PASS); the
*mechanism* differs. Whether a route difference is "a disagreement" that forces `UNKNOWN`
is not stated. **The two readings select different rows** — `O-GATE` (all 28 PASS) versus
`O-UNKNOWN (sites)` (this site listed) — so the Session escalated to the Advisor rather
than choosing (§2.2.6, §4.2).

**Flag 2 (latent; does not affect this run).** The packet's C3 caller-enumeration rule
says to find "generated-tree calls to `sub_<ENTRY>(`". The generated tree spells a call as
`PUSH32(esp, 0x001A363Eu); RECOMP_ABI_CALL(0x001A347Au, sub_001A347A);` — **no `(` after the
name** (`recomp_0005.c:12234`). A literal search for `sub_001A347A(` therefore matches only
the *definition* at `:11940` and finds **zero callers**, which the rule maps to "callers
cannot be enumerated → `UNKNOWN`". This never bit, because C3 was not invoked at any site,
but the rule text is wrong and a re-executor would hit it if any site ever reached a `ret`
with `T` live.

**Correction to an earlier record (advisory).** `docs/reviews/a4b-r3-adequacy-review.md`
cited the caller overwrite for `001A4E4C` at `0x1A5119`. The return address is `1A5128` and
the overwrite is `1A5130 mov eax,edi`; `1A5119 movzx eax,si` precedes the call at `1A5123`.
The conclusion is unaffected — `T` empties at `4E68` via C1 regardless.

## Selected outcome row

**Not selected.** The Session did not choose between `O-GATE` and `O-UNKNOWN (sites)`,
because Flag 1 is an ambiguity in the frozen contract that changes the row and §2.2.6
forbids a contract role resolving it. Escalated to the Advisor; the row will be recorded
here once ruled.
