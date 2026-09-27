# `A4b2-r4` — Session mechanical verification of the frozen draft

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Authoring Planner:** child `b608eb13-a4d9-494d-87e9-20fb15937cf6`, `codex/gpt-6-sol` @ `high` (§1).
**Revision verified:** `docs/packets/a4b2-gp-clears-pending-word.md`, **`A4b2-r4`**, **134 lines**,
SHA-256 **`65BCB7F0C10C393205412C7F10D033604ED63BC982AC23C6967DC8F162F2D250`**.

`docs/agent-workflow.md` §5.1.5(2) makes the Session responsible for the mechanical parts and requires it
to **verify that every command runs before submitting the revision**. The authoring Planner is a
**read-only** role (§5.1.3) and explicitly flagged that it could not execute the new commands. This is
that verification. Every row is a **Session observation** — a command actually run in this session, not
an inference.

## 1. The six original-XBE disassembly ranges (AC-NOCPU population witness)

These are the packet's producing commands for the frozen site count. All six were run from the game root
against the **unchanged original XBE**; each ran successfully and each range contains exactly the store
the packet claims.

| # | Command (`python -X utf8 scripts\inspect-jsrf.py disasm …`) | Instruction found | Verdict |
|---|---|---|---|
| 1 | `0x0006DAB0 0x0006DAE0` | `0006DACE mov dword ptr [eax + 0x810], ecx` | **CORRECT** |
| 2 | `0x0006DBB0 0x0006DBEA` | `0006DBBA mov dword ptr [ecx + 0x810], eax` | **CORRECT** |
| 3 | `0x0006DEE0 0x0006DF00` | `0006DEF3 mov dword ptr [ecx + 0x810], eax` | **CORRECT** |
| 4 | `0x001A1747 0x001A1769` | `001A1751 and dword ptr [edi + 0x810], 0` | **CORRECT** |
| 5 | `0x001A18C0 0x001A18D5` | `001A18CE mov dword ptr [ebx], eax` + `001A18D0 cmp dword ptr [ebx], 0` + `jne` | **CORRECT** |
| 6 | `0x001A1F9B 0x001A1FBF` | `001A1FA7 mov dword ptr [edi + 0x10], ebp`, preceded by `001A1F9B mov edi, [0x1ba858]` and `001A1FA1 add edi, 0x800` | **CORRECT** |

Range 5 independently confirms the control site's guest semantics: `add ebx, 0x810` at `001A18C6`, the
store at `001A18CE`, then the spin `cmp [ebx], 0` / `jne 001A18D0` — the packet's `loc_001A18D0` wait.
Range 6 confirms the packet's `edi = MEM32(0x1BA858) + 0x800` derivation for site `0x001A1FA7`.

### Reconciliation to the generated stores

Each original instruction maps to the generated store the packet cites, verified by reading the
generated chunks:

| Guest site VA | Original instruction | Generated store | Matches packet |
|---|---|---|---|
| `0x0006DACE` | `mov [eax+0x810], ecx` | `recomp_0000.c:135283` `MEM32(eax + 0x810) = ecx;` | **YES** |
| `0x0006DBBA` | `mov [ecx+0x810], eax` | `recomp_0000.c:135406` `MEM32(ecx + 0x810) = eax;` | **YES** |
| `0x0006DEF3` | `mov [ecx+0x810], eax` | `recomp_0000.c:135871` `MEM32(ecx + 0x810) = eax;` | **YES** |
| `0x001A1751` | `and [edi+0x810], 0` | `recomp_0005.c:6524` `MEM32(edi + 0x810) = MEM32(edi + 0x810) & 0;` | **YES** |
| `0x001A18CE` | `mov [ebx], eax` | `recomp_0005.c:6746` `MEM32(ebx) = eax;` immediately before `loc_001A18D0: ;` at `:6748` | **YES** |
| `0x001A1FA7` | `mov [edi+0x10], ebp` | `recomp_0005.c:8088` `MEM32(edi + 0x10) = ebp;` | **YES** |

**The frozen count of 6 is correct**, and every baseline line number in the packet is accurate.

### The text-search dedup claim is correct

The packet asserts the pattern `MEM32\(.* \+ 0x810\) =` hits **four**, not three, because the
`0x001A1751` store also matches. Verified by `git grep -n "MEM32(.* + 0x810) =" -- src/recomp/gen`:
exactly **4** hits — `recomp_0000.c:135283`, `:135406`, `:135871`, and `recomp_0005.c:6524`. Deduplicating
the already-listed `0x001A1751` site leaves **three other** sites, giving 3 + 3 = **6**. **CONFIRMED.**
The packet's instruction to treat the text pattern as a **lead only** and use the XBE commands as the
population witness is therefore both correct and necessary.

## 2. The P3 can-fail check command

```powershell
git -C C:\Users\logic\Repos\xboxrecomp grep -n -E 'ep_ops|ep\.regs\[|0x50000' <build-toolkit-sha> -- src/apu
```

Run with the full SHA `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`: **exit 0, 14 hits.** Run with the short
form `3a3c7c1`: **exit 0, identical 14 hits** (git prints the abbreviated SHA as the path prefix). Both
forms work; the packet's `<build-toolkit-sha>` placeholder resolves either way.

Every hit matches one of the packet's **permitted forms** — no unexpected hit exists at this identity:

| Hit | Permitted form |
|---|---|
| `apu_core.c:30`, `apu_state.h:374` | comment mentioning `ep_ops` |
| `apu_core.c:648`, `:674` | comment mentioning `0x50000` (the unrouted note) |
| `gp_ep.c:568` | `ep_ops` **definition** |
| `gp_ep.h:48` | `ep_ops` **declaration** |
| `gp_ep.c:526` | `ep.regs[` **read** (in `ep_read`) |
| `gp_ep.c:558`, `:559`, `:562` | `ep.regs[` **writes, all inside `ep_write`** |
| `gp_ep.c:612`, `:613`, `:650`, `:651` | `ep.regs[` **reads** (in the frame function) |

**Positive controls the packet requires are all visible:** the definition (`:568`), the declaration
(`:48`), the comments (`apu_core.c:30`, `apu_state.h:374`), and `ep_write`'s `EPRST` write (`:559`).
**No call site or registration of `ep_ops` exists**, and `apu_core.c` has **no `0x50000` routing arm**.
The check can fail: it would trip on a call site, on a `d->ep.regs[` write outside `ep_write`, or on a
new routing arm. **The check is genuinely can-fail and currently PASSes at this identity.**

## 3. Constants and mechanisms AC-INPUTS decides from

Every finite-universe constant the packet states was verified against pinned source at `3a3c7c1`:

| Packet claim | Source | Verdict |
|---|---|---|
| MIXBUF 32 bins, `GP_DSP_MIXBUF_BASE=0x1400` | `apu_regs.h:334` `#define NUM_MIXBINS 32`; `:322` `#define GP_DSP_MIXBUF_BASE 0x001400`; `apu_watch.h:45` | **CONFIRMED** |
| FIFO 6 slots = 2 in + 4 out | `apu_regs.h:324-325` (`GP_OUTPUT_FIFO_COUNT 4`, `GP_INPUT_FIFO_COUNT 2`); `apu_watch.h:80` | **CONFIRMED** |
| PERIPH 128 offsets, base `0xFFFF80` | `dsp_cpu_regs.h:120-121`; `apu_watch.h:49` | **CONFIRMED** |
| DMA 4 region classes | `apu_watch.h:83` `#define APU_WATCH_DMA_CLASSES 4` | **CONFIRMED** |
| `N_SITES = 16` ≥ 6 | `apu_watch.h:110`; six sites listed `:85-108` | **CONFIRMED** |
| Freeze is write-once (`at_clear`) | `apu_watch.c:806-815` — `InterlockedCompareExchange(&s.at_clear.taken, 1, 0)`, then `memcpy(&s.at_clear.gpin, &s.gpin, sizeof(s.gpin))` | **CONFIRMED** |
| Five modelled PERIPH offsets; `0xFFFFB3` a placeholder | `dsp.c:56-57` (`v = 0; // core->num_inst; // ??`), `:59`, `:65`, `:68`, `:71`, `:74` | **CONFIRMED** |
| Retired 256-entry table and `GPIN_OVERFLOW` gone | one hit only, `apu_watch.h:13`, a comment saying the mechanism does not appear | **CONFIRMED** |

### The `at_clear` block structure AC-INPUTS parses

Read directly at `apu_watch.c:851-925`. `emit_gpin_block(tag, g)` prints **exactly five** `[GPIN] <tag>`
lines in fixed order — `mixbuf`, `periph`, `fifo`, `dma`, `flags` — and `emit_gpin_summary` (`:914-925`)
prints the header `[GPIN] at_clear seq=%u frame=%u` (`:921`) **only when** `s.at_clear.taken`, then calls
`emit_gpin_block("at_clear", &s.at_clear.gpin)` (`:923`). **The packet's "exactly five tagged lines" and
its header field names are correct**, and `summary` (`:919`) uses the same emitter — which is the basis of
the admitted inference the packet states.

The `at_clear` block is re-emitted at every summary cadence, which is what makes the packet's check (i)
(the block must be identical to the first) a genuine can-fail check against a write-once freeze.

## 4. Tooling and prerequisite commands

| Command | Result |
|---|---|
| `scripts\build-jsrf.py`, `run-jsrf.py`, `check-run-profile.py`, `check-dump-mapping.py`, `inspect-jsrf.py` | all **present** |
| `python -X utf8 scripts\run-jsrf.py --help` | `--seconds`, `--label`, `--probe`, `--profile {strict,exploratory,fixture}` all exist; the packet's invocation is valid |
| `python -X utf8 scripts\check-run-profile.py --help` | runs; reclassifies archived runs, exit 1 for exploratory/fixture/unknown/missing |
| `python -X utf8 scripts\check-dump-mapping.py <run-dir>` | runs; prints the `0x00011000` control read and `matches` / `content-mismatch` counts. (`--help` is treated as a run path and reports `MISSING` — expected, not a defect; the packet calls it with a run dir.) |
| `python -X utf8 scripts\inspect-jsrf.py memory <run-dir> <VA> <bytes>` | runs against the accepted `A4b1` R0 archive: `001BA858: 803C0000` |

**That last measurement also confirms the packet's motivating evidence independently**: `B = MEM32(0x001BA858) = 803C0000` in the accepted R0 archive, exactly as the packet states.

| Precondition | Result |
|---|---|
| **P1** — `Get-FileHash game\default.xbe -Algorithm SHA256` | **`FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`** — **matches the packet's required hash exactly** |
| **P2** — accepted toolkit identity | `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`, clean; `origin/main` in sync; `upstream/main` at `766ecef` |
| **P3** — see §2 above | **PASS at this identity** |
| Required toolkit symbols | `apu_watch_set_anchor_site` (`apu_watch.h:410`), `apu_watch_cpu_store` (`:406`), `apu_watch_freeze_at_clear` (`:396`), `apu_gpin_periph_read` (`:356`) all exist |
| `BOOT_SCRATCH_READ` / `GPIN_OUT_OF_UNIVERSE` mechanisms | exist (`apu_watch.c:122-123`, `:547-567`, `:657-669`) |

## Disposition

**Every command in the frozen `A4b2-r4` draft runs, and every mechanical claim it makes that the Session
checked is correct.** The six new XBE disassembly ranges are the right ranges, the dedup reasoning that
yields the frozen count of 6 is right, the P3 check is can-fail and currently passes, and all finite-universe
constants match pinned source.

**Two observations carried to the reviewer, not defects:**
1. `check-dump-mapping.py --help` reports `MISSING` because it treats the argument as a run path. The
   packet does not use that form.
2. The packet's `AC-BOOT` compares `0x173` words while the emitter prints **64** `pram` lines of 8 words
   each = `0x200` words (`gp_ep.c:398` passes `pram_words = 0x200`; `apu_watch.c:1012` loops
   `i + 8 <= pram_words`). `0x173` words fall entirely inside those 64 lines, so the two numbers are
   consistent — but a literal executor should not read "64 lines" as "only 64 words are compared."

This record is the Session's mechanical verification under §5.1.5(2). It is **not** the §5.3 adequacy
review and does not substitute for it; that review is by a **different fresh** GPT-6 Sol High Planner.

---

## Addendum — `A4b2-r5` command set (re-verified for §5.1.5(2))

**Revision:** `A4b2-r5`, **134 lines**, SHA-256
**`652088DB528E2ECEEC7E9954530FCBD0C6A9228A144B0AE9066B785FDDD40B8E`**.
**Authoring Planner:** child `dc828854-0cd8-4282-99fa-ab7fa6a45832`, `codex/gpt-6-sol` @ `high`.

`A4b2-r5` is the revision after the `INADEQUATE` verdict. The Session re-enumerated **every** executable
command in the r5 draft and confirmed the set is **identical to r4's**, which §1 above verified:

| Command group | Present in r5 | Verified |
|---|---|---|
| `Get-FileHash game\default.xbe -Algorithm SHA256` (P1) | line 23 | **yes** — matches `FD190557…F3EF9C` |
| P3 `git -C …\xboxrecomp grep -n -E 'ep_ops\|ep\.regs\[\|0x50000' <sha> -- src/apu` | line 26 | **yes** — runs, 14 permitted hits |
| Six `inspect-jsrf.py disasm` ranges (AC-NOCPU) | line 102 | **yes** — all six run and contain their stores |
| `run-jsrf.py --seconds 30 --profile strict` ×2 (R1/R0) | lines 51, 53 | **yes** — argument shape confirmed |
| `build-jsrf.py`, `ctest`, `check-run-profile.py`, `check-dump-mapping.py`, `inspect-jsrf.py memory` | line 31 | **yes** — all present and runnable |

**The `AC-BOOT` redesign adds no new command.** It is a parsing/reconciliation requirement over the R1
`jsrf_run.log` (`[GPBOOT]` headers and `[GPWATCH] counts` lines) plus the same one-off
`default.xbe[0x1A7D60 + 4i]` comparison r4 already required. Verified against source:

- `[GPBOOT]` prints `n=%u sge0=%08X sge0_va=%08X gprst=%08X prev=%08X` then 8-word `pram` lines
  (`apu_watch.c:1009-1019`), with `pram_words = 0x200` (`gp_ep.c:398`) → `i` steps 0, 8, … 504 =
  **exactly 64 lines**, so the packet's "64 contiguous `pram` lines … the full 0x200-word block" is
  **correct**, and `0x173` words fall inside them.
- The packet's anti-adjacency warning is **correct**: `apu_watch_gp_bootstrap_done` (`gp_ep.c:364`, the
  counts emission) precedes `apu_watch_gp_bootstrap` (`:373`, which does `s.boots++`), and
  `apu_watch_gp_bootstrap_done` has exactly **one** call site (`gp_ep.c:364`).
- `boots` is printed on every counts line (`emit_counts`, `apu_watch.c:827`), so last-counts
  reconciliation is mechanically available.

**`AC-CLEAR`'s predicate is unchanged.** Steps 1–8 (r5 lines 88–95) are byte-identical to r4's; only the
**Claim limits** sentence was added (line 96). That matches the binding Advisor ruling
(*"AC-CLEAR's predicate needs NO change — the defect is claim wording"*).

**Disposition: PASS** — every command in `A4b2-r5` runs, and the two new load-bearing parsing
requirements are supported by pinned source. Submitted for §5.3 review.

---

## Addendum 2 — `A4b2-r6` command set and row-routing verification

**Revision:** `A4b2-r6`, **134 lines**, SHA-256
**`3273409262437D42A500A1E988E39CB25234C2E5E2A96007A9BE74FEB6B250E5`** (hash recomputed by the Session
and matching the authoring Planner's report).
**Authoring Planner:** child `636dc591-2e30-41fb-8cf6-46d108135165`, `codex/gpt-6-sol` @ `high`.

### Command set

**Unchanged, and no command added.** The r6 draft contains the same executable commands as r4/r5, all
verified in §1 and Addendum 1 above: `Get-FileHash` (line 23), the P3 `git grep` (line 26), the tool list
(line 31), the two `run-jsrf.py --profile strict` runs (lines 51, 53), and the six `inspect-jsrf.py disasm`
ranges (line 102). The repair is decision-row text only; it introduces no new measurement.

### Scope of the change — verified against r5

Diffed by the Session against the r5 bytes. Exactly **three** regions changed:

| Region | r5 | r6 | Verified |
|---|---|---|---|
| `:3` | `A4b2-r5` | `A4b2-r6` | **changed** (revision identifier) |
| `:119` | `… **any AC other than AC-INPUTS** is UNKNOWN; AC-CLEAR PASS while AC-BOOT or AC-RUN is not PASS; or AC-INPUTS **is evaluated** and UNKNOWN …` | `… **any AC other than AC-INPUTS** is UNKNOWN; or AC-INPUTS **is evaluated** and UNKNOWN …` | **the intercepting cross-criterion clause is removed** |
| `:129` | `… (CLEAR PASS with BOOT/RUN not PASS already selected UNKNOWN) …` | `… otherwise AC-BOOT FAIL selects NOBOOT, including when AC-CLEAR PASS; otherwise AC-RUN FAIL selects NOFRAMES or NOEXEC, including when AC-CLEAR PASS …` | **rewritten to match the new precedence** |

**`AC-CLEAR` steps 1–8 (`:88-95`) are byte-identical to r5**, and **`AC-BOOT` `:71-76` is byte-identical to
r5** (including its `FAIL → R2-NOBOOT` direction at `:76`). `AC-INPUTS`' conditional evaluation is intact
at `:108` and restated at `:129`. That matches the binding requirement that only the row precedence change.

### Independent row-routing trace

The Session traced the ordered rows against the scenario the r5 reviewer gave, and against the
combinations that must still route correctly. Rows in order: `R2-DEFAULT-REGRESS` (`:118`),
`R2-UNKNOWN` (`:119`), `R2-CPU` (`:120`), `R2-UNATTRIBUTED` (`:121`), `R2-NOBOOT` (`:122`),
`R2-NOFRAMES` (`:123`), `R2-NOEXEC` (`:124`), `R2-NOCLEAR` (`:125`), `R2-EXPL-INPUT` (`:126`),
`R2-PASS` (`:127`).

| Scenario | First matching row | Correct? |
|---|---|---|
| **The r5 blocker:** `AC-BOOT` FAIL, `AC-RUN` PASS, `AC-CLEAR` PASS, `AC-NOCPU` PASS | `R2-UNKNOWN` no longer matches (no non-input AC is UNKNOWN; `AC-INPUTS` is `NOT EVALUATED`) → `R2-NOBOOT` (`:122`) | **YES — repaired** |
| `AC-BOOT` **UNKNOWN**, `AC-CLEAR` PASS | `R2-UNKNOWN` (first clause: a non-input AC is UNKNOWN) | **YES** — incomplete evidence stays UNKNOWN, as the reviewer required |
| `AC-RUN` FAIL (frames), `AC-BOOT` PASS, `AC-CLEAR` PASS | `R2-NOFRAMES` (`:123`) | **YES** |
| `AC-RUN` FAIL (`insns=0`), `AC-BOOT` PASS, `AC-CLEAR` PASS | `R2-NOEXEC` (`:124`) | **YES** |
| `AC-BOOT`+`AC-RUN` PASS, `AC-CLEAR` → R2-NOCLEAR | `R2-NOCLEAR` (`:125`) | **YES** |
| `AC-BOOT`/`AC-RUN`/`AC-CLEAR`/`AC-NOCPU` PASS, `AC-INPUTS` FAIL | `R2-EXPL-INPUT` (`:126`) | **YES** |
| all PASS | `R2-PASS` (`:127`) | **YES** |
| `AC-NOCPU` FAIL, or `AC-CLEAR` → R2-CPU | `R2-CPU` (`:120`) | **YES** (pre-existing priority) |
| `AC-CLEAR` → R2-UNATTRIBUTED | `R2-UNATTRIBUTED` (`:121`) | **YES** (pre-existing priority) |
| `AC-DEFAULT2` FAIL | `R2-DEFAULT-REGRESS` (`:118`) | **YES** |
| all four PASS, `AC-INPUTS` evaluated and UNKNOWN | `R2-UNKNOWN` (third clause) | **YES** — the conditional evaluation path is preserved |

**Every combination routes to the row the criteria themselves direct, and no clause can intercept a
directed FAIL.** The r5 blocker is repaired without disturbing any other route.

### Session note on the Planner's `AC-RUN` analysis

The Planner's matrix treats `AC-RUN` FAIL with `AC-CLEAR` PASS as reachable. The Session observes that the
**`AC-RUN` half of the removed clause appears to have been unreachable**, while the **`AC-BOOT` half was
reachable**: `AC-CLEAR` PASS requires `GP_CLEAR` with `insns > 0` (`:89`), which contradicts `R2-NOEXEC`
(`gp_insns = 0` on every counts line) and is impossible under `R2-NOFRAMES` (`gp_frames = 0` everywhere,
so no GP execution and no `GP_CLEAR`). `AC-CLEAR` steps 1–8 never consult `AC-BOOT`, which is why
`AC-BOOT` FAIL with `AC-CLEAR` PASS **is** reachable. This does not affect the verdict — removing the
clause is correct either way, and the row order still sends a reachable `AC-RUN` FAIL to
`NOFRAMES`/`NOEXEC` — but it confirms the asymmetry that made the r5 blocker real. **Session observation,
offered as a lead.**

**Disposition: PASS** — every command in `A4b2-r6` runs, the command set is unchanged, the edit is
confined to the three regions named above, and the row matrix routes correctly in every combination the
Session traced. Submitted for §5.3 review.

---

## Addendum 3 — `A4b2-r7` command set and repair verification

**Revision:** `A4b2-r7`, **136 lines**, SHA-256
**`93E410C3742D0E290A0C0BBA71DCFEC547A77ECAC497E389BA5D58026E551F95`** (hash recomputed by the Session
and matching the authoring Planner's report).
**Authoring Planner:** child `b430c48f-42ea-463f-99e6-97da5989e73a`, `codex/gpt-6-sol` @ `high`.
**Ground:** §5.4(1) — a blocking finding from execution with a concrete false-UNKNOWN scenario
(`docs/reviews/a4b2-r6-execution-ruling.md`).

### Command set

**Unchanged, and no command added.** Same executable commands as `r4`/`r5`/`r6`, all verified in §1 and
Addenda 1–2: `Get-FileHash` (line 23), the P3 `git grep` (line 26), the tool list (line 31), the two
`run-jsrf.py --profile strict` runs (lines 51, 53), and the six `inspect-jsrf.py disasm` ranges (line 104 —
confirmed still exactly **six**). The repair is contract text only.

### The four mandated repairs — verified present

| Mandated repair (Advisor) | Where it landed in `r7` | Verified |
|---|---|---|
| **1. Rebind `B` to validated log evidence**, cross-checked against `anchor va − 0x810`; multi-boot disagreement → UNKNOWN | Gates/definitions `:61` — `B` = the **common `sge0_va`** of all structurally complete `[GPBOOT]` blocks, bound **after** the complete-log, contiguous `n=1..N` and last-counts `boots=N` checks, with `N≥1`; requires all `sge0_va` identical **and** a complete write-once `CPU_ANCHOR` latch with `va = B+0x810` in non-wrapping uint32 arithmetic. Every failure mode → **UNKNOWN, never a dump fallback or PASS** | **PRESENT** |
| **1b. `AC-BOOT`'s `sge0_va = B` conjunct restructured** so it is not tautological | New independent oracle **`T = 0x803C0000`** (`:61`), the title's scratch page measured in the accepted A4a strict run, explicitly *"independent of this run's `[GPBOOT]` headers"*. `AC-BOOT` now requires `sge0_va = T` (`:75`, `:77`) while `B` is the log-bound common page. A reconciled common page `≠ T` is **AC-BOOT FAIL**, not a binding failure (`:61`) | **PRESENT — circularity resolved** |
| **2. No PASS-path dependence on dump integrity; G2 as a PASS-gate goes** | **`G2` is deleted entirely** — `grep '\bG2\b'` returns **0 hits** in the whole packet. `:60` states the dump boundary and its reason: *"no global R1 dump-integrity gate is required because it would protect no PASS-path input."* R1 mapping is checked **only** for the no-`GP_CLEAR` dump-based branches; R0's own `Wf0` keeps its own mapping check under `AC-DEFAULT2` | **PRESENT** |
| **3. Displaced-dump `Wf` must not be meaningful corroboration** | `:62` — `Wf` is meaningful **only** if that R1 archive's mapping check passes; *"`Wf` is not meaningful corroboration when R1 mapping fails."* `:90` — on PASS, `Wf` recorded as corroboration **only if** mapping passes and the word is readable, and *"Do not run/check R1 dump mapping merely to qualify this PASS."* `:92` — before steps 4–8, a failed/missing mapping check or unreadable `Wf` → **UNKNOWN**, *"not cleared/not-cleared"*, and *"`F` … cannot by itself bypass this guard."* `:98` — on a displaced dump, do not report `Wf` or route its apparent zero to CPU/UNATTRIBUTED | **PRESENT** |
| **4. `r6` artifacts are STALE; the revision re-executes** | `:6` records **fresh** R1/R0 identities; `:42` — *"Make **fresh** R1 and R0 below under this revision; prior-revision run archives are premise leads only and cannot satisfy any gate or AC here"*; `:135` — *"prior-revision logs do not replace fresh R1/R0 evidence"* | **PRESENT** |

### Session checks on the repair's soundness

- **The circularity is genuinely resolved.** `T` is defined from the **accepted A4a strict run** and the
  original XBE, not from this run's headers, so `AC-BOOT`'s `sge0_va = T` test can fail independently of
  `B`'s derivation. Verified by reading `:61`, `:75`, `:77`.
- **The Session's own measured values satisfy the new binding.** Across every run that recorded a
  `[GPBOOT]` block — R1 first run, R1 rerun, and the no-edit control — the header is identically
  `n=1 sge0=003C0000 sge0_va=803C0000 gprst=00000003 prev=00000001`, a single `n=1` header, and
  `sge0_va + 0x810 = 0x803C0810` equals the `CPU_ANCHOR` latch `va` exactly. So the new `B`/anchor
  cross-check is satisfiable on this hardware path, and `sge0_va = T = 0x803C0000` holds.
- **The trace-gating trap the Session flagged is handled.** `:42` now states *"R1 has
  `RECOMP_APU_TRACE=1`: a trace-off run has no `[GPBOOT]` blocks and is UNKNOWN here, not an occasion to
  substitute the dump."* That matches the measurement: the trace-off diagnostic run logged **zero**
  `[GPBOOT]` lines because the emission sits inside `if (apu_watch_trace_enabled())` (`gp_ep.c:394`).
- **The no-`GP_CLEAR` branches are correctly guarded.** `:92`'s new guard means a displaced R1 dump yields
  `AC-CLEAR` UNKNOWN rather than a false `R2-UNATTRIBUTED`/`R2-CPU`/`R2-NOCLEAR` — which is exactly the
  wrong-row class §3.1 names, and the row matrix the Planner reports preserves those branches only behind a
  passing mapping check.
- **Step 1 is preserved, not duplicated.** `:36` instructs the executor to *"Verify the already-applied
  step-1 calls/declarations/forwarder match this specification; preserve them rather than duplicate them"* —
  correct, since those edits are already in the working tree and were verified in §2 above.
- **Row order and precedence unchanged** (`:120-131`), including `AC-INPUTS` `NOT EVALUATED` outside the
  qualifying PASS path, and no clause intercepting a directed FAIL.

**Disposition: PASS** — every command in `A4b2-r7` runs, the command set is unchanged, all four mandated
repairs are present, and the two Session-flagged hazards (the `AC-BOOT` circularity and the trace-gating
trap) are both handled. Submitted for §5.3 review.
