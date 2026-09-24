# A3a-r25 plan-adequacy review — VERDICT: ADEQUATE (0 blocking, 0 false claims)

**Packet:** `A3a`, `docs/packets/a3a-ac97-codec-model.md`
**Revision reviewed:** `A3a-r25`, SHA-256 `6F907A42EAC6FD02392E11EADEF127AE840D97A245E771CD15B63A387F0E5BDC` (797 lines, 114,690 B)
**Reviewer role:** Planner (`docs/agent-workflow.md` §1)
**Reviewer child:** `70f82b36-9768-45b8-a078-ae10cade2ab5`
**Route (MEASURED from the child's own `request/header`):** `claude` / `claude-opus-5-5` @ `high`
**Verdict:** **ADEQUATE.** **Zero blocking defects. `FALSE CLAIMS FOUND: 0` — the first claim-clean revision in fourteen.**
**Files changed by the reviewer:** none in either repository.

## Reviewer's own headline results

| Field | Result |
|---|---|
| `PACKET_REVISION_VERIFIED` | **MATCH** |
| `PINNED_TOOLING_HASHES` | **MATCH** — helper `da2a11a3…`, controls `bea4416a…`; `Get-FileHash` under `powershell -NoProfile` equals the packet's L519-520 literals; `pins` exits 0 and prints its `PASS:` line |
| `PREMISE_FRESHNESS` | **PASS** — the record's packet/helper/controls hashes all match; the `r12`–`r18` records it lists exist; the `r19`/`r20` records it says are absent are absent; the baseline and p5 facts reproduce |
| `CONTRACT_COMPLETENESS` | **INTACT** — `AC1`–`AC6` × 12 §5 fields; **15** decision rows (L677-691); named classes L719; next-packet table L741-758; closure L766; steps 0–6 L251-347 |
| `FALSE CLAIMS FOUND` | **0 — none** |
| `VERDICT` | **ADEQUATE** |

## The reviewer's checks

1. **Hash, literals, `pins`** — CONFIRMED.
2. **Contract completeness** — CONFIRMED, with line references.
3. **Controls suite** — exit 0, **33 lines ending `OK` = 32 assertion lines + the summary**; the table's arithmetic (22 rows + 4 + 5 + 1 = 32) is correct. CONFIRMED.
4. **The five `AC2` commands** — guard `ok       : True`; `verify` exit 0; `pins` exit 0; `check` on the known-bad baseline exit 1 with clauses A and D failing; `test_run_profiles.py` exit 0 (31 tests). CONFIRMED.
5. **The freshness record's numbers and pointers** — CONFIRMED.
6. **Spot-checked citations** — `recomp_0005.c:21988` `edi=0x3E8`, `:22022` `MEM32(-20971216)` = `0xFEC00130`, `:22030` `ebx=0`, `:23655` `PUSH32 0x1A72E0`; `kernel_bridge.c:2200-2204` an ungated `fprintf`+`fflush`, `:5261` the `KERNEL_LOG_ON` gate; run-profiles L180 the heading of a 292-line file; p5 L2839 the vector-6 line; baseline L786 the launch page with titleid `0x5345000A`, 989 lines, max `#200`, vectors 3 and 1 only, no `0x001A72E0`, one `HalReturnToFirmware`. CONFIRMED.
7. **`AC2` exit paths** — exit 1 → `R-AC2-PROVENANCE` FAIL ("evidence not bound"); exit 2, or exit 1 with a `BLOCKED:` message → `R-AC2-UNKNOWN`; exit 0 without a `PASS:` line or any other code → FAIL. **Both `AC2` rows precede the model-attributing rows**, so **no path misattributes to the model.** CONFIRMED.
8. **False operative claims** — **none found.** CONFIRMED.

## Advisories (two, both cosmetic; recorded as accepted-and-deferred)

1. **Record pointers are incomplete.** L47-50 enumerate adequacy records through `r18` and say
   `r19`–`r22` have none; they say nothing about `r23` (no record) or `r24` (**its record exists**).
   **Nothing stated is false** — the reviewer's own words: *"This is incomplete, not false."*
2. **`R-AC2-PROVENANCE` lists clauses A–E and omits F.** The row's "any `AC2` FAIL branch" already
   covers F through `Get-FileHash`, so this affects wording only.

**Both were repaired in `r26`, which was then REVERTED.** The owner's instruction is to promote the
**exact reviewed revision**; `r26` was never reviewed, so promoting it would promote unreviewed bytes
— precisely the drift error this packet has suffered before. **`r25`'s bytes were restored and
verified byte-for-byte** (`6F907A42…` recomputed). The two advisories are therefore recorded here as
**accepted and deferred**: they are cosmetic, neither is false, and neither affects any criterion
predicate, command, decision row or pin.

## What PASS would still not establish (reviewer's list)

That `GS.bit8 := GC.bit1` is faithful hardware behaviour — it rests on secondary sources (xemu, Linux
`intel8x0`); that the value is **derived** rather than constant — only `AC1`'s manual review bears on
that; that audio works — no audible output, DSP execution or DirectSound success, and no proof the
next stop is benign; that the execution environment can be trusted beyond the stated invocation
(trust boundary 5); and that the host never writes `GC` — the census finds **literal** addresses only,
not addresses computed at run time.

## Session disposition

`A3a-r25` is **ADEQUATE** with **zero blocking defects and zero false claims**.

**This ends the longest loop in the packet's history.** Fourteen revisions returned ADEQUATE with zero
blocking defects; the first twelve each carried false claims, every one of them in *prose about past
revisions* rather than in the operative contract. The two structural changes that broke the loop were
extracting the packet-level history (`r21`) and, decisively, the Session stopping its pattern of
adding narrative claims about its own repairs — the `r25` review found the operative contract clean
and no claim false.

**`A3a-r25` is the revision to promote.**
