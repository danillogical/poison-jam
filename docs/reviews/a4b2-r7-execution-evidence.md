# `A4b2-r7` execution evidence

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r7`**, frozen SHA-256
**`93E410C3742D0E290A0C0BBA71DCFEC547A77ECAC497E389BA5D58026E551F95`** — verified before and after
promotion, **not edited**.
**Row selected:** **`R2-EXPL-INPUT`** — `AC-BOOT`, `AC-RUN`, `AC-CLEAR`, `AC-NOCPU` all **PASS**;
`AC-INPUTS` **FAIL** on named stub inputs. **The claim is NOT established.**

This row was **predicted in advance** by the `r7` adequacy reviewer and independently by the Session from
the archived `r6` bytes (`docs/reviews/a4b2-r7-adequacy-review.md`). It is not a surprise.

---

## 1. Preconditions

| Precondition | Command | Result |
|---|---|---|
| **P1** | `Get-FileHash game\default.xbe -Algorithm SHA256` | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — **matches. PASS** |
| **P2** | `git -C …\xboxrecomp rev-parse HEAD`; `status --porcelain` | `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`, **clean** (0 entries). **PASS** |
| **P3** | `git -C …\xboxrecomp grep -n -E 'ep_ops\|ep\.regs\[\|0x50000' 3a3c7c1 -- src/apu` | exit 0, **14 hits**, all permitted forms; definition/declaration/comments/`ep_write` EPRST write all visible as positive controls; no call site, no `0x50000` routing arm. **PASS** |

## 2. Build, ctest, identity

| Item | Value |
|---|---|
| Build | `python -X utf8 scripts\build-jsrf.py` → **`Build succeeded (success)`** |
| ctest | **14/14 passed**, 21.92 s |
| **exe SHA-256** | **`CACFB64C819094BA82D393304FED84E0903D00219D34B199CABA82CBD552CE8F`** |
| Step 1 edits | already applied and **verified present, not duplicated**: 6 `jsrf_watch_store` call sites + forwarder + 2 `extern` decls (9 references) |
| Game commit | `a000662aef3bb4d7088f3fa367d19e7ce7c157df` |
| Toolkit commit | `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` (clean) |
| **G4** | `loc_001A18D0: ;` exactly **once** at **`L = 6754`**; `goto loc_001A18D0;` at **6757** (= L+3); preceding nonblank store `MEM32(ebx) = eax;` at 6752 with the step-1 call at 6751 immediately above. **PASS** |

## 3. Fresh runs (game root, 30 s, `--profile strict`)

| Run | Archive | Outcome | Duration |
|---|---|---|---|
| **R1** | `logs/runs/20260927-111948-580-a4b2-r7-gp-trap-trace` | `unhandled_exception` (the A2h defect; **not** a gate under `r7`) | 4.757 s |
| **R0** | `logs/runs/20260927-111959-498-a4b2-r7-default` | **`diagnostic_deadline`** | 31.890 s |

R1 env: `RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_APU_TRACE=1`, `RECOMP_KERNEL_LOG_BUDGET=100000`;
`RECOMP_APU_DSP_ACK` **absent**. R0 env: same minus TRAP/TRACE. **No rerun was needed** — no gate failed.

**G1: both `STRICT`.** **G3: both `result.json` readable.**

## 4. Gates and definitions

| Item | R1 | R0 |
|---|---|---|
| **G1** profile | **`STRICT`** | **`STRICT`** |
| **G3** `result.json` | readable | readable |
| **G4** structure | **PASS** (`L=6754`) | same build |
| R1 dump mapping | `matches: 0, content-mismatch: 1` — **displaced (A2h)**. Under `r7` this is **not a gate**: no PASS-path claim reads the R1 dump. It matters only to the no-`GP_CLEAR` branches, which are not reached (`GP_CLEAR` **is** latched). |
| R0 dump mapping | **`matches: 1, content-mismatch: 0`** — clean, as `AC-DEFAULT2` requires |
| R0 `B0` / `Wf0` / `F0` | `B0 = 0x803C0000`; `Wf0 = 3`; `F0 = 2` |

### `B` binding (the `r7` repair) — PASSES

- **`T` (independent oracle)** = `0x803C0000`.
- **`[GPBOOT]` blocks**: exactly **one**, `n=1`, structurally complete, **64** `pram` lines.
- **Common `sge0_va`** across all blocks: **`0x803C0000`** (single value, no disagreement).
- **`sge0_va = T`** → **true**.
- **Anchor cross-check**: `CPU_ANCHOR.va = 0x803C0810`; `0x803C0810 − 0x810 = 0x803C0000 = B`, non-wrapping.
- Last counts line `boots = 1` = `N`. Transition `prev=00000001` (GPDSPRST clear) → `gprst=00000003` (both set) — the pinned bootstrap transition.

## 5. Criterion results

### `AC-DEFAULT2` — **PASS**
R0 `outcome = diagnostic_deadline`, `Wf0 = 3`, `F0 = 2`, **zero** `[GPBOOT]`/`[GPWATCH]`/`[GPIN]`/`[GPRUN]`/`[GPDMA]` lines, and the archived environment carries **neither** `RECOMP_APU_TRAP` **nor** `RECOMP_APU_TRACE`.

### `AC-BOOT` — **PASS**
`N=1 ≥ 1`; complete header and 64 pram lines; ordinals exactly `1..1`; last counts `boots=1=N`; valid common `B` with anchor cross-check; `gprst=00000003`; `prev` with GPDSPRST clear; `sge0_va = T`; and **all 371 words (`0x173`) equal** `LE32(default.xbe[0x1A7D60+4i]) & 0xFFFFFF` — **0 mismatches**. Control: offset `0x1A7D64` word 0 reads `0BF080` vs expected `000155` — **differs**, as required.

### `AC-RUN` — **PASS**
Last counts line: `boots=1, gp_frames=768, gp_insns=33120534` — some counts line has `boots≥1`, `gp_frames≥1`, `gp_insns>0`.

### `AC-CLEAR` — **PASS**
Step 1 does not match: ledger alive; `CPU_ZERO_OVERFLOW=0` in every counts line and **no** `CPU_ZERO_OVERFLOW` latch; `CPU_ANCHOR` **present**; log-bound `B` and anchor cross-check **valid**; `GP_CLEAR.seq (198852) > CPU_ANCHOR.seq (198851)`.
Step 2 matches on all five conjuncts:

```
[GPWATCH] latch class=CPU_ANCHOR seq=198851 va=803C0810 observed=00000000 payload=00000003 site=001A18CE frame=256 insns=0 dsp_addr=000000
[GPWATCH] latch class=GP_CLEAR   seq=198852 va=803C0810 observed=00000003 payload=00000000 site=00000000 frame=256 insns=124652 dsp_addr=000800
[GPDMA] watch va=803C0810 before=00000003 payload=00000000 after=00000000 cas=ok observed=00000003 dsp_addr=000800 frame=256 insns=124652
```

`va = B+0x810` ✓; `observed=00000003` ✓; `insns>0` ✓; `dsp_addr` recorded ✓; `CPU_ANCHOR.seq < GP_CLEAR.seq` ✓. `F` recorded as corroboration (`F=0` from `stacks.txt`; the run crashed before the deadline). R1 mapping fails, so **`Wf` was not read or reported** — exactly as `r7` requires.

### `AC-NOCPU` — **PASS**
- Six-site reconciliation: **6 generated call sites**, set-equal to the frozen six (`0x0006DACE`, `0x0006DBBA`, `0x0006DEF3`, `0x001A1751`, `0x001A18CE`, `0x001A1FA7`), **0 missing, 0 extra**; `N_SITES=16 ≥ 6`.
- Ack absent from the launch environment; toolkit `git grep` at `3a3c7c1` → **exit 1, zero hits**; exe strings contain **neither** `RECOMP_APU_DSP_ACK` **nor** `dsp_ack_`, with `RECOMP_APU_TRAP` present as a **positive control**.
- **No contested zero**: `CPU_ZERO=0` in every counts line, `CPU_ZERO_OVERFLOW=0`, **0** `CPU_ZERO` latches.

### `AC-INPUTS` — **FAIL → `R2-EXPL-INPUT`**
Evaluated because `AC-BOOT`, `AC-RUN`, `AC-CLEAR` and `AC-NOCPU` all PASS.

**Step 1 — block integrity: VALID.**
- **6** `[GPIN] at_clear` blocks; every one carries **exactly five** tagged lines (`mixbuf`, `periph`, `fifo`, `dma`, `flags`).
- Every array has its **fixed length**: mixbuf `[32,32]`, periph `[128,128,128]`, fifo `[6,6]`, dma `[4,4,4]`.
- All six headers identical: `[GPIN] at_clear seq=198852 frame=256` — **equals `GP_CLEAR.seq`**.
- **(i) every emitted `at_clear` block is numerically identical to the first** — verified field-by-field over all nine arrays plus the flags triple: **all identical**.
- **(ii) every `at_clear` counter ≤ the matching element of the last `[GPIN] summary` block** — **zero violations** across all arrays and flags.

**Step 2 — integrity: VALID.** `out_of_universe=0`; `boot_scratch_read=1`; `fifo reads=[0,0,0,0,0,0]` so slots 2–5 are zero.

**Step 3 — classification: FAIL.** Three independent named stub conditions hold:

| Stub condition | Measured | Packet rule |
|---|---|---|
| PERIPH index **`0x33`** (`0xFFFFB3`) `reads = 1017` | nonzero | *"any read at index `0x33` is FAIL"* (`:115`) |
| MIXBUF `reads_while_stub` nonzero in **26** of 32 bins (each `7648`) | nonzero | *"any bin's `reads_while_stub>0` … → FAIL"* (`:115`) |
| `mixbuf_stub_read = 1` | set | *"or `mixbuf_stub_read=1` → FAIL"* (`:115`) |

The only other nonzero PERIPH offsets are `0x45` (`0xFFFFC5`, 1020 reads) and `0x56` (`0xFFFFD6`, 3065 reads) — both inside the five-offset **modelled** set, so neither contributes. DMA reads are `[0, 2311, 0, 0]` — the nonzero class is `CONTIG`, which counts as guest-written **by region**.

**`AC-INPUTS` FAIL. The clear is observed but is exploratory for the named stub inputs; the claim is not satisfied.**

## 6. Decision-row classification

Rows in order, first match wins (`packet:120-129`):

| Row | Match? |
|---|---|
| `R2-DEFAULT-REGRESS` | **no** — `AC-DEFAULT2` PASS |
| `R2-UNKNOWN` | **no** — no gate fails; no AC other than `AC-INPUTS` is UNKNOWN (all four PASS); `AC-INPUTS` is **evaluated** but **FAIL**, not UNKNOWN |
| `R2-CPU` | **no** — `AC-NOCPU` PASS; `AC-CLEAR` did not select R2-CPU |
| `R2-UNATTRIBUTED` | **no** — `AC-CLEAR` PASS |
| `R2-NOBOOT` / `R2-NOFRAMES` / `R2-NOEXEC` / `R2-NOCLEAR` | **no** — `AC-BOOT`, `AC-RUN` PASS; `AC-CLEAR` PASS |
| **`R2-EXPL-INPUT`** | **YES** — `AC-BOOT`, `AC-RUN`, `AC-CLEAR` and `AC-NOCPU` PASS, and `AC-INPUTS` FAIL |

**Row selected: `R2-EXPL-INPUT` → the Advisor decides whether the named stub input needs its own model packet first.**

**No `R2-PASS` claim is made.** The packet's *Establishes* claim is **not** satisfied.

## 7. Log-interleaving note (measurement integrity)

`emit_gpin_block()` calls `fprintf` once per array element and does **not** hold the stderr lock across the
call, so other threads inject complete foreign records — **including their own newlines** — at arbitrary
element boundaries. Physical log lines are therefore **not** logical lines.

**Three of the Session's early parses produced spurious "block differences"**, each traced to a parser
defect rather than emitter behaviour: (a) a hand-written foreign-tag list that missed `[READ]`;
(b) over-capture past the following `[GPIN] summary` block; (c) stopping only at `[GPIN]`, which let the
final `flags` field swallow the trailing crash text. After reconstruction (strip every `[TAG]` record with
its newline, bound each field at its own key, compare **numerically**), **all six blocks are identical**.

Recorded because a reviewer reproducing this must not repeat those three traps, and because "the blocks
differ" was a **false** intermediate reading that only careful reconstruction refuted.

## 8. Evidence index

| Artifact | Path / value |
|---|---|
| **R1 of record** | `logs/runs/20260927-111948-580-a4b2-r7-gp-trap-trace/` |
| **R0 of record** | `logs/runs/20260927-111959-498-a4b2-r7-default/` |
| exe SHA-256 | `CACFB64C819094BA82D393304FED84E0903D00219D34B199CABA82CBD552CE8F` |
| Game / toolkit commit | `a000662aef3bb4d7088f3fa367d19e7ce7c157df` / `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| `L` (G4) | **6754** |
| `B` / `T` / anchor `va` | `0x803C0000` / `0x803C0000` / `0x803C0810` |
| `GP_CLEAR` | `seq=198852 observed=00000003 insns=124652 dsp_addr=000800` |
| `CPU_ANCHOR` | `seq=198851 site=001A18CE payload=00000003` |

## 8b. What the three stub conditions actually are (Session observation, added for the Advisor consult)

Recorded because the Advisor's decision turns on **which** stub input is real, and two of the three are
not what a reader might assume. Measured from pinned source at `3a3c7c1`.

| Condition | Mechanism | What it means |
|---|---|---|
| `mixbuf_stub_read=1`, and `reads_while_stub` nonzero in 26 bins | `apu_gp_mixbuf_frame_begin` sets `s.mixbuf_stub = (vp_active_voices > 0)`; **and** `apu_gp_mixbuf_note_sample` sets `s.mixbuf_stub = 1` for **any nonzero sample word** anywhere in the frame's mix-buffer content (`apu_watch.c:604-618`) | The flag means **"this frame's mix-buffer content came from the VP (or the test tone)"**, not "the buffer was uninitialised". `apu_gpin_mixbuf_read` (`:620-645`) then counts a read as `reads_while_stub` whenever that flag is set. So this reports that **the GP read VP-produced mixbin data** — which is the designed data path, and is why the packet's own `AC-INPUTS` classification treats `MIXBUF` with `vp_active_voices > 0` as stub content. |
| PERIPH `0x33` (`0xFFFFB3`) `reads=1017` | `dsp.c:56-57` — `case 0xFFFFB3: v = 0; // core->num_inst; // ??` | A **placeholder constant**, which the Advisor's earlier ruling (`docs/reviews/a4b2-r4-planning-rulings.md`) reclassified from "modelled" to **STUB**, expressly so that a read of it would select this row. The GP read it 1017 times. |
| `out_of_universe=0`, `boot_scratch_read=1`, `fifo reads=[0,0,0,0,0,0]` | — | Step-2 integrity is clean; the FIFO input slots (the `NDEBUG` read-arm fall-through) were **not** read. So the one stub class the packet's claim limits single out as "caught" is indeed absent. |

**The Session draws no conclusion from this** — whether a stub input that the GP genuinely consumed
*shapes the specific `3→0` exchange* is a judgment question, and it is the one the Advisor has been asked.

**Also observed, and expressly non-deciding:** every `[GPRUN]` line reports `halt=1 tone=0 pc=00002B`
across all 11 emitted lines (frames 1–8, 256, 512, 768). The packet states `[GPRUN]` `pc`/`halt`/`tone` are
**observation for the next brief, not deciding lines** (`:82`), so this changes no criterion. It is recorded
only because it is the kind of value the next brief may want.

## 9. Boundary

- **`r6`'s artifacts were not used** to discharge any gate or AC (§2.2.5, §2.4.7); all evidence above is
  from the fresh `r7` runs. `r6`'s `R2-UNKNOWN` remains terminal for `r6` and is not rescored.
- **`A4b1-r4` is not implicated** and remains accepted and closed.
- **Not investigated:** the A2h displacement writer (open lead, does not gate this claim), and why the
  pre-`A4b1` A4a trapped run reached `diagnostic_deadline` where these runs crash at ~4.8 s.
- **`Wf` was not read from the displaced R1 dump**, per `r7`; the `no-GP_CLEAR` branches that would have
  required it were not reached.
