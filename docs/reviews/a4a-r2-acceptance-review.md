# A4a-r2 acceptance review — ACCEPT (first stage, final)

**Packet:** `docs/packets/a4a-dsp-pending-word.md`, `A4a-r2`, class **discovery**, frozen
SHA-256 `2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`
**Evidence:** `docs/reviews/a4a-execution-evidence.md`, SHA-256
`AFEC0F18F0BF068C092DE30389819AD81671D6C2A66CC043C8FFC1D647DF1731` (unchanged since 19:20:50)
**Reviewer:** Acceptance reviewer, first stage — child `715163ef-9bd3-442d-8bdb-8c3a17d75bb8`, route `workbuddy-ai` / `deepseek-v4.1-flash` @ `max` (workflow §1)
**Verdict:** **ACCEPT.** Every obligation `AGREED`; `BLOCKING: NONE`. Row selection **`O-6`** confirmed independently.
**Second stage:** not run — a first-stage `ACCEPT` is final and is not passed on (§2.2).

## Reviewer's criteria (verbatim summary)

| Obligation | Result | Reviewer's evidence |
|---|---|---|
| Contract hash frozen | `AGREED` | `Get-FileHash` = `2366E18C…2FBC` |
| Pre-check | `AGREED` | `recomp_0005.c:6748` = `loc_001A18D0: ;`; `:6751` = `goto loc_001A18D0;` |
| `V` validity | `AGREED` | both `STRICT`; both `matches: 1, content-mismatch: 0`; `B=803C0000`, `W=00000003` in **both** runs |
| `S` stop | `AGREED` | both `diagnostic_deadline`; `F=2` both (`+0xB20` / `+0xB24`, line 6749); control `F=2` on all four archived runs |
| `T1` oracle | `AGREED` | `L3190`/`L3192`/`L3201` exactly as claimed; XBE disasm confirms the immediates; pre-change control logs `00000000` |
| `R1` counters | `AGREED` | cap 0, decode-fail 0, GP/EP reads 0, notes 0, `write 0x02040 = 803CC000` `L3188`, `trapped for MMIO` `L26`, 344 lines < 400 cap |
| `C` consistency | `AGREED` | vacuous but the absence is meaningful: positive control (note fires at `0x30200` in 3 archived runs), regex matches synthetic GP/EP lines, hook is the only read path |
| `I` image | `AGREED` | `G=803CC000`; `S0=MEM32(G)=803C0000=B`; 1484 bytes, 0 mismatches vs XBE `0x1A7D60`; known-bad `0x1A7D64` differs at 0; section math exact |
| `L` leak | `AGREED` | R0: 0/0/0; positive control fires in 23 other runs |
| Instrumentation scope | `AGREED` | `git show --stat 0d7929c` → `src/apu/apu_mmio_hook.c` only, 39+/6−; toolkit HEAD `0d7929c`, clean |
| exe identity | `AGREED` | build exe and both archived run-dir exes = `9597ff7c…c553`; pre-change = `87971604…7ef` |
| Build | `AGREED` | `logs\build-current.log` 19:17:51 builds `jsrf_recomp.exe` (19:17:50); R1 19:18:33 |
| `ctest` 12/12 | `AGREED` | by deduction: `CTestCostData.txt` 19:18:19 lists all 12; `LastTestsFailed.log` untouched since 9/22, validated by two passing-run controls ⇒ 12 ran, 0 failed |
| Row selection | `AGREED` | `O-6` |

**Row:** `O-1`…`O-5` each falsified on the artifacts (`O-4` in particular: three GPRST
lines, last `0x00000003`, `&3 = 3`); no `O-UNKNOWN` trigger. Gates and `I` hold;
`last_GPRST & 3 = 3`; `gp_ep_reads = 0` ⇒ **`O-6`** is the first match. The handoff to
`A4b` is on the correct premise.

## Session disclosure (workflow §2.4.7)

The Session edited `docs/reviews/a4a-execution-evidence.md` at **19:20:50**, eleven
seconds **after** the reviewer child was created (19:20:39), to replace four `—`
placeholders with the R0 artifact hashes. Disclosed rather than left implicit because a
review binds to the bytes it saw. The edit is immaterial to the disposition: it added R0
bookkeeping hashes that were not among the obligations the reviewer was asked to check,
and the reviewer independently recomputed all four values and confirmed them
(`4DC5977D…`, `10AD7D41…`, `04DBB88F…`, `2862F784…`). The reviewer noted its first read
of that table appeared to show blanks and re-read it; the file has been stable since
19:20:50 and its hash is recorded above. **No criterion, row, or measured value changed.**

## Deferred advisories (recorded, not acted on)

1. **Raw `ctest` stdout was not archived** and `build\Testing\Temporary\LastTest.log` was
   truncated at 19:23:40. The packet's Closure line names "build/ctest output" as an
   artifact. Consider archiving ctest output into the run directory in future packets.
2. **`O-6`'s prose gloss understates GP traffic.** "The start write is the only GP
   interaction" is looser than the observation: R1 writes five distinct GP offsets
   (`0x3FF00`, `0x3FF04`, `0x3FF10`, `0x3FF14`, `0x3FFFC`×3). The row's *conditions* are
   met, but **the `A4b` brief should scope the whole GP block, not just GPRST.**
3. **Latent definitional gap in `gp_ep_reads`.** It counts only `[APUMMIO] read` lines,
   but a GP read-modify-write (OR/AND, `apu_mmio_hook.c:241,255`) calls
   `mcpx_apu_mmio_read` and is logged as a *write*, so it would leave `gp_ep_reads = 0`
   while genuinely reading GP state. It did not bite here — zero read notes independently
   proves no GP/EP read of any kind — so `O-6`'s meaning holds. Worth tightening if the
   row is reused.
4. Plan and evidence doc were uncommitted at review time; committed at closure below.

## Next packet

Per `O-6`, the next frozen brief is **`A4b` — the GP DSP56300 engine**, with positive
controls: GPRST `0→3` bootstraps from `G`, loaded words equal the `I` image, GP
instructions retire; acceptance: `+0x810` cleared by GP execution in a STRICT run. Two
questions gate it: **Q1** (Advisor — does the upstream `PIO_FREE` stub contaminate A4b's
strict claim?) and **Q2** (owner via Advisor, §3.4 — licensing of a DSP56300 core).
Advisory 2 above widens its scope to the whole GP block.
