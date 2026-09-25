# A4b adequacy review — attempt 1: reviewer exhausted context (no verdict)

**Packet:** `docs/packets/a4b-gp-dsp-engine.md`, `A4b-r1`, SHA-256
`751D8C2FE649E51DC5C92771E6A707555616BBA7AD89E0D8B42BEB1F6C69823C`.
**Reviewer:** Planner child `957224e6-67af-4852-8ecb-cb9ce70092c9`, route
`claude` / `claude-opus-5-5` @ `high` — a Planner that did not write the packet (§5.1.5).
**Outcome:** the child **ran out of context before emitting a verdict.** It produced no
§5.3 block. Per §2.2, a first-stage route/attempt failure is `pending — reviewer
unavailable`, not a failed review: it is repaired, not treated as `INADEQUATE`.

**State:** `A4b-r1` is **not** frozen and **not** promoted. `CURRENT PACKET` is still
`none` (the plan's `A4a-r2` block is closed).

## What the dead reviewer found before it ran out (extracted from its transcript)

The Session recovered the child's own reasoning from
`C:\Users\logic\.dsh\sessions\--C-Users-logic-Repos-my_xbox_game--\957224e6-67af-4852-8ecb-cb9ce70092c9\session.v3.jsonl.zstd`
(zstd-compressed JSONL; 235 records, 40 tool calls, 39 reasoning blocks, **one** text
block, and that block was only "Hash matches. Reading the packet."). Its findings exist
only as reasoning, never as a returned verdict. Two are load-bearing.

### FINDING 1 — BLOCKING, and the Session has independently confirmed it: the `PIO_FREE` site population is 28, not 10

The reviewer's words:

> Actually there are 28 PIO_FREE reads, not 10 — the ruling only caught the literal
> `MEM32(0xFE820010u)` form, missing 18 more that use the signed equivalent
> `MEM32(-25034736)`, which is the same address. So the Advisor ruling's premise of "ten
> read sites" is factually wrong… the "PASS" verdict on PIO_FREE gating only covers 10 of
> 28 actual sites — if any of the 18 unaudited ones let a polled value flow into data,
> that's a false PASS under Q1's reversal condition.

**Session verification (independently reproduced, this is now observed, not a lead):**

| Check | Result |
|---|---|
| `0xFE820010 - 2^32` | `-25034736` — **the same guest VA** |
| `MEM32(0xFE820010u)` sites in `src/recomp/gen/recomp_*.c` | **10** |
| `MEM32(-25034736)` sites | **18** |
| Overlap between the two lists | **0** |
| Distinct total | **28** |
| Any other literal form of the address (`4269932560`, other spellings) | **none** |

The ten sites named in `A4b-r1`'s `AC-PIO` and in the Advisor's Q1 ruling
(`recomp_0005.c:8667, 8720, 13698, 13867, 14172, 16548, 18243, 18377, 19816, 19885`) are
therefore **only 10 of 28**. The 18 uncounted sites are at
`10365, 11307, 11449, 11883, 12018, 12132, 12396, 12547, 12658, 12782, 12882, 13120, 13246, 13770, 14291, 14399, 15755, 18358`.

**Why this is blocking under §3.1.** `AC-PIO`'s PASS predicate is "every site is a
threshold re-poll… the value is not stored inside the loop, and on every exit path the
register is overwritten before any read". A faithful literal execution enumerates **ten**
sites, finds them all conforming, and returns **PASS** — while 18 further sites of the same
address go unexamined. If any of those 18 lets the stub constant flow into data, the
packet reports a false PASS on Q1 condition 3, and the Advisor's Q1 reversal condition (a)
is met without anyone noticing. That is a false PASS in the operative contract.

**Spot-check by the Session (not a substitute for the criterion's own audit):** all 18
uncounted sites do follow the same threshold re-poll shape — `(v & 0xFFFFFFFC) < N` with
N ∈ {8, 0xC, 0x48, 0x4C} or `(v >> 2) < reg` — and the one anomalous-looking site,
`L13770`, is a re-poll loop (`_cf` set from bit 1, then `cmp edx,eax` / `jb loc_001A3F24`
back to the read). So the *conclusion* of Q1 condition 3 looks like it will survive. **But
that is exactly the point: the criterion's enumeration is wrong, so the criterion cannot
establish what it claims, and the spot-check is not the criterion.** Repairing the
enumeration is the Planner's job, not the Session's.

**This invalidates a premise of the Advisor's Q1 ruling**, which was stated over "all ten
`0xFE820010` read sites". Per workflow §4.3, a correction that changes a load-bearing
premise is marked `PREMISE_CHANGED` and the Advisor is asked to reconsider every ruling
that depended on it. The Advisor must be told.

### FINDING 2 — an uninstrumented writer of the watched word (needs Planner judgment)

The reviewer traced the stop path and noted that the store to `edi+0x10` (which is
`B+0x810`) is expressed as an **offset from a pointer**, so it does not match `AC-NOCPU`'s
instrumentation pattern `MEM32\(.* \+ 0x810\) =`. It also noted that the stop path writes
`GPRST` (`0xFE83FFFC`) and calls a routine that resets `GPRST` to `1` then `3`, and raised a
race: the guest CPU could write `0` between the before-sample and the DMA write.

The Session observes that `A4b-r1`'s author already flagged a related gap in its own
advisory 3 ("AC-NOCPU does not instrument block copies (`rep movs`) into `B+0x800..`") and
that the packet's `AC-NOCPU` claim limits already say the in-call payload witness in
`AC-CLEAR` is the primary attribution. Whether the offset-form store and the race are
adequately covered is a **judgment call for the reviewing Planner**, which did not get to
make it. Recorded, not decided, by the Session.

### FINDING 3 — a write-scope gap (advisory-grade, but real)

`recomp_0005.c` includes a **generated header** that is outside the packet's write scope,
so the observation calls in step 6 need a declaration of `jsrf_watch_store`. The reviewer
considered an inline `extern` in each chunk, and noted that adding a new game source file
for it would require touching the game `CMakeLists.txt`, which the scope excludes except
for the fixture — unless an existing built source such as `diagnostics.c` is reused. This
is a scope-completeness question for the Planner.

### FINDING 4 — `AC-PIO` may be undecidable at some exit paths (UNKNOWN, not FAIL)

The reviewer found an exit path reaching an **indirect call** at `sub_001A1BAF` /
`0x1C4004` that is probably a kernel import thunk, whose effect on `eax` cannot be
determined by disassembling the XBE. Under `AC-PIO`'s own rule that is `UNKNOWN` for that
path — which the packet routes to `R-PIO-DATA`/re-plan. The reviewer was still deciding
this when it ran out. Relevant because `AC-PIO`'s PASS requires **every** exit path
resolved.

## Consequence and next step

- The review is **`pending — reviewer unavailable`** and must be repaired by re-running it
  with a fresh Planner child (§2.2: "A first-stage route failure is `pending — reviewer
  unavailable`, not a failed review; it is repaired, not passed to the second stage").
- **Finding 1 must be fixed before, or as part of, that re-review**, and it needs the
  Advisor: the Q1 ruling's premise is `PREMISE_CHANGED`. The Session will consult the
  Advisor and then return the enumeration defect to a Planner for repair.
- The Session did **not** edit the packet. Repairing a criterion is the Planner's authority
  (§2.3, §5.4); a contract role must not reinterpret one (§2.2.6).
- The dead child is not reused (§4.4: reuse the Advisor, but a reviewer that exhausted its
  context is not a reliable continuation for this review).

## Why the Session did not simply re-run the review first

Because Finding 1 is a `PREMISE_CHANGED` against a **binding Advisor ruling** that the
packet's `AC-PIO` criterion is built on. Re-running the adequacy review against an
uncorrected premise would spend another large review to reach the same defect. The
correction comes first, then the review, which is also the cheaper order.
