# ✅ `A2h-slot-writer-attribution-r2` — **STAGE-1 ACCEPTANCE: `ACCEPT`** (final)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-writer-attribution.md`, **r2**, frozen
**`E209D1F4A4405F0B266E8D4DEFDA77A481A277D27615BFAE9ACB9F7A3A6C6378`** — **re-verified unchanged by the
reviewer.**
**Reviewer:** stage 1, `9bc13c91-76f1-4a1e-9291-87f099346cb9` (`workbuddy-ai/hy4-preview-f` @ `high`).
**Adjudication:** Persistent Advisor, turn `01a0ea5b-5a40-7000-88f3-5769e425e8d2`.
**Authority:** §2.2 — *"A first-stage `ACCEPT` is final and is not passed on."*

---

## DISPOSITION: **`ACCEPT`**

**The stage-1 reviewer returned `NOT ACCEPTED` on exactly one criterion (#11), correctly marked `CANNOT
VERIFY` and escalated rather than privately reinterpreting.** **The Advisor ruled the two disputed sub-items
OUT OF SCOPE.** **Every criterion is therefore `AGREED`, and the disposition resolves to `ACCEPT`.**

> **Per §2.2, a first-stage `ACCEPT` is FINAL and is NOT passed to the second stage.** ✓ **No re-run was
> required and none was performed.**

## The criterion table, as reviewed

| # | Criterion | Verdict |
|---|---|---|
| **1** | **Exp0 — range classifier, offline, REAL module bounds** | **`AGREED`** |
| **2** | **Exp1 — installer trap MUST fire; no hit ⇒ INFRA FAILURE** | **`AGREED`** |
| **3** | **Exp2 — N ≤ 5, early-stop, K≥2; zero qualifying ⇒ RE-REFER** | **`AGREED`** |
| **4** | **Qualifying = writer + matching terminal + controls green** | **`AGREED`** |
| **5** | **Coherence gate — mismatch ⇒ UNKNOWN** | **`AGREED`** |
| **6** | **Absence rows DEAD** | **`AGREED`** |
| **7** | **Shape (a) — re-arm every write; narrowing NOT implemented** | **`AGREED`** |
| **8** | **No fault-RIP encoding cited as evidence** | **`AGREED`** |
| **9** | **`unknown > 0 ⇒ INFRA FAILURE`** | **`AGREED`** |
| **10** | **Loss accounting, fail-closed, OFF by default** | **`AGREED`** |
| **11** | **Carry-forward re-verified** | **`AGREED`** — **after the ruling below** |
| **12** | **No DR; page-protection only; no synthetic completion; `RaiseException` unchanged** | **`AGREED`** |

## The #11 escalation, and the Session's OWN error

**The reviewer verified 5 of 7 sub-items** — **`g_xbox_mem_offset` at terminal, base re-derived at ARM and
TERMINAL, the VEH order matrix, the alias census (29/29), and the thread census.** **Two were unevidenced:**
**the no-debugger condition, and tiled/contiguous non-overlap.**

**⚠ THE SESSION CHECKED THE FROZEN PACKET'S OWN TEXT AND FOUND ZERO OCCURRENCES of `no-debugger`,
`debugger`, `tiled`, `contiguous`, `non-overlap`, or `carry`.** ✓

> ## **The Session had imported those sub-items into the acceptance brief from the SUPERSEDED packet's carry-forward list — a list the Advisor gave for the PREVIOUS packet, whose page-guard used a different page. When the corrected shape was ruled, that list was NOT restated, and the Planner wrote the new packet without it.**

**So the Session added a requirement to a brief that the frozen contract does not contain.** **That is the
Session's error — not the Worker's and not the Planner's — and it is recorded as such.** ✓

**And the Advisor's ruling names the general lesson:** ***"Carry-forward lists must be re-justified per packet —
that step was skipped."*** ✓

## The Advisor's ruling, verbatim

**RULING: both sub-items OUT OF SCOPE — clean `ACCEPT`, no re-run, push from this boundary.**

- **Contract only (§2.2):** neither term is in the frozen packet (verified zero occurrences), and **a brief cannot add criteria to it.** **The reviewer was right to escalate rather than reinterpret; all roles behaved correctly except the brief itself, which the Session already owns.**
- **Provenance:** both items belong to a superseded packet's different instrument. **Carry-forward lists must be re-justified per packet** — that step was skipped. **Lesson recorded; no blame beyond the Session's own.**
- **Materiality, so this isn't formalism:** **no-debugger is doubly moot** (this instrument uses no DR registers, which is all that condition ever protected — **and it is unsatisfiable as literally written, since the collector IS a debugger**). **Tiled exclusion rests on verified separate storage; the only residual (contiguous-pool vs slot-page non-overlap) closes by STATIC check** — **required as a recorded check, not a packet, not a re-run.**

**BASIS:** observed — contract absence; superseded-packet provenance; no-DR instrument; separate-storage architecture. **REVERSED BY:** evidence either sub-item appears in the frozen contract (it does not).

## ✅ The required STATIC NON-OVERLAP CHECK — executed

**The Advisor required the contiguous-pool vs slot-page non-overlap to close by a recorded static check. The
Session ran it against the run's own archive:**

```
DUMP_REGION         name=contiguous  va=80000000  address=0000000080010000  size=67108864
DUMP_REGION_SKIPPED name=nv2a_registers va=FD000000 (not fully readable)
```

| Region | Bounds |
|---|---|
| **the contiguous pool** | **`0x80010000 .. 0x84010000`** (64 MB from `0x80010000`) |
| **the slot's page** | **`0x0019D000 .. 0x0019DFFF`** |

> ## **The slot page lies BELOW `0x80000000` and the contiguous pool begins at `0x80010000` — the two ranges are DISJOINT.** ✓

**So the residual the Advisor identified is closed: the watched page cannot be inside the contiguous pool.**
**And the Session notes the check is STATIC and derives from the archived `DUMP_REGION` line, so it needs no
run.** ✓

## What the reviewer established independently — the load-bearing verifications

- **The frozen hash matched**, and the packet was unmodified. ✓
- **The fixture was RE-RUN by the reviewer:** `gate=on checks=227 failed=0`; `gate=off checks=3 failed=0`. ✓
- **The control is genuine, NOT a value-match:** the predicate at `xbox_memory_layout.c:3569` is
  `slot_hit && range_class == GAME_MODULE`, **with the encoding test deleted and the value explicitly excluded
  from the test** (`:3560-3563`). ✓ **This is the substitution the line has a documented history of, and the
  reviewer checked for it specifically.**
- **K≥2 RECOMPUTED from each run's own map:** **Exp1 → `0x52FE38` / `sub_00038530`; Exp2-3b → `0x52FE38` /
  `sub_00038530`; installer control → `0xB3D82D` / `sub_0018CE30` in both.** **Same RVA, same value, different
  image bases and different TIDs.** ✓
- **The row was correctly WITHHELD:** `terminal_target=00000000` and `[ICALL] invalid target 0x00000000` in
  all three runs, read from the RAW logs rather than the records' paraphrase. ✓
- **No withdrawn claim was re-asserted** — all four checked individually. ✓
- **The packet relies on NO dump**, so the known `CONTENT_MISMATCH` condition is not a defect here. ✓

## Advisories (recorded; they do NOT change the disposition and add no criteria)

1. **`installer_control_hits` is over-inclusively NAMED** — it counts any `GAME_MODULE` slot store, so a run with no installer write would still report ≥1. **The underlying fact is safe here** because the event record and the map independently prove the installer fired. **Carry to the successor.**
2. **`term_base_ok=0 / stable=0 / base_changed=1` in EVERY ON run** — the packet honours this (row is `UNKNOWN`/`O-OPEN`), **but the narrative presents the competitor finding without pausing on the fact that the terminal slot could not be re-derived in any run.** **Carry to the successor.**
3. **`unexpected_exception=781/758/781` and `first_touch_dropped≈9000` are reported but unanalysed.** **Carry to the successor.**
4. **The `unknown` INFRA-FAILURE log lines coexist with the CRT-`memset` diagnosis** — correctly handled (rule applied, row not promoted, correction routed to successor design), **but a reader may mistake the diagnosis for a waived rule. The records are explicit that it is not.**
5. **All four runs are `CONTENT_MISMATCH`** — known pre-existing; the packet relies on no dump.

## Final state of the packet

| Item | Value |
|---|---|
| **Packet** | **`A2h-slot-writer-attribution-r2`, `ACCEPT`, FINAL** |
| **Row** | **`O-OPEN`** — **correctly withheld; no promotion** |
| **Finding** | **`sub_00038530` writes `0x001D5078` to the slot, K≥2 agreed** |
| **Runs** | **3 of N=5 used; no extension** |
| **Frozen hash** | **`E209D1F4…6378`, UNCHANGED** |
| **Successor** | **specified in `a2h-competitor-finding-row-ruling.md`; NOT started (owner's instruction)** |

**Prohibitions held:** **no DR anywhere; page-protection only; no synthetic completion; no fault-RIP encoding
cited; `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged; no frozen packet edited; no row
promoted; no run evidence deleted; no Git history rewritten.**
