# `A2h` TARGET yield — the K = 1 outcome and what the data says about it

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Raised because:** the accepted `A2h-arming-coverage-attribution-r2` returned **`O-COVERAGE` on the K ≥ 2
requirement** with **K = 1**, and its own rule for K = 1 is *"preserve the run-local row, `UNKNOWN`
generality, re-refer."* **`N` cannot be extended without Advisor referral.** This record assembles the data the
referral needs.

---

## Two same-build sets of five ON runs now exist

| Set | Packet | Build | TARGETs |
|---|---|---|---|
| **A** | `A2h-slot-within-run-attribution-r1` | `a7e3246…` | **3 of 5** |
| **B** | `A2h-arming-coverage-attribution-r2` | `a7e3246…` | **1 of 5** |
| **Combined** | — | **same build** | **4 of 10 (40%)** |

**Both sets were executed on the SAME executable**, so the two sets are comparable draws from one process.

## The measurement that matters for the referral: the anchor does NOT predict the terminal

| Terminal | Count | Class |
|---|---|---|
| **`0x00000000@0014982E`** | **4** | **TARGET** |
| unresolved `0x001D5078` | 3 | NON-TARGET |
| `0x00000001@0018CE73` | 1 | NON-TARGET |
| `0x3E800000@00147DE2` | 1 | NON-TARGET |
| `0x41200000@00147D36` | 1 | NON-TARGET |

**The deterministic anchor — the identity-1 dispatch prefix — is `5555` in EVERY run of BOTH classes.**

**So the anchor carries no information about which terminal a realization will produce.** That is
**consistent with the packet's own rule that the terminal is excluded from validity**, and it means:

> **the terminal outcome is a genuine DRAW, not a function of anything the anchors measure.**

**The Session did not set out to establish this** — it fell out of comparing the two sets, and it is
**directly relevant to the referral**, because it rules out the tempting idea that some measurable pre-terminal
condition selects the target.

## One caution about the 67% figure

**The archived census over 39 OOM-line runs gave 26/39 (67%) for the target — but those runs span MANY
DIFFERENT BINARIES.** **The same-build combined rate is 4/10 (40%).** **The 67% is not a rate for this build**,
and the packet already says so. **With N = 5 and a true rate near 40%, the probability of drawing K ≥ 2 is
materially lower than the 67% figure would suggest** — which is the practical shape of the problem.

## What the Session is NOT deciding

**Whether to extend N, change the design, or pivot the line.** The packet forbids extension without referral,
and **the choice among (i) more runs at the same N per packet, (ii) a design that raises the target yield, or
(iii) treating the four non-target terminals as the more productive object of study — is a Planner and Advisor
question.** **The Session's job here was to make the referral data-rich rather than a bare yield number, and
that is done.**

## What is NOT in question

**The instrument is coverage-complete and independently verified.** The stage-1 reviewer confirmed **17
`GUEST_DR_ARM_OK` records per run split 10 handshake / 7 create_thread**, the fresh OFF **record-level inert**
(all-zero registry including the watch), and the **harness fix verified with its pre-existing break confirmed
by dates**. **`O-COVERAGE` on K ≥ 2 is correct and is a YIELD shortfall, not a coverage failure.**
