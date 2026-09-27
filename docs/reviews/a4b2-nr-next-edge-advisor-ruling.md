# Advisor ruling — `A4b2-NR-next-edge-r1` execution: the two-image finding, scope of invalidation, and the loader-first mandate

**Advisor:** Persistent Advisor, `docs/agent-workflow.md` §2.3. **Handle:** `muse_FkNhGaXtV9P5`
(`.muse-workers.md`). **Model:** `muse-spark-1.3-contributor`. **Effort:** `max` (plugin default; no
override sent).

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Trigger:** Session ruling request after executing `A4b2-NR-next-edge-r1` and finding that the GP loads a
**second program image** into P-memory after the bootstrap.
**Basis:** `docs/reviews/a4b2-nr-next-edge-execution-evidence.md` (whole), `plan-jsrf-bare-minimum.md`
`CURRENT PACKET`, `docs/reviews/a4b2-nonreliance-discovery-evidence.md` §8.

**Protocol note.** The `muse_session_send` timed out at 600 s. Per §4.4 the turn was collected with
`muse_session_read` **before any further send** on the handle; the second read returned the completed text.
No concurrent send was made.

---

## RULING (verbatim)

> All verifications done, including a direct read confirming the Planner's correction of my line citation (see closing note). Ruling:
>
> ## RULING
>
> **Row: `O-INCONCLUSIVE` CONFIRMED.** The packet's own row covers this exactly ("unproven upper-word identity," packet:29; stops on "missing/partial upper P words," packet:33). Not `REFUTED` — no feasible causal chain found, and with wrong input bytes even a found chain would have been suspect. `A4b2-r7` stays `R2-EXPL-INPUT`.
>
> ### Q1 — Scope of invalidation: agreed, with the withdraw list made explicit. No `§5.4(2)` anywhere
>
> **Withdrawn or re-scoped (may not be relied on in current form):**
> 1. `full_decode.txt` AS A SLICE INPUT above `0x172` — wrong bytes. Keep, relabeled, as the bootstrap-state record (that role is cross-validated and valid).
> 2. The §3 CFG edge set / reachability numbers (194/2340, 21 returns, 0 indirect) AS CLAIMS ABOUT THE EXECUTED PROGRAM. Keep the method, both tooling fixes (`pc+1` fall-through; `(rN)` data-move misclassification), and the finding that single-entry linear CFG undercovers interrupt/vector entry — that gap is about the entry model, robust to the byte issue.
> 3. The seven Q2 re-derivations' SCOPE ("over all 3881 words" of one image). The seven tasks survive; their input bytes are void pending Q3's epochs.
> 4. The "one 3881-word program" framing. It is ≥2 images/epochs until the loader work says otherwise.
>
> **Not reached:** every local reading inside unchanged `0x000`–`0x172` (doorbell path, B3 sites/flows, table, `P 00B9`/`P 00A2`/`P 0086`–`008C`); the §8 measurement table; the doorbell tuple; L2; the `op =` non-execution inference (executed PCs decode whichever image is live; table words remain undecodable data in both snapshots); `A4b2-r7`'s `EXPL-INPUT`; `A4b1-r4` (nowhere near this); my prior forbids and carries.
>
> **No `§5.4(2)`:** (a) next-edge-r1 is discovery, executed, terminal row faithful — its working premise ("I + static upper region") failed in exactly the way the contract anticipated (epoch-identity fail-closed, packet:18) and routed (followup, not revision — that IS the §5.8 lifecycle). A premise whose failure mode the contract routes is not a `PREMISE_CHANGED` event. (b) `A4b2-r7`'s contract contains no single-image premise. (c) This ruling supersedes my last Q2 lists AS TO SCOPE only (same seven items, epoch-stable bytes). The Q2-preparation doc is a working doc — Session updates by editing.
>
> ### Q2 — Both: reassuring as consistency, trap as proof. The instruction/path distinction decides it
>
> **Helps:** doorbell instructions byte-identical + tuple unchanged localizes the unknown — the slice problem is now "callers/guards of a known-stable path + second-image provenance," and L2's exchange is the same event.
>
> **Trap if read further.** Two gaps byte-identity cannot close: (a) **transient modification** between snapshots is invisible to a two-sample comparison; (b) **the doorbell PATH may extend into the second image** — who calls `P 0000`–`0007`, what guards the trigger, and whether THOSE depend on stub inputs is entirely open. Instruction survival ≠ path survival. Forbid citing byte-identity as path-independence.
>
> **Established (not merely observed) requires:** (i) full callers/guards/reaching-defs closure over correct epoch-stable bytes (the eventual slice); (ii) a P-memory write-watch over the image-I range showing no writes bootstrap→exchange (closes the transient gap); (iii) carried L2 (have it). Note (ii) is the same watch Q3 needs — one instrument answers both.
>
> ### Q3 — Loader/epochs FIRST (gating), slice SECOND. Slice-direct is forbidden as a first step
>
> **Target confirmed**, order mandated. Slicing the at-exchange image directly repeats the error class one level deeper: if P-memory is modified continuously (overlays/staging), at-exchange bytes are wrong for parts of the execution too. A slice may only be built on bytes PROVEN STABLE over its claimed window — and that proof IS the loader/epoch work. Provenance is load-bearing for slice validity (guest firmware vs toolkit-generated vs self-modifying mean different things), so deferring it as a "separate thread" is forbidden.
>
> **Required phase 1:** (1) load event(s) — trigger, source bytes, extent; rule a second GPRST bootstrap in/out FIRST from existing logs (`boots` + `[GPBOOT]` block count — cheapest check, before any new run); (2) epoch structure + P-write-watch record showing quiescence (or mapping every modification) across the slice window; (3) image-I stability (closes Q2's transient gap as a by-product). **Phase 2** (slice) proceeds only on phase-1-covered bytes; if P-memory never quiesces, say so (per-epoch slicing or `O-INCONCLUSIVE` with recorded reason — never a fudged single-image slice).
>
> ### Q4 — No priority change. B3-ID stays deferred; B3-enumeration rides the slice
>
> The second image changes nothing about B3's status: known sites + scratch flows sit in unchanged I; meaning still unknown; the no-model obligation stands. Re-derivation #1 (all B3 sites over correct bytes — second image may contain reads; "four" stays forbidden) joins the next slice. Identification (`A4b-B3`) stays deferred — it never gated L1/L2 and still doesn't. Loader/epochs gate everything.
>
> ### Q4-carry, Q5 — The two-image finding is ADMISSIBLE as relied-on diagnostic fact; no extra histogram control required
>
> **Sufficient, on four observed legs:** (1) GP attribution structural (request issued in the `GP_CLEAR` freeze path, `apu_watch.c:995`–1025; consumed by the executing core at next instruction entry, `dsp_cpu.c:1070`–1076) + measured (exactly one core, `is_gp=1` via the fixed opaque test — the precise failure mode from last packet, now verified); (2) read-only (P-memory reads + text emission; pending-flag + disasm scratch only; no X/Y/P/guest writes in the path); (3) no-perturbation (decode runs AFTER latch+freeze, so cannot touch the recorded tuple; tuple unchanged both runs; at-exchange content is coherent multi-word code with intact fill islands — incompatible with a corruption artifact; a one-instruction skew cannot move 2480 words); (4) bootstrap snapshot cross-validated (`[GPBOOT]` 371-word match from accepted r7 evidence; zeros-below-`0x800`/fill-above boundary structure; arithmetic closure — `3964−1007=2957`, `1089+1059=2148=2147+1`-in-I-zero, 99.40%).
>
> The proposed absent-control adds negligible confidence (B9 histogram path is itself separate and gated; tuple-invariance already bounds behavioral perturbation; coherence already excludes corruption). Do not require it as a reliance condition — but still require the standard closure: final absent-env run, zero diagnostic tags, tuple intact.
>
> **BASIS:** observed — evidence record §§1–6 + plan block as cited; packet premise/rows/stops (packet:18,29,33); implementation paths (`apu_watch.c:995`–1026; `dsp_cpu.c:1047`–1076); guardrail correction confirmed by direct read (`dsp.h:79`–89 clobbered `DspCoreState`, `:115` canonical `DSPState`); arithmetic closure computed from the record's own numbers. Inferred — loader-first necessity (slice validity ⇒ byte stability ⇒ epoch proof); instruction/path distinction. Uncertain — loader mechanism; P-memory quiescence (both are next packet's phase 1, not assumptions).
>
> **REVERSED BY:** P-write watch showing image-I modification bootstrap→exchange (reopens doorbell-instruction survival); loader shown to be a second GPRST bootstrap (collapses provenance to bootstrap mechanics — simplifies, route accordingly); at-exchange bytes unstable across repeated snapshots (forces per-epoch model, forbids single-slice).
>
> **RECORD IN:** this ruling verbatim with handle/model/effort in the followup review record; withdraw/relabel list binds Session artifact hygiene; Q3 phasing + Q2-establishment conditions bind the next Planner brief; supersedes last ruling's Q2 scope only.
>
> **Notes (not rulings):** (1) The Planner's correction of my `dsp.h:105` citation is confirmed — the clobbered field is `:89`, canonical `:115`; my substance was right, my line number wrong. I will cite struct-qualified names henceforth. No harm resulted (verified by direct read before use). (2) The mid-write promotion: self-reported, repaired before execution (nothing ran under the draft hash; stable hash verified before+after), lesson recorded. No §5.3 violation persists. No action.

---

## Session note — the line numbers in this ruling and in the packet refer to **different file states**

The Advisor's closing note cites the clobbered field as `dsp.h:89` and canonical as `:115`. **Verified:**
those are the line numbers **after** the Session's guardrail comments were inserted. **Before** insertion
they were `:79` and `:105` — which is what the ruling text above still says in its BASIS paragraph and what
the packet says.

| Field | Before guardrail comments | After guardrail comments |
|---|---|---|
| `DspCoreState.is_gp` (clobbered) | `dsp.h:79` | **`dsp.h:89`** |
| `DSPState.is_gp` (**canonical**) | `dsp.h:105` | **`dsp.h:115`** |
| `dsp_core_t.is_gp` (dead) | `dsp_cpu.h:49` | **`dsp_cpu.h:60`** |

**Hazard, recorded because it will bite a future reader:** both numberings are now "correct" against
different file revisions, so a citation of `dsp.h:105` or `:79` silently points at the **comment text**
rather than the field in the current tree. **The robust identifier is the struct-qualified name**
(`DspCoreState.is_gp` vs `DSPState.is_gp`), which is what the Advisor adopted in its own closing note and
what the next packet should cite. This is the same class of error as the `PIO_FREE` spelling problem: a
citation that is valid against one revision and misleading against another.

---

## Session obligations arising from this ruling

| # | Obligation | Status |
|---|---|---|
| W1 | **Withdraw/re-scope** `full_decode.txt` as a slice input above `0x172` — relabel as the bootstrap-state record | **Pending** |
| W2 | **Withdraw** the §3 CFG edge set / reachability numbers as claims about the executed program; keep the method and both tooling fixes | **Pending** |
| W3 | **Withdraw** the seven Q2 re-derivations' "over all 3881 words of one image" scope; tasks survive, bytes void | **Pending** |
| W4 | **Withdraw** the "one 3881-word program" framing — it is ≥2 images/epochs | **Pending** |
| F1 | **FORBID** citing byte-identity of doorbell instructions as path-independence | **Recorded** |
| F2 | **FORBID** slice-direct as a first step — loader/epochs gate it | **Recorded** |
| F3 | **FORBID** a fudged single-image slice if P-memory never quiesces | **Recorded** |
| N1 | Next packet **phase 1**: rule a second GPRST bootstrap in/out **from existing logs first** (`boots` + `[GPBOOT]` block count) before any new run | **Brief for Planner** |
| N2 | Next packet **phase 1**: epoch structure + **P-write watch** over image-I showing quiescence (also closes Q2's transient gap) | **Brief for Planner** |
| N3 | Next packet **phase 2** (slice) only on phase-1-covered bytes | **Brief for Planner** |
| N4 | B3 **identification** stays deferred; B3 **enumeration** (#1) rides the next slice | **Recorded** |
| N5 | Standard closure still required: final absent-env run, zero diagnostic tags, tuple intact | **Pending** |

**No `§5.4(2)` anywhere. `O-INCONCLUSIVE` confirmed. No prior record requires revision.**
