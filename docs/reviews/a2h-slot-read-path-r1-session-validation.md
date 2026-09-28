# `A2h-slot-read-path-displacement-r1` — Session validation (pre-freeze)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-read-path-displacement.md`, revision `A2h-slot-read-path-displacement-r1`,
**DRAFT, NOT promoted**.
**Authoring Planner:** child `974012bf-e89a-4e28-9ae7-52ed73d5c0f3`, `codex/gpt-6-sol` @ `high`.
**Adequacy:** the writing Planner's own §5.3 review — **`ADEQUATE`**, `BLOCKING: NONE`, **conditional on the
Session's literal-command and feasibility validation before freeze.**

**Shape preflight:** **obtained and recorded** — `docs/reviews/a2h-slot-read-path-advisor-shape-preflight.md`
holds the superseded first ruling **and** the corrected one, plus the Session's `PREMISE_CHANGED` correction,
the collector-identity repair, the DR feasibility gate, and the alias-derivation clarification.

---

## The Planner's stated freeze condition — **SATISFIED**

The Planner made promotion conditional and named what it needed:

> *"Session must mechanically validate literal commands and archive/artifact extraction capability … need
> verify terminal snapshot extraction before approval."*

**Both are now discharged.**

### 1. Artifact extraction capability — **PROVEN, and it was a real gap**

The latch's own fields (`install_ok`, `claimed`, `slots[]`, `overflow`) are **not printed by the collector** —
`stacks.txt` shows the *thread* registry's `claimed`, not the latch's. So a reader could not see the decision
record.

**Resolved:** `scripts/a2h-read-registry.py` (**17 self-tests OK**) extracts the latch from the archived
minidump. Its layout is **validated against real bytes**, not trusted: the registry header must be unique, the
latch install record must be unique, and **the distance between them must equal the computed `LATCH_OFF`** —
which it does, exactly (`538648`). Record: `docs/reviews/a2h-latch-extraction-record.md`.

**A Session error this caught, worth naming:** the first layout used `EVENT_SIZE = 24`; the correct value is
**32**, because `struct JsrfEvent`'s `u64 ticks` forces 8-byte alignment and the struct rounds up. **That one
constant shifted every offset and made the reader report the latch absent from a dump containing it** — an
absent-record-read-as-negative error. Corrected, measured against a real archive, and **pinned as a test**.

### 2. Literal assertions — **VALIDATED**

| Packet claim | Validation |
|---|---|
| Original-XBE call bytes `0x00149828:ff1564401c00` | **verified** — `a2h-oom-slice.py --verify` accepts them |
| **Ten** direct toolkit `CreateThread` call sites, enumerated by file:line | **CONFIRMED EXACTLY.** A full grep over toolkit `src/` returns 21 hits; **11 of them are comments, definitions, or unrelated identifiers** (`win32_compat.c:719`/`.h:156` **define** the compat API; `kernel_bridge.c:538`, `:7452-7453`, `:8348`, `:8885`, `kernel_thread.c:8`, `:20`, `:66`, `:113` are comments or adjacent text). **The ten the packet names are precisely the real call sites** |
| `platform/win32_compat.c:719` defines rather than calls | **confirmed** — it is the `HANDLE CreateThread(...)` definition |
| `XBOX_NUM_MIRRORS = 28` | confirmed (`xbox_memory_layout.h:438`) |
| The slot is on page `0x001C4000`, distinct from the documented `0x003B2000` | confirmed |
| `DEBUG_ONLY_THIS_PROCESS` makes `IsDebuggerPresent()` true | confirmed (`collect.c:294-295`); the packet correctly requires verifying the **expected** collector and DR ownership instead |

### 3. Feasibility gate — **executed, PASSED with a design change the packet encodes**

The packet's line 25 already carries the finding: **six dispatching tids vs five registry records**, the
unregistered one being a **toolkit host worker**, so **the guest registry is not a native-thread census** and
the arming universe must come from source plus collector `CREATE_THREAD` handling. **The packet states this
correctly and does not assume registry completeness.**

---

## Session findings the packet should absorb before freeze

**1. The extraction advisory is now actionable.** The packet relies on frozen-record extraction; the Session
has **built and tested** the extractor. The packet should **name `scripts/a2h-read-registry.py`** as the
extraction path, and **require the collector to print a latch summary line** so a human reviewer finds the
decision record in the archive's own text rather than only through a tool.

**2. Two requirements the Session derived that the packet should state explicitly** (both recorded in the
preflight record, neither yet in the packet):
- **RECORD-BEFORE-OPEN** — the alias handler must publish its touch record **before** `VirtualProtect` opens
  the window. The AC97 precedent does **not** settle this (it records nothing at fault time), so the packet
  must impose it and fixture it.
- **ARM-TO-TERMINAL COVERAGE** — a page with **zero** touches is only "never written" if it was armed for the
  **whole** interval, so mapped-set equality is needed at arm **and** at terminal.

**3. The alias derivation must be the HOST-offset form.** The packet's line 31 says
`g_mirror_views[m] + (0x001C4064 & ~0xfff)`, which is **correct**; the Session's earlier guest-range framing
was wrong by 4×. **Confirmed the packet uses the right form** — recorded so the correction is not
reintroduced.

**4. `install_ok` is now readable from the archive**, so the packet's install positive control can require the
**latch's own verdict** rather than only a log line. **Run 2's archive already shows `install_ok = 1`**, and
Run 1 (gates OFF) shows **no install record at all** — a stronger inertness control than log absence.

---

## What remains before freeze

**The packet is a coherent, adequately-reviewed design and the Planner's two named conditions are met.** The
Session's remaining actions are editorial and additive: fold in items 1, 2 and 4 above, then freeze, hash and
promote.

**Nothing in this validation changes the packet's class (`discovery`), its row set, its two-run bound, or its
prohibitions.** **No synthetic completion** is present or proposed; the producer line stays **PARKED**;
`PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3` stays
**`UNRESOLVED`**.

---

# FROZEN — `A2h-slot-read-path-displacement-r1`

**The Planner applied all four additions and returned the packet.** The Session has validated it and
**frozen** it.

| Item | Value |
|---|---|
| Revision | **`A2h-slot-read-path-displacement-r1`** (unchanged — the additions were within the same revision) |
| Lines | **56** |
| Bytes | **30856** |
| **SHA-256 (frozen)** | **`75E6AE7C9A26A2CF63F9AD19502A170B2960FE2C90523A566F3F382942BB9B6A`** |

**Hash read 3× over ~12 s and identical, matching the Planner's own reported value.**

## The four additions — all present and validated

| # | Addition | Verified |
|---|---|---|
| 1 | `scripts/a2h-read-registry.py` named as the authoritative frozen-latch extractor, plus a required **collector** versioned human-readable latch summary cross-check | present at L20/22/27/33/38/56; **the tool executes on both real archives** |
| 2 | **RECORD-BEFORE-OPEN** stated explicitly, with the AC97 precedent explicitly ruled insufficient and a concurrent fixture required | present at L31, L38, L49 |
| 3 | **ARM-TO-TERMINAL** mapped/armed set equality, with an at-event failure latch | present at L31 |
| 4 | Frozen `install_ok=1` required ON, and **no** OFF install record, both read from the extractor | present at L20, L27, L40 |

**Preserved unchanged, as instructed:** class (`discovery`), the row set, the two-run bound, the prohibitions,
the **HOST-offset** alias formula `g_mirror_views[m] + (0x001C4064 & ~0xfff)`, and the six-vs-five registry
feasibility section.

## §5.1.5 — every named tool and fixture executed

| Named item | Result |
|---|---|
| `scripts/a2h-read-registry.py` | **exists, 17 self-tests OK**, and **extracts both real archives**: Run 2 → `install_present=True install_ok=1`; Run 1 → `install_present=False install_ok=0` |
| `scripts/a2h-slot-triage-classify.py` | **11 self-tests OK** |
| `scripts/a2h-null-slot-triage.py` | **17-test suite + self-test OK** |
| `scripts/a2h-frame-audit.py`, `scripts/a2h-oom-slice.py`, `scripts/run-jsrf.py` | exist; suites pass (29 / 31 tests) |
| **The collector print the packet requires** | **VERIFIED LIVE** — a fresh strict run's `stacks.txt` carries `GUEST_SLOT_LATCH install_seen=1 install_raw=80000115 install_value=FE000104 install_ok=1 claimed=0 overflow=0 partial=0`, **and the extractor reads the same values from the minidump**, so the two paths agree |

**A durable improvement the Session made while validating addition 1:** the collector now **prints** the
latch's fields (`tools/harness/collect.c`, guarded on `registry->version >= 2`). Before this, the latch was
archived but invisible in the archive's text, so the extractor was the only path to a decision record. **Both
paths now agree**, which is the cross-check the packet requires.
