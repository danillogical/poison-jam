# JSRF lessons learned

What went wrong on this port, the rule each incident produced, and where that rule lives now.
The raw records (reviews, packets, rulings, the full history and technical record) are in git at `9d5257f`; "TR §n" means that commit's `docs/jsrf-technical-record.md`.
"Rule: here only" marks a lesson nothing else carries; toolkit lessons are in `xboxrecomp/docs/technical/lessons-learned.md` and are not repeated.

## Evidence and measurement

- **Read the instructions past the failing call before naming a cause.**
  The "unchecked 571 MB allocation → NULL call" premise stood ~5 days; the bytes showed `test eax,eax; jl` handling it, and the NULL call was another pass (2026-09-27).
  Rule: `scripts/qualify-premise.py` question 1.
- **An instrument whose positive control never fired has observed nothing, and zero hits need coverage.**
  The A2h DR0 write watch ran ~15 runs over three packets before anyone saw that no `#DB` had ever arrived, even for a known write (2026-09-28). A body with no trace hook logs nothing whether or not it runs.
  Rule: `scripts/check-instrument-controls.py` (`just instrument-check`); `docs/agent-workflow.md` §6.4.
- **A text match in a log is not an execution witness.**
  Run f25 was credited with `0x00032610`; all 14 matches were its own directory name in `[SAVE]` paths. A `returned; ABI verified` line can also precede an `ABI FAILURE` for the same address.
  Rule: `scripts/check-run-exercised.py`; `scripts/check-stop-chain.py` with `config/stop-chain.json`.
- **No hand counts or hand-copied values in a decision.**
  Two hand counts (1177, 1909) of one quantity; a little-endian dword misread as reversed bytes; "eight missing methods" that were 39, because only packet starts were counted.
  Rule: `scripts/cite.py`; `scripts/check-transcribed-values.py` (a second reader re-checks each row).
- **Check every proof for circularity.**
  A "temporal provenance" proof compared one dump's bytes with themselves (TR §22.1); a depth rule assumed the span was right to conclude its value was wrong (TR §10).
  Rule: here only.
- **Count with counters, not log lines.**
  `[FBPRESENT]` is sampled, `[PFIFO] submit` stops after 64 walks, `[WAIT]` after 256; a "163 vs 2410 regression" was a units error.
  Rule: read `presents=` and the `gpu-report` counters; toolkit totals now count past the print caps (TR §25.8).
- **Align clocks before comparing timestamps.**
  QPC, `GetTickCount64` (`[CHECKPOINT]`) and first-present-relative `[FBPRESENT] t=` have different zeros; ignoring that produced "9.6 s before the wrap" (TR §24.2).
  Rule: the toolkit's one-per-process `nv2a_mono_clock.h` epoch, pinned by a two-file test (TR §26.1).
- **Measure host costs; do not estimate them.**
  Per-word `VirtualQuery` was estimated at 1–3 µs and measured at 185–378 µs; under the owner lock it starved vblank (TR §25.5).
  Rule: toolkit `5d6ebbd` walk-scoped cache and its `nv2a_read_guard` test; the habit is here only.
- **Validate a detector or decoder on a known case before trusting its output.**
  The "ADX worker deaths" were the main thread's print timing (TR §25.1); the xemu gdbstub tool decoded the i386 `g` packet as x86-64, so every register it printed was wrong.
  Rule: `tests/test_xemu_gdbstub.py`; otherwise here only.

## Recompilation and generated code

- **The alias fold deletes real functions; anything reached from data must be recovered explicitly.**
  It folded CRT initializers, 1120 vtable methods and table-dispatched functions into neighbours: zero-argument calls into three-argument bodies, self-recursion, and the `0x37608` image copy that overwrote the thunk table (TR §5, §9).
  Rule: entries in `config/recovered-functions.json`; ledger D5; census `scripts/scan-alias-redirects.py`, whose docstring lists the three cheap discriminators that saturate.
- **Span membership is not ownership.**
  Spans cut at false `gap_prologue` entries emitted shared epilogues and switch arms as tail calls into traps; the span checker missed all three because folded aliases "resolve" (A2e–A2g, 2026-09-22).
  Rule: `scripts/check-span-exits.py` (`genuine_starts()`, `--selfcheck` at the pre-fix spans).
- **A span must end where its own body ends.**
  `0x7DAE0` ended at "the next function entry" and still hid `0x7DBD0` (TR §14); spans that ran into their own jump table or past `ret` made `recover-functions.py` abort with `Incomplete translation`.
  Rule: `scripts/check-hidden-entries.py`, `scripts/check-entry-extents.py`; fix the entry, never weaken the abort.
- **`stack_args` is the `ret N` operand; at a `ret N` reached at depth 0 it must equal N.**
  20 entries declared 0 over bodies ending `ret 4`, each a certain abort on first call, decidable without a run. Call models that guessed depths gave −52 and −7120 (TR §10, §12).
  Rule: `scripts/check-stack-depth.py` (gates depth 0 only; an unresolved callee is UNKNOWN, never zero).
- **Enumerate by value with the runtime's own disassembler, never by spelling or raw bytes.**
  The lifter writes one VA as hex and as signed decimal (a grep found 10 of 28 `PIO_FREE` sites); throwaway capstone desynced at data islands; a raw 4-byte search called 3501 of 3508 addresses "referenced".
  Rule: `AGENTS.md` "Generated-source rules"; `scripts/enumerate-accesses.py`, `scripts/inspect-jsrf.py disasm`, `scripts/check-table-targets.py`.
- **Hand edits inside generated files vanish on regeneration.**
  A full pass reverted the ABI additions and six watch hooks; a lost observation hook reads as "the guest never wrote there" (TR §2).
  Rule: `config/generated-patches.json` via `scripts/patch-generated.py`; `scripts/apply-a4b2-hooks.py` anchors on labels.
- **A patch that removes a table row must fix the table's count.**
  A dropped tuple left the size at 8928 for 8927 rows; the host crashed before `guest_entry` with every gate green, and re-running the archived previous binary localised it in one step (TR §16).
  Rule: `scripts/check-dispatch-table.py`; the toolkit derives the count with `sizeof` (`2cee914`); compare against archived binaries (here only).
- **Record what produced the generated tree, and hash what ships.**
  `--write` once erased 34 provenance amendments (TR §10); `NDEBUG` compiled out a vendored DSP assert (TR §4); manifests hash Windows CRLF files, and a Mac edit turned the gate red for 73 commits.
  Rule: `scripts/check-generation-provenance.py` in `just check` and CTest; amend the manifest on Windows (here only).
- **Ancestry bounds a commit, not an artifact.**
  Lifted code is untracked, so only a run's own `source.zip` says what it ran; all 44 `[ALIAS-ICALL]` firings predated the recovery in their own build (TR §18).
  Rule: `scripts/check-run-exercised.py` (archived `(end, stack_args)` must equal the current one).

## Runtime and GPU model

- **A write-1-to-clear register cannot be asserted by OR-ing into it.**
  The vblank tick OR'd into `PCRTC_INTR_0`/`PMC_INTR_0` and so cleared the pending bits; the ISR queued 968 DPCs and the producer never ran (2026-09-22).
  Rule: toolkit `nv2a_vblank_pulse` with the guest's own W1C acknowledgement; the `jsrf_nv2a_registers` fixture.
- **A gate must name the class of writes it is for; a set switch can be inert.**
  Claiming the MMIO owner cleared the flag the ack thread's guest-memory mirrors and `RECOMP_NV2A_TRACE` sat behind, so both went silent (2026-10-01).
  Rule: `docs/jsrf-run-profiles.md` "Two traps".
- **Guest `rep movs`/`rep stos` run as host `memcpy`/`memset`.**
  One faulted inside `VCRUNTIME140` AVX code the VEH decoder could not read; "VCRUNTIME wrote it" usually means guest code did (A2d, TR §7).
  Rule: toolkit `recomp_range_is_mmio` guard (`484887b`); attribute writes by native return address into `jsrf_recomp.exe`.
- **Admit NV2A methods from a runtime witness, not a decode.**
  The decoder ignores the non-incrementing bit, and one stale table entry (`0x1810`) held presents at 888 (TR §22).
  Rule: `config/nv2a-runtime-witnessed-methods.json` and `scripts/gen-nv2a-method-inventory.py --witness=` (fails closed).
- **Host clock arithmetic overflows inside a long run.**
  `count * 1e9` in signed 64 bits wrapped at 922 s and 1844 s and stopped vblank (TR §24.2).
  Rule: toolkit `nv2a_qpc_to_ns` with `host_clock_wrap_test`.
- **Read the draw surface at its contiguous-window VA, from a frozen dump.**
  `memory <run> 0x00084000` returns XBE `.text`; the surface is `0x80084000`, and `RECOMP_FB_VA` changes what is presented (TR §23.2).
  Rule: `AGENTS.md` "Dump integrity" (actual guest VA); the surface detail here only.
- **Flip-time hashes cannot say what a batch read.**
  "A faithful copy of a black source" was retracted: every hash is taken after all of the frame's batches ran (TR §23.7).
  Rule: here only.

- **A host lock that its holder re-takes in a loop starves guest threads on Windows.**
  The APU frame thread held its mutex for its whole loop and was behind schedule, so a guest audio thread waited minutes and blocked the main thread through a guest critical section (TR §26.6).
  Rule: long-running host loops hand the lock to announced waiters (`apu_lock_handoff.h`); look for host-lock waits in frozen stacks before guessing at a guest-side wait.

## Runs and comparisons

- **The same binary takes different paths; compare like with like.**
  f25 and f26 share an `exe_sha256` and reached 960 and 193 presents. Without `RECOMP_APU_TRAP=1` the guest spins at DSP init, and once a missing switch was misread as a TTD effect.
  Rule: plan "Carried over" (`exe_sha256`, the four switches `just title-run` sets); `scripts/check-run-exercised.py`.
- **A frame-hash blacklist is not a screen criterion.**
  The disclaimer renders in four hashes and a blacklisted one was the Dolby card, so a disclaimer run met M15's letter. A "boot loop" came from mixing two runs' frames (TR §23).
  Rule: plan "Objective: M15" (by content, by eye, reproduced).
- **The SEGA logo was the first-boot cache fill, not a stall.**
  A fresh save root refills ~68 MB of HDD cache at about one file a second, which a 300 s run cannot see past (2026-10-01).
  Rule: the runner's empty disposable save root; 600 s as the practical unit (plan).
- **`diagnostic_deadline` and `normal_exit` are not progress.**
  A strict `normal_exit` at 1.9 s was the title restarting itself through `HalReturnToFirmware(2)`; a failed audio init does exactly that (2026-09-22).
  Rule: `AGENTS.md` "Evidence rules"; `docs/jsrf-run-profiles.md` "Two traps"; read `result.json`.
- **A dump can be structurally fine with displaced contents.**
  460 of 545 XBE-backed pages of the A2h dump read as `original[VA+0x37608]`, and the guest really had read the displaced value.
  Rule: `scripts/check-dump-mapping.py` before any dump claim (`AGENTS.md` "Dump integrity").

## Tooling and environment

- **A clean merge hunk can ship a defect or change what evidence means.**
  v0.11 produced a duplicate `case 138`; v0.13.1 clean hunks added a recursive heap lock and shrank `RECOMP_APU_TRAP`'s range.
  Rule: `scripts/check-merge-structure.py`; `docs/jsrf-run-profiles.md` "Upstream merges never silently change admitted evidence semantics".
- **Copies drift; big always-read files rot.**
  `AGENTS.md` kept naming two removed overrides; the plan reached 170 KB read at every start and was cut twice.
  Rule: `scripts/check-override-drift.py`, `scripts/check-agent-docs.py` (64 KB budget); one authority per fact.
- **A gate outside `just check` and CTest rots.**
  `scripts/test-harness.py` broke unnoticed from `009f624`; the provenance test was not in CTest, so `just test` stayed green over a red gate.
  Rule: register every gate in `justfile` `check` or `CMakeLists.txt`.
- **A test must be able to fail, and must not damage the tree.**
  `host_clock_wrap_test` duplicated the formula and passed with the fix reverted; guard tests run against a reverted generator overwrote its committed tables twice.
  Rule: `docs/agent-workflow.md` §6.6 (show it failing by mutation); `tests/test_gen_nv2a_method_inventory_guard.py`'s redirect preflight.
- **Disk and TTD fail quietly.**
  A full disk truncates archives into what looks like a guest hang; TTD (~170 MB/s) stops at its size cap, and an 8 GB cut was read as "TTD cannot see the write"; an orphaned `cdb` kept a deleted 16 GB trace allocated.
  Rule: `scripts/check-disk-gate.py` (15 GB for runs); `docs/jsrf-run-profiles.md` "TTD trace query"; look for `cdb.exe` before trusting free space (here only).
- **Two machines, and their shell traps.**
  The Mac has no XBE: code there, `scp`, then build, test, commit and push on Windows; over ssh, push with `gh` as the credential helper. Heredocs are rejected; backticks in `bash -c` inside `python -c` vanish.
  Rule: plan "Machines"; `AGENTS.md` "Host and sandbox constraints"; the `gh` and backtick details here only.
- **Repository safety.**
  `.git/objects` vanished on 2026-09-21 and history restarts at `e336a1c`; a pull that untracks `src/recomp/` deletes it; published mistakes get an erratum, not an amend.
  Rule: `AGENTS.md` commit and push policy and "Rebuilding the lifted code".

## Process: what helped, what was ceremony

- **The cheapest decisive experiment first; search forks before chasing.**
  A2h's packets, DR watches and TTD never named the thunk-table writer; one `RECOMP_RDATA_GUARD=1` run did, and it was an alias fold (2026-09-30).
  Rule: plan "Working loop"; `qualify-premise.py` question 5; ledger L32.
- **Fix one stop per run, then census the class when it repeats.**
  Three like stops in a row led to the stack-depth and hidden-entry detectors, which found 24 and 50 defects without runs (TR §10, §14).
  Rule: plan "Lines of attack" item 4; the checkers in `just check`.
- **Put a ceiling on a line of attack.**
  The A2h NULL line ran five packets, each fix exposing the next caveat, until it was dropped as "explanation, not decidability" (2026-09-28).
  Rule: plan (the orchestrator drops lines); `scripts/check-horizon-ledger.py`.
- **One independent review, after the work is finished.**
  Review caught the overclaims of TR §19, §20 and §24.2, each written while fixing the last; but one 2026-10-03 turn spent 75 % of its time in a review loop.
  Rule: `docs/agent-workflow.md` §4 "Turn Reviewer" (no automatic second review); plan "Working loop" (a different worker writes a code step's test).
- **Ceremony that cost more than it caught (packet lifecycle retired 2026-10-03).**
  P0.2's review contract reached interface revision 14; in A3a and P0.2 each repair introduced the next defect; a parser for reviewer prose met an unbounded qualifier vocabulary; 31 of 36 session verification records were never cited.
  Rule: `scripts/check-record-hygiene.py` keeps review records to dispositions and findings.
