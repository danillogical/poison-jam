# A4p-r1 acceptance review — ACCEPT

**Packet:** `docs/packets/a4p-pio-gate-analysis.md`, `A4p-r1`, frozen SHA-256
`B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B`
**Evidence:** `docs/reviews/a4p-execution-evidence.md`, SHA-256
`3DF5D833E89863E5E9BD0264CC64154C6FEADBD6FE10FC1806F6323FF20981F9` (169 lines, 28 site rows)
**Reviewer:** Acceptance reviewer, first stage — child `875b0697-5ccf-423e-aa54-5adb67480ed2`,
route `workbuddy-ai` / `deepseek-v4.1-flash` @ `max`
**Verdict:** **ACCEPT.** Every obligation `AGREED`; `BLOCKING: NONE`. Row **`O-GATE`** independently selected.
**Second stage:** not run — a first-stage `ACCEPT` is final (§2.2).

## Reviewer's block (verbatim)

```text
CRITERIA:
  Contract identity frozen            AGREED  — Get-FileHash docs\packets\a4p-pio-gate-analysis.md = B8BDBFAE…87A4B
  E0 XBE hash                         AGREED  — Get-FileHash game\default.xbe = FD190557…EF9C
  E0 game HEAD                        AGREED  — HEAD 7aeaff6; d52d23e is an ancestor; git diff d52d23e..HEAD touches only the 2 review docs, so src/recomp/gen is "as committed at baseline"
  E0 toolkit commit                   AGREED  — git -C xboxrecomp log -1 = 0d7929c
  E1 population                       AGREED  — inspect-jsrf.py find 0xFE820010 → 28 occurrence(s), 001A2297…001A611D, all DSOUND
  E2 reconciliation                   AGREED  — packet PowerShell verbatim → 28 / `0xFE820010 10` / `-25034736 18` / 28 labels, one-to-one with E1 (each = hit−1 A1 or hit−2 8B /r)
  E2 control                          AGREED  — Select-String 'MEM32\(0xFE820010u\)' = 10 ≠ 28
  E3 FE800000                         AGREED  — find = 7 (6 DSOUND + .data 0022DB72); find 0022DB72 = 0
  E3 FE820000 / 00020010              AGREED  — 0 hits / 1 hit .rdata 001E4888; find 001E4888 = 0
  E3 table 0x1B9ECC                   AGREED  — data = 2054 2058 205C 2060 2064 2068 206C 2070 2074 (voice-list regs)
  sub_001A1BAF reads eax/ebx/edx      AGREED  — disasm 0x001A1BAF 0x001A1BC6: reads only ecx/[esi+4]/[esi]
  [0x1C4004] = ordinal 161            AGREED  — data 0x1C4004 = 800000A1; kernel_thunks.c:201 case 161 → xbox_KfLowerIrql; kernel.h:799 __fastcall(KIRQL)
  E4 loop shape (all 28)              AGREED  — every site's back-jump preceded by and/shr + cmp + jb, no store in any loop body
  E4 001A34EA                         AGREED  — disasm 0x001A347A…: 34EA/34F0/34F3/34F5; exit 34F7; 3563 lea ecx; 3566 call 0x1a1baf → C2+C1 empties T; PASS
  E4 per-site conclusions             AGREED  — 6 sites traced fully (001A2296 eax, 001A4E4C eax, 001A34EA edx, 001A3F24 edx, 001A35AA ecx, 001A4325 ebx); no use at any; remaining 22 checked for shape, in-loop stores, exit overwrite
ROW SELECTION:   O-GATE — independently selected; matches the Session's row. Nothing routes to O-UNKNOWN (identity), O-DATA, or O-UNKNOWN (sites): E0–E2 clean, 0 FAIL, 0 UNKNOWN, E3 clean.
BLOCKING:        NONE
ADVISORIES:      (1) 001A34EA row folds the fall-through exit into path B and omits 356D pop esi / 356E leave — conclusion unaffected. (2) Coverage limit: 6/28 sites re-derived end-to-end; the other 22 verified for loop shape, in-loop stores, and exit overwrite only. (3) Evidence artifact SHA-256 recorded (3DF5D833…81F9) as the contract's closure requires.
DISPOSITION:     ACCEPT
```

The reviewer attempted two falsifications and both came up empty: sweeping all 28 loop
bodies for stores found none before the back-jump, and `sub_001A1BAF` — the C2 fact most
sites rely on — never touches `eax`/`ebx`/`edx`. It confirmed the `001A34EA` route exactly
as recorded, so it is a route difference and not a conclusion disagreement.

## Deferred advisories (recorded, not acted on)

1. **`001A34EA` row wording** folds the fall-through exit into path B and omits
   `356D pop esi` / `356E leave`. Conclusion unaffected; record accuracy only.
2. **Coverage limit** — 6 of 28 sites re-derived end-to-end; the other 22 verified for loop
   shape, in-loop stores, and exit overwrite. Stated, not a defect.
3. **C3 caller-enumeration text is defective** (Advisor-ruled, no revision): it searches
   `sub_<ENTRY>(`, which matches only the definition. C3 ran at 0 of 28 sites, so no verdict
   depended on it. Any future C3 use must enumerate callers by value — XBE `call rel32` to
   the entry, cross-checked by the normalised target literal in `RECOMP_ABI_CALL`.
4. **Earlier-record VA correction:** `a4b-r3-adequacy-review.md` cited the `001A4E4C` caller
   overwrite at `0x1A5119`; the return address is `1A5128` and the overwrite `1A5130`.

## Next packet (from the `O-GATE` row)

An `A4b` revision that deletes `AC-PIO` and `R-PIO-DATA`, adds the precondition "`A4p`
ACCEPTED with `O-GATE` on XBE SHA-256 `FD190557…EF9C`" plus the claim limits, and folds in
the ordinary repairs. Whether that revision is one packet or is **split into smaller change
packets** is the next planning decision.
