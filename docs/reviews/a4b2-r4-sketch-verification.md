# A4b2-r4 sketch — Session verification of the Planner's load-bearing claims

**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-26.
**Planner:** child `cee46374-344e-4698-a67d-c066ed569deb`, `claude/claude-opus-5-5` @ `medium` (§1, §2.3).
**Sketch:** at the top of `docs/packets/a4b2-gp-clears-pending-word.md`; the `A4b2-r3` body below is
untouched. Relayed **unchanged** to the Advisor for the mandatory §5.1.4 shape preflight.

The Session told the Advisor it would verify the Planner's P3 measurements **in parallel with the
preflight** and report any discrepancy rather than let it pass silently. This is that verification. The
Session checked the claims **before** the Advisor's answer, so that a `PROCEED` is not the only thing
standing between an unchecked measurement and the packet body.

## Every claim verified — no discrepancy found

| Planner's claim | Session check | Result |
|---|---|---|
| **P3.1** EP MMIO is unrouted | `apu_core.c:648-650` — *"EP (0x50000) stays unrouted: an EP write is still dropped, and an EP read still returns 0 with the once-per-block note. That is A4b1's explicit non-goal, not an omission."* | **CONFIRMED** |
| **P3.2** nothing in `src/` calls `ep_ops` | `ep_ops` appears **only** as the definition (`gp_ep.c:568`), an `extern` declaration (`gp_ep.h:48`), and two **comments** (`apu_core.c:30`, `apu_state.h:374`). **No call site anywhere.** | **CONFIRMED** |
| **P3.3** `EPRST` is written only by `ep_write` | `gp_ep.c:557-559` is the **only** write; every other `EPRST` occurrence is a **read** (`:612`, `:613`, `:650`, `:651`) | **CONFIRMED** |
| **P3.4** `MCPXAPUState` is `calloc`'d | `apu_core.c:543` — `calloc(1, sizeof(MCPXAPUState))` | **CONFIRMED** |
| **P3 conclusion:** `EPRST` stays 0, so the EP block (`gp_ep.c:650`) never runs | follows from 1–4: no MMIO route → `ep_write` unreachable → `EPRST` never set → the `EPRST`-gated block never entered | **CONFIRMED** |

**So the Planner's P3 is sound, and its decision to make the EP input a structural precondition rather
than a lead is justified by measurement, not by assumption.** The reasoning is also correct on the
failure mode: `apu_gp_dma_write` does **not** distinguish GP from EP, so if the EP were ever reachable an
EP exchange at `W_va` would latch `GP_CLEAR` with no side field — a **false PASS**. That is exactly the
class of outcome §2.3's bounded-initiative rule says must enter the packet.

**Note the irony, and the value:** the EP reaches the choke point (the `A4b1-r4` stage-1 finding the
Session got wrong), but it cannot *run*, and the Planner proved that independently rather than assuming
it from the Session's own carried-forward note. The two facts together are what make P3 correct.

### The Planner's new AC-BOOT finding is also real

The Planner reports that the boundary note **missed** a second baseline-dependent conjunct. Verified:

| Claim | Check | Result |
|---|---|---|
| `A4b2-r3`'s `AC-BOOT` PASS conjunct includes "the last `0x02040` write before it is `803CC000`" | `docs/packets/a4b2-gp-clears-pending-word.md:192`, and the value is introduced at `:62` from the `A4a` R1 evidence | **CONFIRMED** |
| That conjunct is baseline-dependent | Under the `P-F` premise the title may now write the **physical** form (e.g. `003CC000`) because `MmGetPhysicalAddress` is non-injective at `M`; the packet's own text says *"plausibly"* (`a4b1-gp-core-port.md:33`) | **CONFIRMED as a real hazard**; the Planner correctly marks it **INFERRED** rather than measured |
| `[GPBOOT]` prints both `sge0` and `sge0_va` | `gp_ep.c:368-369` declares both; `:385` reads the raw entry; `:387` translates it via `apu_guest_dma_ptr(sge0, 4, &sge0_va)`; `:398` passes both to `apu_watch_trace_gpboot` | **CONFIRMED** |

So `AC-BOOT` must compare **`sge0_va`**, and the `803CC000` conjunct must be demoted to a recorded
value — **the Planner found a defect the boundary note did not name.** That is the Planner doing its job:
a boundary note is a lead, and this one was incomplete.

### The Planner's classification and mechanism claims

| Claim | Check | Result |
|---|---|---|
| The retired 256-entry table and `GPIN_OVERFLOW` are gone | `git grep GPIN_OVERFLOW -- src/apu` → **one hit only**, `apu_watch.h:13`, and it is a **comment saying the mechanism does not appear**. `gpin_table` / `gpin[256]` / `GPIN_MAX` → **zero hits** | **CONFIRMED** |
| The `at_clear` block is frozen and printed when `GP_CLEAR` latches | `apu_watch.c:190-192` calls `apu_watch_freeze_at_clear(seq)` then `emit_gpin_summary()` | **CONFIRMED** |
| Only 6 of 128 `PERIPH` offsets are modelled | `dsp.c:56, 59, 65, 68, 71, 74` = 6 modelled; `v = 0xababa` sentinel at `:54` | **CONFIRMED** |
| 6 CPU store sites remain, within `N_SITES = 16` | see the enumeration note below — **6 is right, but the count is subtler than it looks** | **CONFIRMED, with a caveat** |
| The spin is still at `loc_001A18D0`, `recomp_0005.c:6748` | `A4b1-r4`'s gate G4 measured `L = 6748` | **CONFIRMED** |

### The CPU-site count needed real work, and one detail is easy to get wrong

`jsrf_watch_store` **does not exist anywhere in the game tree** — zero hits across `src/`. That is **not** a
defect: it is the **instrument A4b2 creates**. The watch-ledger ruling says so
(`a4b-watch-ledger-ruling.md:71-72`: *"A4b2 edits the game only: `jsrf_watch_store` becomes a thin
forwarder to `apu_watch_cpu_store`…"*), and the `A4b2` packet's step 1 defines it
(`a4b2-gp-clears-pending-word.md:109`). **The Session's first grep for it returned zero and briefly looked
like a missing instrument; reading the ruling showed it is the packet's own deliverable.** Recorded
because the same zero will mislead the next reader who greps for it before executing `A4b2`.

**The "6 sites" enumeration is correct but must be counted carefully.** The packet enumerates them in two
ways (`a4b2-gp-clears-pending-word.md:114-118`) — three **by guest site VA**, then *"every **other**
generated store that textually matches `MEM32\(.* \+ 0x810\) =`"*:

| # | Site | Location | How enumerated |
|---|---|---|---|
| 1 | `0x001A18CE` `mov [ebx],eax` (the control site) | `recomp_0005.c:6746` | by VA |
| 2 | `0x001A1751` `and dword [edi+0x810],0` | `recomp_0005.c:6524` | by VA |
| 3 | `0x001A1FA7` `mov [edi+0x10],ebp` | `recomp_0005.c:8088` | by VA |
| 4 | `MEM32(eax + 0x810) = ecx;` | `recomp_0000.c:135283` | by pattern |
| 5 | `MEM32(ecx + 0x810) = eax;` | `recomp_0000.c:135406` | by pattern |
| 6 | `MEM32(ecx + 0x810) = eax;` | `recomp_0000.c:135871` | by pattern |

**The trap:** the `MEM32\(.* \+ 0x810\) =` pattern matches **4** sites, not 3 — because `recomp_0005.c:6524`
(site 2) also matches. A reader who adds "3 by VA + 4 by pattern" gets **7** and would conclude the
enumeration over-counts or that `N_SITES` is wrong. The packet's word **"other"** is what makes it 3 + 3 =
**6**, and the `apu_watch.h:93-108` comment lists exactly those six with the same locations. **Verified
consistent.** `N_SITES = 16` (`apu_watch.h:110`) leaves room, as the ruling's "say 16 entries" allowed.

## Disposition of this verification

**No discrepancy. The Session has nothing to correct in the sketch, and reports none to the Advisor
beyond what it already sent.** The sketch's structure, its two questions (a) and (b), and its P3 decision
stand on measured ground.

---

## Appendix — the Session's check of question (a), which sharpens it

Question (a) asks whether the **printed** `at_clear` block is admissible as the `AC-INPUTS` decision
input, given the fixture covers it "only on the snapshot, plus a count of at least 4 lines". The Session
verified that description **exactly**, and it is right:

| What the fixture asserts for `(xi)` | Line | Kind |
|---|---|---|
| `s.at_clear.taken == 1` | `apu_watch_fixture_test.c:1945` | **snapshot** field |
| `s.at_clear.seq == s.latch[GP_CLEAR].seq` | `:1946` | **snapshot** field |
| `s.at_clear.gpin.periph[0x45].reads == 1` | `:1949` | **snapshot** field |
| `s.at_clear.gpin.periph[0x54].reads == 1` | `:1950` | **snapshot** field |
| `s.at_clear.gpin.periph[0x56].reads == 0` (post-clear excluded) | `:1951` | **snapshot** field |
| `cap_count("[GPIN] at_clear ") >= 4` | `:1956` | **LINE COUNT ONLY** |

**So the snapshot side is solid and the printed side is a count.** The field-by-field
line-vs-snapshot comparison (`check_gpin_summary`) runs against the **`summary`** tag only — and
`apu_watch.c:919` (`emit_gpin_block("summary", …)`) and `:923` (`emit_gpin_block("at_clear", …)`) call the
**same** `emit_gpin_block` (`:851`), differing only in the tag string.

**That makes the Planner's question genuinely load-bearing and correctly framed**, and it is not
hypothetical: `AC-INPUTS` would decide `PASS`/`FAIL`/`UNKNOWN` from printed `at_clear` fields
(`out_of_universe`, `boot_scratch_read`, and the per-kind arrays), which the fixture currently validates
only as a count. The Session takes no position on the answer — that is the Advisor's — but confirms the
question is precise, the cited lines are correct, and the gap it describes is real rather than
notional.
