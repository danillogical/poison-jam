# `A4b2-NR-epoch-slice-followup-r1` — archived verification scripts, outputs and hashes

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Why these exist:** the Advisor required (*"Require: script + inputs + outputs + hashes archived before
`A4b2-r8` cites it"*) that the fresh-code re-derivation be **recorded, not merely reported**, so `A4b2-r8`'s
adequacy reviewer can re-run it rather than trust the Session's account.

> **R3 CLOSED.** This directory now holds **18 files**: 8 scripts, **8 archived outputs**, `MANIFEST.sha256`,
> and this README. Every script was **re-run** to produce its `.out.txt`, all exited **0**, and the manifest
> records each script's SHA-256, its output's SHA-256, its exit code, and the SHA-256 of every **input
> artifact** it read — so a reviewer can reproduce the run against the exact bytes it used.
>
> *(An earlier version of this directory held the scripts and input *paths* but no outputs and no hashes.
> The `A4b2-r8` Planner caught the gap and drafted a fail-closed prerequisite against it; this closes it.)*

These are the Session's **verification** scripts. They are evidence tooling, not production code, and they
are deliberately **not** part of the build.

## The scripts and what each establishes

| Script | Establishes | Inputs |
|---|---|---|
| `independent-block24.py` | The block-24 claim, from **raw artifacts with fresh code reusing none of the earlier tooling** — six checks: the verbatim latch line, block 24's presence and ordinal, `P 000E`'s registers read from the decode, builder B's store offsets, the `scratch_addr` arithmetic, and the absence of stub inputs | `bootstrap_state_decode.txt`, `dma_desc_trace.txt`, `jsrf_run.log` |
| `disjointness-proof.py` | Five builder call sites enumerated with their `r0`; all 10 region pairs disjoint | `bootstrap_state_decode.txt` |
| `backward-slice.py` | The five doorbell field leaves and their reaching definitions | `bootstrap_state_decode.txt`, `gpb9_trace.txt` |
| `v2-cross-boundary.py` | V2 directions (out) and (in): static transfers above `0x173`, computed transfers in image `I`, second-image transfers into slice/dead-block PCs | both decodes |
| `v2-computed-writes.py` | The 296 second-image computed X writes and their base registers | second-image decode |
| `v2-register-bounds.py` | Register-value bounds: `r5`'s writes are all immediates ≥ `0x2AA`; `r0`–`r4`/`r6`/`r7` are memory-loaded | second-image decode |
| `v2-pre-vs-post-exchange.py` | **The decisive one** — extracts the histogram block after the `first-exchange` terminal, showing `gp_pc_range=0000..0172` | `gpb9_trace.txt` |
| `v2-cross-check-watch.py` | Cross-checks the pre-exchange claim from a **different artifact** (the P-write watch: image loaded, not executed) | `gpwrite_watch.txt` |

## Reproducing

From the game root:

```
python -X utf8 docs\reviews\a4b2-epoch-slice-verification-scripts\<script>.py <run-dir> [<run-dir>]
```

The exact arguments used are recorded in the first line of each `.out.txt`. **Verify the input hashes
against `MANIFEST.sha256` first** — the outputs are only meaningful against those exact bytes.

## The decisive output, quoted

`v2-pre-vs-post-exchange.out.txt`:

```
# exec_total=37895 gp_exec_total=37895 ngp_exec_total=0 first_pc=0000 is_gp_seen=1
  gp_pc_range=0000..0172 ngp_pc_range=0000..0000 gp_first_high=0000
-> the GP had executed ONLY image I at the exchange.
```

## Input artifacts

All under `logs/runs/` (hashes in `MANIFEST.sha256`):

| Artifact | Run |
|---|---|
| `bootstrap_state_decode.txt` / `.json` | `20260927-134343-777-a4b2-nrf-b9-arch` |
| `gpb9_trace.txt` | `20260927-134343-777-a4b2-nrf-b9-arch` |
| `gpwrite_watch.txt` | `20260927-140820-853-a4b2-nrf-pwrite2` |
| `dma_desc_trace.txt` | `20260927-145605-040-a4b2-nr-blocklink` |
| second-image decode | `20260927-135115-102-a4b2-nrf-decode2` (`jsrf_run.log`) |

## A caution the scripts carry

**Seven of this analysis's errors were in tooling of exactly this kind**, and two of them produced
plausible-looking wrong answers that would have changed a row selection. In particular
`v2-pre-vs-post-exchange.py` exists because an earlier script read the **last** histogram (run-end) instead
of the one after the `first-exchange` terminal, producing a **false `UNKNOWN`**.

**The header fields are the reliable answer.** `gp_pc_range` and `gp_first_high` state the executed extent
directly; inferring it from a PC list is what went wrong.

**A radix caution too:** `block_addr` prints as `%04X` — **hex**. `0018` is block **24**.
