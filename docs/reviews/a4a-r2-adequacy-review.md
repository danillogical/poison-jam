# A4a-r2 adequacy review — ADEQUATE

**Packet:** `A4a`, `docs/packets/a4a-dsp-pending-word.md`
**Revision:** `A4a-r2`, frozen SHA-256 `2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`
**Class:** discovery (`docs/agent-workflow.md` §5.8) — reviewed by its writing Planner against §5.3's two questions; no second Planner spawned (§5.1.5).
**Reviewer:** Planner child `8289add4-6eb3-4063-9156-1c073a24a5e2`, route `claude` / `claude-opus-5-5` @ `high` (workflow §1).
**Brief:** the frozen brief built from `5e739f6` — the r1 draft, the Advisor ruling, the AC5 blocking defect, the uncommitted profile-policy addition, and the five 2026-09-24 runs.
**Session role:** verified that every command in Experiments step 3 runs (§5.1.2); did not author criteria.

## Planner adequacy block (verbatim)

```text
REVISION:          A4a-r2; SHA-256 2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC of lines 1-61 (the contract, before this block was appended)
READ:              docs/agent-workflow.md §5.1-§5.8, §6.1-§6.3; git show 5e739f6:docs/packets/a4a-dsp-pending-word.md (all); git show 5e739f6:docs/reviews/startup-20260924-session-4a79ce06.md lines 196-584 (B1-B16, ruling, pre-check, AC5 defect); docs/jsrf-run-profiles.md 120-186 (uncommitted 146-165); src/recomp/gen/recomp_0005.c 6738-6755; toolkit src/apu/apu_mmio_hook.c 230-283, src/apu/apu_core.c 600-679; scripts/inspect-jsrf.py 10-69; game/mygame_analysis.json DSOUND header; stacks.txt of 143745-909, 143856-017, 143932-031, 155151-292, 155223-143, 100502-623 (frame lines); S frame pattern executed on the five 20260924 runs (F=2 each) and lines 6748/6751 pre-check executed; GP/EP line count on 155151-292 (15)
PREMISE_FRESHNESS: PASS — blocker re-shown STRICT on the exe the packet starts from (879716046b99…) by four 20260924 runs, including the Session's command checks; the only baseline change before execution is the doc-only profile edit
BLOCKING:          NONE
DEFERRED:          (1) Q1/Q2 gate A4b, not A4a; recorded in Limits. (2) the R1 trace covers an early window (log lines ~773-1840 of 3484 in 143932-031); completeness is claimed only up to the cap-line witness, not for a late window with no traffic. (3) the next-packet IDs A4b/A4c/A4d are placeholders the Planner assigns when briefed.
DECISIONS:         AC5 fix — stop decided by result.json outcome + dump word W, frame matched with offset unpinned on recomp_0005.c:6748-6751 and F=0 routed to O-UNKNOWN; reason: offset varies (+0xB20/+0xB24/+0xB2B) on one exe while the line is stable; reversed if a run shows the spin sampled outside 6748-6751 with W=3.
                   O-1 (moved) needs a positive observation (outcome ≠ deadline, or W ≠ 3 on a mapping-valid dump); reason: a missing observation must never select an escalation row; reversed by nothing short of a ruling that absence is evidence.
                   GPRST=3 is an outcome (O-4/O-5/O-6), not a tool gate; T1's value oracle is 0x3FF14=0xFF only; reason: gating on the value under study would hide O-4; reversed if 0x3FF14 is shown written with another value by design.
                   r1's T3 (GP/EP write note) dropped; reason: no row depends on it and it widens write scope to apu_core.c; reversed if C or U1 cannot be decided without it.
                   Image compare shortened 0x800 -> 0x5CC (DSOUND file-backed end = 0x001BA0A0+0x5CC); reason: bytes past the file-backed section are not XBE oracle bytes; Session's 0x800 control is a superset and still holds.
                   Planning trail: sketch 1 written at tool call 20; decision at the 20-call checkpoint was to write the packet with no further investigation; 40/60 checkpoints not reached, so no Advisor send was required. Later calls only executed the packet's own patterns on archives and hashed the file.
VERDICT:           ADEQUATE
```

`VERDICT` is `ADEQUATE` because `BLOCKING` is `NONE` and `PREMISE_FRESHNESS` is `PASS` (§5.3). The two discovery questions are answered: an outcome **cannot** be misread into the wrong row — `F = 0` with `diagnostic_deadline` and `W = 3` routes to `O-UNKNOWN`, never to the moved-stop row `O-1`, which now requires a positive observation; and the packet is **safe and reversible** — both instrumentation changes are trace-only, sit inside the existing `RECOMP_APU_TRACE` block, are off by default, and touch one toolkit file.

## Deferred advisories (recorded, not acted on)

1. **Q1 (Advisor) / Q2 (owner via Advisor, §3.4)** gate `A4b`, not `A4a`.
2. **Early-window trace coverage** — the `[APUMMIO]` window is bounded; completeness is claimed only up to the cap-line witness.
3. **Placeholder next-packet IDs** `A4b`/`A4c`/`A4d` are assigned when briefed.

## Session mechanical verification (§5.1.2) — every command in Experiments step 3 runs

Run against the archived `20260924-143932-031-apu-trap-register-trace` unless stated.

| Check | Result |
|---|---|
| Packet hash, whole file (with block) | `8EAEE5AAAADA7594D717B94FC89920E985E991430D5750A2B20791850F109364` — matches the Planner's |
| Packet hash, contract lines 1–61 | `2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC` — matches; LF line endings with a trailing newline |
| Pre-check `recomp_0005.c:6748,6751` | line 6748 = `loc_001A18D0: ;`; line 6751 = `if (CMP_NE(_fa, _fb)) goto loc_001A18D0;` — exactly as the packet states |
| **Frame pattern `F` (the AC5 fix)** | **`F = 2` on all five 2026-09-24 runs**, including `155151-292` (`+0xB24`) and `155223-143` (`+0xB2B`) which r1's pinned pattern scored 0 on. The defect is fixed. |
| `V`: `B = MEM32(0x001BA858)` | `803C0000` (≠ 0) |
| `V`: `W = MEM32(B+0x810)` | `00000003` |
| `V`: profile / dump-mapping | `STRICT`; `matches: 1`, `content-mismatch: 0` |
| `R1` cap / decode-fail | `0` / `0` |
| `R1` `write 0x3FF14` / `write 0x3FFFC` / `write 0x02040` | 1 / 3 / 1 line |
| `R1` GP/EP read lines and read notes | 0 / 0 (the coverage witness `O-5` vs `O-6` turns on) |
| `R1` `trapped for MMIO` | present |
| `G` = last `write 0x02040` | `803CC000`; `S0 = MEM32(G)` = `803C0000` = `B` |
| `I`: export and compare | `inspect-jsrf.py memory <R1> 0x803C0000 0x5CC --out logs\a4a-scratch0.bin` → 1484 bytes, **all equal** to XBE `0x1A7D60` |
| `L` on R0 | `[APUMMIO]` = 0, `started by the title` = 0, `trapped for MMIO` = 0 |

Independently confirmed while reviewing the Planner's `0x5CC` change: the `DSOUND` section is `VA 0x19E340`, `raw 0x18C000`, `rawsize 0x1C32C`, so its file-backed bytes end at VA `0x1BA66C`; `0x1BA66C − 0x1BA0A0 = 0x5CC` exactly, and the file offset `0x1A7D60 + 0x5CC = 0x1A832C = raw + rawsize`. The title itself stores `0x5CC` as the descriptor size at `0x001A1702`. The shortening is correct, and r1's `0x800` was reading past the section's file-backed end.

## Frozen revision

The packet file was reduced to exactly the reviewed contract (lines 1–61) so the frozen bytes equal the reviewed hash; the adequacy block above was moved into this review record (§5.7: a frozen packet contains the operative contract only; §8: verdicts live in `docs/reviews/`). **No criterion, row, or step was altered** — verified by the hash above, and the Session has no discretion to revise on `ADEQUATE` (§5.3).
