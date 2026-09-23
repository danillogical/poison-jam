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

## 2026-09-22 10:13–11:25 PDT (third recorded run)

Outcome: the DirectSound stop is fully root-caused with a measured mechanism, and
the APU — which the toolkit has always shipped and nothing has ever called — is
now instantiated and routed. The remaining work is named as a **GP SGE engine in
the APU model**, replacing the previous guess of "an APU MMIO hook".

Done:
- Corrected the previous run's register identification: `0xFE8020D4` is
  `NV_PAPU_GPSMAXSGE` (an SGE **count**), not a DMA base. The base is `GPSADDR`
  (`0xFE802040`), written by `sub_001A52F7` (the DMA kick).
- Identified the waited-on object by arithmetic rather than assumption:
  `[0x803C0804] + [0x803C080C]` = 3512 + 3358, times 4 plus `0x818`, lands on
  `0x803C7370`, whose minidump value is the loop count the code reads next. So
  `ebx = 0x803C0000`, the 48K contiguous allocation, whose base and size are
  published at guest `0x1BA858` / `0x1BA860` and whose SGE table is the next
  allocation at `0x803CC000` (`0x1BA868` / `0x1BA870`).
- Proved exhaustively that no guest code can clear the pending word: `+0x810`
  occurs in exactly three sites in the entire recompiled title (the waiter
  `0x1A1769`, the stop `0x1A1747`, and the DSP stop `0x1A1F5D`), and `[0x1BA858]`
  in exactly one. The acknowledgement must come from the APU.
- Wired the APU: new toolkit header `apu_mmio_hook.h` declaring
  `apu_hook_handle_mmio` and `g_apu_state`; `main.c` instantiates the APU and the
  VEH routes `0xFE800000..0xFE880000` to it, both gated on `RECOMP_APU_TRAP`.
- Measured the traffic (344 decoded accesses, `logs/runs/20260922-104131-686-apu-trace`):
  the title programs VPSGEADDR/VPSLADDR, GPFADDR, `GPSADDR = 0x803CC000`,
  EPSADDR, EPFADDR, then `GPSMAXSGE = 206` as its last APU access before the
  spin, after a 12-page `MmGetPhysicalAddress` walk filling entries 194..205. The
  SGE table is 8-byte `{physical address, flags}` entries, zero-terminated, with
  entry 0 = the buffer that carries the pending word.
- Reverted a wrong inference of my own in the same session: adding
  `NV_PAPU_XGSCNT_DS` (0x2010) to `mcpx_apu_read` was dead code, because
  `mcpx_apu_mmio_read` routes `0x20000..0x2FFFF` to `mcpx_apu_vp_read`, and
  `0xFE820010` reads as `NV1BA0_PIO_FREE` (VP method 0x10), which is what the
  title is satisfied by. Left no trace in the code.
- Build clean, CTest 11/11. Commits: toolkit `cf03f46`; game `bd904c2` (toolkit
  revision recorded in its message).

Traps recorded:
- **A kernel-log budget makes a live run look frozen.** Default 200 vs 100000
  changed the same 30 s run from 1422 to 5116 log lines; never conclude "hang"
  from a truncated log.
- **Symbol+offset in a stack line is a mislabel past a body's end; the
  `recomp_XXXX.c:NNNN` file:line on the same line is exact.** That file:line is
  what located `0x001A18D0`.
- `named_frames` and `native_threads` are sampled at the deadline and are not
  before/after metrics. Compare log lines, and only under the same options and
  the same budget.

Next run should start at: give `apu_dsp.c` a GP SGE engine — on the `GPSMAXSGE`
write, walk that many entries from `GPSADDR`, resolve each physical address
through the APU's `ram_ptr`, complete the transfer, and write 0 to the pending
word in the first page (`buffer+0x810`). Add an APU probe test rather than
weakening `jsrf_nv2a_registers`.

## 2026-09-22 10:57-12:50 PDT (fourth recorded run)

Outcome: the indirect-call trap frontier is **cleared** and the NV2A pushbuffer
model now **drains the whole ring the title submits**. The blocker moved from
"the guest traps" to "three guest threads block on one in-place KEVENT that
nothing signals". Two commits in each repo, CTest 11/11 throughout.

Done:
- Confirmed the previous session's four span repairs **by execution**: the log
  shows `0x0007BE30`, `0x0007BDD0`, `0x00024700` and the new `0x001185B0` all
  logging `returned; ABI verified`. No `[ICALL] Failed to resolve VA` anywhere in
  the final run.
- Fixed the last trap `0x001185B0`: a tail call **below** the span start of
  `0x00118610`. `check-span-exits.py` was looking only forward; it is now
  bidirectional (381 -> 294 findings).
- Cancelled the previous run's stated next packet. New
  `scripts/resolution_starts.py` reads the three tables the binary actually
  consults; the "six uncovered table targets plus `0x00011C90`" were a detector
  artefact -- all 64 entries of the class table at `0x0020D2B8` resolve.
- Drained the ring. `scripts/gen-nv2a-method-inventory.py` had hardcoded ring
  ends and a packet classifier that did not match `nv2a_submit_pending`; it is now
  a word-for-word port. Regenerated the method table (NV097 250 -> 362 methods,
  toolkit `18a0837`) and extended `jsrf_nv2a_registers` with both an acceptance
  case (0x1BC8/0x1BCC) and a rejection case (0x1BD0).
- Measured: `[PFIFO] submit #1 diag=ok get=00002764 put=00002764` -- the ring
  drains at a ring 58% longer than the plan recorded.
- Named the new frontier from the run's own thread dump: three guest threads in
  `xbox_KeWaitInplaceEvent` on the in-guest KEVENT at `0x0019D630`, and nothing
  signals it (ordinal 145 `KeSetEvent` 0 times, no data-table or immediate
  reference, the field pair occurring at exactly one site).
- Commits: toolkit `18a0837`; game `17522bf` and `6060e3f`.

Next run should start at: **identify what the structure at `0x0019B200`
describes.** Its `+0x211C` array is indexed in 8-byte records and
`sub_0018CE80(i, out)` copies 24 bytes out of entry `i`, so it is a work-item
queue; naming its subsystem names the producer of the missing signal. Do not
re-derive the `+0x242C` callback -- it is a trace hook over the counter at
`0x265174`, already closed.

Notes for future runs: the APU GP SGE engine from the third run is still open but
is not on the critical path while `RECOMP_APU_DSP_ACK=0x803C0810` is set. Use
`RECOMP_KERNEL_LOG_BUDGET=100000` or a live run looks frozen. `git commit -F
<file>` is needed for multi-line messages (heredocs are blocked), and the path in
`-F` must be a Windows path, not `/c/...`.

## 2026-09-22 15:51–16:35 PDT (fifth recorded run)

Outcome: packet A2 delivered and measured. The GPU interrupt line now exists and
the frame producer runs; the run's stop moved from "three threads wait forever"
to a newly reachable ABI failure in code the baseline never entered.

Done:
- Root-caused the missing transition: the NV2A had no interrupt source at all.
  `kernel_vblank_tick` asserted vblank by OR-ing into `NV_PCRTC_INTR_0` and
  `NV_PMC_INTR_0`, both write-1-to-clear, so it cleared pending bits and set
  none. Confirmed statically and from the published model snapshot.
- Toolkit `7cfbe55` + `008001f`: `nv2a_vblank_pulse`, `nv2a_display_frame_ns`,
  `nv2a_set_irq_sink` (replacing two no-op `pci_irq_*` stubs), display clock in
  the standalone service thread, `NV_PCRTC_INTR_EN_0` published. Kernel: the
  synthetic `kernel_vblank_tick` is deleted and `xbox_Nv2aAttachIrqLine` delivers
  the line on the timer thread. `RECOMP_VBLANK` removed.
- Game `225bb6b`: attach the line; deterministic interrupt-controller fixture in
  `jsrf_nv2a_registers` (assertion, both masks, guest W1C, no spurious repeat).
- Measured strict: `[RECOVERED] 0x00193D90 returned; ABI verified`, waiters on
  `0x0019D630` 3 -> 2. `logs/runs/20260922-160535-643-a2-irq-line/`.
- Advisor consulted (resume `agent-d3294b58`) before implementing; its cheapest
  falsifier and its synthetic/legitimate discriminator both adopted and recorded.
- CTest 11/11 game, 1/1 toolkit. Report CURRENT STATE rewritten; plan A2 marked
  delivered-not-accepted with a new A2b packet.

Next run should start at: A2b — root-cause the ABI failure at `0x00048190`
(declared body `0x00048190..0x0004A6F0`, `stack_args: 4`, esp 0x20 low with all
callee-saved registers clobbered). Second failure in the diagnostic set is
`0x00025310`.

Traps recorded:
- `git commit -F /tmp/msg.txt` fails with "could not read log file"; the path
  must be a Windows path. A message file written inside the repo gets committed
  by `git add -A` — remove it before committing, or place it outside.
- `nv2a_irq_line_asserted` must keep the upstream `pending && enabled`
  condition: `NV_PMC_INTR_EN_0` is a two-bit master enable, not a per-source
  mask. A per-source AND against it silently kills delivery.

## 2026-09-22 16:10-16:35 PDT (sixth recorded run)

Outcome: A2b delivered and measured -- the ABI failure at `0x00048190` is gone.
Its root cause was a span cut short at a disassembler phantom, and the same
failure mode then appeared twice more with a different trigger (A2c, delivered
for two candidates). The stop is now a new class: an APU MMIO decode failure.

Done:
- A2b: `0x00048190`'s declared end `0x00048305` is a `tail_jump_alias` phantom
  from `_build_alias_entries`, not a function start. Real body ends
  `pop edi/esi/ebp/ebx; add esp,8; ret 0xc` at `0x0004838C`. Corrected to end
  `0x00048390`. Neighbours `0x00048390` and `0x00048570` corrected the same way.
  Game `caef022`.
- **`stack_args` is the `ret N` operand**, not an argument count. Verified
  against `0x0004A6C0` (declared 4, epilogue `ret 4`). A wrong value is invisible
  in the span and appears only as an `esp` delta 4 high.
- A2c: functions reached *only* through a data table are never registered by
  `tools/disasm`, so the previous entry's end runs over them.
  `scripts/check-table-targets.py` enumerates 134 candidates. Recovered
  `0x00173DB0` (stack_args 20) and `0x00175300` (stack_args 8). Game `50cc6a8`.
- Measured: `0x00048190`, `0x00173DB0` and `0x00175300` all log
  `returned; ABI verified`; no ABI failure anywhere. Run duration 3.30 -> 4.13 s.
  `0x00025310` did not recur, confirming it was collateral.
- CTest 11/11 game, 1/1 toolkit. Toolkit unchanged at `008001f`.

Next run should start at: **A2d** -- the APU MMIO decode failure. The run stops on
`[APU] MMIO decode fail at RIP=00007FFA628FCCA7 offset=0x30200: C5 FE 6F 02 C4 A1`
then `code=0xC0000005 fault=0xFE840200 (read)`. The faulting RIP is in a system
DLL and the bytes are a VEX/AVX instruction (`vmovdqu xmm0,[edx]`), so a host
routine faulted on the APU aperture and the VEH decoder (legacy MOV/CMP/OR/AND/LEA
only) could not emulate it. Do NOT map the aperture readable -- that turns a named
failure into a silent wrong read. Then A2c's generator for the remaining 132
pointer-table candidates.

Traps recorded:
- capstone in a throwaway script desyncs on bytes that
  `scripts/inspect-jsrf.py disasm` decodes cleanly. The repo tool is the authority
  for evidence; do not build a span detector on raw capstone.
- The detector for a cut-short span is the *emitted code*: a generated
  `body_XXXXXXXX` with no `return` statement at all.
  `scripts/check-entry-extents.py` sees only an informational `NO-TERMINATOR`.
- `0x00048690` is a pointer-table target with no span and `0x00048570`'s corrected
  end now stops just short of it: a latent named trap, not a silent wrong body.

## 2026-09-22 17:29–17:56 PDT (fourth recorded run)

Outcome: **A2d.1 delivered and measured.** The APU decode failure was not an APU
problem at all — it was a guest `rep movs` lowered to a host `memcpy`. The run now
walks past it and stops on a new *named* failure.

Done:
- Traced the fault to the instruction, not the symptom: guest 0x001A1B41
  `rep movsd`, src ESI 0xFE830200 (APU GP window), dst a RAM buffer, 280 bytes,
  ranges provably non-overlapping so the lifter's `memcpy` fast path was taken.
  The VEH can only decode the faulting *guest* instruction, so a library copy
  raises the fault at its own RIP in its own VEX encoding and escapes.
- Reconciled every register in the fault dump against the 4-argument call site
  (0x001A2079 pushes eax, 0, esi+0x70, 0x118; the callee's `ret 0x10` confirms).
  Also resolved an apparent address contradiction: guest 0xFE830200 and host
  fault 0xFE840200 are the same address, guest RAM base is at host +0x10000.
- Fix: lifter guards the `movs`/`stosb` block forms with `recomp_range_is_mmio`
  on both ends; `recomp_range_is_mmio` + the two window constants live in
  `templates/runtime/recomp_types.h`; the APU model now **names** a read of an
  unimplemented block (GP 0x30000 / EP 0x50000) instead of returning 0 silently.
- Commits: toolkit `484887b`, game `5e756a7` (A2d.1) and `a82f679` (CURRENT STATE).
  CTest 11/11 game, 1/1 toolkit, 12/12 Python in the touched modules.
- Measured deltas: decode failure gone, no 0xC0000005, named frames 109 -> 147,
  native threads 11 -> 17, 4.13 s -> 4.50 s, and DSP doorbell command 0x02
  acknowledged after 0x03. Evidence
  `logs/runs/20260922-174141-780-a2d-movs-mmio/` (toolkit `484887b`).

Next run should start at: **A2e — the unresolved indirect call to `0x000252B5`**.
That VA is an interior shared epilogue (`pop edi; pop ebx; pop esi; ret`) inside
**no** database entry; the enclosing region `0x0002524A..0x000252E0` is
unclaimed. The ICALL stack has `[3] 0x00025310` on it (a recovered COM vtable
method), so read that recovered body and find the indirect call site. It is NOT
among the 132 pointer-table candidates — a claim about the detector too. A2d.2
(that 132-candidate generator) is still open.

Traps recorded (both new, both measured):
- **The documented full-translation invocation does NOT reproduce the committed
  tree**: a 6,695-line diff (2,989 additive declarations in `recomp_funcs.h`,
  5,978 in `recomp_dispatch.c`). Cause: `functions.json` now has 8,437 entries vs
  the committed header's 5,653 declarations. **Revert it and forward-port a
  lifter change textually into `src/recomp/gen/*.c`** (463 movs + 95 stosb sites
  here), or the run measures the regeneration instead of the change.
  `recover-functions.py` DOES reproduce `recovered.c` exactly.
- **The translation pass overwrites project-local edits in
  `src/recomp/gen/recomp_types.h`** (the exact-delta ABI instrumentation:
  `recomp_delta_ok`, `jsrf_trace_delta_mismatch`, `jsrf_trace_seq`, and the
  `RECOMP_ABI_CALL` body). Re-apply after any header sync.
- Another session was writing `AGENTS.md` and `plan-jsrf-bare-minimum.md` at
  17:41 during this run. They are left uncommitted on purpose — never
  `git add -A` in this repo.
- `RECOMP_APU_TRAP=1` matters for the GP-window read: without it the aperture is
  plain mapped memory, the element-wise path reads it silently, and the new
  diagnostic does not fire.
