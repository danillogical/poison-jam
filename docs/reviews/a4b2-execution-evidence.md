# `A4b2-r6` execution evidence

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r6`**, frozen SHA-256
**`3273409262437D42A500A1E988E39CB25234C2E5E2A96007A9BE74FEB6B250E5`** — verified at execution start,
unchanged throughout. **Not edited.**
**Row selected:** **`R2-UNKNOWN`** (gate **G2** fails for R1). See "Decision-row classification".

---

## 1. Preconditions (checked before any step)

| Precondition | Command / source | Result |
|---|---|---|
| **P1** — XBE identity | `Get-FileHash game\default.xbe -Algorithm SHA256` | **`FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C`** — matches. **PASS** |
| **P2** — accepted implementation + toolkit identity | `git -C …\xboxrecomp rev-parse HEAD`; `status --porcelain` | `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`, **clean** (0 entries). **PASS** |
| **P3** — EP cannot impersonate a GP clear, at the **build-identity** commit | `git -C …\xboxrecomp grep -n -E 'ep_ops\|ep\.regs\[\|0x50000' 3a3c7c1… -- src/apu` | exit 0, **14 hits**, every one a permitted form: `ep_ops` definition `gp_ep.c:568`, declaration `gp_ep.h:48`, comments `apu_core.c:30`, `apu_state.h:374`; `ep.regs[` reads `:526`, `:612`, `:613`, `:650`, `:651`; writes `:558`, `:559`, `:562` **all inside `ep_write`**; `0x50000` only in comments `apu_core.c:648`, `:674`. **No call site, no routing arm.** Positive controls all visible (definition, declaration, comments, `ep_write`'s `EPRST` write at `:559`). `MCPXAPUState` `calloc`'d (`apu_core.c:543`); EP `EPRST`-gated (`gp_ep.c:650-651`). **PASS** |

**No precondition failed.** All three PASS, so execution proceeded.

## 2. Step 1 — game-only forwarding edits

Write scope honoured exactly: `src/diagnostics.c`, `src/recomp/gen/recomp_0000.c`,
`src/recomp/gen/recomp_0005.c`. **40 insertions, 0 deletions** — no value changes.

| Edit | Location | Content |
|---|---|---|
| Forwarder | `src/diagnostics.c:118-137` | `jsrf_watch_store()` — thin forwarder: one-shot `apu_watch_set_anchor_site(0x001A18CEu)`, then `apu_watch_cpu_store(site_va, target_va, value)`. No filter, cap, output or decision logic. |
| `extern` decls | `recomp_0000.c:12`, `recomp_0005.c:12` | one file-scope declaration per edited chunk |
| Site 1 | `recomp_0000.c:135287` | `jsrf_watch_store(0x0006DACEu, eax + 0x810, ecx)` |
| Site 2 | `recomp_0000.c:135411` | `jsrf_watch_store(0x0006DBBAu, ecx + 0x810, eax)` |
| Site 3 | `recomp_0000.c:135877` | `jsrf_watch_store(0x0006DEF3u, ecx + 0x810, eax)` |
| Site 4 | `recomp_0005.c:6528` | `jsrf_watch_store(0x001A1751u, edi + 0x810, 0)` |
| Site 5 | `recomp_0005.c:6751` | `jsrf_watch_store(0x001A18CEu, ebx, eax)` |
| Site 6 | `recomp_0005.c:8094` | `jsrf_watch_store(0x001A1FA7u, edi + 0x10, ebp)` |

Every edit carries `A4b observation — re-apply after regeneration`.

### The six-site population (AC-NOCPU producing commands)

Run from the game root against the **unchanged original XBE**; each range contains exactly its store:

| # | Command (`inspect-jsrf.py disasm …`) | Instruction found | Guest site VA |
|---|---|---|---|
| 1 | `0x0006DAB0 0x0006DAE0` | `0006DACE mov dword ptr [eax + 0x810], ecx` | `0x0006DACE` |
| 2 | `0x0006DBB0 0x0006DBEA` | `0006DBBA mov dword ptr [ecx + 0x810], eax` | `0x0006DBBA` |
| 3 | `0x0006DEE0 0x0006DF00` | `0006DEF3 mov dword ptr [ecx + 0x810], eax` | `0x0006DEF3` |
| 4 | `0x001A1747 0x001A1769` | `001A1751 and dword ptr [edi + 0x810], 0` | `0x001A1751` |
| 5 | `0x001A18C0 0x001A18D5` | `001A18CE mov dword ptr [ebx], eax` (+ `001A18D0 cmp [ebx],0` / `jne`) | `0x001A18CE` |
| 6 | `0x001A1F9B 0x001A1FBF` | `001A1FA7 mov dword ptr [edi + 0x10], ebp` | `0x001A1FA7` |

**Frozen count 6; count found 6; `APU_WATCH_N_SITES = 16 ≥ 6` (`apu_watch.h:110`).** Text pattern
`MEM32\(.* \+ 0x810\) =` hits **4** (verified), deduplicating the already-listed `0x001A1751` site → **3
other** sites → 3 + 3 = **6**. **Reconciliation PASS.**

## 3. Step 2 — build and ctest

| Item | Value |
|---|---|
| Build | `python -X utf8 scripts\build-jsrf.py` → **`Build succeeded (success)`** |
| New warnings from these edits | **zero** — all 195 `C4700` warnings are pre-existing generated-code noise at lines that are **not** any edited line (verified per-line for both chunks) |
| **exe SHA-256 (edited build)** | **`D447749B9E6063104457734F3793C01A2564425F0141A00A8DB7A55D2E96E35E`** |
| ctest | **14/14 passed**, 23.13 s, `--output-on-failure` |

## 4. Step 3 — runs (game root, 30 s, `--profile strict`)

| Run | Label | Archive | Outcome | Duration | exe |
|---|---|---|---|---|---|
| **R1** | `a4b2-gp-trap-trace` | `logs/runs/20260927-103003-050-a4b2-gp-trap-trace` | `unhandled_exception` | 6.188 s | `D447749B…` |
| **R0** | `a4b2-default` | `logs/runs/20260927-103204-756-a4b2-default` | **`diagnostic_deadline`** | **31.862 s** | `D447749B…` |
| **R1 rerun** (the one permitted) | `a4b2-gp-trap-trace-rerun` | `logs/runs/20260927-103258-133-a4b2-gp-trap-trace-rerun` | `unhandled_exception` | 4.756 s | `D447749B…` |

Launch environment (R1): `RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_APU_TRACE=1`,
`RECOMP_KERNEL_LOG_BUDGET=100000`; `RECOMP_APU_DSP_ACK`, `RECOMP_AC97_READY`, `JSRF_ALLOW_UNRESOLVED`,
`JSRF_ABI_CONTINUE` all removed. R0: same minus `RECOMP_APU_TRAP`/`RECOMP_APU_TRACE`.

**The second R1 result stands** per the packet's one-rerun rule.

## 5. Gates

| Gate | R1 (rerun stands) | R0 |
|---|---|---|
| **G1** `check-run-profile.py` | **`STRICT`** | **`STRICT`** |
| **G2** `check-dump-mapping.py` | **`matches: 0   content-mismatch: 1`** → **FAIL** | **`matches: 1   content-mismatch: 0`** → PASS |
| **G3** `result.json` readable | PASS | PASS |
| **G4** structure at the build commit | **PASS** — `loc_001A18D0: ;` occurs **exactly once**, at **`L = 6754`**; `goto loc_001A18D0;` at **6757** (= L+3); preceding nonblank store `MEM32(ebx) = eax;` at 6752, with the step-1 call at 6751 immediately above | PASS (same build) |

**G2 fails for R1.** The control in §7 shows why, and that it is not caused by this packet's edits.

## 6. What R1 did observe (recorded; not a PASS claim)

These are **log** observations from the R1 rerun. They are recorded because they are the packet's subject
and they bound the brief; **they do not override the failed gate.**

| Observation | Value (R1 rerun) |
|---|---|
| GPSADDR write | `[APUMMIO] write 0x02040 = 003CC000` — the **physical form** the packet's Edit 2 anticipated |
| GPRST writes | `0 → 1 → 3` (one transition to both-bits) |
| `[GPBOOT]` headers | **one**, contiguous: `n=1 sge0=003C0000 sge0_va=803C0000 gprst=00000003 prev=00000001`; **64** `pram` lines |
| AC-BOOT image comparison (Session-run) | all **371** words (`0x173`) equal `LE32(default.xbe[0x1A7D60+4i]) & 0xFFFFFF`; **0 mismatches**; control at `0x1A7D64` differs at word 0 (`0BF080` vs `000155`) |
| AC-RUN counts | `boots=1 gp_frames=768 gp_insns=33120534` (last counts line) |
| `CPU_ANCHOR` latch | `seq=198851 va=803C0810 observed=00000000 payload=00000003 site=001A18CE frame=256` |
| **`GP_CLEAR` latch** | **`seq=198852 va=803C0810 observed=00000003 payload=00000000 insns=124652 dsp_addr=000800 frame=256`** |
| `[GPDMA] watch` | `va=803C0810 before=00000003 payload=00000000 after=00000000 cas=ok observed=00000003` |
| AC-NOCPU ack search | toolkit `git grep -E 'RECOMP_APU_DSP_ACK\|dsp_ack_'` at `3a3c7c1` → **exit 1, zero hits**; exe strings contain **neither** name; positive control `RECOMP_APU_TRAP` **is** present |
| AC-NOCPU launch env | `RECOMP_APU_DSP_ACK` **absent** |
| `CPU_ZERO_OVERFLOW` | 0 in every counts line |
| `GPIN_OUT_OF_UNIVERSE` | 0 in every counts line |

**The GP cleared the word.** `GP_CLEAR.va = 0x803C0810` is exactly `B + 0x810` for `B = 0x803C0000`, the
value the **live log** and the R0 dump both show. `CPU_ANCHOR.seq (198851) < GP_CLEAR.seq (198852)`.

**The rerun reproduced the identical `GP_CLEAR` `seq=198852` and the identical latch set** — the event is
deterministic, not a flake.

## 7. Why G2 fails — a pre-existing guest defect, not this packet's edits

**Decisive control.** The Session reverted **all three** A4b2 edits, rebuilt
(exe **`A7E95B3954975E22DFE00D1480E5DD07D37E7A552A6A38642712E6F859200477`**), ran the identical R1
command, then restored the edits and rebuilt.

| Run | exe | Outcome | `[ICALL] invalid` | heap OOM | G2 |
|---|---|---|---|---|---|
| **R1 with A4b2 edits** | `D447749B…` | `unhandled_exception` 4.76 s | `0x00000000 … return=0014982E` | `requested 598869040` | `content-mismatch: 1` |
| **CONTROL, edits reverted** | `A7E95B39…` | **`unhandled_exception` 4.90 s** | **`0x00000000 … return=0014982E`** | **`requested 598869040`** | **`content-mismatch: 1`** |

**The control crashes identically without any A4b2 code.** The failure is therefore **not** caused by this
packet's instrumentation.

**Two independent proofs that the control genuinely lacked the edits:**

1. **No `CPU_ANCHOR` latch.** The control logs **4** `[GPWATCH] latch` lines — `BOOT_SCRATCH_READ`,
   `MIXBUF_STUB_READ`, `GP_CLEAR`, `GP_ZERO_OVER_OTHER`. The R1 rerun logs **5**, the extra one being
   `CPU_ANCHOR`. `CPU_ANCHOR` can only fire from `apu_watch_cpu_store` via the packet's
   `jsrf_watch_store` call, so its absence proves the control had no instrumentation.
2. **The sequence numbers differ by exactly one.** Control `GP_CLEAR seq=198851`; R1 rerun
   `GP_CLEAR seq=198852`. The anchor store is the only event the edits add, and it consumes exactly one
   `s.seq` increment (`apu_watch.c:746`). The offset of 1 is the arithmetic signature of that single
   added call.

Both runs still reached `GP_CLEAR` at `va=803C0810` with `observed=00000003` and `boots=1 gp_frames=768
gp_insns=33120534` — **the GP clear does not depend on the A4b2 instrumentation at all.**

**It is a documented, pre-existing guest defect.** `docs/jsrf-operating-history.md:993-998` names this exact
signature as the A2g next stop — *"`[ICALL] invalid target 0x00000000 return=0014982E` … reached immediately
after `xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)`"* — and archived run
**`logs/runs/20260922-224429-003-a2g-304f0-span`** (2026-09-22, **before** the A4b1 GP port and before any
A4b2 edit) shows it: `unhandled_exception`, 4.95 s, same OOM byte count, same ICALL target and return, and
**`content-mismatch: 1`**.

**Stated precisely, because a reviewer will check it:** that A2g archive is an **exploratory** run, not a
strict one — its recorded settings are `RECOMP_AC97_READY=1`, **`RECOMP_APU_DSP_ACK=0x803C0810`** and
`RECOMP_APU_TRAP=1` (no `RECOMP_GPU_ACK`). So it is **not** a like-for-like strict comparison. What it
establishes is narrower and sufficient: **the crash signature and the `0x37608` displacement both exist in
a pre-`A4b1` tree**, so neither originates in this packet or in `A4b1`. (Incidentally, that run's synthetic
ack was pointed at `0x803C0810` — the same pending word this packet studies — which is consistent with the
displacement being reached on the trapped path, but the Session draws no conclusion from it.)

**The decisive attribution evidence is the no-edit control above**, which *is* a like-for-like strict
trap+trace run differing from R1 only by the absence of this packet's edits.

The requested size is itself consistent with the displacement: the guest asks for **571 MiB**
(`598869040` bytes) while holding 12 MiB of a 48 MiB heap, i.e. a bogus size computed from displaced
memory — which is the mechanism A2h describes.

The displacement is **exactly `0x37608`**: the R1 dump's bytes at guest VA `0x00011000` are
`83c41456ff5010eb…`, which is `default.xbe[0x38608]`, and `0x38608 − 0x1000 = 0x37608` — the precise
separation A2h recorded (`docs/jsrf-operating-history.md:1013-1033`). A2h **characterized** this
displacement but **never identified its writer**; `A2h-r6` was **retired, premise refuted, never executed**
(`plan-jsrf-bare-minimum.md:1112-1124`).

**So G2's failure is the A2h guest displacement, still unfixed, and independent of `A4b2`.**

### Boundary of this measurement

- The crash occurs **~4.9 s in**, whereas the A4a R1 baseline (2026-09-24, toolkit `0d7929c`, **pre-A4b1**)
  ran the full 32.6 s to `diagnostic_deadline` with G2 `matches: 1`. **Why the A4a trapped run went clean is
  UNKNOWN and immaterial to this packet.**
- **Do not read the A4a difference as a finding against `A4b1`.** The identical crash signature exists in
  **`logs/runs/20260922-224429-003-a2g-304f0-span` (2026-09-22), which predates `A4b1` entirely** — same
  `unhandled_exception`, same OOM byte count `598869040`, same `[ICALL] invalid target 0x00000000
  return=0014982E`, same `content-mismatch: 1`. That archive **affirmatively contradicts** sole causation
  by `A4b1`. `A4b1-r4` remains **accepted and closed**.
- Only **one** control was run, on the trap+trace command. The Session did **not** control R0; R0 passes
  G2 and reaches `diagnostic_deadline`, consistent with the defect being reached only on the trapped path.

> **Correction recorded 2026-09-27.** An earlier revision of this record stated that *"the A4b1 GP port is
> the only difference identified"*. The Persistent Advisor ruled that this must not become a finding
> against accepted work, since the pre-`A4b1` archive above contradicts it — a contradiction **already
> present in this record's own §7 table**. The sentence was an overreach and is withdrawn here. See
> `docs/reviews/a4b2-r6-execution-ruling.md`.

## 8. Decision-row classification

Rows are evaluated in order, first match wins (`packet:116`).

- **`R2-DEFAULT-REGRESS`** — AC-DEFAULT2 is **PASS** (`outcome=diagnostic_deadline`, `Wf=3`, `F=2`, zero
  `[GP*]` lines, and the archived environment carries neither `RECOMP_APU_TRAP` nor `RECOMP_APU_TRACE`).
  Does not match.
- **`R2-UNKNOWN`** — *"after the one identical R1 rerun, **any gate fails**; …"*. **G2 fails for R1.**
  **MATCHES.**

**Row selected: `R2-UNKNOWN` → Planner, with the failing gate as the brief.**

Per `packet:60`: *"A failed gate → `R2-UNKNOWN` after the one R1 rerun, not PASS."* The one rerun was
used and its result stands. **No `R2-PASS` claim is made**, and none of the `AC-*` results above are
offered as acceptance evidence.

### AC status (recorded for the brief; not decisive under the selected row)

`AC-DEFAULT2` **PASS** (R0). `AC-BOOT`, `AC-RUN`, `AC-CLEAR`, `AC-NOCPU`, `AC-INPUTS` — **not claimed**:
G2's failure means R1's dump is not usable for the claims that read `B`/`Wf` from it. Specifically,
`B = MEM32(0x001BA858)` reads **`FFFFFFFF`** in the R1 dump (displaced) and `Wf` reads `00000000`, so
`AC-CLEAR`'s `va = B+0x810` test cannot be satisfied **as written** from the dump, even though the live log
shows `GP_CLEAR.va = 0x803C0810` — exactly `B + 0x810` for the correct `B`. Under `packet:89` step 3 that
is `UNKNOWN`, and under the selected row it is not reached.

## 9. Evidence index

| Artifact | Path |
|---|---|
| R1 (superseded by rerun) | `logs/runs/20260927-103003-050-a4b2-gp-trap-trace/` |
| R1 rerun (**the R1 of record**) | `logs/runs/20260927-103258-133-a4b2-gp-trap-trace-rerun/` |
| R0 | `logs/runs/20260927-103204-756-a4b2-default/` |
| Trap+trace control (trap on, trace off) | `logs/runs/20260927-104252-588-a4b2-diag-trap-notrace/` |
| **Reverted-source control** | `logs/runs/20260927-104618-057-a4b2-control-no-edits/` |
| exe (edited) | `D447749B9E6063104457734F3793C01A2564425F0141A00A8DB7A55D2E96E35E` |
| exe (control, edits reverted) | `A7E95B3954975E22DFE00D1480E5DD07D37E7A552A6A38642712E6F859200477` |
| Game commit | `a000662aef3bb4d7088f3fa367d19e7ce7c157df` |
| Toolkit commit | `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` (clean) |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| `L` (G4) | **6754** |

Each run directory holds `result.json`, `stacks.txt`, `jsrf_run.log`, `process.dmp`, `metadata.json`,
`build-source.json`, `project.patch`, `source.zip` and the copied exe/PDB/map/collector.

---

# `A4b2-r8` execution evidence — **`R2-PASS`**

**Added 2026-09-27.** This section preserves r6's terminal row above and records the **r8** revision's
execution. Full detail: `docs/reviews/a4b2-r8-execution-evidence.md`; preconditions:
`docs/reviews/a4b2-r8-preconditions.md`.

**Packet:** revision **`A4b2-r8`**, frozen SHA-256
**`4D4AFC304F571971EB180C19D6832D56A2CC62FF6EAAFB9E4928716125397C62`** — verified before execution, not
edited. **Adequacy:** `ADEQUATE` by a **non-authoring** Planner (`docs/reviews/a4b2-r8-adequacy-review.md`).

## Evidence index

| Item | Value / path |
|---|---|
| **R1** (trap+trace, strict) | `logs/runs/20260927-160330-655-a4b2-gp-trap-trace/` — **STRICT** |
| **R0** (default, strict) | `logs/runs/20260927-160335-562-a4b2-default/` — **STRICT** |
| R1 reruns | **none** — R1 did **not** select `R2-UNKNOWN`, so the one permitted rerun was not used |
| `result.json` | R1 `unhandled_exception` / exit `3762440515`; R0 `diagnostic_deadline` / exit `3` |
| `stacks.txt` | R0 `F0 = 2`; R1 present (not used as evidence — see the displaced-dump note) |
| `jsrf_run.log` | both, complete and untruncated |
| `process.dmp` | both |
| `build-source.json`, `project.patch`, `source.zip` | both |
| **exe SHA-256 (R1 and R0)** | **`BC8E288DD54D8A09DA1630AB933EB1A808AEC9C60CEE2C64B9742B5C0CB8DC51`** |
| Game commit | `b3f22cb210f453940953fc357f00ab4d57e7d259` |
| **Toolkit commit (both builds)** | **`c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`** (clean) — the **discovery-final** commit per P2 |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| `L` (this build) | **6754** (`loc_001A18D0`); the store at `6751`; matched `F0` line `6755` = `L+1` |
| R0 mapping check | **`matches: 1   content-mismatch: 0   unreadable: 0   missing: 0`** |
| `B0` / `Wf0` | `B0 = 0x803C0000`; **`Wf0 = MEM32(0x803C0810) = 3`** |
| R1 mapping check / `Wf` | **`CONTENT_MISMATCH`** — *not usable for an image-content claim*. **`Wf` explicitly NOT evaluated or reported from R1** |
| ctest | **18/18 passed** |

### Full `[GPWATCH]` records (R1)

```
latch class=GP_CLEAR   seq=198852 va=803C0810 observed=00000003 payload=00000000 site=00000000 frame=256 insns=124652 dsp_addr=000800
latch class=CPU_ANCHOR seq=198851 va=803C0810 observed=00000000 payload=00000003 site=001A18CE frame=256 insns=0      dsp_addr=000000
counts (last) seq=1610629 boots=1 gp_frames=768 gp_insns=33120534 GP_CLEAR=1 GP_ZERO_OVER_ZERO=0 GP_ZERO_OVER_OTHER=1 GP_NONZERO_OVER=0 GP_PARTIAL=0 CPU_ANCHOR=1 CPU_ZERO=0 CPU_ZERO_OVERFLOW=0 CPU_OTHER=0 GPIN_OUT_OF_UNIVERSE=0 frame=768
```

**Anchor `va=803C0810` cross-checks against the log-bound `B+0x810`** ✓. **Last counts `boots=1`** ✓.

### `[GPBOOT]` header and its 64-line block (R1)

```
[GPBOOT] n=1 sge0=003C0000 sge0_va=803C0000 gprst=00000003 prev=00000001
```

**64 `pram` lines = 512 words.** Word-by-word comparison for `0≤i<0x173`: **371 compared, 0 mismatches**.
Negative control (offset `0x1A7D64`): **371 mismatches**, word 0 differs ✓.

### Criteria

| AC | Result | Decisive artifact |
|---|---|---|
| `AC-DEFAULT2` | **PASS** | R0 `result.json` + R0 mapping + `Wf0` + `F0` + zero `[GP*]` lines |
| `AC-BOOT` | **PASS** | R1 `[GPBOOT]` header + 64-line block + XBE comparison + last counts `boots` |
| `AC-RUN` | **PASS** | R1 last `counts` line |
| `AC-CLEAR` | **PASS** | R1 `GP_CLEAR` + `CPU_ANCHOR` latch lines (step 2) |
| `AC-NOCPU` | **PASS** | R1 latches + exe strings + toolkit `src/apu` at `c151d4e` + six XBE disassembly ranges |
| `AC-INPUTS` | **PASS** | R1 `[GPIN] at_clear` blocks (6) and `[GPIN] summary` blocks (9) |

### Row and reviewer disposition

**`R2-PASS`** — the first matching row, with all six mandatory criteria PASS and P1–P4 PASS.

**Awaiting:** the acceptance stage (**Hy4** stage 1, then DeepSeek Max stage 2 for any non-AGREE
criteria). **`A4b2-r7` remains `R2-EXPL-INPUT`; this is not a retroactive PASS of r7.**

## 11. Notes for the Planner's brief

1. **The packet's own subject was observed and is positive**: the ported GP engine's DMA write path took
   the `3→0` CAS at `0x803C0810` while executing image `I` (`GP_CLEAR` `seq=198852`, `insns=124652`),
   deterministically across both R1 runs, with no synthetic ack and `CPU_ZERO_OVERFLOW = 0`.
2. **The blocking problem is a gate, not the GP**: G2 fails because the R1 dump is displaced by `0x37608`
   — the A2h guest defect, reproduced by a **no-A4b2-edit control** and present in a **2026-09-22** archive.
3. **Two questions the Planner owns** (Session does not decide them): whether the R1 dump's displacement
   should route this to a discovery/repair packet for the A2h writer **before** `A4b2` can be decided at
   all; and whether `AC-CLEAR`'s dependence on dump-read `B`/`Wf` — when the live log already carries
   `GP_CLEAR.va` — is the right evidence binding, given that `G2`'s purpose is to protect image-content
   claims while `AC-BOOT`'s image comparison already reads the **log** and passes.
4. **Not investigated, recorded as a boundary**: why the trapped run now stops at ~4.9 s when the pre-A4b1
   A4a baseline ran 32.6 s. The A4b1 GP port is the only difference identified; this packet cannot change
   toolkit code, so it is a lead, not a finding.
