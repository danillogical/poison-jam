# P0.2 executable interface amendment

Revision `P0.2-interface-r14`, adequacy-reviewed. Depends on accepted
P0.1; no implementation starts before that gate. Read with `P0-AC-r1` AC1–AC4.
Advisor `/root/workflow_advisor` recommended a JSON record and one validator;
no database, signatures or automatic rewriting of plan acceptance.

Revision history: `r1` (adequate at `BAD23C07…`, before footer authority existed),
`r2` (`23E898A3…`, **inadequate** — 8 blocking defects), `r3` (`7A4DFD67…`,
**inadequate** — N1 selection fail-open, N2 undecidable negation, N3 unenforced
field), `r4` (`7D8D1802…`, **inadequate** — N4 counterfactual corroboration),
`r5` (`3D9B3C28…`, **inadequate** — 6 unsafe hedges plus a `but` false positive),
`r6` (`BF69DE1F…`, **inadequate** — N5 inversion half-applied, N6 line-scoped
attribution), `r7` (`E3EFF1A0…`, **inadequate** — N7 the tolerances were still a
lexicon), `r8` (`E96A57AE…`, **inadequate** — N9 past tense marks *when*, not
*whether committed*; N10 the same test refused honest prose), `r9` (`391E3D9A…`,
**ADEQUATE** with advisories — the no-finite-verb rule still admitted verbless
qualifiers), `r10` (`9BAF923D…`, **ADEQUATE confirmed** — tolerance removed),
`r11` (`30D13B6A…`, **ADEQUATE carried** — the three r10 advisories fixed: a label
may not bind to a retracting sentence, `for <id>` must name this criterion, and
leading emphasis no longer loses a label), `r12` (`CE459F68…`, four further
retraction forms found by review are refused, and the residual is documented after
measuring that the suggested structural replacement was worse), `r13` (the
parenthetical and appositive tolerances, added after a real acceptance review failed
on them), `r14` (this revision: the parenthetical rule becomes a **verifiable-path**
rule after an independent review defeated the allowlist by writing a hedge in PATH
SHAPE, and the self-consistency limit is stated). A record binding a revision must
cite the revision it actually reviewed; no earlier approval carries forward.

## Required interfaces (TOOLING REQUIRED)

- Schema/documentation: `docs/reviews/review-record.schema.json` (schema version 1).
- Required criterion manifest: `docs/reviews/contracts/<packet-id>.json` contains
  schema version, packet ID, contract file/hash and nonempty unique required IDs.
  Required IDs come from this independent manifest, never from the review itself.
- Durable records: `docs/reviews/<packet-id>/<review-id>.json`.
- Validator command from game root:

```powershell
C:\Python313\python.exe -X utf8 scripts\check-recorded-reviews.py --contract docs\reviews\contracts\P0.2.json --review docs\reviews\P0.2\<review-id>.json
C:\Python313\python.exe -X utf8 tests\test_recorded_reviews.py
```

Replace `<review-id>` with a saved record, never pass placeholders literally.
Exit 0 means acceptance-eligible, 1 means failed criterion/stale evidence, and 2
means invalid/unverifiable input. The command is read-only and never changes plan
status. Persist the complete record before running it and recording acceptance.
Use atomic file replacement when a helper writes records; preserve previous review
revisions and dispositions, rather than overwrite a rejection with a later pass.

## Record fields and validation

Require schema_version, review_id, packet_id, contract_sha256, reviewed_files
(path/hash), harness, parent_id, child_id, requested_model/effort,
identity_evidence (kind/path/hash), turn_id, full verdict_text and criteria.
Each criterion has id, disposition, nonempty evidence (path/hash), procedure and
observed result. Dispositions are AGREED, DISAGREED or CANNOT VERIFY. Missing,
extra or duplicate criterion IDs, empty evidence/verdict, unknown schema or
disposition, and truncated source verdicts cannot pass. Compare every declared
contract/source/evidence hash with existing bytes. Validate parent, child, turn,
route and full verdict association against identity evidence, not labels or a
generic ACCEPT keyword. Post-review changes reopen affected acceptance.

DSH adapter: original JSONL header `parentSession`, subagent descriptor route and
the selected completed turn are the identity/response source. Decode zstd originals
only with an available decoder; missing decoder/source is UNKNOWN. Projection
cache alone is insufficient: measured cache records have no parent ID in
subagent.identity and contain truncated responses. Check all relevant completed
turns, not only the first. Do not accept a nested quote of somebody else's verdict.

Codex adapter: preserve the spawn/follow-up tool receipt and response associated
with that child as a durable source. Record the source type explicitly; manually
transcribed association must not claim source-verified ancestry. Do not require
private internal transcript paths inaccessible to the current harness. Hashes detect
content changes; they do not authenticate authorship. An unverifiable source leaves
acceptance pending; the session's assertion alone cannot close that gap.

**Measured accessible Codex source and required export interface:** this host stores
child rollouts under `%USERPROFILE%\.codex\sessions\`. For example the actual
reviewer is `2026\09\23\rollout-2026-09-23T00-34-50-01a0cd30-4eff-7020-b0d2-6af7828cb476.jsonl`.
Its `session_meta.payload` records child ID, `parent_thread_id` and agent path;
`turn_context.payload` records exact turn ID, model and effort; the completed
`event_msg` with payload type `task_complete` records that turn's full
`last_agent_message`. Parent inspection reproduced these fields, including turn
`01a0cd43-27b7-76b3-8e25-f85bedf2d12e`, model `gpt-6-luna`, effort `max`.
These are local harness records, not the model's self-description.

TOOLING REQUIRED: `scripts/export-codex-review.py --rollout <child-jsonl>
--turn-id <completed-turn-id> --output <source-json>` exports only the identity,
route, selected completed verdict and provenance needed for review. Omit prompts,
reasoning, environment and unrelated turns. Reject missing/ambiguous identities,
missing completion or mismatched turn IDs. Record source path, selected event
ordinals and hash/length of the immutable source prefix through that completion;
later appended turns do not invalidate an earlier completed review. The validator
reopens that source, verifies the prefix identity and compares exported fields with
those events. Missing source is UNKNOWN. A manually transcribed JSON claiming to
be an export is not sufficient without that comparison. Tests must include wrong
parent/turn/model/verdict, a truncated prefix and a valid source with later appended
turns. On hosts without readable rollout records, Codex source-backed acceptance
remains UNKNOWN; do not substitute a user-visible summary for the full verdict.

## Historical AC4 population and recovery procedure

Read only these original review sessions under
`C:\Users\logic\.dsh\sessions\--C-Users-logic-Repos-my_xbox_game--\`:

- `2aa4b25a-25c7-45a0-a196-ad38cc1a8926\session.v3.jsonl.zstd` (A2f).
- `1415063e-fa7d-4c0a-8e89-f827d1a43fba\session.v3.jsonl.zstd` (A2f+A2g).

Identify actual parentage, route and completed review turn from decoded originals;
record source hashes and exact turn IDs. Record extraction command/decoder version
and full response hash. Keep recovered historical verdicts as historical evidence,
including their now-invalid strict-profile assumptions; do not reaccept old packets
by importing them. Missing/unsupported data is recorded UNKNOWN with the exact
reason. Save this two-entry reconciliation under
`docs/reviews/p0-2-historical-reconciliation.json`. This explicit fallback satisfies
honest reconciliation, not acceptance of the unavailable original review.

## Fixture matrix

`tests/test_recorded_reviews.py` must accept one valid nonempty fixture and reject
wrong-parent source, two parents with identical labels, unrelated ACCEPT text,
truncated verdict, first-turn-only lookup when the verdict is in a later turn,
missing/extra/duplicate criterion IDs, unknown disposition/schema, empty evidence,
unreadable source/cache, missing files and stale hashes. Each control asserts the
intended nonzero class and reason. Test both supported harness evidence adapters;
an unsupported shape is UNKNOWN rather than silently guessed. No live provider
access is required for fixture tests, and fixtures cannot prove live DSH readiness.

Review adequacy before releasing this amendment, then implement after P0.1
acceptance. Record both contract hashes and reviewer dispositions in the P0.2 record.

New acceptance reviews must end with one line
`REVIEW_CRITERIA_JSON: [{"id":"<criterion>","disposition":"AGREED"}]`, listing
every required ID and its actual disposition. Validate exactly one such source
line and compare its full ID/disposition map with the durable record. The full
verdict/evidence still remains in the source and record. This prevents a record
from claiming AGREED for a criterion the source called DISAGREED. Historical
prose without this format remains historical reconciliation, not newly eligible
acceptance. No generic verdict substring or nested quotation can supply this map.

### Footer authority — exact rules (revision `P0.2-interface-r4`)

The r1 revision said "end with one line" without defining what makes a line
authoritative. **A quoted or fenced example of the footer is not a verdict**, and
a record must never be able to promote one. Two adequacy reviews then found the
rules under-specified (r2) and, after those were fixed, a **new fail-open in the
source-selection path** (r3). r4 fixes all of it.

#### Authority scope and contradiction scope are different, and both are required

This distinction is the whole point, and conflating it was a real fail-open:

| scope | what it is | why |
|---|---|---|
| **authority** | the **single assistant message** whose footer supplies the map | reviewers emit a short "delivered, result was X" summary *after* the review, so requiring the footer to be last in the *turn* rejects honest reviews |
| **contradiction** | the **whole turn** (every assistant message, in emission order) | otherwise a reviewer could disown an earlier message by quoting a footer shape in a later one |

Measured: a turn whose message A said `P0.2-AC1 is DISAGREED` and whose message B
merely *showed* the footer format validated as **acceptance-eligible AGREED**,
because selection looked for the message containing a footer-like line — the
scanner chose its own input — and message A was then excluded as narration. With
turn-wide contradiction scanning the same record is `UNKNOWN`. Selecting "the last
non-empty message" does **not** fix it (the footer-bearing message is the last one
there); concatenating for *authority* does not either (the real review's footer
becomes non-final, because a summary follows it).

The record therefore carries **both**: `verdict_text` (authority) and a mandatory
`contradiction_evidence` (the wider text the verdict was drawn from). Omitting
`contradiction_evidence`, or supplying one that does not contain `verdict_text`,
is **INVALID** — measured: when the field was optional, omitting it restored the
fail-open exactly.

#### Line rules

**A line is non-empty** iff it contains a non-whitespace character after stripping
any trailing CR. CRLF input is therefore equivalent to LF input.

A **candidate** is a line of the authority message satisfying all of:

1. it begins at column 0 — no leading space or tab;
2. its first characters are exactly `REVIEW_CRITERIA_JSON:` (case-sensitive);
3. it is not inside a fenced code block;
4. it is not a blockquote line (begins with `>`);
5. it is not inside an HTML comment.

**Fence state** follows CommonMark for the parts that matter: an opener may be
indented **up to three spaces**; a closer must use the **same character**, be **at
least as long**, and carry no info string; an **unterminated** fence runs to the
end of the text. So a two-space-indented unterminated fence hides a following
footer, and `~~~` is not closed by ```.

**Comment state** is tracked **independently of fences**. An **unterminated**
`<!--` runs to the end of the text, exactly like an unterminated fence. A line is
excluded if it is inside a fence **or** a comment.

**Payload.** The text after the prefix must parse as a JSON array of objects that
each carry exactly a string `id` and a listed disposition. A line with the exact
prefix and an empty, truncated or otherwise unparseable payload is **still a
candidate** — it must not be skipped, because skipping it would let an earlier
well-formed footer become authoritative by default. Such a line yields `UNKNOWN`.

#### Authority

Exactly one candidate must exist, and it must be the **last non-empty line** of
the authority message. Both conditions are required and both fail closed:

| Observation | Result |
|---|---|
| no candidate | `CANNOT VERIFY` — no machine-readable map exists |
| two or more candidates | `UNKNOWN` — ambiguous; a record must not choose between them |
| exactly one candidate, not the final non-empty line | `UNKNOWN` — it may be superseded or quoted |
| exactly one candidate, final, unparseable payload | `UNKNOWN` |
| exactly one candidate, final, parseable | that line is **authoritative** |

#### Dispositions

The footer may use `AGREED`, `DISAGREED`, `CANNOT VERIFY` or `UNKNOWN`. A footer
`UNKNOWN` is stored in the record as **`CANNOT VERIFY`** and stays pending; it is
never promoted to a pass. Any other word (`PASS`, `OK`, …) is `UNKNOWN`.

#### Corroboration — structural, not lexical

Four revisions tried to *enumerate* the ways a reviewer withholds agreement, and
each was defeated by a construction not on the list:

| attempt | defeated by | collateral false positive |
|---|---|---|
| negation regex | `is **not** AGREED`, `is not marked AGREED`, `should not be AGREED` | — |
| + negation of a disposition | `would be AGREED if the run had completed` | `reject\w*` matched "Over-rejection controls pass" |
| + a hedge word list | `should be AGREED once the rerun lands`, `is tentatively AGREED` | bare `\bunless\b` matched "enabled unless exactly 0" |
| + committed-construction allowlist **with** a hedge list | `is AGREED awaiting the rerun` (while `pending the rerun` failed — same withholding, different word) | `but` matched "is AGREED, but I also reviewed AC2" |

The lesson is that **the qualifier lexicon is unbounded**, so no list closes it.
The rule is therefore **structural and lexical-free**:

> An `AGREED` footer counts only when the source contains a committed construction
> whose disposition token **ends its clause**.

"Ends its clause" needs no vocabulary. A clause ends at the end of the sentence or
at a **semicolon** — a semicolon is a real boundary here, because `is AGREED; the
run completed` and `is AGREED; this is superseded by AC2` have the same shape, and
the earlier sentence-only version let the second through. Table-cell boundaries
(`|`) and sentence-final markdown emphasis (`**AGREED.**`) also close a clause.

The tolerance has been **removed entirely**. A verdict sentence must end at its
disposition token. Nothing is admitted after it.

**Five attempts to admit a trailing clause safely, and every one ended with the
admitted shape being used to qualify the verdict.** Qualification is a matter of
*meaning*, not of syntax or vocabulary, so no rule of the form "recognise the
committed shape" can work:

| attempt | what it admitted |
|---|---|
| a hedge word list | any hedge not on the list |
| `committed AND NOT(hedges)` | `holds until the rerun` (unlisted) while `awaiting the rerun` failed |
| a **past-tense** requirement | `I deferred sign-off`, `the criterion was withdrawn`, `the packet reopened`, `I stopped at a partial result` — ten measured cases. **Tense marks *when*, not *whether committed***: a reviewer describing what they *did* writes in the past tense |
| **no finite verb** | `for now`, `in part`, `without the rerun`, `except for AC2`, `under protest` — all verbless, all withhold. This attempt also cost an honest form (`; the run completed and all 12 tests passed`) |
| **nothing** (this revision) | — |

**The cost is a documentation obligation, stated plainly: a reviewer must not
append a qualifying clause after its disposition token.** References belong
*before* the verdict, which reads naturally:

> `See section 3 for the log. P0.2-AC1 is AGREED.` ✅
> `P0.2-AC1 is AGREED; see section 3 for the log.` ❌ — pending, and the fix is to
> move the reference.

Three shapes after the token are **not** qualifying clauses, because none can
withhold agreement — the verdict is already asserted:

1. **A parenthesised citation.** `**P0.4-AC1 AGREED** (tests/… 21/21).`
2. **A short appositive restating how the verdict was reached.**
   `AGREED, reproduced.` / `AGREED, independently verified.`
3. **A table-cell boundary**, as above.

**The parenthetical rule is a shape plus a deliberately tiny word list, and the
history is its justification.** Three designs were tried:

| design | outcome |
|---|---|
| a hedge **word list** (`not\|never\|pending\|await\|unless\|…`) | an independent review leaked **12 of 18** hedged forms — including `(for now)`, `(in part)`, `(under protest)` and `(except for AC2)` |
| a citation **allowlist** with nouns and connectives | the session leaked **14 of 20** of its own attacks, because `no`, `all`, `see`, `per` and `line` are ordinary words: `(no exit)`, `(no run)`, `(line 0)` and `(tests 0/0)` all rode in |
| a **noun-only** allowlist | still leaked `(line 0)` and `(tests 0/0)`, which are syntactically **identical** to the honest `(tests 21/21)` — the difference is the number, not the words |
| **current:** paths, hashes, numbers, and pointer/result words only | **0 of 38** measured attacks leak; 8 of 9 honest controls admitted |

A parenthesis may therefore contain only paths, hashes, numbers, and
`see, below, above, section(s), exit, reproduced, verified, confirmed, checked,
measured, matched, replayed, independently` — pointers and the same result-verbs the
appositive rule already trusts. None is a quantifier, connective or noun that could
carry a condition.

**One honest form is refused, deliberately, and the cost is measured.** `(152 tests,
exit 0)` is refused because `tests` is a **noun**, and a noun plus a number is
syntactically identical whether the number is honest (`152`) or damning (`0`).
Measured: adding `tests` to the list immediately reopens `(tests 0/0)` as a leak. So
the trade is stated rather than hidden — a reviewer citing a count writes
`AGREED, exit 0.` or cites the path instead.

Tolerances 1 and 2 were added **after** a real acceptance review failed on them:
measured, `**P0.4-AC1 AGREED** (…)` and `**P0.4-AC3 AGREED, reproduced.**` were both
refused, so two honest criteria came out `CANNOT VERIFY`. Each tolerance is a
*shape* that cannot qualify, and each has a matching negative test proving a hedge
in the same position is still refused.

Two structural exceptions remain, and neither can qualify a verdict:

1. `for <criterion-id>`, which names the criterion the verdict applies to. **The
   named ID must be the criterion being judged** — measured, `P0.2-AC1 is AGREED
   for P0.2-AC2.` otherwise credited AC1 on a verdict naming a different criterion.
2. **A verdict label one sentence earlier.** `Status: AGREED. P0.2-AC1 satisfied.`
   names the disposition in one sentence and the criterion in the next. Only the
   explicit label form (`Verdict:`, `Disposition:`, `Status:`, `Result:`,
   `Outcome:`) binds across that boundary; a bare `is AGREED` still requires the ID
   in its own sentence. Leading markdown emphasis is tolerated (`**Verdict:
   AGREED.**`), because a bolded label is still a label. Three conditions gate the
   binding:
   - the **label sentence** must itself be clause-final;
   - the **criterion sentence** must not **retract** the label — measured,
     `Outcome: AGREED. P0.2-AC1 was deferred.` and `Verdict: AGREED. P0.2-AC1 is
     pending rerun.` otherwise corroborated;
   - the label **is** the verdict, so the criterion sentence after it is not
     treated as a qualifier of the label.

   **The retraction list is incomplete in the unsafe direction, and that is
   recorded rather than hidden.** An independent review found four further forms
   after the initial fix (`is on hold`, `was held back`, `needs the rerun`,
   `is under review`). Two things bound the exposure: the miss requires a **blunt
   self-contradiction inside one sentence pair** — a verdict stated and then
   immediately retracted, which no realistic acceptance review produces — and the
   obvious structural replacement was **tried and rejected because it measured
   worse**. A "bare past-participle confirmation" rule (after the ID and any
   copula, at most two content tokens ending in `-ed`) refused an honest
   `P0.2-AC1 reproduced and confirmed.` while **accepting
   `P0.2-AC1 is not confirmed.` and `P0.2-AC1 deferred.`** — a false `AGREED`,
   which is the unsafe direction. A narrower rule made the failure worse, so the
   list stays and the residual is documented.

A table-cell boundary (`|`) also closes the clause, because the following column is
evidence rather than a qualification.

**Attribution is sentence-scoped, not line-scoped.** An earlier version used the
ID's line ± 1, which attributed a neighbouring criterion's verdict to it: measured,
`The verdict for AC2 is DISAGREED. P0.2-AC1 is AGREED.` failed for AC1. A review
that discusses two criteria on adjacent lines must be acceptable.

`DISAGREED` and `CANNOT VERIFY` are held to the same standard, so a footer claiming
either is corroborated the same way.

**Footer lines are never corroboration.** A footer's `"disposition":"AGREED"` sits
adjacent to its ID by construction, so counting it would let the footer vouch for
itself. All footer-prefixed lines are blanked before the prose scan.

#### Required-ID population

The footer must map **exactly** the IDs in the independent required-criterion
manifest (`docs/reviews/contracts/<packet-id>.json`). A footer that omits a
required ID, adds an unknown ID, repeats an ID, or uses an unlisted disposition is
`UNKNOWN`. Missing IDs never default to `AGREED`. The record's criteria list must
satisfy map **equality** with the manifest, not a subset relation, and an empty
criteria list must fail.

#### Anti-hollow: the record must show work

Every ID the footer maps must carry nonempty `evidence`, **`procedure`** and
`observed_result` in the record; an empty or whitespace-only one is
`CANNOT VERIFY`. (An earlier revision's rule required the *prose* to mention each
ID; that punished honest layouts — a heading, an ID in a table header, a row with
the disposition first — so it was removed.)

**Stated limit, so the validator is not credited with more than it does:** a
nonempty check cannot distinguish a real command from plausible-sounding text.
`procedure: "inspected the source"` with `observed_result: "it appears correct"`
passes. Whether evidence is *substantive* is a review-quality property that this
validator does **not** enforce; only a hash-bound artifact produced by the harness
would, and that is out of scope here.

#### Required negative fixtures

Every negative fixture must be a **single-property mutation of the valid control**,
and the un-mutated twin must be asserted to pass — otherwise a fixture violating
two properties "fails for the intended reason" only by accident of check order.
Each must be rejected with its own stated class and reason, in
`tests/test_recorded_reviews.py`:

| Fixture | Expected |
|---|---|
| footer inside a column-0 fenced block, no top-level footer | `CANNOT VERIFY` |
| footer inside a **two-space-indented** unterminated fence | `CANNOT VERIFY` |
| `~~~` opener "closed" by a ``` line | `CANNOT VERIFY` |
| footer inside an **unterminated** HTML comment | `CANNOT VERIFY` |
| footer inside a closed HTML comment, real footer after | the real one is authoritative |
| top-level real footer **plus** a fenced example of a different map | the top-level one wins; the example is ignored |
| quoted old footer earlier, real footer last, both top-level | `UNKNOWN` (two candidates) |
| one candidate that is not the final non-empty line | `UNKNOWN` |
| exact prefix with empty payload | `UNKNOWN` |
| exact prefix with garbage payload | `UNKNOWN` |
| malformed trailing footer after an earlier valid one | `UNKNOWN` (not the earlier one) |
| **`DISAGREED` in an earlier message, footer-bearing later message** | **not eligible** (the N1 control) |
| **`contradiction_evidence` omitted** | **INVALID** |
| **`contradiction_evidence` not containing `verdict_text`** | **INVALID** |
| footer says `AGREED`, prose says `DISAGREED` for the same ID | `UNKNOWN` |
| prose `is not AGREED`, `is **not** AGREED`, `is not marked AGREED`, `should not be AGREED`, `I do not think … is AGREED` | uncorroborated → `CANNOT VERIFY` |
| prose `cannot agree`, `reject … outright`, `FAILS`, typo `DISAGREEED` | uncorroborated → `CANNOT VERIFY` |
| prose states the disposition **before** the ID | detected |
| footer `AGREED` with **no** affirmative statement for that ID | `CANNOT VERIFY` |
| a footer-only "agreement" (no prose at all) | `CANNOT VERIFY` — the footer cannot vouch for itself |
| footer omits a required ID | `UNKNOWN` |
| footer adds an ID absent from the manifest | `UNKNOWN` |
| footer repeats an ID with two dispositions | `UNKNOWN` |
| footer uses `PASS` or another unlisted disposition word | `UNKNOWN` |
| footer says `UNKNOWN` for an ID | `CANNOT VERIFY`, pending |
| record criteria list is empty | fails |
| empty/whitespace `evidence`, `procedure` or `observed_result` | `CANNOT VERIFY` |
| no footer at all, prose only | `CANNOT VERIFY` |
| blockquote or 4-space-indented footer | `CANNOT VERIFY` |
| **clean control: affirmative prose + footer + nonempty fields** | **acceptance-eligible** |
| **`rejection`/`rejected` used in an unrelated phrase** | **still eligible** — a false negation must not fire |

A valid control must also exist: one top-level final-line footer, prose agreeing
with it, and nonempty evidence for every required ID, which must validate as
acceptance-eligible. The validator returns the reason code and the test asserts on
it.

#### Known residual limits of the scan

Recorded rather than hidden:

* **The validator proves a record is SELF-CONSISTENT, not that the review
  happened.** Every binding it checks is internal to the files the record names:
  `verdict_text` equals the named turn's text, the ancestry equals the session
  header, the route equals the descriptor. A party who can author **both** the
  session log and the tracked source file satisfies all of them, because there is no
  trust anchor outside those files — no hash pinned at review time by a third party,
  no signature, nothing tying the log to the harness that produced it. This is
  inherent to a hash-only design and is why this document says hashes detect content
  changes rather than authenticating authorship. It is stated here rather than left
  implicit because it is the limit of what the whole mechanism claims.
* **Trailing dots are a platform alias, not an escape.** Windows collapses
  trailing dots, so `...`, `....` and `.....` all resolve to `.` — the repository
  root. A `...`-prefixed citation therefore names a real repository file and is
  admitted. Measured across `...` through `......`: every resolution stays inside
  `repo_root`, so containment is not bypassed. Recorded because a path with trailing
  dots *looks* like an escape attempt and is not one here.
* **A real file whose NAME reads as a hedge would be admitted as a citation.**
  The resolution rule asks whether a path exists, so `(pending.py)` is admitted if a
  file `pending.py` exists. Self-attack measured every combination tried — a real
  path plus a pointer word, and slash/comma/semicolon/colon/space/tab separators —
  and all refuse, so this is the rule's one residual. It is bounded: exploiting it
  requires *creating* a file in the repository, which is a visible act, and it falls
  under the self-consistency limit above rather than beside it.
* A typoed disposition (`DISAGREEED`) and a disagreement stated more than a region
  away from the ID are **not** detected as contradictions. Both leave the
  criterion uncorroborated and therefore pending, which is the safe direction, but
  they are not "detected".
* **A trailing summary that names a disposition, in the *same* message as the
  footer, makes the footer non-final** and the record fails. Measured: footer +
  `Review delivered; result was AGREED.` in one message → `not-final`; the same
  content in a *later* message → authoritative. This is deliberate: a line naming
  a disposition after the footer could be *superseding* it, so tolerating it would
  reopen the quoted-footer hole from the other side. Trailing lines that carry no
  disposition token and no criterion ID (a sign-off, a fence marker, blank lines)
  **are** tolerated, so the failure only occurs when the trailing text restates a
  verdict. The consequence is that a reviewer's final summary must not repeat a
  disposition in the footer's own message.
