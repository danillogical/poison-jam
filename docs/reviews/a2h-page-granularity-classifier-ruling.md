# Advisor ruling — the page-granularity flaw and the classifier premise: **both adjudicated**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e940-1473-7000-94a5-8feacec44c0f`.
**Raised because:** the Planner found that the approved narrowing is **fatally flawed by page granularity**,
and the Session found that the **classifier's premise is void.**
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: neither (a)-as-stated nor (b)/(c)/(d) — narrowed acceptance with terminal-coherence gate; classifier rebuilt in native domain; new packet (this ruling = preflight if applied verbatim).**

**1. Options adjudicated — the space is closed, not open.** Accept/prove × guard/other-mechanism/retire exhausts it: **(a) accepted as modified below**; **(b) rejected — re-protect without stepping livelocks the guest, with stepping keeps the identical window plus double faults (strictly dominated; the dilemma is structural at page granularity, not choreography)**; **(c) no third mechanism exists in scope — DR excluded on delivery grounds (stands), DBI/PT out of scope or value-blind**; **(d) retire rejected — live observation is the line's only route to writer attribution (static exhausted as a class for runtime facts) on the critical path.**
- **The modification that makes (a) sound: terminal-value coherence as a REQUIRED gate** (last-recorded slot write vs terminal read; mismatch → `UNKNOWN`). **Windows threaten only unrecorded writes; recorded positives stand; coherence converts the residual same-value race to fail-closed.** **Worked example already in hand: last-recorded (installer `0x15F9D0`) vs terminal (`0x001D5078`) mismatches — `UNKNOWN`, never misattribution.**
- **Record-before-open, window tick-logging, and no-exclusion carry unchanged.**

**2. Classifier premise VOID — categorical, not site-specific.** **Native RIPs in recompiled code never carry guest encodings, so every guest-byte classification ever emitted from a fault RIP is unsound** (the `0x7B3` residual dies with the paradigm — **do not fix its arithmetic**). **Replace with range-based guest/host classification** (RIP ∈ recompiled-module bounds vs toolkit/host image, else unknown) **+ optional native-disasm corroboration; guest-byte expectations forbidden for native RIPs.** **One packet (fix + rerun — splitting strands the fix unverified): classifier fix and coherence gate are gating preconditions to any attribution read, in that order.**

**BASIS:** observed — page-sharing arithmetic (`0x62C` in `0x0019D000` page), 9077-write dilemma as framed, native-vs-guest ISA mismatch. Inferred — nothing load-bearing; **the option space is closed by mechanism, not judgment.**

**REVERSED BY:** a window-free observation mechanism in scope (none exists today); terminal coherence failing on a writer-observed run (→ re-examine, not override).

**RECORD IN:** verbatim in the line's review record; **Planner writes the new packet (no second preflight if applied verbatim)**; **Session appends the paradigm correction (native-domain classification) beside the withdrawn residual.**

---

## ⚠ The Planner found a fatal flaw, and the Advisor confirms it is STRUCTURAL

**The Planner's finding, which the Session verified:** **`VirtualProtect` is PAGE-GRANULAR, and the slot is at
offset `0x62C` of page `0x0019D000`.** **So *"leave RW after a non-slot write"* ALSO makes the slot
writable — and after the FIRST traffic write the instrument goes BLIND.**

**With `nonslot_writes=9077` on ON-3, the narrowing would have blinded it after the first one.** ✓

**And the Advisor's adjudication of the option space is the important part:**

> **"the dilemma is structural at page granularity, not choreography"**

**Option (b) — re-protect without stepping — is *"strictly dominated"*:** **without stepping it livelocks the
guest; with stepping it keeps the identical window PLUS double faults.** **So there is no choreography that
escapes it.** ✓

**And (c) and (d) are closed on grounds the Session could not have established alone:** **no third mechanism
exists in scope (DR excluded on delivery grounds — which STANDS), and retiring would abandon the line's only
route to writer attribution because *"static exhausted as a class for runtime facts."***

## The modification that makes (a) sound — and the worked example is already in hand

> **"terminal-value coherence as a REQUIRED gate (last-recorded slot write vs terminal read; mismatch →
> `UNKNOWN`). Windows threaten only unrecorded writes; recorded positives stand; coherence converts the
> residual same-value race to fail-closed."**

**And the Advisor points at ON-3's own numbers as the worked example:** **last-recorded `0x0015F9D0` vs
terminal `0x001D5078` MISMATCH → `UNKNOWN`, never misattribution.**

> **So the very contradiction the Session found and could not resolve is the GATE working.** **The Session had
> treated the mismatch as a problem; the Advisor makes it the detector.** ✓

**And the boundary is stated exactly:** **"Windows threaten only unrecorded writes; recorded positives
stand."** **So a RECORDED positive remains rowable, and an UNRECORDED write is caught by coherence rather than
assumed absent.** ✓

## ⚠ The classifier premise is VOID — and the Advisor makes it CATEGORICAL

**The Session had framed it as a site-specific question** — *"the classifier read the native bytes at the
installer's RIP and did not find `89 81 2C 24 00 00`; four possible causes"* — **and proposed Exp0 to settle it
at that site.**

**The Advisor's ruling is broader and decisive:**

> **"Native RIPs in recompiled code never carry guest encodings, so EVERY guest-byte classification ever
> emitted from a fault RIP is unsound."**

> ## **So it is not a bug at one site — the PARADIGM is wrong.** **`a2h_slotw_classify_store` reads native bytes and tests for GUEST encodings, which cannot ever match in recompiled code.**

**And: *"the `0x7B3` residual dies with the paradigm — do not fix its arithmetic."*** ✓ **So the Session's
withdrawn residual should NOT be repaired; the whole approach is replaced.**

**The replacement:** **range-based guest/host classification** — **`RIP ∈ recompiled-module bounds` vs
`toolkit/host image`, else unknown** — **plus optional native-disasm corroboration.** **And *"guest-byte
expectations forbidden for native RIPs."*** ✓

## And the packet count is fixed at ONE

> **"One packet (fix + rerun — splitting strands the fix unverified): classifier fix and coherence gate are
> gating preconditions to any attribution read, in that order."**

**So the Session must NOT split the classifier fix from the rerun.** ✓ **And the order is fixed: classifier
fix → coherence gate → attribution read.**

## The Planner's conduct, recorded

**The Planner wrote the packet as ordered, marked its own §5.8 `INADEQUATE / BLOCKING`, and said:**

> *"I encoded approved shape verbatim and flagged explicit stop/re-refer rather than redesign."*

**That is exactly right.** **It did not silently redesign an approved shape, and it did not freeze a packet it
judged unsafe.** ✓ **The Session records that as the correct handling of a defective instruction.**

**Its draft (`d3cd8ae`, `C2DD3ED8…AA27`) is therefore SUPERSEDED by this ruling's shape and must be revised,
not promoted.**

## The Session's next actions

1. **Record this ruling** — done.
2. **Append the paradigm correction beside the withdrawn residual** — per the Advisor.
3. **Have the Planner revise the packet** to: **range-based classification + terminal-coherence gate + one
   packet (fix + rerun) + the re-arm-after-every-write shape (option (a) as modified).**
4. **No second preflight if the ruling is applied verbatim.**
