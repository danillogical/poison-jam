# `A2h-oom-causal-slice-r1` — acceptance record: **`ACCEPT`** (stage 2, final)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a2h-oom-causal-slice.md`, revision **`A2h-oom-causal-slice-r1`**, frozen SHA-256
**`E9CDB1B39066CE5F5626FF74246E5EAD34C1612BC31BE622341B50FCC541108C`** — **verified unchanged and untouched
throughout all five rounds.**
**Class:** discovery (§5.8).
**Reviewers:** stage 1 child `64a1c995-…` (`workbuddy-ai/hy4-preview-f` @ `high`);
stage 2 child `7808a37d-…` (`workbuddy-ai/deepseek-v4.1-flash` @ `max`).
**Reviews:** `docs/reviews/a2h-oom-causal-slice-acceptance-review.md` (stage 1),
`docs/reviews/a2h-oom-causal-slice-stage2-acceptance-review.md` (stage 2, rounds 1–5).

---

## Disposition

> **`DISPOSITION: ACCEPT`.** Every mandatory criterion `AGREED`.

| Criterion | Disposition | Basis |
|---|---|---|
| **1** — parser tests cover the packet's named fixtures | **`AGREED`** (round 1, not reopened) | **31 tests OK**; 9 boundary tests; mid-instruction start rejected against the **real** XBE |
| **2** — artifacts match the commands / the A2g characterisation | **`AGREED`** (round 5) | Trap state confirmed (`settings.RECOMP_APU_TRAP=1`; log line 26 *"trapped for MMIO"*); **no surviving assertion** of the withdrawn claim; census reproduces **four times** |

**§5.8 compliance:** the acceptance confirms the artifacts exist, match the commands, and select the recorded
outcome row. **It satisfies no strict criterion** — no boot, audio, GPU, liveness or device claim — and
**nothing here says anything work.**

## The acceptance took FIVE rounds, and every round found a real defect

**This is recorded because it is the strongest available argument for the acceptance stage existing.**

| Round | Finding | Whose |
|---|---|---|
| **1** (stage 1) | **Banners added, bodies never swept** — the withdrawn claim survived unflagged in **six** places, incl. a conclusions list and a document title | **Session** |
| **1** (stage 1) | **A required parser fixture was missing** — the *"mid-instruction start"* fixture existed **only in a docstring**; the tool had **no disassembly surface at all** | **Session** |
| **2** (stage 2) | **The sweep's pattern was too narrow** — it searched `no-trap` while the survivor spelled it **`NO trap`** | **Session** |
| **3** (stage 2) | **The fix malformed a table row** — a stray `|` left 3 cells against a 2-cell header | **Session** |
| **4** (stage 2) | **The new guard failed on CORRECT input** — `cells()` promised escaped-pipe handling it did not have, so it passed **by luck of the corpus** | **Session** (new non-contract code) |

**None of the five was found by the author re-reading his own work.** Each was found by a reviewer
**replaying a claim against real bytes** rather than trusting a summary.

**And the guard prompted by round 3's diagnosis immediately found a sixth, pre-existing defect no reviewer
had reported:** `docs/jsrf-run-profiles.md:92` was **born malformed at `ad400294`** (2026-09-22), missing its
third cell, in a durable evidence-rules document. **The reviewer corrected the provenance** — `7f63c45`
merely carried it forward.

## Three distinct failure modes, each now guarded rather than remembered

1. **A correction banner is not a correction** — the document bodies must be swept.
2. **A sweep is only as good as its pattern** — enumerate the **claim**, not a spelling of it.
3. **A change applied to the sentence in front of you can break the structure it sits in** — hence
   `tests/test_markdown_tables.py`, a **machine check** rather than a resolution to be more careful.

**The guard is correct by construction, not by luck.** Its first version passed only because none of the four
guarded documents happened to contain an escaped pipe; the reviewer demonstrated it would report **correct**
rows (`` | `A\|B` | … | ``, `` | `GS |= 1` | … | ``) as malformed. It now skips escaped characters and
backtick spans, and **replaying it over the historical revisions still catches both real defects** while HEAD
is clean. **12 tests.**

**The reviewer's sharpest observation:** the round-history table added to the plan contains a row whose text
includes **a literal pipe inside backticks** — *"a stray `|` left 3 cells…"*. **The buggy guard would have
scored that correct row as malformed.** The Session's own prose about the defect would have tripped the bug.

## What the execution established

| Finding | Witness |
|---|---|
| **The failure predates the A4b2 trap work** | Different exe (`2cd0472a256e9d` vs `bc8e288dd54d`), five days earlier, **all 94 invocation sizes identical index-for-index**, same size/type/OOM tuple/terminal ICALL. **Not trace-caused.** **The trap is NOT shown to be unnecessary** — **all 35** archived runs carrying the request are trapped |
| **The chain is bound to the producer** | `RegionSize = align16([ebp+0x10]) + 0x20`; producer **`0x0014980E`**; value from **`[ebp+0x10]`, the 3rd argument**. **Reproduces both observed sizes exactly** |
| **Dominance, doubly derived** | Two independent CFG methods agree: 46 reaching instructions, **2 writers**, single entry, call **unreachable** with writers removed; the `movzx` writer **never reaches the call** |
| **The arena and toolkit behaved correctly** | `alloc_type 0x801000` is `MEM_COMMIT` with **no `MEM_RESERVE`**, so no reserve branch applies; the bridge returns `0xC0000017`; the guest then calls through NULL |

## Advisories carried forward (outside the contract)

1. **Latent guard edge case, zero current exposure.** An **unclosed** backtick span can hide a malformed row:
   `cells("| a | b | \`c |")` returns 2 and matches a 2-cell header. **`has_unescaped_pipe_ambiguity()`
   detects it but is called only in tests**, not wired into the document check. **Measured exposure: the four
   guarded docs contain 0 odd-backtick rows; repo-wide there is exactly 1, correctly flagged anyway.** If the
   guard is widened, **gate on the predicate** — a diagnostic predicate that is never called reports nothing.
2. **Three genuine malformed rows outside the guard's scope:** `docs/reviews/a4b2-r7-execution-evidence.md:54-56`
   — real 2-cell rows under a 3-cell `| Item | R1 | R0 |` header, **in an accepted packet's execution
   evidence**, where a missing cell means **a column of measurements is silently absent**. **Not this packet's
   contract; worth a separate bounded look.** The ordering is recorded: **fix the counting first, then widen**
   — widening today yields **3 real hits against 33 false positives**.

## Identity

| Item | Value |
|---|---|
| Game revision at acceptance | **`a19052c7dc5e3884cb126c4db1e20ab44417e6b3`**, clean |
| Toolkit revision | `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`, clean — **unchanged** |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` |
| **New guest runs** | **NONE** — the packet was decided offline |
| **Toolkit / runtime changes** | **NONE** |
| **Instrumentation** | **NONE added** |
| Suites | **six, all OK**: 12 / 30 / 178 / 31 / 29 / 31 |

**A reviewer identity note, recorded because reviews bind to bytes:** stage 2 verified at `1e1e041` and
reported HEAD as `a19052c` — one **documentation-only** commit later. **The guard is byte-identical between
them** (`git diff 1e1e041..HEAD -- tests/test_markdown_tables.py` empty), so the artifact verified is exactly
the one submitted; only documentation moved.
