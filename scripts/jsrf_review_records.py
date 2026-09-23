"""Durable review-record validation for P0.2.

Replaces the session-specific heuristic in ``check-recorded-reviews.py``.  This
module is the *transaction* half of review ingestion: a recorded review is
acceptance-eligible only when its declared identity, ancestry, route, verdict and
evidence all bind to real bytes, and when its per-criterion verdicts come from an
authoritative machine-readable footer in the reviewer's own completed turn.

The rules implemented here are specified in ``docs/packets/p0-review-records.md``
revision ``P0.2-interface-r2``.  Two properties are load-bearing and fail closed:

* **Footer authority.**  A quoted or fenced *example* of the footer is never a
  verdict.  Exactly one candidate must exist and it must be the final non-empty
  line of the selected turn.  Anything else is UNKNOWN or CANNOT VERIFY.
* **Contradiction is not resolution.**  If the reviewer's prose states a
  disposition for a required ID that differs from the footer, that criterion is
  UNKNOWN.  The footer wins over silence, never over a stated disagreement.
"""
from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1
VALIDATOR_VERSION = 'jsrf-review-records/1'

AGREED = 'AGREED'
DISAGREED = 'DISAGREED'
CANNOT_VERIFY = 'CANNOT VERIFY'
UNKNOWN_DISPOSITION = 'UNKNOWN'
DISPOSITIONS = (AGREED, DISAGREED, CANNOT_VERIFY)

ELIGIBLE = 'acceptance-eligible'
FAILED = 'failed'
INVALID = 'invalid'

FOOTER_PREFIX = 'REVIEW_CRITERIA_JSON:'

# Exit codes are part of the interface; tests assert on them.
EXIT_ELIGIBLE = 0
EXIT_FAILED = 1
EXIT_INVALID = 2


class ReviewError(ValueError):
    """The record or its source cannot be validated (exit 2)."""


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def digest_file(path: Path) -> str:
    return _sha256_bytes(path.read_bytes())


def is_sha256(value: Any) -> bool:
    return isinstance(value, str) and re.fullmatch(r'[0-9a-fA-F]{64}', value) is not None


def load_json_unique(path: Path) -> Any:
    """Load JSON rejecting duplicate object keys at every nesting level."""
    def object_hook(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'duplicate JSON object key {key!r}')
            result[key] = value
        return result

    # `utf-8-sig` tolerates a leading BOM rather than failing on it.  Windows
    # tooling writes one by default (PowerShell's `Set-Content -Encoding UTF8`
    # does), and measured, every required-criterion manifest written that way was
    # rejected until the BOM was stripped.  Tolerating it changes no field.
    text = path.read_text(encoding='utf-8-sig')
    return json.loads(text, object_pairs_hook=object_hook)


# ── footer extraction ────────────────────────────────────────────────────────

_FENCE_RE = re.compile(r'^\s{0,3}(`{3,}|~{3,})')


def _fence_spans(lines: list[str]) -> list[tuple[int, int]]:
    """Return (start, end) line-index spans that are inside fenced code blocks.

    An unterminated fence stays open to the end of the text, which is what makes
    "a fence swallowing the footer" fail closed rather than exposing the footer.
    A fence closes only on a marker of the same character and at least the same
    length, which is the CommonMark rule and stops ``` from being closed by ~~~.
    """
    spans: list[tuple[int, int]] = []
    open_at: int | None = None
    open_char = ''
    open_len = 0
    for index, line in enumerate(lines):
        match = _FENCE_RE.match(line)
        if open_at is None:
            if match:
                open_at = index
                open_char = match.group(1)[0]
                open_len = len(match.group(1))
            continue
        if match:
            marker = match.group(1)
            if marker[0] == open_char and len(marker) >= open_len:
                spans.append((open_at, index))
                open_at = None
    if open_at is not None:
        spans.append((open_at, len(lines) - 1))
    return spans


def _in_spans(index: int, spans: list[tuple[int, int]]) -> bool:
    return any(start <= index <= end for start, end in spans)


def _html_comment_spans(text: str) -> list[tuple[int, int]]:
    """Byte spans inside HTML comments.

    An **unterminated** ``<!--`` runs to the end of the text, mirroring the fence
    rule.  Treating it as inert would let a footer inside a stray comment become
    authoritative; swallowing it instead can only produce a spurious failure,
    which is the safe direction.
    """
    spans: list[tuple[int, int]] = []
    position = 0
    while True:
        start = text.find('<!--', position)
        if start < 0:
            break
        end = text.find('-->', start + 4)
        if end < 0:
            spans.append((start, len(text)))
            break
        spans.append((start, end + 3))
        position = end + 3
    return spans


def _line_offsets(text: str) -> list[int]:
    offsets = [0]
    for match in re.finditer(r'\n', text):
        offsets.append(match.end())
    return offsets


def extract_footer(text: str) -> dict[str, Any]:
    """Locate the authoritative ``REVIEW_CRITERIA_JSON:`` footer.

    Returns a dict with ``status`` in ``('authoritative', 'none', 'ambiguous',
    'not-final')``, the raw line, its index, and the candidate list.  Every
    non-authoritative status is a closed failure, never an empty success.
    """
    if not isinstance(text, str):
        return {'status': 'none', 'reason': 'source text is not a string', 'candidates': []}

    lines = text.split('\n')
    fences = _fence_spans(lines)
    comments = _html_comment_spans(text)
    offsets = _line_offsets(text)

    candidates: list[dict[str, Any]] = []
    for index, line in enumerate(lines):
        if not line.startswith(FOOTER_PREFIX):
            continue
        if _in_spans(index, fences):
            continue
        if _in_spans(offsets[index], comments):
            continue
        candidates.append({'index': index, 'line': line})

    if not candidates:
        return {'status': 'none',
                'reason': 'no top-level REVIEW_CRITERIA_JSON footer in the source turn',
                'candidates': []}
    if len(candidates) > 1:
        return {'status': 'ambiguous',
                'reason': f'{len(candidates)} top-level footers; authority is undefined',
                'candidates': candidates}

    candidate = candidates[0]
    # Finality is measured against the last line that could plausibly be *part of
    # the verdict*, not the last line of the message.  Reviewers append a sign-off
    # or a one-line summary after the footer, and whether that lands in the same
    # assistant message or a later one is an artifact of how the model emits
    # bubbles -- measured: footer + "Review delivered; result was AGREED." in one
    # message returned `not-final`, while the same content in a later message was
    # authoritative.  Being hostage to that is brittle, so trailing lines that are
    # plainly not part of the map are tolerated.
    final = max((i for i, line in enumerate(lines) if line.strip()), default=-1)
    if candidate['index'] != final and _only_trailing_signoff(
            lines[candidate['index'] + 1:final + 1]):
        final = candidate['index']
    if candidate['index'] != final:
        return {'status': 'not-final',
                'reason': 'the only footer is not the final non-empty line of the turn',
                'candidates': candidates, 'final_nonempty_line': final}
    # A line carrying the exact prefix but no parseable payload is still a
    # *candidate*: it must not be skipped so that an earlier, well-formed footer
    # becomes authoritative by default.
    try:
        parse_footer_map(candidate['line'])
    except ReviewError as error:
        return {'status': 'malformed',
                'reason': f'the final footer line does not carry a valid map: {error}',
                'candidates': candidates}
    return {'status': 'authoritative', 'line': candidate['line'],
            'index': candidate['index'], 'candidates': candidates}


_JSON_ARRAY_RE = re.compile(r'REVIEW_CRITERIA_JSON:\s*(\[.*\])\s*$')


def _only_trailing_signoff(trailing: list[str]) -> bool:
    """Whether the lines after a footer are plainly not part of the map.

    Deliberately narrow.  A trailing line qualifies only if it is empty, a fence
    marker, or a short acknowledgement/sign-off that carries **no disposition
    token and no criterion id** -- because a line that names a criterion or a
    disposition could be superseding the footer, and tolerating that would reopen
    the quoted-footer hole from the other side.
    """
    for line in trailing:
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith(('```', '~~~')):
            continue
        if any(token in stripped for token in DISPOSITIONS):
            return False
        if re.search(r'P0\.\d+-AC\d+', stripped):
            return False
        if re.search(r'REVIEW_CRITERIA_JSON', stripped):
            return False
        if len(stripped) > 160:
            return False
        if not re.search(r'(?i)\b(?:review|delivered|sent|result|summary|verdict|'
                         r'complete|done|above|regards|thanks|signed)\b', stripped):
            return False
    return True


def parse_footer_map(line: str) -> dict[str, str]:
    """Parse the footer's JSON array into an id -> disposition map.

    Raises ReviewError for any malformed shape.  A malformed footer is never an
    empty successful map.  The record schema admits only AGREED / DISAGREED /
    CANNOT VERIFY, so a footer that says ``UNKNOWN`` for an ID is stored as
    CANNOT VERIFY and stays pending -- it is never promoted to a pass.
    """
    match = _JSON_ARRAY_RE.match(line)
    if not match:
        raise ReviewError('footer does not end with a JSON array')
    try:
        parsed = json.loads(match.group(1))
    except Exception as error:
        raise ReviewError(f'footer JSON is unreadable: {error}') from error
    if not isinstance(parsed, list) or not parsed:
        raise ReviewError('footer JSON must be a non-empty array')
    mapping: dict[str, str] = {}
    for index, entry in enumerate(parsed):
        if not isinstance(entry, dict) or set(entry) != {'id', 'disposition'}:
            raise ReviewError(f'footer entry {index} must have exactly id and disposition')
        cid, disposition = entry['id'], entry['disposition']
        if not isinstance(cid, str) or not cid:
            raise ReviewError(f'footer entry {index} has an invalid id')
        if disposition == UNKNOWN_DISPOSITION:
            # Legal in a reviewer's footer, but not a disposition the record
            # schema can store: it maps to a pending CANNOT VERIFY.
            mapping[cid] = CANNOT_VERIFY
            continue
        if disposition not in DISPOSITIONS:
            raise ReviewError(
                f'footer entry {index} uses unsupported disposition {disposition!r}; '
                f'expected one of {", ".join(DISPOSITIONS + (UNKNOWN_DISPOSITION,))}')
        if cid in mapping:
            raise ReviewError(f'footer repeats criterion id {cid!r}')
        mapping[cid] = disposition
    if len(mapping) != len({entry.get('id') for entry in parsed}):
        raise ReviewError('footer repeats a criterion id')
    return mapping


# ── per-ID prose scan ────────────────────────────────────────────────────────

# A window of characters around an ID in which a disposition token is attributed
# to it.  Generous on purpose: over-attribution can only produce a spurious
# UNKNOWN, while under-attribution would let a wrong AGREED stand.
_PROSE_WINDOW = 240

# Corroboration span: how close an *affirmative* disposition must sit to the ID.
# Much tighter than the attribution window, because this is the direction where
# under-detection is unsafe: an unbounded negation pattern cannot be completed
# ("is **not** AGREED", "is not marked AGREED", "should not be AGREED", "I do not
# think … is AGREED" all defeated a stricter one), so rather than trying to
# recognise every negation, the footer's AGREED must be *positively* corroborated
# near the ID.  Anything uncorroborated stays pending instead of passing.
_CORROBORATION_WINDOW = 160

# Negation of *agreement*.  Kept deliberately narrow, because a broad pattern
# produces false negations on honest reviews: `reject\w*` matched the phrase
# "Over-rejection controls pass" in a real review and left three criteria
# uncorroborated.  A false negation costs a pending criterion (safe but noisy);
# a missed one is caught by the corroboration requirement below, which is why
# this list can stay conservative.
# ── corroboration is an ALLOWLIST, and that is the whole design ──────────────
#
# Three attempts to enumerate the ways a reviewer can *withhold* agreement all
# failed the same way, each defeated by a word that was not on the list:
#
#   1. `reject\w*` matched "Over-rejection controls pass" (false positive).
#   2. a bare `\bunless\b` matched the C expression "enabled unless exactly 0".
#   3. a hedge list containing `would be`/`yet`/`pending` was defeated by
#      "should be AGREED once the rerun lands", "is tentatively AGREED",
#      "is provisionally AGREED", and a hedge on a later clause.
#
# Enumeration cannot be completed, because the space of hedges is unbounded.  So
# the predicate is inverted: a disposition is corroborated only by a **committed
# construction** -- a copula immediately followed by the token -- and everything
# else is uncorroborated.  An unlisted hedge then costs a *pending criterion*
# rather than a false AGREED, which is the safe direction, and the broad-token
# false positives disappear as a class because no broad token is consulted.
_COMMITTED_RE = re.compile(
    r'(?:'
    # "P0.2-AC1 is AGREED", "**P0.2-AC1** is **AGREED**", "is: AGREED"
    r'\b(?:is|are|was|were|becomes?|remains?)\b[\s*_`\'":=-]{0,4}'
    r'(?:\*\*|__|`)?\s*(?:AGREED|DISAGREED|CANNOT[ _-]?VERIFY)\b'
    # "Verdict: AGREED", "Disposition = AGREED"
    r'|\b(?:verdict|disposition|result|status|outcome)\b[\s*_`\'":=-]{0,4}'
    r'(?:\*\*|__|`)?\s*(?:AGREED|DISAGREED|CANNOT[ _-]?VERIFY)\b'
    # "**P0.2-AC1 — AGREED.**" and "P0.2-AC1: AGREED" -- a criterion ID, an
    # optional em-dash/colon, then the token.  This is the shape the real P0.1
    # review uses for every criterion, so it must be recognized.
    r'|P0\.\d+-AC\d+[\s*_`\'":=—–-]{0,6}(?:\*\*|__|`)?\s*'
    r'(?:AGREED|DISAGREED|CANNOT[ _-]?VERIFY)\b'
    # A table cell whose *only* content is the disposition: "| AGREED |"
    r'|\|\s*(?:\*\*|__|`)?(?:AGREED|DISAGREED|CANNOT[ _-]?VERIFY)'
    r'(?:\*\*|__|`)?\s*(?=\|)'
    r')',
    re.IGNORECASE)

# The guard is checked against the **sentence containing** the matched
# construction, not the match span alone.  Measured: checking only the span let
# "is tentatively AGREED" through, because the copula branch matched ` AGREED`
# (the looser bare-token alternative won) and the adverb sat outside it.  A
# sentence is the unit a reviewer hedges in.
_SENTENCE_SPLIT_RE = re.compile(r'(?<=[.;:!?])\s+|\n')

# Adverbs that sit *between* the copula and the token and withhold commitment.
# Checked against the span the committed pattern matched, so it is a guard on an
# already-narrow construction rather than a second enumeration.
_HEDGE_ADVERBS = re.compile(
    r'\b(?:tentatively|provisionally|possibly|perhaps|arguably|presumably|'
    r'seemingly|apparently|reportedly|nominally|effectively|essentially|'
    r'not|never|no\s+longer|hardly|barely|partially|partly|mostly|nearly|'
    r'almost|conditionally|contingently|hypothetically|theoretically)\b',
    re.IGNORECASE)

# Hedging *clauses*: a conditional or adversative attached to the same sentence.
# Narrower than a word list because each is a construction, and each was measured
# to let a withheld verdict corroborate.
_HEDGE_CLAUSE_RE = re.compile(
    r'(?:\bwould\s+(?:be|have|only\s+be|become)\b'
    r'|\b(?:if|unless|suppose|assuming|provided\s+that)\b'
    r'|\b(?:yet|however|although|though)\b'
    r'|\b(?:pending|unresolved|superseded|contingent)\b'
    r'|\bnot\s+yet\b|\bremains?\s+open\b|\bstill\s+open\b'
    r'|\bsubject\s+to\b|\bonce\s+the\b)',
    re.IGNORECASE)


def _sentence_spans(text: str) -> list[tuple[int, int]]:
    """Spans of sentences, used as the unit of attribution and corroboration.

    A boundary is a `.`/`!`/`?` followed by whitespace or end of text, a blank
    line, or a newline that starts a markdown structural line (table row, bullet,
    heading, blockquote).  Ordinary soft-wrapped prose stays one sentence, which
    matters: a verdict wrapped across two lines must not look sentence-final on
    its first line.
    """
    spans: list[tuple[int, int]] = []
    start = 0
    lines = text.split('\n')
    offset = 0
    for index, line in enumerate(lines):
        line_start = offset
        line_end = offset + len(line)
        stripped = line.strip()
        structural = bool(re.match(r'^(?:\||[-*+]\s|#{1,6}\s|>|\d+\.\s)', stripped))
        blank = not stripped
        if structural or blank:
            if line_start > start:
                spans.append((start, line_start))
            spans.append((line_start, line_end))
            start = line_end + 1
        offset = line_end + 1
    if start < len(text):
        spans.append((start, len(text)))

    # Split further at sentence terminators.  The terminator may be followed by
    # markdown emphasis that *closes* the sentence ("**AGREED.** Next."), so the
    # lookahead allows closing markers before the whitespace.
    refined: list[tuple[int, int]] = []
    for begin, end in spans:
        chunk = text[begin:end]
        position = 0
        for match in re.finditer(r'[.!?](?:[\*_`\'"\)\]]*)(?=\s|$)', chunk):
            cut = match.end()
            refined.append((begin + position, begin + cut))
            position = cut
        if position < len(chunk):
            refined.append((begin + position, end))
    return [(b, e) for b, e in refined if text[b:e].strip()]


def _sentence_at(text: str, index: int) -> str:
    for begin, end in _sentence_spans(text):
        if begin <= index < end:
            return text[begin:end]
    return text


def _sentence_before(text: str, index: int) -> str | None:
    """The sentence ending immediately before ``index``, if any."""
    previous: str | None = None
    for begin, end in _sentence_spans(text):
        if end <= index:
            previous = text[begin:end]
        else:
            break
    return previous


# Explicit verdict labels: these name a disposition as a field, so they can bind
# to a criterion named in the following sentence.  Leading markdown emphasis is
# tolerated: measured, `**Verdict: AGREED**` lost its verdict to a leading `**`.
_LABEL_RE = re.compile(
    r'[\s*_`>]*\b(?:verdict|disposition|result|status|outcome)\b[\s*_`\'":=-]{0,4}'
    r'(?:\*\*|__|`)?\s*(?:AGREED|DISAGREED|CANNOT[ _-]?VERIFY)\b',
    re.IGNORECASE)

# Markers that make a criterion sentence a *retraction* of a bound label rather
# than a confirmation of it.  Consulted only for the one sentence a label binds
# to, and only to *refuse*.
#
# This list is **incomplete in the unsafe direction and that is recorded rather
# than hidden**: a review can write `Verdict: AGREED.` and then withhold in words
# not listed here.  Two things bound the exposure, and both were measured:
#
#   * The miss requires a **blunt self-contradiction inside one sentence pair** --
#     a verdict stated and then immediately retracted.  No realistic acceptance
#     review does that.
#   * The obvious structural replacement was tried and **rejected because it is
#     worse**.  A "bare past-participle confirmation" rule (after the ID and any
#     copula, at most two content tokens ending in `-ed`) was measured on 12
#     sentences: it refused `P0.2-AC1 reproduced and confirmed.` (honest) and
#     **accepted `P0.2-AC1 is not confirmed.` and `P0.2-AC1 deferred.`** -- a false
#     AGREED, which is the unsafe direction.  A narrower rule made the failure
#     worse, so the list stays.
_RETRACTION_RE = re.compile(
    r'(?:\b(?:was|were|is|are|remains?|stays?)\s+(?:deferred|withdrawn|withheld|'
    r'pending|unresolved|open|provisional|tentative|conditional|superseded|'
    r'outstanding|reopened|blocked|held\s+back|on\s+hold|under\s+review)\b'
    r'|\b(?:deferred|withdrawn|withheld|reopened|superseded)\b'
    r'|\bneeds?\s+the\b|\brequires?\s+the\b'
    r'|\bnot\s+yet\b|\bno\s+longer\b|\bpending\s+the\b|\bsubject\s+to\b'
    r'|\bawait\w*|\buntil\b|\bnot\s+(?:met|satisfied|confirmed|verified)\b)',
    re.IGNORECASE)


def _sentence_retracts(sentence: str) -> bool:
    """Whether a criterion sentence retracts a verdict rather than confirming it."""
    return _RETRACTION_RE.search(sentence) is not None


# A parenthesised citation is admitted only as a *shape* plus a few safe words.
#
# Three designs were tried; the first two were defeated and the third is bounded by
# measurement rather than by hope:
#
#   1. a hedge word list: an independent review leaked 12 of 18 hedged forms;
#   2. a citation allowlist including nouns and connectives: the session leaked 14 of
#      20 of its OWN attacks, because `no`, `all`, `see`, `per` and `line` are
#      ordinary words -- `(no exit)`, `(no run)`, `(line 0)` and `(tests 0/0)` rode in;
#   3. a noun-only allowlist: still leaked `(line 0)` and `(tests 0/0)`, which are
#      syntactically IDENTICAL to the honest `(tests 21/21)` -- the difference is the
#      number, not the words.
#
# No vocabulary separates a citation from a qualification.  So a parenthesis may now
# contain only: paths, hashes, numbers, and the words below.  Those are *pointers*
# and the same result-verbs the appositive rule already trusts; none of them can
# assert a condition, and none is a quantifier, connective or noun that could carry
# one.  Everything else refuses.
CITATION_POINTERS = frozenset({
    'see', 'below', 'above', 'section', 'sections',
    'exit', 'reproduced', 'verified', 'confirmed', 'checked', 'measured',
    'matched', 'replayed', 'independently',
})

# Retained as the canonical statement of the REJECTED designs, so the vocabulary
# that leaked is pinned by tests and cannot be reintroduced by accident.
CITATION_CONNECTIVES = frozenset({
    'no', 'not', 'none', 'never', 'all', 'any', 'some', 'both', 'each', 'every',
    'and', 'or', 'but', 'yet', 'if', 'unless', 'except', 'only', 'still', 'again',
    'also', 'however', 'though', 'although', 'while', 'when', 'once', 'until',
    'before', 'after', 'since', 'because', 'so', 'then', 'than', 'as', 'without',
    'with', 'per', 'at', 'in', 'on', 'of', 'to', 'from', 'for', 'by', 'into',
})

CITATION_NOUNS = frozenset({
    'test', 'tests', 'suite', 'suites', 'case', 'cases', 'check', 'checks',
    'run', 'runs', 'log', 'logs', 'file', 'files', 'line', 'lines', 'section',
    'sections', 'table', 'row', 'rows', 'exit', 'code', 'output', 'result',
    'results', 'evidence', 'criterion', 'criteria', 'hash', 'sha', 'command',
    'commands', 'pass', 'passes', 'passed', 'ok',
})


def _citation_only(parenthesis: str, repo_root: Path | None) -> bool:
    """True when a parenthesised clause is a citation and cannot be a condition.

    A parenthesis is admitted only when every token in it is one of:

      * a **verifiable path** -- a path-shaped token whose target EXISTS in the
        repository (or is a run directory under `logs/runs`),
      * a **number** or hash,
      * a **pointer** word from `CITATION_POINTERS`.

    The verifiability requirement is the whole point, and this is the fourth design.
    Three earlier ones failed:

      1. a hedge word list -- 12 of 18 hedged forms leaked;
      2. a citation allowlist with nouns -- 14 of 20 of the session's own attacks
         leaked, because `no`, `all`, `see`, `per` and `line` appear in both roles;
      3. a noun-only allowlist -- still leaked `(line 0)` and `(tests 0/0)`;
      4. **this one**, after an independent review defeated (3) by writing the hedge
         in *path shape*: `(pending/awaiting.py)`, `(deferred/rerun.md)`,
         `(provisional.json)` -- 8 of 8 leaked, because the word check ran on the
         string AFTER paths were stripped, so the hedge was deleted before it was
         ever examined.

    No grammar separates `tests/test_dump_controls.py` from `pending/awaiting.py`:
    both are `<word>/<word>.<ext>`.  What separates them is that one exists.  So the
    check no longer *subtracts* shapes and inspects the remainder -- it asks whether
    each path-shaped token resolves, which is a property of the world rather than of
    the spelling.
    """
    inner = parenthesis.strip()
    # The regex that matches a parenthesis starts with the emphasis characters that
    # precede it (`** (` ...), so a bolded citation arrives here with a leading `**`.
    # Measured: `**P0.4-AC1 AGREED** (`tests/foo.py` 21/21).` was refused for this
    # reason alone -- the honest citation this whole tolerance exists to admit.
    inner = inner.lstrip('`*_\'" \t')
    if not (inner.startswith('(') and inner.endswith(')')):
        return False
    inner = inner[1:-1]
    # Markdown emphasis is stripped PER TOKEN, not from the whole string first.
    # Measured: stripping backticks globally refused four honest citations, because
    # `` `tests/test_dump_controls.py` `` became a token whose backticks were already
    # gone but whose neighbours had been merged by the substitution.  Stripping per
    # token keeps each path intact and still lets `(verified)` through.
    for token in re.split(r'[\s,;]+', inner):
        token = token.strip().strip('`*_\'"')
        if not token:
            continue
        if re.fullmatch(r'[0-9a-fA-F]{7,}', token):        # hash
            continue
        if re.fullmatch(r'\d+(?:[./:]\d+)*', token):      # number or ratio
            continue
        if token.casefold() in CITATION_POINTERS:          # pointer word
            continue
        if _resolves_to_a_real_path(token, repo_root):     # verifiable citation
            continue
        return False
    return True


def _resolves_to_a_real_path(token: str, repo_root: Path | None) -> bool:
    """True when a token is a path-shaped citation naming something IN the repo.

    Containment is the point, and it is the fifth design.  Round 4 defeated the
    fourth by exploiting pathlib semantics: `repo_root / candidate` with an
    *absolute* candidate replaces the base, so any file anywhere on disk resolved --

        (C:/Windows/System32/drivers/etc/hosts)   (//AGENTS.md)
        (C:/Users/.../Temp/p02ac1/awaiting-the-rerun.md)

    all leaked.  The reviewer writes the verdict AND can create files, so "resolves
    on this machine" is not an independent property.  "Resolves *inside the
    repository*" is: it is bounded by the same tree the review is about.

    A token is rejected before resolution when it carries a drive letter, a UNC
    prefix, a leading separator or a `..` segment -- each of which can escape -- and
    the resolved path is additionally required to be inside `repo_root`.
    """
    if repo_root is None:
        return False
    looks_like_path = bool(re.search(r'[/\\]', token)) or bool(
        re.search(r'\.(?:py|json|md|txt|log|ps1|sh|c|h|cpp|xbe|zstd|jsonl)$',
                  token, re.IGNORECASE))
    if not looks_like_path:
        return False
    # Shapes that can name something outside the repository, refused outright.
    if re.match(r'^[A-Za-z]:', token):          # drive letter
        return False
    if token.startswith(('//', '\\\\')):            # UNC
        return False
    if token.startswith(('/', '\\')):              # rooted
        return False
    # Split into segments BEFORE stripping separators.  Measured bug: `lstrip('./')`
    # on `../AGENTS.md` removes the two dots as well as the slash, leaving
    # `AGENTS.md`, which resolves -- so the `..` guard below never saw it.
    candidate = token.replace('\\', '/').lstrip('/')
    segments = [s for s in candidate.split('/') if s not in ('', '.')]
    if not segments or '..' in segments:
        return False
    try:
        base = repo_root.resolve()
    except OSError:
        return False
    for target in (base.joinpath(*segments),
                   base / 'logs' / 'runs' / Path(*segments)):
        # `.resolve()` is LOAD-BEARING, not tidiness: it follows symlinks BEFORE the
        # containment test below, so a symlink inside the repository pointing outside
        # resolves to its outside target and is refused.  Comparing the lexical path
        # instead would admit it.  An independent review confirmed the ordering is
        # correct by reading this code -- it could not measure a symlink here
        # (`WinError 1314`, privilege not held) -- and asked for this comment so a
        # future reader does not drop `.resolve()` as an optimisation.
        try:
            resolved = target.resolve()
        except OSError:
            continue
        if resolved != base and base not in resolved.parents:
            continue
        if resolved.is_file() or resolved.is_dir():
            return True
    return False


def _is_clause_final(text: str, token_end: int,
                     expected_criterion: str | None = None,
                     repo_root: Path | None = None) -> bool:
    """True when nothing qualifying follows the disposition token.

    This is the structural half of the corroboration rule.  A token followed by
    further prose is being *qualified*, and the vocabulary of qualifiers is
    unbounded -- measured defeats include `awaiting the rerun`, `pending the
    rerun`, `subject to the rerun`, `superseded by AC2`, `unless …`, `once …` and
    `would be … if …`.  So the rule is not "the token is not hedged" but "the
    token ends its clause", which needs no lexicon.

    **A clause** ends at the end of the sentence or at a semicolon.  A semicolon
    is a *clause* boundary in the sense that matters here: `is AGREED; the run
    completed` and `is AGREED; this is superseded by AC2` have the same shape, so
    the qualifier after the semicolon must be judged, not ignored.  That is why
    the earlier sentence-scoped version let `superseded by AC2` through.

    **One narrow tolerance**, because refusing everything after the token would
    also refuse honest prose that continues the clause: a trailing clause is
    accepted only when it begins with a **plain additive conjunction** and carries
    no uncertainty marker.  The default is refusal, so an unlisted qualifier that
    does not begin with a conjunction stays uncorroborated -- which is what makes
    the tolerance safe rather than another enumeration.  Note the tolerance is
    checked against the whole remainder, so `is AGREED; the run completed and all
    12 tests passed` passes on `and`, while `is AGREED awaiting the rerun` fails
    because it begins with no conjunction at all.
    """
    rest = text[token_end:]
    if re.fullmatch(r"[\s*_`'\")\]|.,;:!?—-]*", rest):
        return True
    # A parenthesised citation is evidence, not a qualification of the verdict.
    #
    # Measured on a real acceptance review: `**P0.4-AC1 AGREED** (tests/... 21/21).`
    # and `**P0.4-AC3 AGREED, reproduced.**` were both refused, so two honest
    # criteria came out CANNOT VERIFY.
    #
    # Two designs were tried and both were defeated by adversarial review, which is
    # why this one is a *grammar* rather than a vocabulary:
    #
    #   1. A hedge WORD LIST (`not|never|pending|await|unless|...`) -- an independent
    #      review leaked 12 of 18 hedged forms through it.
    #   2. An ALLOWLIST of citation words -- the session defeated its own design with
    #      14 of 20 hedges, because `no`, `all`, `see`, `per` and `line` are ordinary
    #      words: `(no exit)`, `(no run)`, `(line 0)`, `(tests 0/0)` all rode in.
    #
    # The common failure is that a *vocabulary* cannot separate a citation from a
    # qualification, because the same words appear in both.  A citation has a shape
    # instead: an optional noun, an optional count or path, and nothing that asserts
    # a condition.  So a parenthesis is admitted only when, after removing paths,
    # hashes and numbers, it is EMPTY or consists solely of a short noun phrase --
    # and any connective or quantifier (`no`, `all`, `and`, `but`, `if`, `yet`,
    # `except`, `only`, `still`, `again`, `not`) refuses outright.
    parenthetical = re.match(r"^[\s*_`'\"]*\((?:[^()]|\([^()]*\))*\)", rest)
    if parenthetical:
        if not _citation_only(parenthetical.group(0), repo_root):
            return False
        return _is_clause_final(text, token_end + parenthetical.end(),
                                expected_criterion, repo_root)
    # A short appositive after the verdict is a restatement of it, not a
    # qualification: `AGREED, reproduced.` names what was done to reach it.
    appositive = re.match(r"^[\s*_`'\"]*,[\s*_`'\"]*"
                          r"(?:reproduced|verified|confirmed|re-?run|checked|"
                          r"measured|matched|replayed|independently\s+\w+)\b",
                          rest, re.IGNORECASE)
    if appositive:
        return _is_clause_final(text, token_end + appositive.end(),
                                expected_criterion, repo_root)
    # A table cell boundary closes the clause: in `| P0.2-AC1 | AGREED | evidence |`
    # the disposition cell ends at the next pipe, and the following column is
    # evidence, not a qualification of the verdict.
    if re.match(r'^[\s*_`\'"]*\|', rest):
        return True
    stripped = rest.lstrip(' \t*,;—-"\'`')
    # "Verdict: AGREED for P0.2-AC1" -- naming the criterion the verdict applies
    # to is not a qualification of it.  The named ID must be supplied by the
    # caller; measured, `P0.2-AC1 is AGREED for P0.2-AC2` credited AC1 on a
    # verdict that names a *different* criterion.
    if re.match(r'(?i)^for\s+P0\.\d+-AC\d+\b', stripped):
        named = re.match(r'(?i)^for\s+(P0\.\d+-AC\d+)\b', stripped).group(1)
        if expected_criterion is not None and named.upper() != expected_criterion.upper():
            return False
        remainder = re.sub(r'(?i)^for\s+P0\.\d+-AC\d+\b', '', stripped)
        if re.fullmatch(r"[\s*_`'\")\]|.,;:!?—-]*", remainder):
            return True
    # Nothing else.  Five attempts to admit a trailing clause safely each ended
    # with the admitted shape being used to qualify the verdict, because
    # qualification is a matter of meaning, not of syntax or vocabulary.  So the
    # verdict sentence must end at its disposition.
    return False


# ── the tolerance is GONE, and this is the final design ──────────────────────
#
# Five attempts to admit a trailing clause safely, each defeated:
#
#   1. a hedge word list          -> defeated by an unlisted hedge
#   2. committed AND NOT(hedges)  -> `awaiting` failed while `holds until` passed
#   3. a past-tense requirement   -> **tense marks WHEN, not WHETHER COMMITTED**
#   4. no finite verb             -> **a verbless fragment can still qualify**:
#      `for now`, `in part`, `without the rerun`, `except for AC2`, `under protest`
#      are all verbless and all withhold
#
# Attempt 4 also cost an honest form (`; the run completed and all 12 tests
# passed`, refused because `completed` is a finite verb).
#
# The pattern is now unmistakable: **any admitted shape can be made to qualify**,
# because qualification is a property of meaning, not of syntax or vocabulary.  So
# the rule admits *nothing* after the token.  A verdict sentence must end at its
# disposition.
#
# The cost is real and is a documentation obligation, not a hidden failure: a
# reviewer must not append a clause after the token.  References belong *before*
# the verdict -- `See section 3 for the log. P0.2-AC1 is AGREED.` -- which reads
# naturally and is stated in the amendment.
_FINITE_VERB_RE = None  # retained name for import compatibility; unused
_QUALIFIER_RE = None    # retained name for import compatibility; unused

_NEGATION_RE = re.compile(
    r'(?:\bnot\b|\bnever\b|\bisn\'?t\b|\bwasn\'?t\b|\baren\'?t\b|\bno longer\b|'
    r'\bcannot\b|\bcan\'?t\b|\bdo not\b|\bdon\'?t\b|\bdoes not\b|\bdoesn\'?t\b|'
    r'\bunproven\b|\bunverified\b|\bunsupported\b|\bnot\s+marked\b|'
    r'\bfails?\b|\bfailed\b|\brejected\b|\brejects\b)',
    re.IGNORECASE)

# A negation only matters when it is near a disposition token; a `not` elsewhere
# on a long line is not a statement about this criterion.  The pattern allows the
# criterion ID to sit between the negation and the disposition, because reviewers
# write "I do not think P0.2-AC1 is AGREED" -- measured to defeat a pattern that
# required the disposition to follow the negation directly.
_NEGATED_DISPOSITION_RE = re.compile(
    r'(?:\bnot\b|\bnever\b|\bisn\'?t\b|\bwasn\'?t\b|\bno longer\b|\bcannot\b|'
    r'\bcan\'?t\b|\bdo not\b|\bdon\'?t\b|\bdoes not\b|\bdoesn\'?t\b|'
    r'\bnot\s+marked\b)'
    r'[\s*_`\'"]*(?:[A-Za-z0-9._\-\'"`*]+[\s*_`\'"]+){0,6}'
    r'(?:AGREED|DISAGREED|CANNOT[ _-]?VERIFY)',
    re.IGNORECASE)

# Words that are *negation of agreement* but must NOT be in `_NEGATION_RE`,
# because the disposition token `DISAGREED` contains them: `disagree\w*` matches
# the word `DISAGREED` itself, so a region reading "P0.2-AC1 is DISAGREED" was
# discarded as "negated, therefore uninformative" and the disagreement vanished.
# Measured: that made the N1 attack validate as AGREED even with turn-wide
# scanning.  A region containing DISAGREED is informative -- it states a
# disposition -- so only negation of *agreement* belongs in the regex above.
_DISAGREEMENT_TOKENS = ('DISAGREED',)


def prose_dispositions(text: str, footer_index: int, required_ids: list[str],
                       repo_root: Path | None = None) -> dict[str, Any]:
    """Collect dispositions attributed to each required ID, per line region.

    Two error directions, deliberately asymmetric:

    * **Contradiction (unsafe if missed).**  A disposition token on the same line
      as the ID, or on an adjacent line, that differs from the footer's.
    * **Corroboration (unsafe if over-credited).**  An *affirmative* disposition
      token in that same region, with no negation in the region.

    Detection is **region-local on purpose**.  A window measured in characters
    leaked across sections: the real P0.1 review contains a "Falsification
    attempts that failed" section, and a 240-character window around an ID picked
    up `failed`/`rejected` from unrelated prose, failing an honest review.  A
    line region is what a reviewer actually writes a verdict in — a table row, a
    bullet, or a heading plus its sentence.

    A region containing a negation contributes to **neither** set: recognising
    every negation form is impossible (`is **not** AGREED`, `is not marked
    AGREED`, `should not be AGREED`, `I do not think … is AGREED` all defeated a
    stricter pattern), so a negated region is treated as uninformative rather than
    as agreement.  The footer's `AGREED` then has to be corroborated somewhere
    else to pass, and an uncorroborated agreement stays pending.
    """
    lines = text.split('\n')
    fences = _fence_spans(lines)

    # Blank every footer line: a footer is the *authority*, not prose, and its
    # `"disposition":"AGREED"` sits adjacent to the ID by construction.  Counting
    # it as corroboration would let the footer vouch for itself -- measured as a
    # real defect, because the caller passed an out-of-range index and nothing was
    # blanked, so a record whose only agreement was the footer validated clean.
    # Blanking can only ever *remove* corroboration, never add it, so this is safe
    # for a fenced example too.
    body = list(lines)
    for index in range(len(body)):
        if body[index].startswith(FOOTER_PREFIX) or _in_spans(index, fences):
            body[index] = ' ' * len(body[index])
    if 0 <= footer_index < len(body):
        body[footer_index] = ' ' * len(body[footer_index])
    haystack = '\n'.join(body)

    result: dict[str, Any] = {}
    for cid in required_ids:
        stated: set[str] = set()
        corroborated: set[str] = set()
        for begin, end in _sentence_spans(haystack):
            sentence = haystack[begin:end]
            # Attribution is SENTENCE-scoped, not line-scoped.  Measured: a line
            # window attributed "The verdict for AC2 is DISAGREED." to a following
            # "P0.2-AC1 is AGREED.", so a review discussing two criteria on
            # adjacent lines could not be accepted.
            #
            # One widening: a verdict **label** may name the disposition in the
            # sentence *before* the one naming the criterion.  Measured,
            # `Status: AGREED. P0.2-AC1 satisfied.` was refused because the
            # disposition and the ID sat in different sentences.  The condition is
            # "this sentence has no disposition of its own", NOT "this sentence has
            # no ID" -- measured, the ID is in the second sentence while the label
            # is in the first, so keying on the ID alone missed it entirely.
            region = sentence
            label_binding = False
            if not any(d in sentence for d in DISPOSITIONS) and not re.search(
                    r'CANNOT[ _-]?VERIFY', sentence, re.IGNORECASE):
                previous = _sentence_before(haystack, begin)
                if previous and _LABEL_RE.search(previous):
                    # The label sentence must itself be committed, and the
                    # criterion sentence it binds to must not retract it.
                    # Measured: `Outcome: AGREED. P0.2-AC1 was deferred.` and
                    # `Verdict: AGREED. P0.2-AC1 is pending rerun.` both
                    # corroborated before these two checks existed.
                    label_match = _LABEL_RE.search(previous)
                    if (_is_clause_final(previous, label_match.end(), cid, repo_root)
                            and not _sentence_retracts(sentence)):
                        region = previous + ' ' + sentence
                        # The label *is* the verdict, so the criterion sentence
                        # after it is not treated as a qualifier of the label.
                        label_binding = True
            if cid not in region:
                continue
            if _NEGATED_DISPOSITION_RE.search(region):
                continue
            for match in _COMMITTED_RE.finditer(region):
                # Corroboration is structural, not lexical: the token must END its
                # sentence.  Anything following qualifies the verdict, and the
                # vocabulary of qualifiers is unbounded -- measured defeats include
                # `awaiting`, `pending`, `subject to`, `superseded by`, `unless`,
                # `once`, and `would be ... if ...`.  A lexicon cannot close that,
                # so the rule needs none.
                token_end = match.end()
                if not label_binding and not _is_clause_final(region, token_end, cid,
                                                      repo_root):
                    continue
                span = match.group(0)
                for disposition in DISPOSITIONS:
                    if disposition in span.upper() or (
                            disposition == CANNOT_VERIFY
                            and re.search(r'CANNOT[ _-]?VERIFY', span, re.IGNORECASE)):
                        corroborated.add(disposition)
                        stated.add(disposition)
        result[cid] = {'stated': sorted(stated), 'corroborated': sorted(corroborated)}
    return result


# ── record validation ────────────────────────────────────────────────────────

REQUIRED_RECORD_FIELDS = (
    'schema_version', 'review_id', 'packet_id', 'contract_sha256', 'reviewed_files',
    'harness', 'parent_id', 'child_id', 'requested_model', 'requested_effort',
    'identity_evidence', 'turn_id', 'verdict_text', 'criteria',
    # Mandatory, not optional.  Measured: when the contradiction scope was
    # optional, omitting it restored the N1 fail-open exactly -- a turn whose
    # first message said DISAGREED validated as acceptance-eligible AGREED,
    # because only the footer-bearing message was scanned.  A record must
    # therefore *declare* the text its verdict was drawn from.
    'contradiction_evidence',
)
CRITERION_FIELDS = ('id', 'disposition', 'evidence', 'procedure', 'observed_result')

# The two scopes are deliberately different, and conflating them was a real
# fail-open:
#
#   * **Authority scope** -- where the footer must live.  One assistant message.
#     A footer must be the final non-empty line *of its own message*, because
#     reviewers routinely emit a short "delivered, result was X" summary after the
#     review.  Requiring the footer to be last in the *turn* would reject honest
#     reviews; requiring nothing would let a quoted example qualify.
#   * **Contradiction scope** -- where prose is scanned for disagreements.  The
#     **whole turn**, because a reviewer must not be able to disown an earlier
#     message by later quoting a footer shape.  Selecting the verdict message by
#     looking for a footer-like line let the scanner choose its own input: a turn
#     whose message A said DISAGREED and whose message B merely quoted the footer
#     format validated as AGREED.  Scanning the whole turn makes that UNKNOWN.
CONTRADICTION_FIELD = 'contradiction_evidence'


def load_required_manifest(path: Path) -> dict[str, Any]:
    """Read the independent required-criterion manifest."""
    if not path.is_file():
        raise ReviewError(f'required-criterion manifest is missing: {path}')
    try:
        manifest = load_json_unique(path)
    except Exception as error:
        raise ReviewError(f'required-criterion manifest is unreadable: {error}') from error
    if not isinstance(manifest, dict):
        raise ReviewError('required-criterion manifest root is not an object')
    if manifest.get('schema_version') != SCHEMA_VERSION:
        raise ReviewError('unknown required-criterion manifest schema version')
    ids = manifest.get('required_ids')
    if (not isinstance(ids, list) or not ids
            or any(not isinstance(i, str) or not i for i in ids)
            or len(set(ids)) != len(ids)):
        raise ReviewError('required-criterion manifest has no nonempty unique required_ids')
    if not is_sha256(manifest.get('contract_sha256')):
        raise ReviewError('required-criterion manifest has no valid contract hash')
    return manifest


def _normalise_text(text: str) -> str:
    """The one normalisation used by every text comparison in this module.

    Line-ending style and surrounding whitespace only.  Anything more would let two
    genuinely different texts compare equal, which is the failure this whole area
    exists to prevent.
    """
    return text.replace('\r\n', '\n').replace('\r', '\n').strip()


def _contains_text(wider: str, inner: str) -> bool:
    """True when `wider` contains `inner`, under the shared normalisation.

    Kept as one function so the containment check and the verdict binding cannot
    drift apart -- measured, they did, and a documented CRLF tolerance became
    unreachable on the containment route.
    """
    return _normalise_text(inner) in _normalise_text(wider)


def _same_text(declared: Any, candidate: str) -> bool:
    """True when a declared verdict is the source turn's own text.

    Trailing whitespace and line-ending style are normalised, because a producer
    writing the turn to a file may change `\\r\\n` to `\\n` and append a newline.
    Nothing else is: an equality test that tolerated paraphrase would not bind the
    verdict to anything.
    """
    if not isinstance(declared, str) or not isinstance(candidate, str):
        return False
    return _normalise_text(declared) == _normalise_text(candidate)


def read_source_identity(session_path: Path, harness: Any) -> dict[str, Any]:
    """Re-open an original reviewer session and read its own identity.

    Deliberately reads the *source*, not the record: the point is to compare what
    the record claims against what the log says.  A DSH log is a zstd-compressed
    JSONL whose header carries `id` and `parentSession`; a Codex rollout is plain
    JSONL whose `session_meta` carries `id` and `parent_thread_id`.  A missing
    decoder is an error, never an empty success.
    """
    import json as _json

    if not session_path.is_file():
        raise ReviewError(f'source session is missing: {session_path}')

    records: list[dict[str, Any]] = []
    if str(session_path).endswith('.zstd'):
        try:
            import zstandard  # noqa: PLC0415
        except Exception as error:  # pragma: no cover - environment dependent
            raise ReviewError(
                f'the zstandard decoder is unavailable, so the original session '
                f'cannot be verified: {error}') from error
        try:
            with session_path.open('rb') as handle:
                with zstandard.ZstdDecompressor().stream_reader(handle) as reader:
                    raw = reader.read()
        except Exception as error:
            raise ReviewError(f'source session could not be decoded: {error}') from error
        lines = raw.split(b'\n')
    else:
        lines = session_path.read_bytes().split(b'\n')

    for line in lines:
        if not line.strip():
            continue
        try:
            parsed = _json.loads(line)
        except Exception:
            continue
        if isinstance(parsed, dict):
            records.append(parsed)
    if not records:
        raise ReviewError('source session decoded to no records')

    result: dict[str, Any] = {'turn_present': False, 'model': None, 'effort': None}

    header = next((r for r in records if r.get('type') == 'session'), None)
    meta = next((r for r in records if r.get('type') == 'session_meta'), None)
    if header is not None:                       # DSH
        result['source_harness'] = 'dsh'
        result['child_id'] = header.get('id')
        result['parent_id'] = header.get('parentSession')
        descriptor = next((r for r in records if r.get('type') in
                           ('subagent/descriptor', 'delegation/descriptor')), None)
        if isinstance(descriptor, dict):
            data = descriptor.get('data') or {}
            result['model'] = data.get('agentModel')
            result['effort'] = data.get('agentReasoningEffort')
    elif meta is not None:                       # Codex
        result['source_harness'] = 'codex'
        payload = meta.get('payload') or {}
        result['child_id'] = payload.get('id')
        result['parent_id'] = payload.get('parent_thread_id') or (
            ((payload.get('source') or {}).get('subagent') or {})
            .get('thread_spawn', {}).get('parent_thread_id'))
        # The route lives in `turn_context` for a Codex rollout, not in the header.
        # Measured: omitting this left `requested_model` unverified on the Codex
        # path, so an invented model validated as eligible against a Codex source --
        # the binding applied to DSH only, which is the sort of gap that looks like
        # coverage until the other harness is exercised.
        for record in records:
            if record.get('type') != 'turn_context':
                continue
            context = record.get('payload')
            if not isinstance(context, dict):
                continue
            if result['model'] is None:
                result['model'] = context.get('model')
            if result['effort'] is None:
                result['effort'] = context.get('effort') or context.get(
                    'reasoning_effort')
    else:
        raise ReviewError('source session has no recognisable header record')

    # The turn must exist AND be completed.  An errored turn is not a verdict.
    turns = {r.get('data', {}).get('turn') for r in records
             if r.get('type') in ('turn/end',)
             and isinstance(r.get('data'), dict)
             and (r['data'].get('reason') or {}).get('kind') == 'completed'}
    codex_turns = {str(r.get('payload', {}).get('turn_id')) for r in records
                   if r.get('type') == 'event_msg'
                   and isinstance(r.get('payload'), dict)
                   and r['payload'].get('type') == 'task_complete'}
    result['completed_turns'] = sorted(
        {str(t) for t in turns if t is not None} | {t for t in codex_turns if t})

    # The text of each completed turn, so a record's `verdict_text` can be bound to
    # the turn it claims to come from rather than merely to a tracked file.
    #
    # Measured residual from an independent review: `verdict_text` was bound only by
    # the source-file hash and by `contradiction_evidence` containment, both of
    # which a self-authored record satisfies.  A record could therefore declare a
    # genuinely verified ancestry and an INVENTED verdict body -- and the verdict
    # body is what carries the dispositions.  Verifying ancestry without verifying
    # the verdict verifies the part that does not decide anything.
    texts: dict[str, list[str]] = {}
    for record in records:
        data = record.get('data') if isinstance(record.get('data'), dict) else {}
        if record.get('type') == 'assistant/message':
            turn = data.get('turn')
            content = (data.get('message') or {}).get('content') or []
            text = ''.join(part.get('text', '') for part in content
                           if isinstance(part, dict) and part.get('type') == 'text')
            if turn is not None and text.strip():
                texts.setdefault(str(turn), []).append(text)
        if (record.get('type') == 'event_msg'
                and isinstance(record.get('payload'), dict)
                and record['payload'].get('type') == 'task_complete'):
            body = record['payload'].get('last_agent_message')
            if isinstance(body, str) and body.strip():
                texts.setdefault(str(record['payload'].get('turn_id')), []).append(body)
    result['turn_texts'] = texts
    return result


def validate_against_published_schema(record: Any, repo_root: Path) -> list[str]:
    """Check a record against `docs/reviews/review-record.schema.json`.

    The published schema used to be documentation only: nothing read it, so a
    schema that drifted from the code would have documented a contract nothing
    enforced.  An independent review measured that -- "no `jsonschema` call
    anywhere in scripts/".  `jsonschema` is not installed here and is not a project
    dependency, so this is a deliberately small structural check covering the
    keywords the schema actually uses: `type`, `required`, `enum`, `const`,
    `minLength`, `minItems`, `pattern`, `properties`, `items`.

    Returns a list of problems; empty means conformant.  A missing or unreadable
    schema is reported, not silently skipped: an unenforced schema is the defect
    this exists to catch.
    """
    schema_path = repo_root / 'docs' / 'reviews' / 'review-record.schema.json'
    if not schema_path.is_file():
        return ['the published review-record schema is missing']
    try:
        schema = load_json_unique(schema_path)
    except Exception as error:
        return [f'the published review-record schema is unreadable: {error}']

    problems: list[str] = []

    def check(value: Any, node: dict[str, Any], where: str) -> None:
        if 'const' in node and value != node['const']:
            problems.append(f'{where}: expected {node["const"]!r}, got {value!r}')
        if 'enum' in node and value not in node['enum']:
            problems.append(f'{where}: {value!r} is not one of {node["enum"]}')
        expected = node.get('type')
        if expected == 'object' and not isinstance(value, dict):
            problems.append(f'{where}: expected an object')
            return
        if expected == 'array' and not isinstance(value, list):
            problems.append(f'{where}: expected an array')
            return
        if expected == 'string' and not isinstance(value, str):
            problems.append(f'{where}: expected a string')
            return
        if expected == 'boolean' and not isinstance(value, bool):
            problems.append(f'{where}: expected a boolean')
            return
        if isinstance(value, str):
            if 'minLength' in node and len(value) < node['minLength']:
                problems.append(f'{where}: shorter than minLength {node["minLength"]}')
            if 'pattern' in node and not re.search(node['pattern'], value):
                problems.append(f'{where}: {value!r} does not match {node["pattern"]}')
        if isinstance(value, list):
            if 'minItems' in node and len(value) < node['minItems']:
                problems.append(f'{where}: fewer than minItems {node["minItems"]}')
            item_schema = node.get('items')
            if isinstance(item_schema, dict):
                for index, item in enumerate(value):
                    check(item, item_schema, f'{where}[{index}]')
        if isinstance(value, dict):
            for field in node.get('required', []):
                if field not in value:
                    problems.append(f'{where}: missing required field {field}')
            declared = node.get('properties') or {}
            # `additionalProperties: false` is a real constraint the schema relies
            # on, so ignoring it would enforce less than the schema claims.
            # Measured: it is used on `reviewed_files` entries, criterion entries
            # and `identity_evidence`, so an undeclared key would otherwise pass.
            if node.get('additionalProperties') is False:
                for field in value:
                    if field not in declared:
                        problems.append(
                            f'{where}: unexpected field {field!r} '
                            f'(additionalProperties is false)')
            for field, sub in declared.items():
                if field in value:
                    check(value[field], sub, f'{where}.{field}')

    check(record, schema, 'record')
    return problems


def validate_record(record: Any, manifest: dict[str, Any],
                    repo_root: Path) -> dict[str, Any]:
    """Validate one durable review record.  Returns a result dict.

    ``status`` is ELIGIBLE, FAILED or INVALID.  INVALID is for unusable input
    (malformed record, unknown schema, unreadable source).  FAILED is for a record
    that is well formed but does not establish acceptance -- a DISAGREED
    criterion, a stale hash, or a non-authoritative footer.
    """
    problems: list[str] = []
    invalid: list[str] = []

    if not isinstance(record, dict):
        raise ReviewError('review record root is not an object')

    for field in REQUIRED_RECORD_FIELDS:
        if field not in record:
            invalid.append(f'missing required field {field}')
    if invalid:
        return {'status': INVALID, 'reasons': invalid, 'criteria': {}}

    # The published schema is enforced, not merely documented.
    schema_problems = validate_against_published_schema(record, repo_root)
    if schema_problems:
        return {'status': INVALID,
                'reasons': [f'does not satisfy the published schema -- {p}'
                            for p in schema_problems],
                'criteria': {}}

    # Presence is not identity.  Every declared identity field must be a usable
    # value, and the ancestry/route fields must match the *source*.
    #
    # Measured fail-open before this check: setting `parent_id`, `child_id`,
    # `turn_id`, `requested_model`, `requested_effort` and `harness` to arbitrary
    # values -- including all six at once -- still returned acceptance-eligible,
    # because the validator only asked whether the keys existed.  A record with
    # invented parentage could close a packet.
    for field in ('review_id', 'packet_id', 'parent_id', 'child_id', 'turn_id',
                  'requested_model', 'harness'):
        value = record.get(field)
        if not isinstance(value, str) or not value.strip():
            invalid.append(f'{field} must be a nonempty string, got {value!r}')
    if invalid:
        return {'status': INVALID, 'reasons': invalid, 'criteria': {}}
    if record.get('schema_version') != SCHEMA_VERSION:
        return {'status': INVALID, 'reasons': ['unknown review-record schema version'],
                'criteria': {}}

    required_ids: list[str] = manifest['required_ids']
    if record.get('packet_id') != manifest.get('packet_id'):
        invalid.append('record packet_id conflicts with the required-criterion manifest')
    if (record.get('contract_sha256') or '').lower() != manifest['contract_sha256'].lower():
        invalid.append('record contract hash conflicts with the required-criterion manifest')

    contract_path = repo_root / manifest.get('contract_file', '')
    if not contract_path.is_file():
        invalid.append(f'contract file is missing: {manifest.get("contract_file")}')
    elif digest_file(contract_path) != manifest['contract_sha256'].lower():
        invalid.append('contract file bytes do not match the manifest contract hash')

    # Reviewed files must exist and match.
    reviewed = record.get('reviewed_files')
    if not isinstance(reviewed, list) or not reviewed:
        invalid.append('reviewed_files must be a nonempty list')
    else:
        for entry in reviewed:
            if not isinstance(entry, dict) or set(entry) != {'path', 'sha256'}:
                invalid.append('reviewed_files entry must have exactly path and sha256')
                continue
            if not is_sha256(entry['sha256']):
                invalid.append(f'reviewed file {entry.get("path")} has a malformed hash')
                continue
            path = repo_root / entry['path']
            if not path.is_file():
                invalid.append(f'reviewed file is missing: {entry["path"]}')
            elif digest_file(path) != entry['sha256'].lower():
                invalid.append(f'reviewed file bytes changed since review: {entry["path"]}')

    # Identity evidence: a source file plus the turn selected inside it.
    identity = record.get('identity_evidence')
    source_text = ''
    if not isinstance(identity, dict) or 'kind' not in identity or 'path' not in identity:
        invalid.append('identity_evidence must declare kind and path')
    else:
        source_path = repo_root / str(identity.get('path'))
        if not source_path.is_file():
            invalid.append(f'identity evidence source is missing: {identity.get("path")}')
        else:
            recorded_hash = identity.get('sha256')
            if recorded_hash and is_sha256(recorded_hash):
                if digest_file(source_path) != recorded_hash.lower():
                    invalid.append('identity evidence source bytes changed since review')
            if identity.get('kind') == 'turn_text':
                source_text = source_path.read_text(encoding='utf-8', errors='replace')
            else:
                invalid.append(f'unsupported identity evidence kind: {identity.get("kind")!r}')

        # Verify the declared ancestry and route against the ORIGINAL source.
        #
        # Without this the identity fields are decoration: measured, a record with
        # `parent_id=session-TOTALLY-DIFFERENT`, `child_id=deadbeef-…`, `turn_id=99`,
        # `requested_model=gpt-6-luna` and `harness=codex` all at once still
        # returned acceptance-eligible.  The validator re-opens the session the
        # record names and compares what the source actually says.
        source_session = identity.get('source_session') if isinstance(identity, dict) else None
        if source_session is None:
            invalid.append(
                'identity_evidence must declare source_session: the original log the '
                'turn was drawn from, so ancestry and route can be verified rather '
                'than trusted')
        else:
            session_path = Path(str(source_session))
            if not session_path.is_absolute():
                session_path = repo_root / session_path
            if not session_path.is_file():
                invalid.append(f'source session is missing: {source_session}')
            else:
                try:
                    actual = read_source_identity(session_path, record.get('harness'))
                except ReviewError as error:
                    invalid.append(f'source session could not be read: {error}')
                else:
                    mismatches = []
                    if actual.get('child_id') != record.get('child_id'):
                        mismatches.append(
                            f'child_id: record says {record.get("child_id")!r}, '
                            f'source says {actual.get("child_id")!r}')
                    if actual.get('parent_id') != record.get('parent_id'):
                        mismatches.append(
                            f'parent_id: record says {record.get("parent_id")!r}, '
                            f'source says {actual.get("parent_id")!r}')
                    if str(record.get('turn_id')) not in (
                            actual.get('completed_turns') or []):
                        mismatches.append(
                            f'turn_id {record.get("turn_id")!r} is not a completed '
                            f'turn in the source session (completed: '
                            f'{actual.get("completed_turns")})')
                    else:
                        # The verdict must be the text of the turn it names.
                        #
                        # Without this, ancestry is verified but the dispositions --
                        # which are the part that decides anything -- are not.  A
                        # record could name a real turn and supply an invented body.
                        # Measured before this check: replacing `verdict_text`
                        # entirely passed, provided the record also controlled the
                        # tracked source file.
                        declared_verdict = record.get('verdict_text')
                        available = (actual.get('turn_texts') or {}).get(
                            str(record.get('turn_id')), [])
                        if not available:
                            mismatches.append(
                                f'turn {record.get("turn_id")!r} has no readable '
                                f'assistant text in the source session, so the '
                                f'verdict cannot be bound to it')
                        elif not any(
                                _same_text(declared_verdict, candidate)
                                for candidate in available):
                            mismatches.append(
                                'verdict_text is not the text of the turn it names: '
                                'the declared verdict does not match any assistant '
                                'message of turn '
                                f'{record.get("turn_id")!r} in the source session')
                    if actual.get('source_harness') != record.get('harness'):
                        mismatches.append(
                            f'harness: record says {record.get("harness")!r}, but the '
                            f'source session is a {actual.get("source_harness")!r} log')
                    for field in ('model', 'effort'):
                        claimed = record.get(
                            'requested_model' if field == 'model' else 'requested_effort')
                        seen = actual.get(field)
                        if claimed and seen and claimed != seen:
                            # A route may be recorded fully qualified
                            # (`provider/model`) while the source stores only the
                            # model id.  Compare the model component in that case,
                            # which is the part that identifies the route.
                            claimed_model = claimed.rsplit('/', 1)[-1]
                            seen_model = str(seen).rsplit('/', 1)[-1]
                            if claimed_model != seen_model:
                                mismatches.append(
                                    f'{field}: record says {claimed!r}, source says '
                                    f'{seen!r}')
                    invalid.extend(f'identity does not match the source -- {m}' for m in mismatches)

    if not isinstance(record.get('verdict_text'), str) or not record['verdict_text'].strip():
        invalid.append('verdict_text is empty')

    if invalid:
        return {'status': INVALID, 'reasons': invalid, 'criteria': {}}

    # ── delta: a re-bind is NOT a re-review ──────────────────────────────────
    #
    # Measured fail-open before this check: `"delta"` occurred zero times in this
    # module, so a record could carry `delta.affected_criteria=['P0.1-AC1']` -- an
    # explicit admission that a criterion's supporting text changed -- and still
    # return acceptance-eligible.  That is a laundering path: rebind a stale review
    # to new bytes, declare the affected criteria, and pass anyway.
    #
    # The amendment's rule is that a re-bind records the delta so a human can
    # confirm it is confined to non-criterion text.  That confirmation is not
    # machine-checkable, so the validator does the only safe thing: a non-empty
    # `affected_criteria` leaves the packet pending.
    delta = record.get('delta')
    if delta is not None:
        if not isinstance(delta, dict):
            invalid.append('delta must be an object when present')
        else:
            affected = delta.get('affected_criteria')
            if affected is None:
                invalid.append('delta must declare affected_criteria (use [] to claim none)')
            elif not isinstance(affected, list):
                invalid.append('delta.affected_criteria must be a list')
            elif affected:
                problems.append(
                    'delta declares affected criteria '
                    f'{sorted(str(a) for a in affected)}: a re-bind is not a '
                    're-review, so these stay pending until a reviewer confirms '
                    'them')
            if not delta.get('reason'):
                invalid.append('delta must record a reason for the re-bind')

    if invalid:
        return {'status': INVALID, 'reasons': invalid, 'criteria': {}}

    # ── footer authority ─────────────────────────────────────────────────────
    # `verdict_text` is the authority scope: one assistant message.  If the record
    # supplies `contradiction_evidence` (the whole turn, or any wider text), the
    # prose scan uses *that* instead -- so a disagreement stated in an earlier
    # message cannot be disowned by quoting a footer shape in a later one.
    footer = extract_footer(source_text)
    if footer['status'] != 'authoritative':
        problems.append(f'footer not authoritative ({footer["status"]}): {footer["reason"]}')
        return {'status': FAILED, 'reasons': problems, 'criteria': {},
                'footer_status': footer['status']}

    try:
        footer_map = parse_footer_map(footer['line'])
    except ReviewError as error:
        return {'status': FAILED, 'reasons': [f'footer is malformed: {error}'],
                'criteria': {}, 'footer_status': 'malformed'}

    # Footer population must match the manifest exactly.
    missing = [cid for cid in required_ids if cid not in footer_map]
    extra = [cid for cid in footer_map if cid not in required_ids]
    if missing:
        problems.append('footer omits required criterion id(s): ' + ', '.join(missing))
    if extra:
        problems.append('footer adds id(s) absent from the manifest: ' + ', '.join(extra))

    scan_text = source_text
    wider = record.get(CONTRADICTION_FIELD)
    if not isinstance(wider, str) or not wider.strip():
        invalid.append(f'{CONTRADICTION_FIELD} must be a nonempty string: the record '
                       f'must declare the text its verdict was drawn from, because '
                       f'scanning only the footer-bearing message lets a disagreement '
                       f'in an earlier message be disowned')
    else:
        # Containment uses the SAME normalisation as the verdict binding, so the
        # documented CRLF tolerance is actually reachable.
        #
        # Measured inconsistency, reported by an independent review: `_same_text`
        # normalises CRLF, but this check was exact-string, so a CRLF-normalised
        # record exited 2 here and never reached `_same_text`.  The tolerance was
        # documented but not exercised on this route -- a tolerance that cannot be
        # reached is worse than none, because it is claimed.
        if not _contains_text(wider, source_text):
            invalid.append(f'{CONTRADICTION_FIELD} does not contain the verdict text, '
                           f'so it is not a wider view of the same source')
        scan_text = wider
    if invalid:
        return {'status': INVALID, 'reasons': invalid, 'criteria': {}}

    prose = prose_dispositions(scan_text, len(scan_text.split('\n')),
                               required_ids, repo_root)

    # ── per-criterion dispositions ───────────────────────────────────────────
    declared = record.get('criteria')
    declared_map: dict[str, dict[str, Any]] = {}
    if not isinstance(declared, list) or not declared:
        invalid.append('criteria must be a nonempty list')
    else:
        for entry in declared:
            if not isinstance(entry, dict):
                invalid.append('criteria entry is not an object')
                continue
            for field in CRITERION_FIELDS:
                if field not in entry:
                    invalid.append(f'criteria entry {entry.get("id", "?")} is missing {field}')
            cid = entry.get('id')
            if not isinstance(cid, str) or not cid:
                invalid.append('criteria entry has an invalid id')
                continue
            if cid in declared_map:
                invalid.append(f'criteria repeats id {cid!r}')
                continue
            declared_map[cid] = entry
    if invalid:
        return {'status': INVALID, 'reasons': invalid, 'criteria': {}}

    unknown_ids = [cid for cid in declared_map if cid not in required_ids]
    if unknown_ids:
        problems.append('record declares id(s) absent from the manifest: '
                        + ', '.join(sorted(unknown_ids)))

    per_criterion: dict[str, Any] = {}
    for cid in required_ids:
        entry = declared_map.get(cid)
        if entry is None:
            per_criterion[cid] = {'disposition': CANNOT_VERIFY,
                                  'reason': 'record has no row for this required criterion'}
            continue
        disposition = entry.get('disposition')
        if disposition not in DISPOSITIONS:
            per_criterion[cid] = {'disposition': CANNOT_VERIFY,
                                  'reason': f'record uses unsupported disposition {disposition!r}'}
            continue
        if not isinstance(entry.get('evidence'), list) or not entry['evidence']:
            per_criterion[cid] = {'disposition': CANNOT_VERIFY,
                                  'reason': 'criterion evidence is empty'}
            continue
        if not isinstance(entry.get('procedure'), str) or not entry['procedure'].strip():
            per_criterion[cid] = {'disposition': CANNOT_VERIFY,
                                  'reason': 'criterion procedure is empty'}
            continue
        if not isinstance(entry.get('observed_result'), str) or not entry['observed_result'].strip():
            per_criterion[cid] = {'disposition': CANNOT_VERIFY,
                                  'reason': 'criterion observed_result is empty'}
            continue

        footer_disposition = footer_map.get(cid)
        if footer_disposition is None:
            per_criterion[cid] = {'disposition': CANNOT_VERIFY,
                                  'reason': 'footer omits this criterion'}
            continue

        # Contradiction: any disposition token attributed to this ID that differs
        # from the footer's.  Over-attribution is intentional (see
        # prose_dispositions): a spurious token costs a pending criterion, a
        # missed one would let a contradicted AGREED stand.
        stated = prose.get(cid, {}).get('stated', [])
        contradicting = [d for d in stated if d != footer_disposition]
        if contradicting:
            per_criterion[cid] = {
                'disposition': UNKNOWN_DISPOSITION,
                'reason': ('source prose states ' + ', '.join(sorted(contradicting))
                           + f' while the footer states {footer_disposition}'),
            }
            continue

        if disposition != footer_disposition:
            per_criterion[cid] = {
                'disposition': UNKNOWN_DISPOSITION,
                'reason': (f'record claims {disposition} but the source footer states '
                           f'{footer_disposition}'),
            }
            continue

        # Corroboration: an AGREED footer must be positively supported near the ID.
        # Negation recognition cannot be made complete, so an uncorroborated
        # agreement stays pending rather than passing on the footer's word alone.
        if disposition == AGREED:
            corroborated = prose.get(cid, {}).get('corroborated', [])
            if AGREED not in corroborated:
                per_criterion[cid] = {
                    'disposition': CANNOT_VERIFY,
                    'reason': ('the footer says AGREED but the source never states an '
                               'affirmative AGREED for this criterion near its ID; '
                               'negation cannot be recognised reliably, so the '
                               'agreement is uncorroborated'),
                }
                continue

        per_criterion[cid] = {'disposition': disposition, 'reason': 'footer and record agree'}

    # Evidence hashes inside criteria must resolve.
    for cid, entry in declared_map.items():
        for item in entry.get('evidence') or []:
            if not isinstance(item, dict) or 'path' not in item:
                problems.append(f'{cid} evidence entry is malformed')
                continue
            path = repo_root / str(item['path'])
            if not path.is_file():
                problems.append(f'{cid} evidence file is missing: {item["path"]}')
            elif is_sha256(item.get('sha256')) and digest_file(path) != item['sha256'].lower():
                problems.append(f'{cid} evidence bytes changed since review: {item["path"]}')

    unresolved = sorted(cid for cid, value in per_criterion.items()
                        if value['disposition'] != AGREED)
    if unresolved:
        problems.append('criteria not AGREED: ' + ', '.join(unresolved))

    status = ELIGIBLE if not problems else FAILED
    return {
        'status': status,
        'reasons': problems,
        'criteria': per_criterion,
        'footer_status': footer['status'],
        'validator_version': VALIDATOR_VERSION,
    }
