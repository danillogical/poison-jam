# `A2h-slot-within-run-attribution-r1` — bounded post-review confirmation

**Scope:** confirm that the four stage-1 corrections (**C1–C4**) and the reviewer's **sharpening** were
applied to `docs/reviews/a2h-slot-within-run-execution-evidence.md`, that the edit was **documentation
only**, and that the underlying artifacts still support **`O-COVERAGE`**.

**Method:** artifact and diff inspection only. **The game was not run.** Evidence:
`docs/reviews/a2h-slot-within-run-acceptance-review.md` (§ `## Corrections`, lines 190–221), the edit commit
`178e17f`, the five run directories under `logs/runs/20260928-0204*-a2h-within-run-on-N`, and the three
document guards.

**Verdict of this confirmation:** the correction commit is **documentation-only** and **no load-bearing
number changed**. **`O-COVERAGE` still follows.** **One residual defect found (C3 is only half applied)** —
reported below, not papered over.

---

## Q1 — Were C1–C4 applied correctly?

| | Status | Evidence in the current evidence record |
|---|---|---|
| **C1** | **APPLIED** | Line 200: "the one armed thread (`66428`, **guest identity 1** — corrected, see below)" and line 202: "**`66468` — guest identity 4 — is the thread that witnessed the transition and then faulted.**" Restated at lines 224–227. |
| **C2** | **APPLIED** | Line 144–145, §4 now reads: "one of the threads the instrument did NOT arm — the nine failed attempts AND the never-attempted guest threads** (corrected per acceptance finding C2; see below)." Restated at lines 229–233. This is the alignment C2 asked for. |
| **C3** | **PARTIALLY APPLIED** | Disclosed at lines 235–243 (`57376`, "**at least 5 of at least 6**", 1148 first-chance `0xC0000005`, "**unexamined contrastive data**"). **But the body text C3 told the Session to fix is unchanged:** lines 197–198 still assert "**it is \"4 of 5 guest threads were never even attempted.\"**", and line 196 still asserts "**exactly one** of the **five** guest threads that ran guest code." The record now carries **both** the superseded figure and the corrected one. |
| **C4** | **APPLIED** | Lines 49–51: "Every run reports (these lines live in **`stacks.txt`, not `jsrf_run.log`** — there are **zero** `GUEST_DR` lines in any of the five logs, stated per acceptance finding C4)". Restated at lines 247–250. |

**C3 is the one real finding of this pass.** The correction section *describes* the supersession but never
edits the assertion it supersedes. A reader who reads the body (lines 193–202) and stops before the appended
corrections section still gets "4 of 5" as the record's claim, which C3 established is wrong. The fix is a
one-line edit at line 197–198 (and, if the sixth thread is claimed for run 1's population, at line 196).

**I independently confirmed C3's underlying facts** (run 1,
`logs/runs/20260928-020421-240-a2h-within-run-on-1`):

- `tid 57376` appears **1149** times in `stacks.txt` — matches the record's figure exactly.
- `DEBUG_EXCEPTION tid=57376 code=C0000005 first=1` appears **1148** times — matches exactly.
- It reaches `[KERNEL] #286: ordinal 145 (slot 45) esp=0x007BFF94 ret=0x00193E62 tid=57376`
  (`jsrf_run.log:33282`) — matches.
- It is **never armed**: **0** `GUEST_DR_ARM_TID` lines mention `57376`.

**C1 could not be independently re-derived by me** — see *Could not verify*. The corrected labels are at
least consistent with the log: run 5's `host_entry`/`memory_ready`/`guest_entry` checkpoints are all
`tid=66428` (the handshaking/main thread), while the A2HSLOT witness is `tid=66468` — i.e. the armed thread
is not the witnessing thread, which is the point C1 preserves.

---

## Q2 — Was the SHARPENING applied? **APPLIED**, and verified from the log

The record now states it at lines 126–132: call `#246`'s `phase=before` **already read `00000000`**, so the
record's `before=FE000104` is the sampler's *previous* read (call `#245`'s `phase=after`), and "**The write is
therefore bracketed strictly BETWEEN `#245`'s after-sample and `#246`'s before-sample — i.e. OUTSIDE any
bridge body for `#246`**".

**My own verification**, from
`logs/runs/20260928-020512-239-a2h-within-run-on-5/jsrf_run.log`, `tid=66468` only:

```text
34235:   [A2HSLOT] tid=66468 call=#245 ordinal=159 slot=001C4064 value=FE000104 phase=before
34410:   [A2HSLOT] tid=66468 call=#245 ordinal=159 slot=001C4064 value=FE000104 phase=after
34413:   [A2HSLOT] tid=66468 call=#246 ordinal=224 slot=001C4064 value=00000000 phase=before
34414:   [A2HSLOT] tid=66468 call=#246 ordinal=224 slot=001C4064 value=00000000 phase=after
34415:   [A2HSLOT] write tid=66468 slot=001C4064 before=FE000104 after=00000000 phase=boundary
34417: [ICALL] invalid target 0x00000001 tid=66468 esp=0126BF88 return=0018CE73
34418:   [A2HSLOT] terminal tid=66468 target=00000001 slot=001C4064 live=00000000 call=#246 observed=0
```

The reviewer's reading is **correct and I reproduce it**: `#245`'s after-sample (line 34410) reads
`FE000104` and `#246`'s before-sample (line 34413) reads `00000000`. The last non-zero observation precedes
the first zero observation with no `#246` bridge body in between, so the write is bracketed **strictly
between those two samples — outside any bridge body for `#246`.** The bracketing claim in the record is
sound.

**One discrepancy found in the record's own quotation** (minor, non-load-bearing): the record's block at
line 105 shows the first line as `[A2HSLOT] tid=66468 call=#246 ordinal=224 … value=FE000104 phase=after`.
**No such line exists in the log** (0 matches). The line is a faithful rendering of the *sampler's previous
read*, but it is labelled `call=#246` when the log attributes that read to **`call=#245`** (line 34410). The
correction prose at lines 126–132 states this correctly; the quoted block above it was not updated to match.
This is cosmetic — the bracketing argument depends on the *values* and their order, both of which are
correct — but the quotation should be relabelled `#245` so the block agrees with the prose that corrects it.

---

## Q3 — Did the edits change anything LOAD-BEARING? **NONE**

`git show 178e17f --stat` → **1 file changed, 73 insertions(+), 5 deletions(-)**, and the only file is
`docs/reviews/a2h-slot-within-run-execution-evidence.md`. No source, config, script, or log was touched.
The five deletions are all replaced prose (the old "Every run reports:" lead-in, the old "one of the NINE
UNARMED threads" sentence, and the old run-5 identity-4 sentence). **Documentation only — confirmed.**

Every load-bearing figure is **unchanged**, verified by grepping the diff for numeric changes and by reading
the current record:

| Load-bearing item | Current value | Line | Changed by the edit? |
|---|---|---|---|
| Row | **`O-COVERAGE`** → `A2h-slot-write-coverage-provenance` | 47 (heading), 62, 75, 149, 277 | **No** |
| K | **K = 3** (≥ 2 requirement met) | 33, 277 | **No** |
| Targets | **3 of 5** observed; coverage-disqualified **0** | 33–34 | **No** |
| Arming failures | **`armed=10 failed=9`**, every run | 54 | **No** |
| DR hits | **`GUEST_DR_HIT` … ZERO, all five runs** | 56 | **No** |
| Alias `touched_count` | **`touched_count = 0`, `publish_failed = 0`**, all five runs | 80 | **No** |

The diff's added lines that mention `O-COVERAGE` (lines 243, 255) are prose stating the row is *unaffected*;
they do not restate it as a different value. **No number moved.**

---

## Q4 — Do the underlying artifacts still support the row? **YES — `O-COVERAGE` still follows**

Re-derived independently from the five run directories (all figures below are mine, from the artifacts):

**Terminal event per run** — every one of the five terminates on the same fatal unresolved-call path:

| Run | Terminal event (`jsrf_run.log`) | `result.json` `exit_code` |
|---|---|---|
| 1 | `[ICALL] invalid target 0x00000000 tid=50616 … return=0014982E` → `[EXCEPTION] tid=50616 code=0xE0424943` | `3762440515` |
| 2 | `[ICALL] invalid target 0x00000000 tid=56204 … return=0014982E` → `[EXCEPTION] tid=56204 code=0xE0424943` | `3762440515` |
| 3 | `[EXCEPTION] tid=61900 code=0xE0424943` | `3762440515` |
| 4 | `[ICALL] invalid target 0x00000000 tid=67512 … return=0014982E` → `[EXCEPTION] tid=67512 code=0xE0424943` | `3762440515` |
| 5 | `[ICALL] invalid target 0x00000001 tid=66468 esp=0126BF88 return=0018CE73` → `[EXCEPTION] tid=66468 code=0xE0424943` | `3762440515` |

`3762440515 = 0xE0424943` exactly — the fatal unresolved-call code — in **all five**. Fatal-by-default is
behaving as documented; no run was rescued by `JSRF_ALLOW_UNRESOLVED`.

**`GUEST_DR_ARM` summary and `GUEST_DR_ARM_FAIL` count, per run, from `stacks.txt`** (as C4 requires):

| Run | `GUEST_DR_ARM` line | `GUEST_DR_ARM_TID` lines | `GUEST_DR_ARM_FAIL` lines |
|---|---|---|---|
| 1 | `why=handshake ok=0 armed=10 failed=9 collision=0 canonical=00000000001D4064 aliases=28 dr7=000D0001 tids=10` | 10 | **9** |
| 2 | identical | 10 | **9** |
| 3 | identical | 10 | **9** |
| 4 | identical | 10 | **9** |
| 5 | identical | 10 | **9** |

**9 arming failures per run, in all five runs — confirmed.**

**`GUEST_DR_HIT` across all five runs: 0.** (And, confirming C4, **0** `GUEST_DR` lines in any of the five
`jsrf_run.log` files — the DR evidence genuinely lives only in `stacks.txt`.)

**Alias census, all five runs** (`jsrf_run.log`): `[A2HSLOT] alias census armed=1 mapped=28 protected=28
mask=0FFFFFFF/0FFFFFFF slot_page=001C4000` — identical in every run, i.e. 28/28 aliases mapped and
protected.

**Does `O-COVERAGE` still follow? YES.** The three legs the record relies on all re-derive cleanly:
**(1)** the DR0 watch armed on the wrong population — `ok=0`, `failed=9` per run, **zero** canonical-slot
hits across five runs; **(2)** the alias census was complete (28/28) and the frozen latch reports no alias
touch; **(3)** the slot nevertheless demonstrably reached zero in run 5, positively witnessed. Coverage of
the write was **not** achieved, so the row is **`O-COVERAGE`** — a coverage gap, **not** "no write occurred".
**K = 3 ≥ 2** and targets **3 of 5** are also re-confirmed as the record states them.

---

## Defects found in this pass

**D1 (real, should be fixed): C3 is only half applied.** `docs/reviews/a2h-slot-within-run-execution-evidence.md`
lines 196–198 still assert the superseded census ("exactly one of the **five** guest threads", "**4 of 5
guest threads were never even attempted**") while the appended corrections section at line 239 states the
corrected "**at least 5 of at least 6**". The body was not aligned with its own correction. **Non-blocking**
— it does not change the row, K, the counts, or the conclusion, and the corrected figure *is* present in the
record — but it leaves the record internally contradictory at exactly the point C3 was raised.

**D2 (cosmetic): the quoted log block at line 105 mislabels a call index.** The record shows
`call=#246 … value=FE000104 phase=after`; the log attributes that sample to `call=#245` (line 34410), and no
`#246 … FE000104 phase=after` line exists. The prose immediately below (lines 126–132) states the correct
attribution. The block should be relabelled `#245` to agree with it.

Both are defects in the record's prose, **not** in the execution, and neither falsifies an acceptance
criterion. Reporting them is the point of this pass; **D1 is the one worth a follow-up edit.**

## Could not verify

- **C1's frozen-registry identity labels.** I could not re-derive `slot 0 / tid 66428 / identity 1` and
  `slot 3 / tid 66468 / identity 4` from the archived artifacts. Those values live in the frozen registry as
  **binary struct fields**, not text: an ASCII scan of `process.dmp` returns **0** matches for `identity=1`,
  `identity=4`, `66428`, `66468`, and `GUEST_DR_ARM_TID`, and the registry is not emitted as a text file in
  the run directory. I relied on the reviewer's extraction. **Mitigating:** the corrected labels are
  consistent with the log (`66428` is the `host_entry`/`guest_entry` tid — the handshaking thread; `66468` is
  the A2HSLOT witness), and **C1 touches no load-bearing number**, so this does not move the row.
- **The alias `touched_count = 0` / `publish_failed = 0` latch values.** The record attributes these to "the
  frozen latch", i.e. frozen memory in `process.dmp`, **not** to a text line. I did not decode the latch
  struct, so I confirm only the **census** half (`armed=1 mapped=28 protected=28`, all five runs) and not the
  latch values themselves.
- **The read-path leg** (`BRIDGE_MEM32(0x001C4064)` vs. the guest's own read resolving to the same host
  storage) — carried forward unverified, as both the review and the record already state. The row is
  `O-COVERAGE` regardless.
- **Run 3's faulting thread `61900`** does not appear in the record's per-run armed-thread table and I did
  not establish its relationship to the armed thread `10116`; I report only that run 3 terminates with
  `[EXCEPTION] tid=61900 code=0xE0424943`.

## Guards

All three required guards pass, run after the corrections commit and re-run before this record was committed:

- `python -X utf8 tests/test_agent_docs.py` — **OK** (30 tests)
- `python -X utf8 tests/test_recorded_reviews.py` — **OK** (178 tests)
- `python -X utf8 tests/test_markdown_tables.py` — **OK** (12 tests)

No failing guard.

## Status

**`O-COVERAGE` stands.** The correction commit `178e17f` is **documentation only**, **no load-bearing number
changed**, and the underlying artifacts still support the row on independent re-derivation. **C1, C2, C4
APPLIED; C3 PARTIALLY APPLIED (D1); SHARPENING APPLIED and independently reproduced.** No criterion was
falsified. **No push.**
