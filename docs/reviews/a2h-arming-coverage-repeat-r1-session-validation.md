# `A2h-arming-coverage-repeat-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-arming-coverage-repeat.md`, authored by Planner
`8949838e-dba5-4cae-9387-70c58c3baba9` (`codex/gpt-6-sol` @ `high`), committed `3db6b9a`.
**Authority:** `docs/reviews/a2h-null-line-yield-advisor-ruling.md` — *"One more N=5 packet, r2 design repeated
verbatim."*
**Adequacy:** the Planner's §5.3 review — **`ADEQUATE`**, `BLOCKING: NONE`, conditional on the Session's
literal binary/identity checks.

---

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-arming-coverage-repeat-r1`** |
| Lines | **15** (13 before the K-scope amendment; 14 non-empty plus a trailing newline) |
| Bytes | **5552** |
| **SHA-256 (frozen)** | **`4AC455E7F427ABCD9AEB6A81FA34B2A570A826964A6FB76EC3478ADA4B1174A1`** |

**Fifteen lines, and that is the right size** — the Advisor ruled the design repeats **verbatim**, so the
packet's value is in its deltas, **not** in restating an instrument that is already built, tested and accepted.

---

## The K-scope ambiguity — the Planner flagged it, the Session escalated it, the Advisor ruled **(i)**

**This is worth recording in full, because the Planner's flag materially changed the packet and it would have
been easy to decide privately.**

**The question:** the ruling said *"K counts only r2-qualifying realizations — set-A targets stay leads, never
K"* **and** *"the next packet needs [run 2] as the agreement comparator and template."* **Run 2 IS an
r2-qualifying realization** (same instrument, same build, coverage-complete TARGET). **Set-A is from r1 — a
different packet.**

- **(i) Run 2 counts toward K** ⇒ the successor needs **one** new agreeing TARGET.
- **(ii) Run 2 does not count; K needs two from the new set** ⇒ the successor needs **two**.

**The Planner implemented (ii) deliberately and explicitly flagged it as a concern for the Session** — *"comparator
cannot substitute for second K."* **The Session escalated rather than choosing**, because the difference is
roughly a **40% vs 65%** chance of reaching a decision within the bounded packet.

**Advisor ruling: `(i) is correct. Amend the packet before freezing.`** And the reasoning is the part worth
keeping:

> *"The Planner's caution is the right instinct at the wrong layer — implement it as mechanical agreement +
> independent re-assessment, not doubled sampling... The K≥2 rule guards against **single-draw** flukes, and
> **draws vary run-to-run, not packet-to-packet**. Run 2 is a fixed, accepted, same-instrument, same-build draw
> — textually, the rule as written counts 'two coverage-complete TARGETs,' with no packet-membership qualifier.
> (ii) imports a restriction the text doesn't contain, **at zero protective value**: two fresh realizations would
> share run 2's instrument and build anyway, so (ii) guards against nothing (i) doesn't, while dropping success
> from ~92% to ~66%."*

**Set-A stays excluded** — *"the (i)/(ii) line turns exactly on same-instrument + same-build, which set A fails
and run 2 meets."*

**The Session records this because the pattern recurs:** the Planner's instinct was **substantively right**
(protect independence) and **misapplied** (it protected independence by weakening the sample rather than by
strengthening the agreement test). **Escalating a genuine ambiguity instead of resolving it privately is what
surfaced the distinction.**

## The amendment — four binding conditions, all implemented

| # | Condition | Verified in the packet |
|---|---|---|
| 1 | **Run 2 cited by ARTIFACT HASH, never summary** | **Session-verified:** the named review's hash is pinned as `C5E5DE05…92FF` and **matches the file exactly**; the raw `result.json`, `metadata.json`, `jsrf_run.log`, `stacks.txt` and `process.dmp` hashes are pinned and **the Session recomputed all five** |
| 2 | **Agreement MECHANICAL and pre-specified** | same independently certified **writer class/row**, **compatible observed mechanism/event order**, **terminal triple** (slot `0x001C4064`, target zero, site `0x0014982E`), assessed **from both raw artifacts** and **re-assessed independently at adequacy AND acceptance**; **run 2's prior `UNKNOWN` remains until certification, and shared silence is insufficient** |
| 3 | **Early stop restated** | first new coverage-complete TARGET ⇒ compare with pinned run 2; **agree ⇒ K=2, general certified row, stop**; **disagree ⇒ report both + `UNKNOWN` generality + re-refer, no majority-seeking**; no new TARGET after `N=5` ⇒ **`0/5` + re-refer** |
| 4 | **K scope** | **run 2 counts** (same instrument/build, supplies K=1 initially, *"never a certified conclusion"*); **set-A excluded**; **"4/10" never K** |

**The Session recomputed every pinned hash and all six match** — so the comparator is pinned to **bytes**, not
prose, which is what makes the mechanical agreement test enforceable.

## Validation — the identity anchors, verified by the Session

**The OFF-reuse condition depends on binary hashes, and the Session measured them:**

| Artifact | OFF run (`…030751-407`) | **Current build** | Match |
|---|---|---|---|
| `jsrf_recomp.exe` | `a7e324642a41df3f9070a0366e4e199cca6ded34304320bc12cef24e7acdead9` | **identical** | **YES** |
| `jsrf_collect.exe` | `95440ec43011a01d25ce0ab9d0077c892312ba39fd9238c1c7ac8001676b3ef7` | **identical** | **YES** |

**The packet pins exactly these values** (line 5) **and the XBE SHA** `fd190557…3ef9c`, and it **correctly
states that binary hashes govern rather than a differing documentation-only `project.revision`** — which is the
caveat the stage-1 reviewer raised about the previous set, now carried forward as a requirement.

**So the OFF control legitimately carries, and no fresh OFF is taken.** **On any mismatch the packet requires
`O-IDENTITY`/STOP, never a borrowed or substituted control** (line 6) — which is the correct fail-closed form.

## The four deltas — all present and correctly stated

| # | Delta | Where |
|---|---|---|
| 1 | **No fresh OFF**; reuse `…030751-407` and its record-level inertness review, gated on the hashes above | line 6 |
| 2 | **Run 2 is the named comparator** — `a2h-run2-target-run-local-record.md`, explicitly *"a comparator, not a certified writer/read-path conclusion"* | line 8 |
| 3 | **K-scope anti-fishing guard** — K counts **only** this run set's qualifying realizations; *"set-A TARGETs remain leads, never K. Never import '4/10' into K"*; the 40%/66% figures are *"a planning input only, not evidence or a rate claim"* | line 10 |
| 4 | **The stopping point, fixed in advance** — K<2 ⇒ *"re-refer for pivot-or-defer with 10+ qualifying runs on record"*; the `0x001D5078` pivot **queued, not concurrent** | line 11 |

## One deliberate strictness the Session notes rather than changes

**The packet's agreement rule (line 9) is STRICTER than the Advisor's minimum.** The ruling said stop early on
*"two coverage-complete TARGETs with independent attribution [that] agree."* The packet requires **both**:

- **two new coverage-complete TARGETs that agree with EACH OTHER**, **and**
- **that both agree with run 2's observed signature.**

**That is a faithful reading of Q4** (*"the next packet needs it as the agreement comparator and template"*) and
it is **conservative in the safe direction** — it cannot manufacture agreement. **The consequence is worth
stating: the early stop may be harder to trigger, so the packet will more often run the full `N = 5`.** **That
is safe, because the hard cap still binds and the anti-fishing structure is intact.**

**The Session records this as a deliberate strictness, not a defect, and does not weaken it.** The packet also
correctly adds that **run 2's `UNKNOWN` "cannot be upgraded into a writer or read-path claim by resemblance
alone"** — which is exactly the discipline this line has needed repeatedly.

## Freeze decision

**Every checkable claim is validated, the OFF-reuse condition is measured and holds, and the packet implements
the ruling without expanding scope.** **The packet is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class (`discovery`), its row set, its `N = 5` cap, its validity anchors, or its
prohibitions.** **No synthetic completion.** **No code changes are authorized** — this re-runs an unchanged
build. The producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are
**not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.

## Execution order

1. **Verify identity** — hash the built binaries against the pinned values; confirm tree deltas since
   `d369880`/`571982d` are **documentation-only**. **Mismatch ⇒ `O-IDENTITY`/STOP.**
2. **Reuse the OFF** — no fresh run.
3. **Up to `N = 5` ON**, strict, unchanged gates, fresh labels and save roots.
4. **Apply the agreement rule**; early stop only on K ≥ 2 agreeing **and** agreeing with run 2.
5. **§5.8 acceptance.**
