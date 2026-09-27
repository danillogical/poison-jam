# `A4b2-NR-epoch-slice-followup-r1` — archived verification scripts

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Why these exist:** the Advisor required (*"Require: script + inputs + outputs + hashes archived before
`A4b2-r8` cites it"*) that the fresh-code re-derivation be **recorded, not merely reported**, so `A4b2-r8`'s
adequacy reviewer can re-run it rather than trust the Session's account.

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

## Input artifacts

All under `logs/runs/`:

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
