# A4b1-r4 execution evidence

**Revision:** `A4b1-r4`, frozen SHA-256
`6DD62A57E87445F5C12085210146204AA4E77D26FD316071FD41CAEC76835C38` (445 lines).
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25/26.
**Toolkit baseline at start:** `M` = `3f8bf67c450861aefcbc376698750bc1446bc9dd` (A4s accepted, pushed).

## Result

| AC | Verdict |
|---|---|
| **AC-PORT** | **PASS** |
| **AC-LIC** | **PASS** |
| **AC-FIX** | **PASS** (14/14, both can-fail twins mutation-proven) |
| **AC-DEFAULT** | **PASS** |
| **Gates G1–G4** | **all PASS** |
| **Row** | **`R1-PASS`** |

---

## Evidence index (Closure requirement)

| Artifact | Path | SHA-256 / value | Result |
|---|---|---|---|
| R0 `result.json` | `logs/runs/20260926-010303-411-a4b1-default/result.json` | `outcome=diagnostic_deadline` | AC-DEFAULT |
| R0 `stacks.txt` | same dir | `F = 2` | Gate F |
| R0 `jsrf_run.log` | same dir | 0 `[APUMMIO]`, 0 `[GP*]`, 0 `[A4BSTORE]` | AC-DEFAULT |
| R0 `process.dmp` | same dir | present | — |
| `a4b1-ctest.txt` | `logs/a4b1-ctest.txt` **and copied into R0's dir** | 14/14 | AC-PORT, AC-FIX |
| `ls-files --eol` output | `src/apu/dsp`, 27 files | all `attr/-text` | AC-PORT step 2 |
| pin record | `docs/reviews/a4b-xemu-pin.md` | complete, incl. 28 local modifications | AC-PORT step 1 |
| **AC-PORT step 4 enumeration** | **`docs/reviews/a4b1-r4-acport-step4-enumeration.md`** | choke point + callbacks + all input paths + `PERIPH`/FIFO classifications | **AC-PORT step 4** |
| exe SHA-256 | `build/Release/jsrf_recomp.exe` | **`B13521858A344919731E73F6E602186E951A49D7E57CA4E929BEA15CC2736ADD`** | AC-PORT |
| toolkit vendor commit | `090682ef81627cc42c2e70288dd58722aaea4152` | 17 files byte-exact | AC-PORT step 3 |
| toolkit head commit | **`3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`** | — | — |
| game commit | `43ed30a37dbad981f610ac12065de1dfc32884f8` | — | — |

**Toolkit commit series (8):** `6e8b8b3` (.gitattributes) → `090682e` (vendor) → `8f8f6e4` (port) →
`772d723` (licences) → `2333b6e` (latch mirror) → `231596b` (FIFO hook) → `4d841d0` (NOTICE fix) →
`3a3c7c1` (fixture).

---

## Readiness / `Stop if` gates — all clear

| Gate | Result |
|---|---|
| `A4s` accepted, strict stop is the `loc_001A18D0` spin | **PASS** — `R-SAME`; `B = 0x803C0000`, `W = 3`, `F = 2` |
| `dsp_ack_frame` / `RECOMP_APU_DSP_ACK` present in `src/apu` | **PASS** |
| GP writes still dropped, reads return 0 | **PASS** |
| Pin record's files at the pinned SHA | **PASS** — all 17 verified |
| No forbidden env vars in the launch environment | **PASS** — all six verified empty at run time |

---

## Gates — all PASS

| Gate | Requirement | Result |
|---|---|---|
| **G1** | `check-run-profile.py` prints `STRICT` | **PASS** — `20260926-010303-411-a4b1-default  STRICT` |
| **G2** | `check-dump-mapping.py` reports `matches ≥ 1`, `content-mismatch: 0` | **PASS** — `matches: 1   content-mismatch: 0` |
| **G3** | `result.json` readable | **PASS** |
| **G4** | `loc_001A18D0: ;` occurs exactly once at `L`, and a line in `L+1..L+3` contains `goto loc_001A18D0;` | **PASS** — **`L = 6748`** (one occurrence); `goto` at `6751` |
| **F** | count in `stacks.txt` of `sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:(L\|L+1\|L+2\|L+3)\b` | **`F = 2`** — both `sub_001A1769+0xB2B … recomp_0005.c:6749` |

`F = 2` matches the `A4s` accepted baseline exactly.

---

## Step 1 — Vendor: **COMPLETE**

All **17** files fetched as **raw bytes** from the pin and SHA-256-matched, then re-verified on the
written files **and on the staged git blobs** — the third check is the one that catches a line-ending
transformation. `git ls-files --eol` reports **`attr/-text`** on every file. `dsp_jit.*` is absent.
**17 files, 427498 bytes, all hashes match.** (Full per-file table in the pin record.)

---

## Steps 2–6 — the port: **COMPLETE**

**Worker** `f9d0e439-…` implemented; the **Session verified independently**. Full record:
`a4b1-r4-implementation-verification.md`.

| Check | Result |
|---|---|
| `build-jsrf.py` | succeeded |
| `ctest` | 12/12 at that point |
| exe SHA-256 | matched the Worker's report exactly |
| `DS3` four-case inverse | read line by line against the Advisor's ruling — **window-VA first**, then high-water, then low-RAM identity, then fail closed; cites `dma_resolve` and `xbox_memory_layout.c:2694`; does **not** import `surface_hits_image`; does **not** use `& 0x03FFFFFF` |
| `DS5` choke point | exact CAS sequence with the D1 retry loop; the dword at `W_va` never ordinary-stored |
| Single choke point | **exactly one** call site: `gp_ep.c:144` |
| **`AC-PORT` step 4 enumeration** | **`a4b1-r4-acport-step4-enumeration.md`** — derived from source, not asserted. It surfaced two things a summary had hidden: **`MIXBUF` has two** call sites (`dsp_cpu.c:916`, `:922`) and **`FIFO_READ` has two** (`gp_ep.c:252` and `dsp_dma.c:333`), the second existing only because `AC-FIX (viii)` found the first insufficient |

**The Worker deleted `src/apu/apu_dsp.c`** (`DS2`+`DS4` left it an empty translation unit; its mixdown
moved to `apu_mixdown.c`). **The Session accepted the deletion.**

---

## Step 7 — licence bookkeeping: **COMPLETE**

`LICENSES/GPL-2.0.txt` is **verbatim** (SHA-256 `EDAEF632CBB643E4E7A221717A6C441A4C1A7C918E6E4D56DEBC3D8739B233F6`,
identical to `gnu.org`). `NOTICE` lists **all 17** ported files under the licence their own header
carries — **11 GPL**, **5 LGPL**, plus the header-less `trace.h` — with the pinned SHA, and states the
**combined-work** licence. `LICENSES/README.md` names both texts.

**A defect the Session found in its own step 7:** the five LGPL files (`gp_ep.*`, `dsp_dma*`) appeared in
NOTICE **nowhere** — they had been put in the GPL section's prose but never listed under LGPL. Found by
verifying AC-LIC against the pin record's licence column instead of trusting the edit; fixed in `4d841d0`.

---

## Step 8 — the `AC-FIX` fixture: **PASS**, 14/14

Full record: `a4b1-r4-fixture-green.md`.

- **17 cases**, all called, none skipped; **no test-local copies** of ported logic.
- Registered **twice** in ctest — trace-on via `ENVIRONMENT`, trace-off.
- `ctest`: **100% tests passed out of 14** (baseline 12 + both registrations), and again after a
  **clean `--clean-first` rebuild**.
- Direct: trace-on **715 checks, 0 failed**; trace-off **156 checks, 0 failed**.

**Both can-fail twins are MUTATION-PROVEN**, not merely present — the Session broke production and
confirmed each twin catches it:

| Mutation | Fixture response |
|---|---|
| remove the `is_gp` gate (`if (s->is_gp)` → `if (1)`) | `FAIL: (viii) EP twin: the EP's DMA leaves every gpin.fifo count at 0 (the is_gp gate)` |
| drop the universe test (`buf_id < GP_INPUT_FIFO_COUNT` → `if (1)`) | 6 failures, incl. `fifo[5] unchanged (got reads=1 words=64)` |

Both reverted, source byte-verified against `HEAD`, tree returned to the committed state.

**A build trap found while reverting, recorded because it nearly produced a false result:** `Copy-Item`
**preserves the original mtime**, so the restored source looked *older* than the object built from the
mutated source and **MSBuild skipped recompiling it**, keeping the mutated object. Touching the source
fixed it; a clean rebuild then confirmed the result does not rest on incremental state.
**Restoring a file by copy does not restore its build state.**

---

## `AC-DEFAULT` — **PASS**

| Requirement | Measured |
|---|---|
| `outcome = diagnostic_deadline` | **`diagnostic_deadline`** |
| `W = MEM32(MEM32(0x001BA858)+0x810) = 3` | base `0x001BA858` → `0x803C0000`; `W_va = 0x803C0810` → **`3`** |
| `F ≥ 1` | **`F = 2`** |
| `[APUMMIO]` count = 0 | **0** |
| `[GP(BOOT\|RUN\|DMA\|IN\|WATCH)]` count = 0 | **0** |
| `[A4BSTORE]` count = 0 | **0** |

**The zero counts are meaningful because the patterns are not dead:** `AC-FIX`'s trace-on arm asserts
those exact lines exist and match the snapshot when `RECOMP_APU_TRACE=1`. The `A4s` accepted baseline
shows the same three zero counts, so the default path is unchanged.

---

## Advisor rulings during execution (binding)

`docs/reviews/a4b1-r4-execution-rulings.md` — the `FIFO_READ` ruling **(C)** and its addendum, recorded
verbatim.

**`AC-FIX (viii)` found a genuine hook-completeness gap.** The Session's first reading was that
`FIFO_READ` was structurally 0 and needed a claim limit; the Advisor **reversed** that. The decisive fact:
the APU library builds with **`NDEBUG`**, so xemu's `assert(!"Unhandled dsp dma buffer")` is **compiled
out** — a GP read from an unimplemented `buf_id` falls through and consumes **stale bytes as its input**,
and no hook recorded it. The Session **verified `NDEBUG` in every Release `PreprocessorDefinitions`**
before implementing.

The Advisor's addendum then caught a defect **in the Session's fix**: `dsp_dma.c` is shared by the GP and
the EP, so the hook had to be gated on `is_gp`. Applied via `DSPDMAState.is_gp` (the permitted
alternative; `container_of` is unavailable).

**Twice in this packet the Session's first reading was wrong** (the other was `P-F`), both times by
reasoning from source text without checking the configuration the artifact ships in.

---

## Preserved, not reopened

Watch-ledger ruling (DS5/6/7), Q1 ruling, owner Q2 decision, checkpoint-40 constraints, `[GPIN]`
accounting ruling, xemu pin `67cc79e663038d1f55448c0f566b37dde016adf6`, the `A4s` AC'97 hunk rulings.

## Follow-ups (recorded, not done)

1. **`NDEBUG`-elided asserts** — enumerate those in `src/apu/dsp/` that change GP state or inputs.
   Known: `unk2`/`unk13`, the `format` default, `dsp_offset` out of range. *Advisor follow-up lead.*
2. The classifier's treatment of the inert `RECOMP_APU_DSP_ACK`.
3. The game repository has no licence file.
4. EP routing.
5. **`A4b2` boundary** — `A4b2-r3`'s `P2` and `AC-INPUTS` are **stale**; `A4b2-r4` is re-briefed later
   (its own revision), per the packet's Closure.
