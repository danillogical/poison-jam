# `A2h-oom-causal-slice-r1` — Session verification and promotion

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Packet:** `docs/packets/a2h-oom-causal-slice.md`.
**Class:** **discovery** (§5.8) — the writing Planner reviews its own packet against §5.3's two questions and
**no second Planner is spawned**. **No shape preflight** (that is triggered only for a new **change**
packet).
**Authoring Planner:** child `8161685f-9951-4fd7-a8da-a5a707cc5e40`, `codex/gpt-6-sol` @ `high`.
**Adequacy:** the writing Planner's own review — **`ADEQUATE`**, `BLOCKING: NONE`,
`PREMISE_FRESHNESS: BOUNDED`.

| Item | Value |
|---|---|
| Revision | **`A2h-oom-causal-slice-r1`** |
| SHA-256 (frozen) | **`E9CDB1B39066CE5F5626FF74246E5EAD34C1612BC31BE622341B50FCC541108C`** |
| Lines | **47** |

**Hash verified 3× over ~10 s immediately before promotion**, all identical, and matching the Planner's own
reported hash.

## The Planner's premise revision, and why it was right

The Session characterised the OOM from archived runs and the original XBE (`docs/reviews/a2h-mechanism.md`)
and sent it to the Planner mid-authoring. **The Planner revised the packet around it**, and the revision is
correct on three points:

1. **It dropped the trap as a necessary cause.** ~~The no-trap A2g run~~
   **CORRECTION (acceptance stage 2): the A2g run IS TRAPPED** — `metadata.json`'s `settings` dict sets
   `RECOMP_APU_TRAP=1` and its log line 26 says *"trapped for MMIO"*. The Session had read
   `run_profile.effective_settings`, found it empty, and reported "ABSENT".
   **What survives is narrower and still correct: the failure predates the A4b2 trap work** — it reproduces
   on a different exe five days earlier, with all 94 invocation sizes identical index-for-index. **The strong
   "trap is not necessary" claim is withdrawn.** The Planner's decision to drop trap-*causation* as the
   packet's premise remains right; only the evidentiary basis is narrowed.
   (`20260922-224429-003-a2g-304f0-span`) shows the identical request, refusal and NULL ICALL. The packet
   cites that log directly.
2. **It made the primary question the size's producer** — *"which instruction and live input defined
   `[ebp-0x24]=0x23B20410` before the `0x00149E24` addition"* — rather than "fix the heap." That is the
   bounded backward question, and it is the only route that genuinely unblocks trapped observation.
3. **It put the guest's missing NULL check explicitly out of scope**, which is the right call: repairing a
   guest error-handling gap would be patching *title* behaviour, a far larger claim than a modelling fix.

## The Planner's `O-SEMANTICS` nuance, verified — and then narrowed

The Planner raised a load-bearing point from **`src/kernel/kernel_bridge.c:833-899`**, which the Session
**verified by reading the toolkit source**: the toolkit **distinguishes** a large pure `MEM_RESERVE` from
RAM backing, and on real hardware a reservation costs address space rather than pages, so *"a large virtual
request is not automatically invalid."* **The Planner was right to refuse to blame the guest on the arena
size alone**, and it added an `O-SEMANTICS` row rather than assuming guest blame.

**But the Session then decoded the failing invocation's allocation type, and it narrows that row:**

| Bit | Value | Present in the observed `0x801000`? |
|---|---|---|
| `MEM_COMMIT` | `0x1000` | **YES** |
| `MEM_RESERVE` | `0x2000` | **NO** |

The toolkit's reserve branches (`:857` grant-above-RAM, `:876` clamp) both require
**`(alloc_type & 0x2000) && !(alloc_type & 0x1000)`** — **FALSE** for `0x801000`. **Neither can run for
this call**, and the archive confirms it: **zero** "reserve of N granted" and **zero** "clamped" messages
anywhere. **So the failing call is a `MEM_COMMIT` with `BaseAddress = NULL` for 571 MB**, and a commit
backs pages — so on a 64 MB console it **cannot succeed on hardware either**.

**This is recorded as a narrowing, not a refutation.** The Planner's caution remains correct in general; it
simply does not rescue *this* invocation. **The Session's own "bad input" hypothesis is also still only a
hypothesis** — the producer of `[ebp-0x24]` has **not** been traced, which is precisely what the packet
exists to do. Both records now say so.

**A further measured fact the packet can use:** the same call site (`alloc_type = 0x801000`, 70 occurrences)
passes **exactly two sizes** — `2097200` (`0x00200030`) normally and `598869040` (`0x23B20430`) when it
fails, **~285× larger**.

> **A Session error, corrected before it propagated.** The Session's first pass claimed the normal pre-add
> local was **exactly `0x200000` (2 MB, a clean power of two)**. **That was wrong** — the normal pre-add
> local is **`0x00200010`**, neither page-aligned nor a power of two. The "exactly 2 MB" reading was
> **withdrawn** and the record corrected. What survives is the weaker two-size fact above.

## Command validation before freeze (the Planner asked for it explicitly)

| Named item | Validation |
|---|---|
| `scripts/inspect-jsrf.py` | **exists** |
| `scripts/check-run-profile.py` | **exists** |
| `scripts/check-dump-mapping.py` | **exists** |
| `scripts/a2h-oom-slice.py`, `scripts/test_a2h_oom_slice.py` | **do not exist** — they are the packet's **own deliverables**, not prerequisites |
| `JSRF_TRACE_A2H_SIZE` | **does not exist** in the toolkit — likewise the packet's deliverable |
| `python -X utf8 scripts/run-jsrf.py --profile strict --seconds 8 --label …` | **valid**: `run-jsrf.py` accepts `--seconds`, `--label` and `--profile {strict,exploratory,fixture}` |

**No named command is broken, and the missing paths are the packet's outputs.** The packet is executable as
written.

## Why the class and scope are right

The packet **investigates the cause** rather than counting OOMs or assuming the trap caused them: a
**backward, invocation-bound slice** from `[ebp-0x24]` at `0x00149E24`, using pinned archived runs and
original-XBE instructions first, with **at most one bounded 8-second diagnostic strict capture** if needed.
Its `O-OPEN` row explicitly refuses a fabricated fix (*"record the observation and select `O-OPEN`, not a
fabricated A2h fix"*), and it treats the 8 s as a **capture ceiling, not a progress budget**.

**Prohibitions carried and verified present:** it does **not** suppress the trap, fake the allocation, or
widen the arena; the NULL-guard guest-behaviour change is **out of scope**; **`PIO_FREE` stays deferred**;
`A4b2-r7`, accepted/closed `A4b2-r8` and `A4b1-r4` are not reopened; `0xFFFFB3` stays **`UNRESOLVED`**; any
toolkit change requires **re-establishing P4's discovery-transfer bridge** before inheriting P4.

## Promotion

`A2h-oom-causal-slice-r1` at **`E9CDB1B3…41108C`** was **frozen and promoted into `CURRENT PACKET` in the
same step**, byte-identical with no revision (§5.3). **The Session did not edit the packet.**

**Next:** execute the packet — the backward slice, producing
`docs/reviews/a2h-oom-causal-slice-evidence.md` and a first-match row, then §5.8 acceptance.
