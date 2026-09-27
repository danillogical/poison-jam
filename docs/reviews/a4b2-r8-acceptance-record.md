# `A4b2-r8` — acceptance record: **`ACCEPT`** (stage 1, final)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a4b2-gp-clears-pending-word.md`, revision **`A4b2-r8`**, frozen SHA-256
**`4D4AFC304F571971EB180C19D6832D56A2CC62FF6EAAFB9E4928716125397C62`**.
**Acceptance reviewer (stage 1):** child `c3d3eba7-1733-4cf2-a68d-f05042aa53f1`,
`workbuddy-ai/hy4-preview-f` @ `high` — the first-stage adversarial gatekeeper per
`docs/agent-workflow.md` §2.2.
**Review:** `docs/reviews/a4b2-r8-acceptance-review.md` (368 lines).

---

## Disposition

> **`DISPOSITION: ACCEPT. BLOCKING: NONE.`**

**The reviewer independently reconfirmed the packet hash** (`4D4AFC30…397C62`) by reading the file itself,
and confirmed the packet was **not edited**.

**Every mandatory criterion and every precondition is `AGREED`:**

| Item | Verdict |
|---|---|
| P1 — XBE and reachability boundary | **AGREED** |
| P2 — accepted implementation and exact decision fields | **AGREED** |
| P3 — EP cannot impersonate a GP clear | **AGREED** |
| P4 — discovery transfer / causal boundary | **AGREED** |
| `AC-DEFAULT2` | **AGREED** |
| `AC-BOOT` | **AGREED** |
| `AC-RUN` | **AGREED** |
| `AC-CLEAR` | **AGREED** |
| `AC-NOCPU` | **AGREED** |
| `AC-INPUTS` | **AGREED** |

## No second stage — and that is required, not an omission

**`docs/agent-workflow.md` §2.2:** *"The second stage runs **only** when the first-stage review does not
return `ACCEPT`. A first-stage `ACCEPT` is final and is not passed on."*

**So the DeepSeek Max stage-2 reproducer is correctly NOT run**, and the fresh Sol High adjudicator is not
engaged (it exists only for a genuine dispute surviving both stages). The reviewer stated this explicitly,
and the Session **did not** route the packet onward.

## What the reviewer reproduced rather than accepted

The reviewer's brief required it to reproduce load-bearing measurements and to try to falsify. It did:

- **`AC-INPUTS`** — *"I did NOT accept 'I fixed my parser'. I rebuilt blocks independently by stripping
  mid-line foreign `[TAG]` injections"*: **6/6** `at_clear` complete, **9/9** summary complete, all six
  identical, **0** monotonicity violations. **Full arrays checked**: PERIPH 128 entries with **only**
  indices `0x33` (exempt), `0x45` and `0x56` (modelled) nonzero and **125 zero**; MIXBUF 26 bins +
  `mixbuf_stub_read=1`; FIFO all zero; DMA `[0,2311,0,0]` with `DEVICE`/`OTHER_MAPPED` zero. **"NO OTHER
  nonzero stub index/class exists."**
- **Wording compliance** — the exempted classes are labelled `EXEMPT` and stated as *"recorded as stub
  reads and not described as modelled or guest-written"*; `MODELLED` appears only on `0x45`/`0x56`.
- **`AC-NOCPU`** — the exe absence check **with a positive control**: `RECOMP_APU_DSP_ACK` and `dsp_ack_`
  both 0 hits, while `JSRF_ALLOW_UNRESOLVED` (1), `RECOMP_GPU_ACK` (2), `RECOMP_APU_TRACE` (1) and
  `RECOMP_APU_TRAP` (1) are found — **"absence is real and search is capable."** All six XBE sites
  reproduced and reconciled 1:1; count `6 == 6`; `N_SITES=16 ≥ 6`; the text lead gives exactly the
  predicted 4 hits.
- **`AC-BOOT`** — its own parser: 64 contiguous `pram` lines, 512 words; **371 words vs the XBE, 0
  mismatches**; **negative control at `0x1A7D64` → 371 mismatches**; last counts `boots=1` reconciles.
- **`AC-DEFAULT2`** — `B0=0x803C0000`, `Wf0=3`, `F0=2` at `:6755 = L+1` with `L=6754`, zero `[GP*]`, R0
  mapping `matches:1 content-mismatch:0`.
- **`AC-CLEAR`** — `CPU_ANCHOR.seq=198851 < GP_CLEAR.seq=198852` **confirmed**; the displaced R1 dump
  handled exactly as the packet requires (`Wf` not reported, nothing routed to CPU/`UNATTRIBUTED`).
- **P4's bridge, measured rather than assumed** — the reviewer diffed the discovery toolkit against r8's:
  `--numstat` shows **165/0** and **26/0**, i.e. **purely additive with zero deletions**, and the added
  code is **env-gated, read-only instrumentation never enabled in R1**. So the interpreter, DMA, scratch,
  loader/watch, periph/MIXBUF handling and latch are **byte-identical**. **The forbidden bridges were
  checked**: descriptor-region counters and DMA chain-restart theory were **not** used, and the tuple match
  is corroboration on top of R1's own VA/tuple/image bind rather than the bridge.

## Advisories — recorded, and they do not change the disposition

The reviewer listed six concerns **outside the contract**, correctly separated:

1. **R1 ended `unhandled_exception` (`0xE0424943`) at 4.77 s, not the 30 s deadline** — heap OOM
   (`requested 598869040`) → invalid ICALL. **No contract violation**: the packet sets no R1
   outcome/liveness requirement and every decision input is log-bound and already emitted. **But R1 is a
   4.8 s prefix of a 30 s window**, and this OOM class may cap future strict runs.
2. R1's `F` is 0, so `AC-CLEAR` step 2's `F` corroboration is empty (R0 supplies `F0=2`).
3. *"Default path unchanged"* compares a 31.7 s R0 against a 4.8 s R1 — evidence about the **path**, not
   parity of coverage. Exactly what the AC asks.
4. **Block-24 identity still rests on one historical latch line** (discovery log `:6899`). The transfer is
   sound as measured, but it is one event in one exploratory run.
5. **Discovery toolkit `aaedd572` ≠ r8 `c151d4e`** — the delta is additive and env-gated so continuity
   holds, **but any future toolkit advance re-opens P4 and cannot be inherited silently.**
6. `logs/` is gitignored, so long-term reproducibility depends on run-archive retention.

**Advisory 1 is the most consequential for planning**: it is the same A2h heap-OOM class the Session
characterized earlier, and it means strict runs terminate at ~4.8 s. It does not touch `A4b2-r8`, whose
decision inputs are all emitted before that point — but it will bound other strict criteria.

## `UNCERTAIN` — nothing load-bearing

Two minor items the reviewer named: (a) the `a4p-r1` and `a4b1-r4` acceptance records were read as
**records**, their own measurements not re-run (accepted-packet provenance, not r8 criteria); (b) the
`at_clear` **printed** block remains admitted on the packet's stated inference — the shared emitter was
verified in source (`apu_watch.c:1223-1296`) but no fixture asserts every printed field, and both in-run
can-fail checks reproduce as passing.

## Outcome

**`A4b2-r8` is ACCEPTED.** The claim it establishes is only what its Claim section permits — the `3→0`
transition at `B+0x810` performed by the GP engine's memory-write path while executing validated image `I`,
in one strict run, with the restated input qualifier the two-leg non-reliance earned. **`A4b2-r7` remains
`R2-EXPL-INPUT`; this is not a retroactive PASS of r7.**

**Next:** the `A4b2` row's next step is the **`PIO_FREE` model**, which the Advisor confirmed is genuinely
next.
