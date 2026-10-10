# Jet Set Radio Future: the plan to the title screen

**Status: execution authority** (owner, 2026-10-09). It replaces `plan-jsrf-bare-minimum.md`, which is
retired. Its full text is `git show d6b8336:plan-jsrf-bare-minimum.md`, including the slice's
Definition of done, milestones M16–M30, the backlog and the IDs other files cite (V, T, W, C, ML, F).
Those return once M15 is met.

**Authorities, so this file does not repeat them.**
- `docs/agent-workflow.md` owns roles.
- `docs/jsrf-run-profiles.md` owns evidence profiles.
- `docs/jsrf-technical-record.md` ("TR §n") owns established facts.
- `docs/jsrf-compatibility-ledger.md` owns every shortcut (`Lnn`).
- `config/stop-chain.json` owns the runtime stop chain.
- `AGENTS.md` owns build and repository discipline.
- This plan owns the objective, the current state and what to try next.

## Objective: M15

**The title screen on the recomp, identified by content:** the JSRF emblem and "PLEASE PRESS START TO
BEGIN" over a perspective city street, as in the xemu capture
`logs/workers/title007/xemu/deliverable/jsrf-title-screen-xemu-press-start-640x480.png` (Windows box,
reached with no input).

- A frame hash alone is not the criterion. The disclaimer renders in four hashes and
  `87683a748e27d071` is the Dolby card (TR §23).
- Met when a presented frame (an `[FBPRESENT]` dump or a window capture) shows it, compared by eye
  against the capture.
- The record names the run, the frame hash and every ledger ID the run relied on.
- A second run reproduces it.

## Stance (owner, 2026-10-09)

**Pragmatic, like Mercenaries Recompiled.** Patches, bypasses, high-level emulation and faked signals are
all acceptable on the way. Performance and fidelity are not goals; a slow or approximate path that reaches
the title is better than a faithful one that does not.

One rule stays, because it is cheap and keeps shortcuts from being forgotten: **every shortcut gets one
line in the ledger**, and the run record lists the IDs (`docs/jsrf-run-profiles.md` §"Pragmatic bare
minimum").

**The orchestrator calls the shots.**
- It picks the next experiment.
- It reads the result.
- It advances, re-ranks or drops a line of attack.
- It keeps going until M15 is met, stopping only for the owner-decisions below.

## Current state (2026-10-09, late)

**M15 is MET (2026-10-09).** First observation: Run `20261009-225354-960-title012-m15-long` (1800 s,
`just title-run`, game `82f80f1`, toolkit `60bf20a`) presented the title screen at ~1580 s: the JSRF
emblem, "PLEASE PRESS START TO BEGIN", the JETSETRADIOFUTURE logo and the SEGA/Smilebit line, matching
the xemu capture (present dump `m15\f_p0389.bmp` in that run's folder on Windows, sha256
`5e663a97bd5f11d1596b16d74bb77ebf090d931141aaff0f4a3fb5387fa4d3b3`; TR §26.6). Ledger IDs relied on:
L56 (the load timeout), plus the `just title-run` switches (L16, L18, L47; `RECOMP_APU_TRAP`).

What got it there, all on 2026-10-09:
- stop 32 (`0x91830` widened; now runtime-confirmed);
- L56 (the title's 15 s wall-clock load timeout skipped);
- toolkit `60bf20a` (the APU frame thread no longer starves guest threads, which was the "Now Loading"
  hold).

**Reproduced** by `20261009-232934-172-title012-m15-repro2` (1800 s, the same build): the same
press-start frame (`m15\f_p0209.bmp`, sha256 `71ebdbd5bb35ed756b16abd54d29a4fb3f90641c7f1792f140401736368fc419`;
its frame-sequence scores match the first run's exactly). The emblem and logo screen alone also appeared in
`…221513-243` and `…223657-233`. Per this plan, the retired plan's post-title milestones (M16 onward,
`git show d6b8336:plan-jsrf-bare-minimum.md`) come back next; that is the owner's call.

**The next stop:** the same run died at 1598 s, after the title, on `[ICALL] Failed to resolve VA
0x000BE190` (stop 33, DISCOVERED). Closing it is the first line below; the reproduction run follows it.

## Lines of attack (ranked; the orchestrator re-ranks)

1. **Done 2026-10-09: the fatal path is closed (L56).** It was the title's own 15 s wall-clock
   "load still pending" timeout (`0xE4E1C0` µs in `0x25310`, `0x25400`, `0x66440`; TR §8). The recomp's
   slowness tripped it. L56 skips it; `…213955-475-title012-timeout-off` (900 s) has no `[FATAL-*]`
   line, reads `title.adx`, and holds.
2. **Break the "Now Loading" hold.**
   - Find what the main thread and the loader wait on during it: digest the stacks of a held run, then
     guest-code analysis of the wait (the lifted C in `src/recomp/gen/`, plus `inspect-jsrf.py disasm`).
   - Then satisfy it pragmatically: a ledgered patch in `config/generated-patches.json`, a manual
     override, or the missing signal fed.
   - R3, the one run that passed, had by far the best vblank delivery (~49 pulses/s against ≤ 23). A
     faked steady vblank, or pulsing without the owner lock, is the cheap test of that reading.
   - Advances when presents pass ~2460 and keep climbing.
3. **The fatal path itself**, only if combiners turn out to be needed for the title's look. Find what
   the loading job `0x01330060` (`+98=30000074`) failed on (TR §26.4; the L41 instrumentation is in
   place).
4. **New stops — now first.** Stop 33 (`0x000BE190`) ends the run after the title. Close it as stops
   29–32 were, then rerun ≥ 1800 s to reproduce the press-start frame.
5. **Presentation.** If the title renders into a surface but is not shown, the draw→present questions
   (TR §24.3, §25.7, the flip trace L47, `RECOMP_FB_VA`) and the surface dumps.
6. **Throughput,** only if a run cannot reach the title in ~900 s. Pragmatic levers: skip or cap the
   expensive passes, or take the fill off the owner lock.

## Ruled out (do not re-run)

- **`RECOMP_NO_COMBINERS` avoiding the fatal path.** One run with it held and the next went fatal
  (`…204648-520-title012-hold-a`); it was run-to-run variation around a wall-clock timeout (L56).

- **Vblank delivery alone deciding the hold.** R1 and R2 (~1.3 pulses/s) hold like B (~23 /s);
  only R3 passed (TR §26.4).
- **A toolkit bisect against `5d6ebbd` on today's game tree.** It does not build; the tree's tests
  use newer APIs.
- **The settled results of the retired plan** ("Settled — do not re-run" in
  `git show d6b8336:plan-jsrf-bare-minimum.md`): the ADX worker deaths, `budget_exhausted` Case A, the
  black interval, the method admissions, and the eliminated premises.

## Working loop

1. Pick the top open line; state its cheapest decisive experiment and the reading that would advance
   it, before running.
2. Code on the Mac, copy it to Windows with `scp`, build and test there (`just build`, `just ctest`,
   `just check`). A code step's test is written by a different worker.
3. Run with `just title-run <label> 600` (or longer), plus the line's switches. Read `result.json`, the
   log and `gpu-report.md`, never the exit code alone.
4. Record what moved here and in TR. Each new shortcut gets a ledger line. Commit and push from Windows
   at every real step forward (toolkit first), with `PUSHED_TO:` receipts in the history.
5. Re-rank and repeat.

**Stop and ask the owner** only for:
- a force push or rewritten history;
- reverting the v0.13.1 merge;
- anything touching original assets or saves;
- credentials or the ssh link failing twice;
- a choice between two routes that changes what "the title screen" means, such as drawing a fake
  title rather than reaching the real one.

## Carried over from the retired plan

- **Evidence.** `MEASURED` means inspected with an identity and procedure; `INFERRED` is a hypothesis.
  A run that does not reach a changed address is NOT EXERCISED. Values copied into a record by hand are
  re-checked by a second reader (W5). Original assets and saves stay unchanged.
- **Runs.** Same-binary runs take different paths. Compare executables only after checking
  `exe_sha256`. A run without the four title-path switches is not comparable. A run must pass ~2430
  presents to say anything about the title.
- **Owner decisions in force.**
  - Lifted code (`src/recomp/gen/`, `recovered.c`) is untracked and rebuilt locally.
  - Ordinary runs need 15 GB free.
  - Commit and push policy and the public-repository rules are in `AGENTS.md`.
  - Performance is not a revert criterion (2026-10-09).
- **Machines.** The Mac has no game assets. Code is written there, copied to the Windows box
  (`ssh logic@192.168.0.122`) and built, run, committed and pushed from Windows, never from the Mac.
