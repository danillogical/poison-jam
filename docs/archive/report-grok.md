> **RETIRED 2026-09-22. Archived for history only. Do not follow this file.**
>
> The model roster and delegation policy now live in **\docs/agent-workflow.md\**,
> which supports exactly two harnesses (Codex and DeepSeek/DSH). This document
> describes a superseded harness and names models that are retired
> (Grok, \gpt-5.6-sol\, \gpt-5.6-luna\, \gpt-5.6-terra\, \gpt-5.5\,
> \workbuddy-ai/gpt-*\). It is kept because its harness mechanics may still
> explain an old log or commit message. Nothing here is a current instruction.

# Grok session decisions and issues

Standing rule: keep going; record choices here instead of stopping to ask.

## 2026-09-21 — Continue past event-bridge review wait

**Decision: treat the last reviewer acceptance criteria as implemented and start `0x00194ADD`.** The other agent said they would accept after per-waiter payment, Type-1 pulse matching that accounting, and two-set / one-pulse tests with a third wait timeout. That is in `kernel_sync.c` and `jsrf_inplace_event_bridge` (1280 checks). Waiting for another review round would stall 11c. `0x00197C50` and `0x00194780` stay fatal. `0x00194ADD` is recovered only from original-XBE evidence, not stubbed.

**Issue:** Overlapping `run-jsrf.py` with the 19-probe harness previously produced `HalReturnToFirmware` / `Partition5` `CreateFileW` stalls. Solo captures after the harness finishes. Do not launch both at once.

## 2026-09-21 — Recover `0x00194ADD` and `0x00194780`

**Decision: recover both from original XBE; keep `0x00197C50` fatal.** `OUT DX,AL` is now lifted to `xbox_outb` (GPIO 0x80C0 is logged, not invented as electrical success). `0x00194780` calls trapped `0x00197C50` after PMC/PTIMER/FIFO setup. Focused 11c1 seams that call and the time imports. Expected next ordinary stop is `0x00197C50`.

**Issue:** `xbox_outb` is a record/log stub. GPIO[0] electrical effect remains a hypothesis from 11a.

**Result:** Focused 11c1 PASS 2048. Ordinary run `logs/runs/20260921-013349-836-gpu-setup-94add/` fails at `0x00197C50` (`0xE0424943`), dump_ok. `0x00194ADD` is no longer the first missing target.

## 2026-09-21 — Long continuation

**Decision: recover `0x00197C50`.** Original body is PGRAPH debug/channel setup plus recovered `0x00196C0B` and `0x00197AAC`. The old cycle is now contracted. Idle STATUS makes polling a no-op at setup. End exclusive `0x00197EE3`. Focused 11c1 PASS 2054.

## 2026-09-21 — Two-hour continuation past `0x00197C50`

**Decision: keep recovering original-XBE GPU-setup leaves until the first pushbuffer kick.** `0x00194ADD` returned in `logs/runs/20260921-014350-238-gpu-setup-97c50/`. WBINVD at `0x0019243A` ran. `0x001912A0` published the framebuffer and returned. Ordinary stop is now `0x001918E0`.

Recovered this stretch: `0x001948B9`, `0x00194913`, `0x00196E92`, `0x00194573`, `0x0019701F`, `0x001945D6`, `0x00197058`, `0x001970CD`, `0x001949E2`, `0x001912A0`. Focused 11c1 PASS 2197. NV2A contracts PASS 228 including PFB WBC read-idle after a flush write.

**Decision: leave `0x001918E0` fatal.** It writes NV097 methods into the ring then tail-jumps to `0x001917F0`, which calls wrap helper `0x001916B0` → `0x00191530` (RET 8, waits on DMA GET) and `0x00190240`. Recovering it without that kick/GET contract can spin. Keep `0x00193F70` and `0x0018E120` fatal.

**Issue:** NV_PRAMIN is a NULL block in `nv2a_core.c`. `0x00194913` / `0x001949E2` write instance objects through `device+0x700000`. Those stores currently miss the detached RAMIN buffer. Dest objects live in guest RAM, so setup still advanced. 11b4b3 still needs PRAMIN backing aliased to claimed instance memory.

## 2026-09-21 — PRAMIN backing accepted

**Decision: implement packet B without recovering the kick chain.** PRAMIN is
unbound until the validated GPU-instance claim, then aliases that exact
top-of-contiguous range for both MMIO and `nv_dma_load`. Claim/init publication
uses the MMIO owner lock. Focused NV2A contracts PASS 269; Release CTest 11/11;
fresh Terra review is clean after two Luna passes and a Sol concurrency repair.

Solo run `logs/runs/20260921-100901-149-pramin-backing/` still stops at fatal
`0x001918E0` with dump/GPU analysis successful. Memory `0x83FFB000` contains
the game-created instance table.

## 2026-09-21 — RAMHT handle-to-class lookup

**Decision: production SET_OBJECT uses the captured RAMHT, and the fixture seam stays test-only.** `0x001945D6` hashes a handle with two 11-bit XOR-folds (`AND 0x7FF` for the game's 4K table). `0x0019701F` writes an 8-byte pair at PRAMIN+hash*8: word0=handle, word1=`NV_RAMHT_STATUS|engine|tag`. The class is the low 8 bits of the 16-byte RAMIN object at `tag<<4`, matching handle `0xD` → tag `0x49C` → class `0x97` at `0x83FFF9C0`. Channel id is not mixed in; `0x001945D6` does not. Hashes that fall outside the programmed RAMHT size are invalid handles, not wrapped.

Failed streams roll GET, bindings, and clip state back. Missing, zero, invalid-bit, mismatched, out-of-range, and class-zero handles report `invalid_handle`. Fixture execution remains required for the isolated 11b4b2 tests. `0x001918E0` and the kick chain were not recovered. GET/PUT advancement rules are unchanged.

Focused NV2A PASS 304. Release CTest 11/11. Solo `logs/runs/20260921-103827-656-ramht-lookup/` still dies at `0x001918E0`, dump_ok, GET=PUT=`0x1000`.

**Decision: accept the independent review.** No blocking defects. Known limits, not blockers: the 11-bit fold follows the RAMHT SIZE field while `0x001945D6` is hardcoded 11-bit; SEARCH_128 is a single slot because the game inserts one entry; channel id stays unmixed. Next packet is the kick/GET contract for `0x001918E0` / `0x001917F0` / `0x001916B0` / `0x00191530` / `0x00190240`. Do not recover that chain until wait, wrap, kick, stalled-GET, ABI, and completion are proved.

## 2026-09-21 — Orchestrator model

**Decision: the parent orchestrator is Grok 4.7 high.** The user switched
from Grok 4.6 xHigh for orchestration. Codex packet roles are unchanged.
`spawn_subagent` still inherits this parent, so children of this session are
Grok 4.7 high. Review stays a fresh context. Recorded in `grok-role-map.md`.

**Decision: the parent orchestrator is Grok 4.7 xhigh so it can review.**
This replaces the short-lived Grok 4.7 high parent. The same session
orchestrates and reviews. Children inherit xhigh. A spawned reviewer is a
fresh context. Recorded in `grok-role-map.md`.
