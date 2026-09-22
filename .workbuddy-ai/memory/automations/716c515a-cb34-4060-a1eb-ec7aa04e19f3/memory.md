# Automation 716c515a — "Continue the plan for an hour"

## 2026-09-22 05:27–06:00 PDT (first recorded run)

Outcome: the run's blocker moved from the NV2A fence/ring wait to DirectSound
init, and the new blocker is root-caused to the instruction that drops the error.

Done:
- Fixed the fence mirror so it publishes the device's fence **counter**
  (`device+0x30`) rather than the push buffer position, and verified it: the wait
  at `0x00191440` returns, both PFIFO submissions are `diag=ok`. Corrected two
  earlier report claims (the `reserved_opcode` reading and the credited run with
  no matching build artifact).
- Found the real reason the title exits: DirectSound polls the AC'97 codec-ready
  bit at `0xFEC00130`, times out, and the title reboots itself. Separated
  `RECOMP_AC97_READY` from `RECOMP_APU_TRAP` (the trap had no handler and made the
  codec bit crash the process).
- With the codec bit set the run reaches a divide by zero at `0x001A2BFC`, and
  traced it to a `WAVEFORMATEX` with `wFormatTag = 0` that `0x001A4BC4` silently
  rejects.
- Repaired `inspect-jsrf.py disasm` (empty listings) and the stale
  `-DRECOMP_ABI_CHECK` cache flag; extended the build script to build every
  CTest target. CTest 11/11.
- Commits: toolkit `a02780d`; game `ccc1e5d` and `638dd9e`.

Next run should start at: dump the `DSBUFFERDESC` and its `lpwfxFormat` at
`0x001A09DE`'s call site. Everything downstream is already measured.

Notes for future runs: `logs/` is gitignored, so probe scripts written there do
not survive; `scripts/inspect-jsrf.py memory <run-dir> <va> <len>` already reads
guest VAs out of a run's minidump. A `git commit -F <file>` is needed for
multi-line messages — heredocs get blocked.

## 2026-09-22 08:41–09:41 PDT (second recorded run)

Outcome: the DirectSound divide-by-zero was root-caused and fixed; a second
defect class (tail-thunk ABI declarations) was found and corrected; the frontier
moved from a crash to a clean named failure and then past it into thread startup.
Two commits, CTest 11/11.

Done:
- Root-caused the divide-by-zero the previous run left open. It was not the
  game's format: `0x0019E5CC` (vtable slot 1, the AddRef) was dispatched to an
  unrelated body by the toolkit's `ff4d442` abutting-alias fold. That left the
  simulated ESP 4 bytes low inside `0x0019EE1F`, which clobbered ESI, which made
  `0x0019F926` write the derived vtable into `0x010DD004` instead of
  `0x803E4224`, so `0x001A09DE` read `lpwfxFormat` from the wrong object and got
  tag `0x00010001`. Four entries recovered. Verified: the device format is now
  PCM / 2ch / 48000 / 16-bit and `0x001A2BFC div esi` no longer faults.
- Found and fixed the tail-thunk ABI class. A pure tail thunk inherits its
  target's cleanup, so its declared `stack_args` must equal the target's; several
  entries had none, or an impossible value (13 is not a multiple of 4). New tool
  `scripts/check-tail-thunks.py` finds them statically from the emitted bodies
  plus `recomp_abi_deltas.c`. 11 entries corrected.
- `scripts/build-jsrf.py`, because the host environment block has three case
  variants of PATH and CL.exe throws MSB6001 on the duplicate, so no shell script
  can build here.
- Recovered four thread-startup trampolines at 0x0013B180 / 0x0013B1C0 /
  0x0013B230 / 0x0013B2A0, in a 439-byte hole the disassembler never claimed.
  Measured: named frames 88 -> 109, native threads 9 -> 11.
- Commits: game `791c056` and `71aaae1`; toolkit clean at `a02780d` (recorded in
  both game commit messages).

Traps recorded:
- **Compare runs under the same environment.** The first run of the rebuilt tree
  exited 0 with 0 named frames and looked like a regression. It was not: the
  earlier run had `RECOMP_AC97_READY` set and this one did not. Unset, the title
  takes its documented `DSERR_NODRIVER` (0x88780078) exit, which is intended.
- A gap scan over `tools/disasm/output/functions.json` is not a detector for
  missing entries: 105 `.text` gaps are >= 0x80 bytes and most are padding. The
  detector is `[ICALL] Failed to resolve VA`, driven by
  `scripts/converge-manifest.py` (its build path is Python + cmake, no shell
  script, so it works on this host).

Next run should start at: add the already-measured entry for `0x00178F40` —
`start 0x00178F40`, `end 0x001791BA`, `stack_args 20` (epilogue `ret 0x14`,
matching the `sub esp,0x14` prologue). The `0xE0424943` exception is a symptom of
that unresolved call, so expect it to move once the entry is added.
