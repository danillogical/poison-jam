# A4b1-r4 — stage-1 acceptance review: **NOT ACCEPTED** (recorded verbatim)

**Reviewer:** child `fb109c49-2039-434d-b665-a604e84cacae`, `workbuddy-ai/hy4-preview-f` @ `high`
(`docs/agent-workflow.md` §1, §2.2 first-stage reviewer).
**Contract:** `A4b1-r4`, SHA-256 `6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38`, 445 lines
— **verified by the reviewer**.
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-26.

## Disposition

```text
VERDICT: NOT ACCEPTED
BLOCKING: AC-PORT
```

The review is recorded **verbatim** below, as §2.2 requires, before the second stage.

---

## Per-criterion dispositions (verbatim)

**G1 — AGREED.** `check-run-profile.py logs\runs\20260926-010303-411-a4b1-default` →
`20260926-010303-411-a4b1-default  STRICT`. (MEASURED)

**G2 — AGREED.** `check-dump-mapping.py` → `matches: 1   content-mismatch: 0   unreadable: 0   missing: 0`.
(MEASURED)

**G3 — AGREED.** `result.json` parses: `outcome=diagnostic_deadline`, `dump_ok=True`, `gpu_report_ok=True`.
(MEASURED)

**G4 — AGREED.** `loc_001A18D0: ;` at line 6748 exactly once; `goto loc_001A18D0;` at 6751 (within
L+1..L+3). Source unmodified vs HEAD. (MEASURED)

**F = 2 — AGREED.** 2 matches in `stacks.txt`, both `sub_001A1769+0xB2B … recomp_0005.c:6749`. Matches the
`A4s` baseline. (MEASURED)

**AC-PORT — DISAGREED (BLOCKING).**

> Nearly everything passes: all 17 vendored blobs at vendor commit `090682e` hash byte-exact against the
> pin record (0 hash mismatches, 0 size mismatches) — I parsed the pin table programmatically rather than
> typing hashes; `ls-files --eol` shows `attr/-text` on all 27; build succeeds; ctest 14/14; both ack
> searches zero **with a working positive control** (baseline exe contains `RECOMP_APU_DSP_ACK` at byte
> offset 13916648; new exe returns -1; `git grep` at baseline `3f8bf67c` returns 14 hits in
> `src/apu/apu_dsp.c` vs 0 now).
>
> The failure is **step 4's enumeration**, in `docs/reviews/a4b1-r4-acport-step4-enumeration.md:37`. It
> states `ep_scratch_rw` (`gp_ep.c:170`) reaches the choke point = **"no"**, giving as its reason
> *"scatter_gather_rw writes only when `dir` is set and the transfer is the GP's"*. That second half is
> false. `scatter_gather_rw` has **no `is_gp` gate** — I read the whole function (`gp_ep.c:85-159`) and
> grepped: the only `is_gp` uses in `gp_ep.c` are at lines 351/364/366 (comments and the bootstrap path),
> none in the write path. And `ep_scratch_rw` (`gp_ep.c:170-177`) calls `scatter_gather_rw`
> unconditionally at `:175`. So the EP's write direction does reach `apu_gp_dma_write` at `gp_ep.c:144`.
> This is a factual error in a mandatory enumeration, in the row that decides "every pinned write
> callback". Whether it is harmless in practice (the EP is only enabled if EPRST is set, `gp_ep.c:650`)
> is exactly the kind of judgment I must not make privately — the criterion says *every* write callback,
> and the record says "no" with a stated reason that is wrong. Fallout: it also means the EP can, if ever
> enabled, land an exchange on `W_va` and take `GP_CLEAR`, which is a `A4b2` decision input.

**AC-LIC — AGREED.** GPL text byte-identical to gnu.org (17984 bytes; SHA256 of the fetched text =
`EDAEF632…233F6`, same as the file). The reviewer **re-derived the 11 GPL / 5 LGPL / 1 headerless split by
reading each vendored file's own header independently** — it matches both NOTICE and the pin record's
licence column. NOTICE states the combined-work licence and amends its MIT paragraph;
`LICENSES/README.md` names both texts. (MEASURED)

**AC-FIX — AGREED.**

> ctest 14/14 (baseline 12 + both registrations, `CMakeLists.txt:141-145`); direct trace-on 715/0 and
> trace-off 156/0 reproduced by me. Critically, **I independently reproduced both mutation tests** rather
> than trusting the summary, and both behave as claimed: (1) `if (s->is_gp)` → `if (1)` gives exactly the
> reported `FAIL: (viii) EP twin: …` + `715 checks, 1 failed`; (2) `if (buf_id < GP_INPUT_FIFO_COUNT)` →
> `if (1)` gives 7 failures including `fifo[5] unchanged (got reads=1 words=64)`. I went further and
> mutation-tested the DS3 inverse that the twins do not cover: disabling the window-VA case fails the
> *window VA* placement and disabling the high-water case fails the *physical offset* placement — so
> (vii) can fail in **both** directions, as required. Line-vs-snapshot is genuinely enforced, not sampled:
> `check_line_vs_snapshot` (`:519-613`) compares `seq/va/observed/payload/site/frame/insns/dsp_addr`
> field-by-field against every latch and asserts exactly-one-line, and `check_gpin_summary` (`:712-810`)
> compares all array elements — including the load-bearing printed-flag-vs-class-latch mirror at
> `:800-809`, which is precisely the check that catches a "parsed but never compared" vacuity. 17 cases
> all defined and called; no test-local copies of ported logic; toolkit tree returned clean at `3a3c7c1`
> after my mutations. (MEASURED)

**AC-DEFAULT — AGREED (one advisory).** `outcome=diagnostic_deadline`; `MEM32(0x001BA858)=0x803C0000` →
`W = MEM32(0x803C0810) = 3`; `F = 2`; counts 0/0/0 — all reproduced, identical to the `A4s` baseline. The
zeros are **not** dead patterns: `[GP*]` has a real control (954 lines in the fixture's stderr file — the
fixture `freopen`s its own stderr, so counting the parent stream gives a misleading 0, a trap the evidence
documents), and `[APUMMIO]` has `A4a` R1 with 344. (MEASURED)

---

## Advisories (verbatim; no effect on disposition)

1. Evidence index names game commit `43ed30a3`; HEAD is `fdce52aa` (`43ed30a` is an ancestor). Stale, not
   wrong.
2. `logs/a4b1/acport-step4.py` derives choke-point and input-path rows from source, but the write-callback
   "reaches?" column is hand-written — which is how the false cell survived an otherwise sound script.
3. MIXBUF's bin formula is correct: `apu_watch.c:628` divides by `NUM_SAMPLES_PER_FRAME` = 32
   (`apu_regs.h:333`). Not a defect; flagged because the two spellings look inconsistent.
4. The fixture's stderr capture lands in the CWD; the reviewer created and removed one in the game root.
5. Current `build\Release\jsrf_recomp.exe` = `939B20EA…` vs R0's archived `B1352185…` — expected after the
   reviewer's mutation rebuilds; R0's archived exe matches the evidence claim and its own metadata.
6. R0's exe **does** embed `[GPBOOT]`/`[GPIN]`/`[GPWATCH]`/`[GPDMA]`/`[APUMMIO]` strings, so the trace
   paths are compiled in and merely unemitted with the trace off.
7. `[A4BSTORE]` is zero because that string exists in **no source and no binary** — instrumentation never
   built in this packet. The count the criterion asks for is 0, so it does not block, but that particular
   zero proves nothing.

---

## Session response

**The reviewer was right, and the Session verified the defect in the source before accepting it.**

| Reviewer's claim | Session verification |
|---|---|
| `scatter_gather_rw` has no `is_gp` gate | **CONFIRMED** — `gp_ep.c:85-159`; the only `is_gp` uses in `gp_ep.c` are `:351` (comment), `:364` (`apu_watch_gp_bootstrap_done(dsp->is_gp)`), `:366` (the bootstrap trace block) |
| `ep_scratch_rw` calls it unconditionally at `:175` | **CONFIRMED** |
| so the EP write direction reaches `apu_gp_dma_write` at `:144` | **CONFIRMED** |

**Re-deriving the call graph showed the error was larger than the reviewer found — three false cells, not
one.** `gp_fifo_rw` (`:212` → `circular_scatter_gather_rw` `:261` → `scatter_gather_rw` `:196` → `:144`)
and `ep_fifo_rw` (`:281` → `:327` → `:196` → `:144`) also reach the choke point and were **not listed at
all**. **All four pinned write callbacks reach it**, each only when `dir` is set.

**Root cause:** the generating script printed the callback lines but **computed no reachability**, so the
"reaches?" column was **hand-written** — the reviewer's advisory 2, and the reason a false cell survived a
script that otherwise reads well. The enumeration has been rewritten with the edges derived from source.

**`DS5` itself is satisfied, and more strongly than the first version claimed:** one choke point, one call
site, **four** callbacks feeding it. The reviewer's consequence — that an enabled EP can land an exchange
on `W_va` and take `GP_CLEAR` — is **confirmed and carried forward as an `A4b2` decision input**, not
silently absorbed. The Session does **not** rule on whether it is benign; that is the judgment the
reviewer correctly declined to make privately, and `DS5`'s text ("from every pinned write callback")
already requires the shared choke point.

**This is the second completeness claim in this packet to be wrong** — the missing `FIFO_READ` hook
(`AC-FIX (viii)`) and now this enumeration. Both were caught by adversarial review, not by the Session's
own checking.

Per §2.2, `NOT ACCEPTED` means the Session now sends the **frozen contract, the evidence, and this
first-stage record** to a **fresh second-stage reviewer** (`workbuddy-ai/deepseek-v4.1-flash` @ `max`),
which re-reviews **only the criteria the first stage did not `AGREE`** — here, **`AC-PORT` alone**.
