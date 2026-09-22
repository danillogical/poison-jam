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
