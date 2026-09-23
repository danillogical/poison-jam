# P0.2 and A2h adequacy-review dispositions

Recorded per `docs/agent-workflow.md` §2.4/§2.7. Both artifacts were reviewed by
`workbuddy-ai/hy4-preview-f` @ `high` — a different model family from the session —
across repeated rounds. **Every round found a real defect**, which is why the
revision counts are high; the sequence is the evidence that the review was
adversarial rather than confirmatory.

## P0.2 — durable review ingestion

Artifact: `docs/packets/p0-review-records.md`. **Verdict: ADEQUATE**, revision
`P0.2-interface-r12`, confirmed as carrying forward from r9 → r10 → r11 → r12.

| revision | verdict | defects found |
|---|---|---|
| `r2` `23E898A3…` | INADEQUATE | 8 blocking: footer authority delegated to an under-specified line scanner |
| `r3` `7A4DFD67…` | INADEQUATE | N1 the scanner **chose its own input**; N2 the negation scan was undecidable; N3 a mandated field was unenforced |
| `r4` `7D8D1802…` | INADEQUATE | N4 counterfactual/adversative agreement still corroborated |
| `r5` `3D9B3C28…` | INADEQUATE | 6 further hedges plus a `but` false positive |
| `r6` `BF69DE1F…` | INADEQUATE | N5 the inversion was half-applied; N6 line-scoped attribution failed two-criterion reviews |
| `r7` `E3EFF1A0…` | INADEQUATE | N7 the tolerances reintroduced a lexicon |
| `r8` `E96A57AE…` | INADEQUATE | N9 **past tense marks *when*, not *whether committed***; N10 the same test refused honest prose |
| `r9` `391E3D9A…` | **ADEQUATE** (with advisories) | the no-finite-verb rule still admitted verbless qualifiers |
| `r10` `9BAF923D…` | **ADEQUATE confirmed** | tolerance removed entirely |
| `r11` `30D13B6A…` | **ADEQUATE carried** | 3 advisories fixed (label retraction, `for <id>` mis-credit, bolded labels) |
| `r12` (final) | **ADEQUATE carried** | 4 further retractions refused; the reviewer's suggested structural replacement **measured worse** and was rejected |

**What the rounds established.** Five successive attempts tried to recognise the
*withheld* shape and admit everything else. Each failed because the vocabulary of
withholding is unbounded. The design that survived is the inverse: corroboration
requires a committed construction, and **nothing may follow the disposition token**
in a verdict sentence.

**One reviewer suggestion was measured and rejected.** At r11 the reviewer proposed
replacing the retraction list with a structural "bare past-participle confirmation"
rule. Measured on 12 sentences it refused an honest `P0.2-AC1 reproduced and
confirmed.` while **accepting `P0.2-AC1 is not confirmed.` and `P0.2-AC1
deferred.`** — a false `AGREED`. The list stayed and the residual was documented.
A plausible fix adopted without measuring it would have made the validator worse.

## A2h — bounded writer-investigation packet

Artifact: `docs/packets/a2h-writer-investigation.md`. **Verdict: ADEQUATE** at
`A2h-r4`; `r5` applies three non-blocking advisories and is recorded as a
hardening delta.

| revision | verdict | defects found |
|---|---|---|
| `r1` | INADEQUATE | objective unreachable by the authorized experiment; non-exhaustive decision table; PASS did not pin the failing invocation; overstated profile claim; no positive control |
| `r2` `EC48DFBB…` | INADEQUATE | **B6** PASS and row 1 pinned to the literal `0x00700010`, itself INFERRED; A10 the archived strict run does not reach the site; A11–A14 |
| `r3` `499BF09C…` | INADEQUATE | **B7** the decision table's rows did not cross — *heap base with a bogus slot* was uncovered, and it is the **most likely** case; A15 the dispatcher has two callers sharing the return address; A16 |
| `r4` `36FEEAC6…` | **ADEQUATE** | none blocking; A17–A19 advisory |
| `r5` (final) | hardening | A17 slot-value agreement, A18 two corrected MEASURED numbers, A19 instrumented-address list |

**Three factual errors of mine were corrected, all in material published as
MEASURED:**

1. `sub_0017DBBD` ends with a plain `ret`; the **callers** clean. I had written that
   the callee cleans `esp, 0x10`.
2. There are **three** direct call sites (`0x0017DDFD`, `0x0017E03A`, `0x0017E329`);
   `0x0017DD6B`/`0x0017DDAE` are `tail_jump_alias` fragments, not callers.
3. The disassembly listing omitted `0x0017DC1C mov ecx,[edi+8]`, so as printed the
   slot computed to `base + index*8 + index*8 + 4`.

**One of the session's corrections was right and the reviewer withdrew its
defect:** the reviewer claimed strict runs were not launchable because `run-jsrf.py`
had no `--profile`. Measured, `--profile strict|exploratory|fixture` exists (added
in P0.1) and an archived run reclassifies STRICT. The reviewer confirmed and
withdrew it. Its **A10** finding was the more valuable one: that archived strict run
**does not reach the site at all** — zero matches for every relevant symbol, ending
`normal_exit` in 1.92 s. A strict *launch* is executable; strict *reachability* is
unproven, and the packet now says so in three places.

**A18 was inherited and propagated.** The packet's MEASURED block claimed
`exit_code 0xE0464643` and "both issue 157 kernel calls". Measured: both runs are
`0xE0424943` (3762440515) — the code `src/recomp_manual.c` raises — and the counts
are **200** pre-fix and **157** post-fix. The wrong code appeared in `AGENTS.md` and
`report-deepseek.md` too, so all four places were corrected.

## Closure

Both artifacts are adequate for their purpose. P0.2's implementation is the
validator plus producer (`scripts/jsrf_review_records.py`,
`scripts/record-review.py`, `scripts/check-recorded-reviews.py`), which is proven
end-to-end by rebuilding the real P0.1 record from the reviewer's own DSH log and
validating it as `acceptance-eligible`. A2h is published as a bounded packet and
authorizes exactly one discriminating experiment.
