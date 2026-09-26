# A4b2-r4 planning rulings — binding

**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-26.
**Advisor:** child `5c555969-dea9-4b47-be05-62aa0835cde2`, `claude/claude-opus-5-5` @ `high` (persistent, §4.4).
**Planner:** child `cee46374-344e-4698-a67d-c066ed569deb`, `claude/claude-opus-5-5` @ `medium`.
Marker **`JSRF-ADVISOR-PROBE-K7Q4M-20260926`**.

Recorded **verbatim** as §4.4 requires. This is the **shape preflight** (§5.1.4), which is a bounded
shape/policy review and **not** the packet's formal §5.3 adequacy review.

---

## Ruling — `SHAPE: PROCEED`

> **What I read myself, read-only, at toolkit `3a3c7c1`:**
> - `apu_watch.c`: lines 849–925, `emit_gpin_block` and `emit_gpin_summary`; lines 806–815, the freeze;
> - `apu_watch.h`: lines 88–110;
> - `dsp.c`: lines 49–95, `read_peripheral`;
> - `gp_ep.c`: lines 526–562 and 610–660;
> - `apu_core.c`: lines 636–654;
> - the `ep_ops` and `ep.regs[]` references;
> - the fixture's `(xi)` block and its line-vs-snapshot helpers, lines 460–712;
> - `a4b-xemu-pin.md`, the `dsp_dma` rows;
> - `a4b1-r4-acport-step4-enumeration.md:129`;
> - the r3 `A4b2` body at lines 20, 64–118, 164–172 and 288–315;
> - `a4b1-a4b2-r3-adequacy-review.md`, lines 43–77.
>
> **SHAPE: PROCEED**
>
> **REASON:**
> - The claim is unchanged, and this is the smallest revision that decides it. The class is change.
> - **Correct the grounds statement, though.** Besides §5.4(2), it is also **§5.4 After-INADEQUATE** (it closes r3 `A4b2` B1 by the repin and `sge0_va`, and B2 by the not-evaluated rule) and **§5.4(3)** (the `A4b1-r4` planning rulings, part 2's `A4b2` boundary). **It is not "only a re-bind".**
> - **A full fresh §5.3 adequacy review follows, not a delta review.** r3 was INADEQUATE, `AC-INPUTS` is rewritten, and P3 is new.
> - Edits 1–5 are right in substance, subject to the rulings below:
>   - **Edit 2:** demoting the GPSADDR `803CC000` conjunct is correct. That is exactly the P-F hazard, and **my boundary note did miss it**.
>   - **Edit 4:** NOT-EVALUATED, not UNKNOWN, is the r3 B2 repair as required.
>   - **Edit 5:** P3 is right, because a reachable EP is a false-PASS path. The measurements agree with what I read: EP MMIO is unrouted at `apu_core.c:648–653`, `ep_ops` has only its definition, its declaration and comments, and `ep.regs[EPRST]` is written only by `ep_write` at `gp_ep.c:559`.
>
> **(a) The `at_clear` block is ADMISSIBLE as the decision input, on a stated inference with two added can-fail checks.**
> - Basis: `emit_gpin_block` is one function called with a tag and a struct pointer. `"summary"` passes `&s.gpin`, and `"at_clear"` passes `&s.at_clear.gpin`, which is a `memcpy` of `s.gpin` at the freeze. The line-vs-snapshot test of the `"summary"` tag therefore tests **every field format** the `at_clear` block uses, and `(xi)` tests the `at_clear` snapshot values. **The only untested link is the pointer argument.**
> - State that as an **inference** in the packet. A4b1 is accepted, so nothing is added to A4b1.
> - **Add two in-run consistency checks to `AC-INPUTS` step 1**, each yielding UNKNOWN on violation:
>   - **(i)** every emitted `"[GPIN] at_clear"` block, which is re-emitted at each summary cadence, is **identical to the first one**, including its header `seq` and `frame`. The freeze is write-once, so any difference is an emitter or copy defect;
>   - **(ii)** every `at_clear` counter is **≤ the matching element of the last `"[GPIN] summary"` block**. That covers `reads`, `words`, `bytes` and `reads_while_stub`, and each flag. The counters are uncapped and monotone.
> - These make the inference **can-fail against the run itself, at no fixture cost**.
>
> **(b) Do NOT inherit "modelled" for `0xFFFFB3`. It is a STUB constant for `AC-INPUTS`:** a read before the clear gives FAIL, which is `R2-EXPL-INPUT`.
> - Basis: `dsp.c:57` is `v = 0; // core->num_inst; // ??`. **Upstream itself marks it as an unknown placeholder**, not a model of anything.
> - The step-4 entry "modelled — `v = 0`" describes the source. It is a **Session record, not an Advisor ruling**, and A4b1's acceptance did not decide A4b2's `AC-INPUTS` semantics.
> - Classifying it in A4b2 **reopens nothing**: no A4b1 text or code changes. Record in A4b2 that step-4's label is **superseded for decision purposes** by this ruling.
> - The cost is bounded. `R2-EXPL-INPUT` names the input and routes to me, which is the honest result if the GP's clear followed a read of a placeholder.
> - **The modelled set is therefore FIVE offsets: `0x45` and `0x54`–`0x57`.**
> - Add a claim limit: **`0x56` (`DMA_CONTROL`) is upstream's read-count completion heuristic** (RUNNING→STOPPED after 3 reads). It is inherited as modelled, but it is a **timing heuristic, not a hardware model**.
>
> **Four further bounded corrections, to fold in:**
> 1. **P3 must be bound to the toolkit commit recorded in the R1/R0 builds' identity, not to "HEAD".** The check must be a can-fail text check with a fixed pattern set:
>    - `ep_ops` referenced only at its definition, its declaration and comments;
>    - no write to `ep.regs[` outside `ep_write`;
>    - `apu_core.c` has no routing arm for `0x50000`.
>    Any hit means **stop, and no row is selected**.
> 2. **NDEBUG claim limit:** the sketch's *"the NDEBUG fall-through inputs that matter are already caught"* **overstates it**. Only the read-arm FIFO fall-through is caught; the others (`unk2`/`unk13`, `format` default, `dsp_offset` range) are **unaudited**. State this as a **claim limit on `R2-PASS`**, not as coverage. It remains a lead, not an obligation.
> 3. **`[APUMMIO]`:** after Edit 2, state whether any decision still reads the capped `[APUMMIO]` log. If none does, record that the r2/r3 deferred **D1** tension with §6.1 is resolved. If one does, name it.
> 4. **Wording:** the sites are *"6 enumerated, `N_SITES = 16` ≥ 6"*, **not "matching"**. `AC-INPUTS` FIFO slot 0/1 > 0 = FAIL and slots 2–5 ≠ 0 = UNKNOWN are both **CONFIRMED**. So is `OUT_OF_UNIVERSE` ≠ 0 = UNKNOWN, as my ruling (C) routes unknown read-arm `buf_id`s there.
>
> **POLICY_ISSUE: NONE.** Items (a), (b) and 1–4 are interpretation rulings folded into the draft; none needs owner action.
>
> **REVERSED_BY:**
> - **(a):** the `at_clear` block shown to be emitted by any function other than `emit_gpin_block`, or the freeze shown not to be write-once. `AC-INPUTS` then falls back to the snapshot-less inference, stated as UNKNOWN.
> - **(b):** an xemu or hardware source showing that `0xFFFFB3` is architecturally constant 0 on the MCPX GP.
> - **P3:** any route that makes EP MMIO reachable at the built commit.
> - **The whole shape:** `A4b1-r4` found not to be accepted at the commit A4b2 pins.
>
> **RECORD IN:**
> - `docs/reviews/a4b2-r4-planning-rulings.md` (new), verbatim;
> - cross-referenced from the `A4b2-r4` governing-requirement block;
> - a one-line note beside `a4b1-r4-acport-step4-enumeration.md:129` that `0xFFFFB3`'s decision class is set by this ruling.

---

## Session verification of the ruling's citations

The Session checked the ruling's two load-bearing citations **before** recording it, because both are
facts about files the Advisor read and the Session is about to change behaviour on:

| Advisor citation | Session check | Result |
|---|---|---|
| `dsp.c:57` is `v = 0; // core->num_inst; // ??` | read at toolkit `3a3c7c1`: `case 0xFFFFB3:` → `v = 0; // core->num_inst; // ??` | **CONFIRMED** — upstream's own `// ??` marks it a placeholder |
| `a4b1-r4-acport-step4-enumeration.md:129` says `0xFFFFB3` is "modelled — `v = 0`" | read: exactly that, and it is the **first row** of the classification table | **CONFIRMED** |
| `emit_gpin_block` is one function taking `(tag, struct*)`, called with both `"summary"` and `"at_clear"` | `apu_watch.c:851` definition; `:919` `emit_gpin_block("summary", &s.gpin)`; `:923` `emit_gpin_block("at_clear", &s.at_clear.gpin)` | **CONFIRMED** |
| the freeze is write-once | `apu_watch.c:808` `InterlockedCompareExchange(&s.at_clear.taken, 1, 0)` | **CONFIRMED** |
| `at_clear.gpin` is a `memcpy` of `s.gpin` at the freeze | `apu_watch.c:814` `memcpy(&s.at_clear.gpin, &s.gpin, sizeof(s.gpin))` | **CONFIRMED** |
| `ep.regs[EPRST]` written only by `ep_write` | `gp_ep.c:559`; all other `EPRST` occurrences are reads | **CONFIRMED** (also verified independently before the preflight) |
| EP MMIO unrouted | `apu_core.c:648-650` | **CONFIRMED** |

**No discrepancy.** The ruling stands on measured ground, and the Session applies it.

## Effect on the draft

The Planner's sketch is **approved in substance** with the corrections above. Two of the Advisor's
rulings **change a decision the Planner had made**:

- **(b) reverses the Planner's leaning.** The Planner leaned toward inheriting A4b1's "modelled"
  classification for `0xFFFFB3` *"because A4b1 is accepted and frozen and a baseline change does not
  reopen a ruling."* The Advisor ruled the opposite: the step-4 label is a **Session record, not a
  ruling**, A4b1's acceptance did not decide A4b2's `AC-INPUTS` semantics, and reclassifying it reopens
  nothing. **The modelled set is five offsets, not six.** The Planner's instinct to avoid reopening a
  ruling was right in principle; its premise — that the step-4 label *was* a ruling — was wrong.
- **(a) answers the Planner's admissibility question** with a conditional yes: admissible **on a stated
  inference**, plus **two new can-fail checks** that make the inference testable against the run at no
  fixture cost. The Session's independent check confirmed the Planner's framing of the gap was precise
  (`a4b2-r4-sketch-verification.md`, appendix).

The grounds statement is also corrected: this is **not "only a re-bind"** — it is §5.4(2) **and**
§5.4 After-INADEQUATE **and** §5.4(3).

## Consequential obligations

- **A full fresh §5.3 adequacy review** follows (r3 was `INADEQUATE`, `AC-INPUTS` is rewritten, P3 is
  new). **Not** a delta review. Per §5.1.5(3) it goes to a **fresh Opus 5.5 Medium Planner**, not the
  authoring child and not the Opus High Advisor.
- The `A4b1` step-4 enumeration gets a **one-line note** at `:129` recording that `0xFFFFB3`'s decision
  class is set by this ruling. **No A4b1 text or code changes** — the ruling says so explicitly.
