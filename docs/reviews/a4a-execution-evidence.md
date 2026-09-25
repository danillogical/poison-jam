# A4a-r2 execution evidence

**Packet:** `docs/packets/a4a-dsp-pending-word.md`, `A4a-r2`, frozen SHA-256
`2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`
**Executed:** 2026-09-24, Session `session-4a79ce06-0bd5-46f1-a760-c800ce8b62f9`.
**Baseline:** game `5b58d13` (promotion commit), toolkit `c97ce2c` + `0d7929c` (T1/T2).
**Selected outcome row: `O-6`.**

## Instrumentation (Experiment 1)

- **Toolkit commit:** `0d7929c86771dd0b971941592fd4f15436116e82`, `src/apu/apu_mmio_hook.c`
  only, `39 insertions(+), 6 deletions(-)`. T1 carries the moved value out of
  `apu_decode_and_handle` by out-parameter; T2 prints once on the first access past the
  unchanged 400-line cap. Both inside the existing `RECOMP_APU_TRACE` block.
- **Build:** `python -X utf8 scripts\build-jsrf.py` → `Build succeeded (success)`, exit 0.
- **ctest:** `ctest --test-dir build -C Release --output-on-failure` → **12/12 passed**.
- **exe SHA-256:** pre-change `879716046b99cecfb61a8373237df24fcdefa4d00317f50c726aeae7a35187ef`
  → post-change `9597ff7c2a377265aba8dbb90b461ebe763e02d65432e9dfa13acd925539c553`.

## Runs

| | R1 | R0 |
|---|---|---|
| Directory | `logs/runs/20260924-191833-331-a4a-r2-trap-trace` | `logs/runs/20260924-191906-091-a4a-r2-default` |
| Settings | `RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_APU_TRACE=1`, `RECOMP_KERNEL_LOG_BUDGET=100000`, 30 s | same without trap/trace |
| Profile (reclassified) | **STRICT** | **STRICT** |
| Dump mapping | `matches: 1`, `content-mismatch: 0` | `matches: 1`, `content-mismatch: 0` |

Artifact SHA-256:

| Artifact | R1 | R0 |
|---|---|---|
| `result.json` | `C1FAED65CF53AE93D34A25E73E6BDCAD1D1645731206B2D55935ACC82D164924` | `4DC5977D62BBB8943C5BD780FD81CFF3FA81B726CDC8D1765589E1CA95EDE5EE` |
| `stacks.txt` | `BF98F28CE4F12961DD6A0ECA43DECC8AA0DC44EC93F8D3417238BE7B5766104C` | `10AD7D4188F696FA9211BD01B8D90EBC2D4AF57105FD09122A2479948AB8ACCF` |
| `jsrf_run.log` | `0693315325C2A0688DC2B99A1639138DE5F8EC735BBA9FDB160C1B4B8359D296` | `04DBB88F37E27D42A526C99A3FEACD358D944298795B3A98FE50274D9FA8D2E9` |
| `process.dmp` | `57FDDB608874CC317BC33F31ED63EB1BDE4A0A2A051BBA7886F145A0BC45C73D` | `2862F784E2E63FF836A2D444C4E6D88BECC3435845B752514736CA42D521ED14` |
| `logs\a4a-scratch0.bin` | `C2527379062A138B53DBAADCB3F9AC5762409FF0E38D2F320AEA47F8A3FFD8E8` | n/a |

## Measurements (Experiment 3)

**V — validity.** Both runs `STRICT`; both dumps pass mapping; R1 `B = MEM32(0x001BA858)`
= `803C0000` (≠ 0), `W = MEM32(B+0x810)` = `00000003`.

**S — stop.** R1 `outcome = diagnostic_deadline`, frame count `F = 2`. R0
`outcome = diagnostic_deadline`, `F = 2`. The offset-unpinned pattern matched on both,
which is the AC5 fix working on the instrumented build.

**R1 trace.** cap-reached lines `0`; `MMIO decode fail` lines `0`;
`write 0x3FF14 = 000000FF` present; `write 0x3FFFC` lines present, last value
`0x00000003`; `write 0x02040 = 803CC000` present; `trapped for MMIO` present;
GP/EP read lines `0`; read notes `0`.

**T1 value oracle (the point of the packet).** The XBE writes immediates `0xFF` at
`0x001A582A` (`0xFE83FF14`) and `3` at `0x001A585E` (`0xFE83FFFC`). The instrumented trace
now logs:

```text
L3190: [APUMMIO] write 0x3FFFC = 00000001
L3192: [APUMMIO] write 0x3FF14 = 000000FF
L3201: [APUMMIO] write 0x3FFFC = 00000003
```

Against the same offsets the pre-change build logged `00000000`. T1 is value-faithful.

**C — consistency.** Read notes `0` and GP/EP read lines `0`: every note has a line and
every line has a note, both directions vacuously. The read note fires once per 64 KiB
block on the first read through the hook and is not bounded by the trace cap, so zero
notes is a coverage witness that the guest made **no GP/EP reads at all** in R1.

**I — image.** `G` = last `write 0x02040` = `803CC000`; `S0 = MEM32(G)` = `803C0000` =
`B`; exported `[B, B+0x5CC)` = 1484 bytes, **all equal** to XBE file offset `0x1A7D60`.

**L — leak.** R0: `[APUMMIO]` = 0, `started by the title` = 0, `trapped for MMIO` = 0.
The instrumentation is inert on the default path.

## Row evaluation

Gates hold (both runs STRICT, both `diagnostic_deadline` with `W = 3` and `F ≥ 1`, R1 has
zero cap and zero decode-fail lines, `write 0x3FF14 = 000000FF`, `write 0x02040`,
`trapped for MMIO`; C holds; L all 0). `I` holds.

- `O-1` no — no positive moved-stop observation; both runs are `diagnostic_deadline` with `W = 3`.
- `O-2` no — the `0x3FF14` oracle line carries `000000FF`; C holds; L is 0.
- `O-3` no — `I` holds.
- `O-4` no — a `write 0x3FFFC` line exists and `last_GPRST & 3 = 3`.
- `O-5` no — `gp_ep_reads = 0`.
- **`O-6` yes — `last_GPRST & 3 = 3` and `gp_ep_reads = 0`.**

**Selected: `O-6`** — "the start write is the only GP interaction; the image is in place."

## Next packet (from the `O-6` row)

`A4b` brief: GP DSP56300 engine. Positive controls: GPRST `0→3` bootstraps from `G`,
loaded words equal the `I` image, GP instructions retire. Acceptance: `+0x810` cleared by
GP execution in a STRICT run. Needs **Q1** (Advisor: does the upstream `PIO_FREE` stub
contaminate A4b's strict claim?) and **Q2** (owner via Advisor, §3.4: licensing of a
DSP56300 core) answered first.

## Claim limits carried forward

A discovery packet claims observations only (`docs/agent-workflow.md` §5.8); it never
satisfies a strict criterion and never claims anything works. R1 reached the spin only
after its `0xFE820010` polls were answered by the `PIO_FREE` stub constant `0x80`, so
**R1's arrival at the spin is exploratory-grade however R1 classifies** (Advisor ruling B).
R0 is a leak check, not a comparison: no R1/R0 difference is attributed to any single trap
effect (ruling C). Nothing here shows the GP would clear `+0x810` if it ran — only real GP
DSP56300 execution can (ruling D). Not established: the order of the GPRST write relative
to the `+0x810` write (the trace has no thread attribution); scratch state at GPRST time;
the units of `0x5CC`; GP/EP register semantics; whether SGE VAs need mapping in A4b. The
`[APUMMIO]` window is bounded (no cap line was reached, so no traffic was lost to the cap).
