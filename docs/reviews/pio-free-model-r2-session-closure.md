# `PIO_FREE-model-r2` — Session closure and verification

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/pio-free-model.md`, revision **`PIO_FREE-model-r2`**, frozen SHA-256
**`11D6ECB195D51159785D6E94C98439BD4AA6979FDEE6B8EC79B28A80961DDA9E`** — verified before execution and
**not edited** during it.
**Execution evidence:** `docs/reviews/pio-free-model-execution-evidence.md`.
**Selected row:** **`O-UNKNOWN`**.

## Closure discipline

| Item | Value |
|---|---|
| Game revision at execution | `d54b5ff55e98f40c0a990858b543bcbf3b802af8` |
| Toolkit revision | `c151d4e32a782e4e5adcecbc68afe61ed5fc7e52`, clean |
| XBE SHA-256 | `FD19055756719893C466302809B433B785ECF5732DF0441286F3F605F0F3EF9C` — matches baseline |
| ctest | **18/18** (unchanged; no build was performed) |
| **Toolkit changes** | **NONE** — so **P4's discovery-transfer bridge is not re-opened** |
| **Game code changes** | **NONE** — write scope was this evidence record only |
| **Instrumentation** | **NONE exists**, so nothing is enabled at closure (§5.8) |
| Guest runs / builds | **none**, as the packet requires |

**§5.8 compliance:** the packet is a **discovery**, its output is **knowledge**, it **never satisfies a
strict criterion and never claims anything works**, and its outcome table names the next packet. **No
instrumentation was added**, so the "off by default at closure" requirement is satisfied vacuously.

## What the execution found, in one paragraph

`0xFE820010` is `APU+0x20010` = `VP+0x10` = **`NV1BA0_PIO_FREE`** — the same address `A4p` called "PIO",
not a second candidate device. The current model returns a **constant `0x80`** with the comment *"Always
pretend queue is empty"*, and every other VP offset reads `0`; VP writes dispatch **synchronously** through
`fe_method`, so the model has **no queue at all**. The guest's two threshold forms — one constant
(`val & ~3` vs `4`) and one **variable** (`val >> 2` vs a register) — are reconciled, and the shift form
reads the word as a **count in units of 4**, so `0x80` asserts **32 free units** rather than a neutral
"empty". The site's shape is **poll-then-push** to `NV1BA0_PIO_SET_HRTF_HEADROOM`. **Five of the eight
ledger leaves remain `UNKNOWN`** — units, capacity, drain, overflow, ordering — and **the independent-source
rule is unmet**: no primary specification was found, and the one admissible secondary source
(xboxdevwiki `APU`) is **silent**, verified by term count on its raw wikitext.

## Why `O-UNKNOWN` and not the neighbours

| Row | Rejected because |
|---|---|
| `O-IDENTITY` | Identity, provenance and coverage all reconcile; no pinned premise changed |
| `O-CONFLICT` | Conflict needs **two authenticated sources giving incompatible concrete meanings**. There is **one admission** (xemu — excluded as toolkit ancestry) and **one silence** (the wiki). **Silence is not conflict**, so no Advisor referral on this evidence |
| `O-SPEC` | Requires **every** ledger leaf covered **and** the independent-source rule met. Five leaves are `UNKNOWN` and sourcing is inadequate — it fails on **two independent grounds** |

**The false-model test is well-posed** — a queued write that must consume space cannot truthfully leave
the free count unchanged, and the synchronous `fe_method` cannot represent that — **but well-posed is not
covered.** It is recorded as the **prediction a future model must satisfy**, not as a finding.

## `A2h` is deliberately **not** named as prerequisite

The row's A2h condition applies only *"if the only missing witness requires trapped strict time beyond the
available pre-OOM prefix."* **The missing witnesses are documentation, not guest time.** No amount of
trapped guest execution would produce a vendor's queue-depth figure. **So the successor packet is a
sourcing/interface discovery, not a run**, and `A2h` is not engaged.

## The Session's own error, recorded

**A verification error during sourcing.** My first check of the "wiki is silent" claim grepped a **spill
file that actually held the xemu source**, and reported `PIO_FREE` = 1 — which **appeared to falsify** the
claim the row depends on. Re-fetching the wiki's **raw wikitext** into a known file and counting terms
there gives `PIO_FREE` = 0. **The conclusion is unchanged, but it was briefly unsupported**, and the lesson
is the one this project keeps re-learning: **verify against the artifact you name, not against a buffer you
assume holds it.** Recorded in the execution evidence rather than quietly fixed.

## Next packet, per the row

**A focused `PIO_FREE` source/queue-interface discovery** on the exact `UNKNOWN` leaves: **units and bit
encoding, capacity, drain/completion, overflow/backpressure, and read-vs-write ordering** — with the
false-model test carried as the positive prediction it must satisfy.
