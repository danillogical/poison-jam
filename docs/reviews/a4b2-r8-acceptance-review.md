# `A4b2-r8` — stage-1 acceptance review

**Role:** Acceptance reviewer (stage 1), `docs/agent-workflow.md` §2.2 / §2.2 "Acceptance reviewer".
**Subject:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r8`**.
**Frozen SHA-256 read by this reviewer:**

```
Get-FileHash -Algorithm SHA256 .\docs\packets\a4b2-gp-clears-pending-word.md
4D4AFC304F571971EB180C19D6832D56A2CC62FF6EAAFB9E4928716125397C62
```

**Matches the SHA-256 named in the brief.** These exact bytes were reviewed; the packet was not edited.

**Session row under review:** `R2-PASS`, per `docs/reviews/a4b2-r8-execution-evidence.md`.

**Method.** Every load-bearing measurement below was reproduced by this reviewer from the archived
artifacts. All commands are read-only (hashing, log parsing, `git grep`/`show` at the pinned toolkit
commit, `inspect-jsrf.py`, `check-dump-mapping.py`, `check-run-profile.py`). No file other than this
review was written; nothing was rebuilt; the guest was not run.

**Evidence artifacts:** R1 `logs/runs/20260927-160330-655-a4b2-gp-trap-trace/` (STRICT), R0
`logs/runs/20260927-160335-562-a4b2-default/` (STRICT).

---

## 0. Identity and gate reproduction

| Item | Required | Measured | Verdict |
|---|---|---|---|
| XBE SHA-256 | `FD190557…EF9C` | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` | match |
| exe SHA-256 (R1) | `BC8E288D…DC51` | `BC8E288DD54D8A09DA1630AB933EB1A808AEC9C60CEE2C64B9742B5C0CB8DC51` | match |
| exe SHA-256 (R0) | same | `BC8E288D…DC51` | match (identical exe) |
| Toolkit HEAD | `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52` | same, **clean** (0 dirty files) | match |
| R1 profile | STRICT | `20260927-160330-655-a4b2-gp-trap-trace  STRICT` | match |
| R0 profile | STRICT | `20260927-160335-562-a4b2-default  STRICT` | match |
| R1 log integrity | complete | `run_log_sha256` = `c52d7165…c874` = measured | match |
| R0 log integrity | complete | `run_log_sha256` = `35427436…d575` = measured | match |

Game commit at review time is `0fa09444172ae4094c921d2c421a98bb282bedca`; the evidence's
`b3f22cb210f453940953fc357f00ab4d57e7d259` is a verified ancestor (`git merge-base --is-ancestor`
exit 0). The two later commits are the r8 execution and closure records — the step-1 forwarding edits
are unchanged between them (P4.5 verified below at the current tree).

**G4 reproduced:** `Select-String -Path .\src\recomp\gen\recomp_0005.c -Pattern 'loc_001A18D0:'` →
exactly one hit, line **6754**. Line 6751 is `jsrf_watch_store(0x001A18CEu, ebx, eax);` immediately
above line 6752 `MEM32(ebx) = eax;` (the preceding nonblank store), and line 6755 is
`if (CMP_NE(...)) goto loc_001A18D0;` — i.e. `L+1`. **G4 holds**, `L=6754`.

---

## P1 — XBE and reachability boundary — **AGREED**

- `Get-FileHash -Algorithm SHA256 .\game\default.xbe` → `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` (measured).
- `docs/reviews/a4p-r1-acceptance-review.md:9` — **ACCEPT**, row **`O-GATE`**; `:17` records the same
  XBE hash `FD190557…EF9C`.
- The Session restates P1's caveats (inferred register-convention premise, blind beyond E3, no
  timing/true-`0x80` claim) at `a4b2-r8-preconditions.md:17-20`.

## P2 — accepted implementation and exact decision fields — **AGREED**

- Required toolkit `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`; measured HEAD is exactly that commit and
  the tree is clean. Not the earlier accepted `3a3c7c1f…`, not a later HEAD — the packet's condition is
  met literally, so no source-delta analysis is triggered.
- `A4b1-r4` ACCEPT with `R1-PASS`: `docs/reviews/a4b1-r4-stage2-acceptance-review.md:1,13,14` —
  `# A4b1-r4 — stage-2 acceptance review: ACCEPT (final disposition)`, `DISPOSITION: AGREED`,
  `BLOCKING: NONE`. Its accepted toolkit identity `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d` is
  recorded at `a4b1-r4-stage2-acceptance-review.md:77` and `a4b1-execution-evidence.md:56`.
- The six decision-field call sites and the observer map persist (P4.5 below, measured).

*Note:* the brief's "game `b3f22cb`" is the promote commit, an ancestor of the current HEAD. The
packet's P2 requirement is on the **toolkit** identity and effective source state, which is exact.

## P3 — EP cannot impersonate a GP clear — **AGREED**

Command run exactly as specified:

```
git -C C:\Users\logic\Repos\xboxrecomp grep -n -E 'ep_ops|ep\.regs\[|0x50000' c151d4e32a782e4e5adcecbc68afe61ed5fc7e52 -- src/apu
```

**14 hits**, all read. Permitted-form classification reproduced:

| Site | Form | Permitted |
|---|---|---|
| `apu_core.c:30`, `apu_state.h:374` | comment | yes |
| `gp_ep.c:586` `const MemoryRegionOps ep_ops = {` | definition (**positive control**) | yes |
| `gp_ep.h:48` `extern const MemoryRegionOps ep_ops;` | declaration (**positive control**) | yes |
| `gp_ep.c:544` `r = d->ep.regs[addr];` | read in `ep_read` | yes |
| `gp_ep.c:576,577,580` | writes, all inside `ep_write` — EPRST write present (**positive control**) | yes |
| `gp_ep.c:630,631,668,669` | EPRST reads in gate conditions | yes |
| `apu_core.c:648`, `:674` | `0x50000` in **comments**, no routing arm | yes |

Additional negatives measured: `grep -E '&ep_ops|ep_ops\)'` returns **empty** (no registration/call);
`MCPXAPUState` is `calloc`'d (`apu_core.c:543`); EP execution is EPRST-gated (`gp_ep.c:668-669`).
Build identity equals the checked SHA. Output was complete, not truncated.

## P4 — discovery transfer / causal boundary — **AGREED**

| P4 element | Measurement |
|---|---|
| Manifest hashes | re-hashed every `SCRIPT` (script + output) and `INPUT` entry in `docs/reviews/a4b2-epoch-slice-verification-scripts/MANIFEST.sha256`: **ok=25, mismatched=0, missing=0**; all 8 recorded script exits are `0` |
| Archived latch artifact | `independent-block24.out.txt` line: `[GPDMADESC] GP_CLEAR produced by block_addr=0018 (dsp_addr=000800)`; the same line occurs verbatim in the original run log `logs/runs/20260927-145605-040-a4b2-nr-blocklink/jsrf_run.log:6899` — the **only** `GP_CLEAR produced by` line in that log |
| Block-24 identity | the archived output's own arithmetic: `r0=#$18` = 24, `+4` slot `#$000800`, `scratch_addr = 0 + 0x800 = 0x800`, `MATCH: YES`; `VERDICT: no stub input in any field source` |
| Six sites / XBE / forwarder | forwarder present at `src/diagnostics.c:130-138` (one-shot `apu_watch_set_anchor_site(0x001A18CEu)` then `apu_watch_cpu_store`); externs at `recomp_0000.c:12`, `recomp_0005.c:12`; six call sites at `recomp_0000.c:135287/135411/135877` and `recomp_0005.c:6528/6751/8094`, each with the required comment; XBE = P1's hash |

**Source/version bridge (the load-bearing transfer step).** The discovery run
`20260927-145605-040-a4b2-nr-blocklink` was built at toolkit **`aaedd572e7a4844cfa52194ebc23351b8e9c9071`**
(`metadata.json → repository_identities.toolkit.revision`), while r8 built at `c151d4e…`. I measured the
gap directly:

```
git merge-base --is-ancestor aaedd572… c151d4e…          → exit 0 (ancestor)
git log --oneline aaedd572…..c151d4e…                    → 2 commits
git diff --numstat aaedd572… c151d4e…                     → 165  0  src/apu/apu_watch.c
                                                            26  0  src/apu/dsp/dsp_dma.c
```

Both files are **purely additive (0 deletions)**, and `git diff | grep '^-'` on `apu_watch.c` returns
**no non-header removal**. The added code is the descriptor/scratch trace instrumentation, env-gated and
read-only: `dsp_dma_descriptor_trace` (`apu_watch.c:1043`) inits from `RECOMP_APU_DMA_DESC_TRACE` and
returns unless enabled; `dsp_dma_scratch_fields` (`apu_watch.c:1028`) returns unless `dmad.on && s->is_gp`.
Neither was enabled in R1 (`grep -c PERTURB` on the R1 log = 0; no `[GPDMADESC]` line in R1). So the
discovery-final interpreter, DMA descriptor and scratch calculation, P-memory loader/watch, periph/MIXBUF
input handling and clear latch are **byte-identical** between the discovery run and the r8 build. The
bridge is checkable and holds.

**Forbidden-bridge check.** P4 forbids descriptor-region counters and DMA chain-restart theory. I grepped
both r8 records for those terms: the only hits are the Session's explicit *disavowal*
(`a4b2-r8-preconditions.md:136` — "Descriptor-region counters and DMA chain-restart theory are NOT used
to bridge identity") and two unrelated uses of the word "chain" describing reaching chains. The archived
`independent-block24.out.txt` does print `in_doorbell_region=255 in_mixbin_region=254` as raw trace
context, but the identity conclusion in that artifact rests on the latch line plus the `P 000E`
register/field arithmetic and read ordinals — **not** on those counters. The Session did not rely on them.

**Tuple match is not the whole bridge.** P4 requires the new R1 to bind its own VA, tuple and image. It
does: `va=803C0810`, `observed=3`, `payload=0`, `dsp_addr=000800` from R1's own latch (AC-CLEAR), image
`I` from R1's own `[GPBOOT]` block (AC-BOOT). The tuple agreement is corroboration on top of a checked
source-continuity bridge, not the bridge itself.

---

## AC-DEFAULT2 — **AGREED**

| Required | Measured | Command / evidence |
|---|---|---|
| R0 outcome `diagnostic_deadline` | `diagnostic_deadline`, exit 3, 31.7 s | `logs/…/20260927-160335-562-a4b2-default/result.json` |
| `Wf0 = 3` | `B0 = MEM32(0x001BA858) = 0x803C0000`; `MEM32(0x803C0810) = 00000003` | `python -X utf8 scripts\inspect-jsrf.py memory <R0> 0x001BA858 4` and `… 0x803C0810 4` |
| `F0 ≥ 1` | **2** | `Select-String -Path <R0>\stacks.txt -Pattern 'sub_001A1769\+0x[0-9A-Fa-f]+ .*recomp_0005\.c:(6754\|6755\|6756\|6757)\b'` → lines 21123, 21124 (both `:6755` = `L+1`) |
| zero `[GP*]` lines | **0** for `GPBOOT`, `GPRUN`, `GPDMA`, `GPIN`, `GPWATCH`; **0** for a broad `\[GP` scan | `Select-String` counts |
| `RECOMP_APU_TRAP`/`RECOMP_APU_TRACE` absent | **both absent** from R0 `metadata.json → settings` (only `JSRF_COLLECTED`, `JSRF_LOG_PATH`, `RECOMP_GPU_ACK=0`, `RECOMP_KERNEL_LOG_BUDGET`) | `metadata.json` |
| R0 mapping `matches≥1`, `content-mismatch: 0` | `matches: 1   content-mismatch: 0   unreadable: 0   missing: 0`; control `8b512c85d28b4130c70190431c00741c` | `python -X utf8 scripts\check-dump-mapping.py <R0>` |

`[APUMMIO]` count in R0 = 0, recorded as observation only. `F0=2` matches the accepted `A4b1-r4` oracle
(`W=3`, `F=2`). All three positive-control elements present.

## AC-BOOT — **AGREED**

- **N=1** `[GPBOOT]` header, `jsrf_run.log:3217`: `n=1 sge0=003C0000 sge0_va=803C0000 gprst=00000003 prev=00000001`.
  My own parser (independent of the Session's) confirms: 1 block, **64** pram lines, indices contiguous
  `000..1F8` step 8, **512** words, `contig=True`.
- `gprst=00000003` (GPRST and GPDSPRST both set) ✓; `prev=00000001` → both **clear** ✓.
- `sge0_va=803C0000 = T` ✓; `sge0=003C0000` raw, recorded as context.
- **Last** counts line (`jsrf_run.log:16063`) has `boots=1`; `N=1` — reconciles. 13 counts lines total,
  the first two showing `boots=0` before the bootstrap, consistent with the packet's note that counts
  emission precedes `boots++`.
- **Image comparison reproduced:** all 0x173 = **371** words compared; `0 mismatches`.
  **Negative control:** same comparison at XBE file offset `0x1A7D64` → **371 mismatches**, first three
  `word0: 0BF080 vs 000155`, `word1: 000155 vs 300600`, `word2: 300600 vs 310000`. The control fails as
  required, so the pass is not a vacuous comparison.
- **B binding:** `B = sge0_va = 0x803C0000`; anchor `va = 0x803C0810`; `B + 0x810 = 0x803C0810` exactly,
  no wrap. Common-`B` and anchor cross-check valid.

## AC-RUN — **AGREED**

Counts line `jsrf_run.log:16063`: `boots=1 ≥ 1`, `gp_frames=768 ≥ 1`, `gp_insns=33120534 > 0`. PASS per
the packet's rule (PASS takes precedence over the two FAIL rows, which require *every* line to be 0).

## AC-CLEAR — **AGREED**

Latch lines reproduced from the R1 log (raw, not reconstructed):

```
6817: [GPWATCH] latch class=GP_CLEAR   seq=198852 va=803C0810 observed=00000003 payload=00000000 site=00000000 frame=256 insns=124652 dsp_addr=000800
5769: [GPWATCH] latch class=CPU_ANCHOR seq=198851 va=803C0810 observed=00000000 payload=00000003 site=001A18CE frame=256 insns=0 dsp_addr=000000
```

Step-2 conditions, all measured:

| Condition | Value | Result |
|---|---|---|
| `GP_CLEAR.va == B+0x810` | `803C0810 == 0x803C0000 + 0x810` | ✓ |
| `observed` | `00000003` | ✓ |
| `insns > 0` | `124652` | ✓ |
| `dsp_addr` recorded | `000800` | ✓ |
| **`CPU_ANCHOR.seq < GP_CLEAR.seq`** | **`198851 < 198852`** | ✓ |

Ledger alive (13 counts lines); `CPU_ZERO_OVERFLOW` never latched (0 in every counts line); `CPU_ANCHOR`
present. So step 1's UNKNOWN triggers do not fire and step 2 PASS is reached. `GP_CLEAR` is write-once
(one latch line only).

**Displaced-dump handling (probe 4).** R1 mapping measured independently:
`matches: 0   content-mismatch: 1` → `CONTENT_MISMATCH`, "Not usable for an IMAGE-CONTENT claim". The
Session reports `Wf` **NOT** as evidence and routes nothing to CPU/`UNATTRIBUTED`
(`a4b2-r8-execution-evidence.md:77-85`, and `a4b2-execution-evidence.md:268` — "**`Wf` explicitly NOT
evaluated or reported from R1**"). Compliant. Displacement does **not** displace the row: the packet's
AC-CLEAR step 2 needs only log-bound latch and counts, and its dump boundary explicitly says no
GP-transition PASS-path claim reads the R1 dump and no global R1 mapping gate is required.
`F` (R1 `stacks.txt`) measured **0** — recorded as corroboration only; it neither helps nor blocks step 2.

## AC-NOCPU — **AGREED**

**Absence check with a positive control** (probe: absence must be shown findable). Byte-level scan of
R1's `jsrf_recomp.exe`:

| Token | Hits |
|---|---|
| `RECOMP_APU_DSP_ACK` | **0** |
| `dsp_ack_` | **0** |
| `JSRF_ALLOW_UNRESOLVED` (**positive control**) | **1** — the search finds things |
| `RECOMP_GPU_ACK` (control) | 2 |
| `RECOMP_APU_TRACE` (control) | 1 |
| `RECOMP_APU_TRAP` (control) | 1 |

The absence is real and the search is demonstrably capable. Environment: R1 `metadata.json → settings`
lists `RECOMP_APU_TRACE=1`, `RECOMP_APU_TRAP=1`, `RECOMP_GPU_ACK=0`, `RECOMP_KERNEL_LOG_BUDGET=100000`
— **no `RECOMP_APU_DSP_ACK`**. Toolkit `src/apu` at `c151d4e` has no ack token hits.

**Contested zero:** latch classes present are only `BOOT_SCRATCH_READ`, `MIXBUF_STUB_READ`,
`CPU_ANCHOR`, `GP_CLEAR`, `GP_ZERO_OVER_OTHER`. **No `CPU_ZERO` and no `CPU_ZERO_OVERFLOW` latch**, and
`CPU_ZERO=0` on all 13 counts lines. No contested zero.

**Six-site reconciliation.** All six `inspect-jsrf.py disasm` ranges from the packet were run against
the unchanged XBE; each site's instruction and guest VA reproduce exactly:

| Guest VA | XBE instruction (measured) | Generated call (measured) |
|---|---|---|
| `0006DACE` | `mov dword ptr [eax + 0x810], ecx` | `recomp_0000.c:135287 jsrf_watch_store(0x0006DACEu, eax + 0x810, ecx)` |
| `0006DBBA` | `mov dword ptr [ecx + 0x810], eax` | `recomp_0000.c:135411 jsrf_watch_store(0x0006DBBAu, ecx + 0x810, eax)` |
| `0006DEF3` | `mov dword ptr [ecx + 0x810], eax` | `recomp_0000.c:135877 jsrf_watch_store(0x0006DEF3u, ecx + 0x810, eax)` |
| `001A1751` | `and dword ptr [edi + 0x810], 0` | `recomp_0005.c:6528 jsrf_watch_store(0x001A1751u, edi + 0x810, 0)` |
| `001A18CE` | `mov dword ptr [ebx], eax` (after `001A18C6 add ebx, 0x810`) | `recomp_0005.c:6751 jsrf_watch_store(0x001A18CEu, ebx, eax)` |
| `001A1FA7` | `mov dword ptr [edi + 0x10], ebp` (after `001A1FA1 add edi, 0x800`) | `recomp_0005.c:8094 jsrf_watch_store(0x001A1FA7u, edi + 0x10, ebp)` |

Disassembly was complete for all six ranges (no truncation; `.byte` occurrences at `0006DBE7` and
`0006DEFF` lie outside the enumerated sites). `count found == 6 == frozen count`, each site evaluated.
`APU_WATCH_N_SITES 16` (`apu_watch.h:110`) `≥ 6`. The text lead `MEM32\(.* \+ 0x810\) =` yields exactly
**4** hits (`recomp_0000.c:135288/135412/135878`, `recomp_0005.c:6529`) — the packet's predicted
4-because-`0x001A1751`-also-matches behaviour, deduplicated to three other sites. Exactly six
`jsrf_watch_store` call sites exist in total (no extras).

## AC-INPUTS — **AGREED**

Evaluated (AC-BOOT/RUN/CLEAR/NOCPU all PASS).

**Step 1 — block integrity, reconstructed independently.** Foreign `[TAG]` records (e.g. `[APUMMIO]`,
`[KERNEL]`, `[RECOVERED]`) are injected *mid-line*, splitting `at_clear`/`summary` arrays across physical
lines. I stripped foreign records including their newline from the raw log and re-parsed:

- **6 `at_clear` blocks**, each with a `seq=198852 frame=256` header and **exactly 5** tagged lines
  (`mixbuf`, `periph`, `fifo`, `dma`, `flags`) — 6/6 complete.
- **9 `summary` blocks**, **9/9 complete** (each with all five lines).
- **(i)** all 6 `at_clear` blocks **identical to the first** after whitespace normalisation (single
  distinct normalised block). Header `seq=198852` **== `GP_CLEAR.seq=198852`**.
- **(ii)** **0 monotonicity violations** of every `reads`, `reads_while_stub`, `words`, `bytes` element
  and every flag against the **last complete** summary block.

*On the Session's six reported parser errors:* I did not take the correction on trust — I rebuilt the
blocks with my own parser and reached the same 6/6 and 9/9 completeness, the same all-identical result
and the same zero violations. Two of their diagnoses are independently confirmed by my run: a naive
header matcher on `seq=` alone matches `first_seq=[…]` fields inside array lines (I anchored on
`seq=<n> frame=<n>`), and line-window parsing is broken by mid-line `[APUMMIO]` injection (line 13041 is
exactly such a case — `[GPIN] at_clear mixbuf reads=[8128  [APUMMIO] read  0x3022C = 000009BE`).
Their corrected result is correct. Error 6 is also sound: `mixbuf_stub_read` is emitted from
`g->mixbuf_stub_read.latched` (`apu_watch.c:1281`), i.e. a latched provenance flag, and the packet's
step (ii) monotonicity rule is stated for **counters**; its value is `1` in all six `at_clear` emissions.

**Step 2 — integrity.** `out_of_universe=0` ✓; `boot_scratch_read=1` ✓ (positive presence witness, the
`BOOT_SCRATCH_READ` latch is at `jsrf_run.log:3209`); FIFO all six slots `reads=[0,0,0,0,0,0]`
`words=[0,0,0,0,0,0]`, so slots 2–5 are zero ✓.

**Step 3 — classification. Full arrays checked, not just the claimed ones.** From the first `at_clear`
block (array lengths vs the packet's universes: MIXBUF 32 = `NUM_MIXBINS`, PERIPH 128 =
`DSP_PERIPH_SIZE`, FIFO 6 = `GP_INPUT_FIFO_COUNT(2)+GP_OUTPUT_FIFO_COUNT(4)`, DMA 4 =
`APU_WATCH_DMA_CLASSES`):

| Class | Measured nonzero | Disposition |
|---|---|---|
| PERIPH idx `0x33` (51) → `0xFFFFB3` | 1017 | **exempt** (two-leg non-reliance) |
| PERIPH idx `0x45` (69) → `0xFFFFC5` | 1020 | **modelled** (`dsp.c:59,111` case arms) |
| PERIPH idx `0x56` (86) → `0xFFFFD6` | 3065 | **modelled** (`dsp.c:71,123`) |
| all other 125 of 128 PERIPH offsets | **0** | — |
| MIXBUF `reads` / `reads_while_stub` | 26 of 32 bins nonzero, `mixbuf_stub_read=1` | **exempt** |
| FIFO slots 0–1 and 2–5 | **0** | — |
| DMA `[LOW_RAM, CONTIG, DEVICE, OTHER_MAPPED]` | `[0, 2311, 0, 0]`; `bytes=[0,49972,0,0]`; `first_va=[…,803CC000,…]` | `CONTIG` guest-written **by region**; `DEVICE`/`OTHER_MAPPED` = 0 |

DMA index order verified from source (`apu_watch.h:160-164`: `LOW_RAM=0, CONTIG, DEVICE,
OTHER_MAPPED`), so index 1 is `CONTIG` — the nonzero class is the exempted-by-region one, and both
`DEVICE` and `OTHER_MAPPED` are 0 as required. PERIPH indices verified against
`DSP_PERIPH_BASE=0xFFFF80` (idx 51 → `0xFFFFB3`, idx 69 → `0xFFFFC5`, idx 86 → `0xFFFFD6`).

**No other nonzero stub index or class exists.** Every nonzero element in the entire frozen block is
accounted for above: PERIPH 3 of 128, MIXBUF 26 of 32, DMA 1 of 4, FIFO 0 of 6. The two exempted classes
are the only nonzero stub classes.

**Wording (probe 2).** The exempted classes are labelled `EXEMPT — two-leg non-reliance` and the record
states they "are recorded as **stub reads** and are **not described as modelled or guest-written**"
(`a4b2-r8-execution-evidence.md:125-126,131-132`). `MODELLED` is applied only to `0x45` and `0x56`.
Compliant with the packet's requirement.

**Exchange-identity reconciliation.** Observed tuple `va=803C0810`, `observed=3`, `payload=0`,
`dsp_addr=000800` matches the proved exchange's `dsp_addr=000800` (block 24, `x:[24..28]`, scratch
`0x800`), the pinned image `I` is validated by AC-BOOT, and the source/bytes/guard continuity is
measured above (additive-only, env-gated delta). No new stub→clear field/guard chain was found by this
reviewer's independent scan of the frozen counters.

**PASS:** steps 1–2 valid; P4 valid; every step-3 stub counter/flag outside the two exempted classes is
zero.

---

## Disposition

Every mandatory criterion and every precondition is `AGREED`.

**DISPOSITION: ACCEPT. BLOCKING: NONE.**

Per §2.2, a first-stage `ACCEPT` is final and is **not** passed to stage 2.

---

## Advisories (outside the contract; do not change the disposition)

1. **R1 terminated `unhandled_exception` / `0xE0424943` at 4.77 s, not the 30 s diagnostic deadline.**
   `result.json` (`outcome: unhandled_exception`, `exit_code: 3762440515`, `duration_seconds: 4.765646`);
   the log tail shows `xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)` →
   `[ICALL] invalid target 0x00000000` → `[EXCEPTION] … code=0xE0424943`. The packet has no
   outcome/liveness requirement for R1 — its decision inputs (latch, counts, `[GPBOOT]`, `[GPIN]`) are
   log-bound and already emitted before the OOM — so this is **not** a contract violation. But it means
   R1 is a 4.8-second prefix of a 30-second window, and the same OOM class may limit how far any later
   strict run can get.
2. **R1's `F` is 0**, so `AC-CLEAR` step 2's `F` corroboration is empty in this run. Harmless here
   (`F` is corroboration only, and R0 supplies `F0=2`), but it means the R1 stack witness is absent.
3. **The evidence's headline "the default strict path is unchanged" rests on a 31.7 s R0 while the
   trap/trace R1 lasted 4.8 s.** The two runs are not the same length of guest execution, so
   `AC-DEFAULT2`'s "unchanged" is evidence about the default *path*, not about parity of coverage.
   Within the contract this is exactly what `AC-DEFAULT2` asks for.
4. **Block-24 identity still rests on a single historical latch line** (`jsrf_run.log:6899` of the
   discovery run), corroborated by immediate field arithmetic. The r8 transfer is sound as measured
   (byte-identical source, matching tuple, R1's own image bind), but the underlying observation is one
   event in one exploratory run.
5. **The discovery run's toolkit (`aaedd572…`) is not the r8 build's (`c151d4e…`).** The delta is
   additive and env-gated, so continuity holds; but any *future* toolkit advance re-opens P4 and cannot
   be inherited silently.
6. **The `logs/` tree is outside git** (gitignored), and the game tree is clean while the review-record
   files are untracked at the time of the runs. Long-term reproducible verification of these measurements
   depends on `logs/runs/` retention, which version control does not guarantee.

## Uncertain

- Nothing load-bearing was left unverified. Two minor items, neither affecting any verdict:
  - `docs/reviews/a4p-r1-acceptance-review.md` and `a4b1-r4-stage2-acceptance-review.md` were read as
    *records*; their underlying measurements were not re-run (they are accepted-packet provenance, not
    r8 criteria, and the packet asks only that the acceptance and identities be verified).
  - The `at_clear` **printed** block remains admitted on the packet's stated inference: the shared
    emitter is verified in source (`apu_watch.c:1223-1296` — `emit_gpin_block("at_clear",
    &s.at_clear.gpin)` uses the same formatter as `summary`), but no fixture asserts every printed
    `at_clear` field against the snapshot. The packet admits this explicitly and supplies the two in-run
    can-fail checks, both of which I reproduced as passing.
