# A4a revision history (non-authoritative)

Per `docs/agent-workflow.md` §5.7. This log is not reviewed for accuracy and loses to the
frozen contract on any conflict. It exists so the packet itself carries no revision
narrative.

## A4a-r1 — superseded, never executed

Written by Planner child `5673a592-dc14-4059-bd41-d3bfa233e7b8`, preserved at game commit
`5e739f6`, SHA-256 `B493B3FA0D7D0138B85EFF185ABDEE952495AB175B7DF3F3A814138EA2EFF816`.
A change-style packet of 483 lines aimed at the DSP pending-word spin.

Superseded for one blocking defect found by the Session's mechanical pass (§5.1.2):
`A4a-AC5` pinned the spin frame offset to exactly `+0xB20`, but the collector samples that
frame at a varying offset on one byte-identical executable — `+0xB20`, `+0xB24`, `+0xB2B`
were all observed with `exe_sha256 879716046b99cecf…` and the same `recomp_0005.c:6749`.
The literal pattern matched 0 times on two of four runs, so a faithful execution would
have reported FAIL, selected decision row `R-MOVED`, and escalated a correct run to the
Advisor as a possible nondeterministic stop. The full measurement is in
`docs/reviews/startup-20260924-session-4a79ce06.md` (commit `5e739f6`), section "BLOCKING
DEFECT FOUND IN `A4a-r1` AC5". Repairing a criterion is the Planner's authority, so the
Session recorded the defect and returned it to planning rather than editing the packet.

The owner also directed, by direct message to that Planner, that planning stop
reverse-engineering the GP DSP and produce an observability packet instead. r2 is that
packet. This was an owner instruction under §4.1, not an anomaly.

## A4a-r2 — frozen and promoted

Written by Planner child `8289add4-6eb3-4063-9156-1c073a24a5e2` (fresh child, per §5.1)
against the frozen brief built from `5e739f6`. Frozen SHA-256
`2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`. Adequacy review:
`docs/reviews/a4a-r2-adequacy-review.md` (`VERDICT: ADEQUATE`, `BLOCKING: NONE`).

Changes from r1, all recorded as decisions in the adequacy block:

- **Class changed to discovery.** r1 already asked an observation question; §5.8 makes
  that a discovery packet, which the writing Planner reviews itself (§5.1.5, §5.3).
- **AC5 defect fixed.** The stop is now decided by `result.json`'s `outcome` plus the dump
  word `W`; the frame pattern leaves the offset unpinned and matches the stable source
  lines `recomp_0005.c:6748-6751`; `F = 0` routes to `O-UNKNOWN` and can never select the
  moved-stop row. Verified `F = 2` on all five 2026-09-24 runs, including the two that
  broke r1's pattern.
- **r1's T3 dropped** (the GP/EP write note in `apu_core.c`): no outcome row needs it, and
  dropping it narrows the write scope to one toolkit file.
- **GPRST = 3 became an outcome, not a tool gate** (`O-4`/`O-5`/`O-6`). Gating on the
  value under study would have hidden `O-4`. The tool's value oracle is `0x3FF14 = 0xFF`
  only.
- **Image compare shortened `0x800` → `0x5CC`.** `0x5CC` is where the `DSOUND` section's
  file-backed bytes end (`0x001BA0A0 + 0x5CC = 0x1BA66C`, the section's `raw + rawsize`);
  bytes past that are not XBE oracle bytes. r1's `0x800` read past the section end.
- **Length reduced 483 → 61 lines** (about one page, §6.3).

Planning trail: sketch 1 was written into the draft before the 20-call checkpoint and the
decision there was to write the packet with no further investigation; the 40- and 60-call
checkpoints were not reached, so no Advisor send was required.
