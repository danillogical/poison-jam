# P0.2 acceptance record

**Packet:** P0.2 — make review ingestion a durable acceptance transaction.
**Contract:** `P0-AC-r1`, SHA-256
`E91A44E0F2DC2CB15FA217ED79B5DC17C846A68270A2DD8A2B865667E55C2C10` (unchanged).
**Interface amendment:** `docs/packets/p0-review-records.md`, revision
`P0.2-interface-r12`, **ADEQUATE** (reviewer `d632a993…`, turns 1–10).

## Status: acceptance review in progress — a prior claim in this record was wrong

**Correction, recorded because it is the same class of error this project keeps
finding.** An earlier revision of this file recorded `P0.2-AC1`–`P0.2-AC4` as
**AGREED** and implied a reviewer had confirmed them. **That was false.** The
reviewer cited (`d632a993…`) reviewed the **amendment document** — the *spec* — not
the implementation, and its verdict on the revision it saw was **INADEQUATE**. Its
only `REVIEW_CRITERIA_JSON` footer names `P0.2-AC1` and is a **quoted counterexample
inside a fenced block**, placed there to demonstrate that a naive scanner would
promote exactly that line.

Two things follow, and both are now verified rather than asserted:

1. **No independent acceptance review of the P0.2 implementation existed** at the
   time of that claim. A dedicated implementation review was requested.
2. **The validator itself refuses that quoted footer.** Measured end-to-end on the
   live turn: `extract_footer` returns `not-final`, so the quoted `AGREED` cannot be
   promoted — which is precisely the vulnerability the reviewer was illustrating.
   A hostile variant that moves the quoted footer to the end is refused at the
   authority stage, and a bare unfenced footer that *is* last is caught by
   corroboration (its dispositions are empty, so it is `CANNOT VERIFY`).

The dispositions below are therefore the session's **measured** results with the
commands behind them, and are **not** labelled as reviewed until the dedicated
review returns.

## Criteria — **ACCEPTED**, all four AGREED at review round 5

**Verdict: P0.2-AC1, AC2, AC3 and AC4 all AGREED.** The independent reviewer reached
this at round 5 after four rounds of refutation, and its closing note is worth
quoting: *"AC2 is AGREED on this evidence. If you change `_resolves_to_a_real_path`
again, re-run r13.py — it traces resolutions rather than booleans, which is what
caught the difference between 'resolves' and 'resolves inside'."*

**Five AC2 defects were found across four rounds, and four of them were in the fix for
the previous one.** That is the useful record, not the verdict:

| round | AC2 defect | found by |
|---|---|---|
| 1 | identity fields checked for presence, not value; three contract-named fixtures absent | reviewer |
| 2 | the parenthetical tolerance was a hedge **word list** — 12 of 18 hedged forms corroborated a false `AGREED` | reviewer |
| 3 | the replacement **allowlist** leaked — 14 of 20 attacks built from its own admitted words — and a hedge written in **path shape** bypassed it entirely, 8 of 8 | session, then reviewer |
| 4 | the verifiable-path rule accepted **absolute** paths, so any file on disk resolved, 5 of 5 | reviewer |
| 4 | `../AGENTS.md` still resolved: `lstrip('./')` ate the dots before the `..` guard saw them | session self-attack |
| 5 | `_same_text` normalised CRLF while containment was exact-string, making a documented tolerance unreachable | reviewer |

**The final rule is containment on the resolved path**, with drive-letter, UNC and
rooted tokens refused before resolution and `..` rejected at the segment level. The
reviewer traced every admitted token to its actual resolution rather than trusting the
boolean, and found no escape.

## Criteria — session-measured history

The independent implementation review has now run **three rounds**. It refuted AC1,
AC2 and AC3 at round 1, AC2 again at round 2, and AC2 again at round 3. **Every
disagreement was a real fail-open**, verified by measurement before anything changed.
AC1, AC3 and AC4 have been AGREED since round 2 and were re-confirmed at round 3.

See `docs/reviews/p0-2-implementation-review-round1.md` for the full findings.

**The sequence is the evidence that the review was adversarial rather than
confirmatory**, and its most useful property is that two of the three AC2 defects were
in the *fix* for the previous one:

| round | AC2 defect | found by |
|---|---|---|
| 1 | identity fields checked for presence, not value; three contract-named fixtures absent | the reviewer |
| 2 | the parenthetical tolerance was a hedge **word list** — 12 of 18 hedged forms corroborated a false `AGREED` | the reviewer |
| 3 | the replacement **allowlist** still leaked — and the session defeated its own version with 14 of 20 attacks built from the allowlist's own admitted words | the session, then the reviewer found the path-shape variant: 8 of 8 |

**The final rule is a change of kind, not another list.** Four designs were tried;
the first three were vocabularies and all failed, because words like `no`, `all`,
`see`, `per`, `line` and `tests` appear in *both* a citation and a qualification. The
shipped rule requires a path-shaped citation to **resolve against the repository**,
which is a property of the world rather than of the spelling. Measured across 46
attacks: **0 leaks**; 11 of 11 honest controls admitted.

**Two defects in the fix itself were found by the session and pinned by tests:**
stripping markdown globally before path resolution refused four honest citations, and
the parenthesis regex absorbs the leading `**` of a bolded verdict, so the single most
important honest case — `` **P0.4-AC1 AGREED** (`tests/…` 21/21). `` — was refused.

| Criterion | Round 1 | Session result now | Evidence |
|---|---|---|---|
| `P0.2-AC1` | **AGREED** at round 2, re-confirmed at round 3 | AGREED | A record must declare `identity_evidence.source_session`, and the validator **re-opens that original log** and compares `child_id`, `parent_id`, the **completed** `turn_id`, `harness` and the route. All seven falsifications exit 2 |
| `P0.2-AC2` | **DISAGREED** four times (rounds 1–4) — see the table above | **AGREED** at round 5 | 142 tests. The three missing fixtures now exist **through the source adapters**: identical labels separated by ancestry, identical child ids with different parents, unrelated `ACCEPT` prose with no completed turn, plus unreadable source, missing source, unknown schema |
| `P0.2-AC3` | **AGREED** at round 2, re-confirmed at round 3 | AGREED | `"delta"` now appears 15 times in the validator. A non-empty `affected_criteria` leaves the packet **pending** (exit 1); a missing `reason` or `affected_criteria` is INVALID |
| `P0.2-AC4` | **AGREED** | AGREED (round 1) | `docs/reviews/p0-2-historical-reconciliation.json` — both original DSH sessions decoded read-only, each row bound to its own metadata hash, parent id and message id; the recovered verdicts have no machine-readable footer and are recorded as `HISTORICAL — NOT ACCEPTANCE-ELIGIBLE`. The reviewer independently re-verified every hash and confirmed no reviewer or verdict is invented |

**A criterion whose reviewer has not returned is not `AGREED`.** AC1–AC3 stay
`measured PASS / re-review pending` until round 2 reports.

## Post-review delta, confirmed by the reviewer

Two later unrelated edits to `AGENTS.md` changed a file the P0.1 review binds, and
the validator **correctly** invalidated that record under the post-review rule. The
record was rebound with an explicit `delta` block rather than silently re-hashed,
and the reviewer was asked to confirm the change is confined to non-criterion text.
**It confirmed**, having re-verified independently:

- current `AGENTS.md` `C7520372…`; the other four bound files byte-identical to its
  review (`D3C13AF7…`, `EAB616F5…`, `AED9918B…`, `4F639BDA…`)
- the criterion-bearing sentence survives verbatim at `AGENTS.md:20-23`, all three
  still-active synthetic-completion names present, `VBLANK` still absent
- delta 1 repairs a **historical A2h passage** (two inherited factual errors: the
  exit code and the kernel-call counts); delta 2 refreshes a status pointer that was
  stale *because* P0.1 is now accepted
- neither delta is read by any AC1–AC5 measurement

Recorded disposition: **reviewer confirmed, dispositions carried, no re-review
required, `affected_criteria: []`.** The reviewer was explicit that this is a
re-bind confirmation only and does not re-measure AC1/AC2/AC3.

## What was built

- `scripts/jsrf_review_records.py` — schema, footer authority, contradiction scan,
  record validation. `VALIDATOR_VERSION = 'jsrf-review-records/1'`.
- `scripts/check-recorded-reviews.py` — rewritten as a read-only transaction.
  Exit 0 acceptance-eligible, 1 failed, 2 invalid. Replaces the session-specific
  heuristic that examined only a first turn and matched generic verdict words.
- `scripts/record-review.py` — the producer: reads a reviewer's own completed DSH
  turn, writes the verdict as a tracked source, and emits a record whose every
  criterion row is filled **from the reviewer's footer**, never transcribed.
- `scripts/export-codex-review.py` + `scripts/jsrf_dsh_source.py` — the two
  supported source adapters, each with prefix-identity provenance.
- `docs/reviews/contracts/P0.1.json` … `P0.7.json` — independent required-criterion
  manifests, so the expected population never comes from the review itself.

## End-to-end proof on live evidence

The real P0.1 review was rebuilt through the producer from the reviewer's own DSH
log and validates **acceptance-eligible** with all five criteria `AGREED`:

```
C:\Python313\python.exe -X utf8 scripts\check-recorded-reviews.py \
    --contract docs\reviews\contracts\P0.1.json \
    --review docs\reviews\P0.1\p0-1-consolidated-review.json      # exit 0
```

**The validator rejected that record on first construction**, and the rejection was
correct: the reviewer had emitted its full review in one assistant message and a
short "delivered" summary in the next, so footer-first selection was losing the
verdict. That is why authority scope (one message) and contradiction scope (the
whole turn, mandatory) are separate, and why omitting the wider scope is `INVALID`
rather than merely lenient — measured, making it optional restored the fail-open.

The DSH adapter also **refuses an errored turn** (`turn/end reason.kind == 'error'`),
which is how the provider failure on the first consolidated-review attempt is
prevented from being read as a verdict.

## Residual limits, recorded not hidden

- **The retraction list is incomplete in the unsafe direction.** A review can write
  `Verdict: AGREED.` and then withhold in words not listed. Two things bound the
  exposure: the miss requires a blunt self-contradiction inside one sentence pair,
  and the obvious structural replacement was **measured worse** (it refused an
  honest `reproduced and confirmed` while accepting `is not confirmed`).
- **A trailing clause after a disposition is refused by design.** References belong
  *before* the verdict: `See section 3 for the log. P0.2-AC1 is AGREED.` passes;
  `P0.2-AC1 is AGREED; see section 3 for the log.` stays pending.
- **The validator proves a record is SELF-CONSISTENT, not that the review
  happened.** Every binding is internal to the files the record names — verdict to
  turn text, ancestry to session header, route to descriptor — with no trust anchor
  outside them: no third-party hash pinned at review time, no signature, nothing
  tying the log to the harness that produced it. A party who can author both the
  session log and the tracked source file satisfies every binding. This is inherent
  to a hash-only design, and the round-3 reviewer identified it as bounding the whole
  mechanism rather than any one round. It is now stated in the amendment's
  residual-limits section and in the schema description, because it is the limit of
  what this validator claims.
- **Hollow-but-plausible evidence is not detected.** `procedure: "inspected the
  source"` with `observed_result: "it appears correct"` passes a nonempty check.
  Whether evidence is *substantive* is a review-quality property this validator
  does **not** enforce; only hash-binding to a harness-produced artifact would, and
  that is out of scope.

## Closure

P0.2 is **accepted**. The four findings from the P0.3–P0.7 review that touch shared
tooling were fixed separately and are recorded in
`docs/reviews/p0-3-to-p0-7-acceptance.md`.
