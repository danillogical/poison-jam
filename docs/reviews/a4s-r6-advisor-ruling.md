# A4s-r6 Advisor hunk ruling — SUPERSEDED PLACEHOLDER

> **DO NOT CITE THIS FILE AS A RULING.**
>
> This was a **placeholder** created while the Advisor's response was still in flight. **The ruling
> arrived and is recorded in full at `docs/reviews/a4s-r6-advisor-hunk-ruling.md`** — that is the
> authoritative record, and it is the one every packet, review and plan must cite.
>
> This file is retained only as a provenance note: it shows what was asked and in what order, which is
> useful if the ruling's coverage is ever questioned. **Its "NOT YET RECEIVED" status below is stale and
> was true only when the file was written.** It contains no ruling text of its own.

**Superseded by:** `docs/reviews/a4s-r6-advisor-hunk-ruling.md` (verbatim, with BASIS / REVERSED BY /
RECORD IN for each of the six rulings).

**Advisor:** child `5c555969-dea9-4b47-be05-62aa0835cde2`, route `claude` / `claude-opus-5-5` @ `high`
— the persistent Advisor probed at startup in
`docs/reviews/startup-20260925-session-58e86358.md`, reused per `docs/agent-workflow.md` §4.4.

## What was asked

One consolidated brief plus fourteen deltas, covering:

| Ref | Subject | Record |
|---|---|---|
| brief | the five UNDECIDED hunks: exact base/local/upstream text + surrounding facts | `a4s-r6-advisor-hunk-brief.md` |
| A–D | the four bounded hunk questions (1–2, 5, 8, 9) | the brief |
| E | structural post-resolution check | delta 1 |
| F | the two `case 138` duplicates | delta 1 |
| G | LOCAL vs UPSTREAM vs combined for hunks 1–2 | deltas 2, 7 |
| H | hunk 5's merged form + the H1 rule property | delta 3 |
| I | hunk 9's merged form | delta 4 |
| J | hunk 8's disposition | delta 5 |
| K | the crux of hunks 1–2 (revised form) | delta 9 |
| L | the H1/H2 rule property | deltas 11, 12 |
| M | whether H2 needs the same repair | delta 12 |
| N | how far the post-resolution check should reach | delta 14 |

## Required response format (per the owner's instruction)

For each bounded ruling:

```
RULING:
BASIS: observed / inferred / uncertain
REVERSED BY:
RECORD IN:
```

## Expected content — ALL RECEIVED (checklist kept for provenance)

- [x] **Hunks 1–2** — ruled: **COMBINED form**, fixed precedence, exact C text, exactly one
      `bridge_KeResetEvent`, no `bridge_resolve_handle` tier.
- [x] **Hunk 5** — ruled: per-line combined form.
- [x] **Hunk 8** — ruled: LOCAL.
- [x] **Hunk 9** — ruled: full union.
- [x] **The H1 rule property** — ruled: accepted as proposed (containment includes deletions).
- [x] **H2** — ruled: keep the **original** precondition; define the **action** as **edit application**;
      the Session's added delete-vs-keep precondition was **rejected**.
- [x] **The post-resolution check** — ruled: **structural only**, with specified properties, predicates,
      controls and the `R-BUILD` backstop.

**All six are recorded verbatim in `docs/reviews/a4s-r6-advisor-hunk-ruling.md`.**

## Evidence the ruling rests on (all measured, all on disk)

`docs/reviews/a4s-r6-evidence-index.md` is the index. Headline measurements:

- the merge produces **duplicate `case 138` labels** in both dispatch switches and a **duplicate
  `bridge_KeResetEvent` definition** — all in **clean** hunks (`a4s-r6-structural-findings.md`);
- the accepted, currently-passing ctest **`jsrf_inplace_event_bridge`** requires guest `KEVENT`
  `SignalState` writes that upstream's bodies do not perform, with the address identity proven from
  source and all 15 helpers surviving the merge (`a4s-r6-contract-discrimination.md`);
- **ordinals 145 and 159 are declared** (live) while **108, 110, 138, 146 are not**
  (`a4s-r6-ordinal-reachability.md`, `a4s-r6-event-ordinals.md`);
- exactly **three** functions were changed by both sides outside any conflict, and all three merges are
  **unions** (`a4s-r6-clean-hunk-semantics.md`);
- the **H1 defect is measured and a repair is tested** against all nine real hunks, and **H2 has the
  same defect class** (`a4s-r6-h1-repair-proposal.md`).

## Standing constraints the ruling must respect

- **Hunk 4 (`HA`)** resolves to LOCAL; upstream's NABM trap enters unarmed with zero call sites.
- **`SCOPE`** = toolkit `src/` + `include/`; deleted names match only as quoted literals;
  comments/docs/tests/unbuilt scaffolds are inventoried, never failed.
- **Hunks 3, 6, 7** keep their mechanical classifications.
- Any criterion depending on **set G** must re-measure the baseline (currently 9 pass / 1 fail).
- The ruling is recorded **verbatim**, with the child ID and route, **before** any `A4s-r6` drafting.
