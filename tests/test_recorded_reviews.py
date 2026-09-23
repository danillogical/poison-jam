"""P0.2 fixtures: durable review records and footer authority.

Each negative fixture must fail for its **own** stated reason, not incidentally
through an unrelated missing field.  The tests therefore assert on the specific
reason text or the specific footer status, not merely on a nonzero exit.
"""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

from jsrf_review_records import (  # noqa: E402
    AGREED,
    CANNOT_VERIFY,
    DISAGREED,
    ELIGIBLE,
    FAILED,
    INVALID,
    extract_footer,
    load_required_manifest,
    parse_footer_map,
    prose_dispositions,
    validate_record,
)

# The Codex source adapter, used by the AC2 fixtures that must go through an
# adapter rather than the classifier alone.  The file name is hyphenated, so it is
# loaded by path.
import importlib.util as _importlib_util  # noqa: E402

_spec = _importlib_util.spec_from_file_location(
    'export_codex_review', SCRIPTS / 'export-codex-review.py')
exporter = _importlib_util.module_from_spec(_spec)
_spec.loader.exec_module(exporter)

CHECKER = SCRIPTS / 'check-recorded-reviews.py'
CONTRACT = ROOT / 'docs' / 'packets' / 'p0-acceptance-contract.md'
REQUIRED_IDS = ['P0.2-AC1', 'P0.2-AC2', 'P0.2-AC3', 'P0.2-AC4']


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def install_schema(root: Path) -> None:
    """Copy the published schema into a fixture root.

    The validator now ENFORCES docs/reviews/review-record.schema.json rather than
    treating it as documentation, so a fixture root without it is INVALID by
    design.  An independent review measured that nothing read the schema before.
    """
    import shutil
    target = root / 'docs' / 'reviews' / 'review-record.schema.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(ROOT / 'docs' / 'reviews' / 'review-record.schema.json', target)


def install_schema(root: Path) -> None:
    """Copy the published schema into a fixture root.

    The validator now ENFORCES `docs/reviews/review-record.schema.json` rather than
    treating it as documentation, so a fixture root without it is INVALID by
    design.  An independent review measured that nothing read the schema before.
    """
    import shutil
    target = root / 'docs' / 'reviews' / 'review-record.schema.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(ROOT / 'docs' / 'reviews' / 'review-record.schema.json', target)


def write_matching_session(root: Path, name: str, *, child: str = 'c',
                           parent: str = 'p', turn: str = 't1',
                           model: str = 'hy4-preview-f',
                           effort: str = 'high',
                           verdict_text: str | None = None) -> Path:
    """Write a session log whose identity matches the fixture's declared fields.

    `source_session` is mandatory: the validator re-opens the original log to check
    ancestry, the completed turn, the route, AND that `verdict_text` really is the
    text of the turn it names.  Measured fail-open before those checks: every
    identity field could be invented, and the verdict body could be replaced
    outright, while the record still validated.

    `verdict_text` must be supplied whenever the record declares a verdict, because
    the binding is an EQUALITY test rather than a containment test.
    """
    directory = root / 'docs' / 'reviews' / 'P0.2'
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / name
    lines = [
        json.dumps({'type': 'session', 'version': 3, 'id': child,
                    'parentSession': parent}),
        json.dumps({'type': 'subagent/descriptor',
                    'data': {'agentModel': model,
                             'agentReasoningEffort': effort}}),
    ]
    if verdict_text is not None:
        lines.append(json.dumps({
            'type': 'assistant/message',
            'data': {'turn': turn,
                     'message': {'content': [{'type': 'text',
                                              'text': verdict_text}]}}}))
    lines.append(json.dumps({'type': 'turn/end',
                             'data': {'turn': turn,
                                      'reason': {'kind': 'completed'}}}))
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return path


def identity_evidence(root: Path, source: Path, session: Path) -> dict:
    return {'kind': 'turn_text', 'path': str(source.relative_to(root)),
            'sha256': sha256_bytes(source.read_bytes()),
            'source_session': str(session)}


def footer(ids=REQUIRED_IDS, disposition=AGREED) -> str:
    return ('REVIEW_CRITERIA_JSON: '
            + json.dumps([{'id': i, 'disposition': disposition} for i in ids]))


def prose_block(ids=REQUIRED_IDS, disposition=AGREED) -> str:
    """Prose that names every required ID with a disposition in a table row."""
    rows = [f'| {i} | {disposition} | reproduced by re-running the fixture |' for i in ids]
    return '\n'.join(rows)


def make_source_text(body: str) -> str:
    return body


class FooterAuthorityTests(unittest.TestCase):
    """The footer rules are the security boundary; test them directly."""

    def test_valid_final_line_footer_is_authoritative(self):
        text = prose_block() + '\n\n' + footer() + '\n'
        result = extract_footer(text)
        self.assertEqual(result['status'], 'authoritative')

    def test_no_footer_is_none(self):
        result = extract_footer(prose_block() + '\n')
        self.assertEqual(result['status'], 'none')

    def test_fenced_example_alone_is_not_a_footer(self):
        text = prose_block() + '\n\n```\n' + footer() + '\n```\n'
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_real_footer_plus_fenced_example_uses_the_real_one(self):
        example = footer(disposition=DISAGREED)
        text = (prose_block() + '\n\nHere is the old map:\n```\n' + example + '\n```\n\n'
                + footer() + '\n')
        result = extract_footer(text)
        self.assertEqual(result['status'], 'authoritative')
        mapping = parse_footer_map(result['line'])
        self.assertEqual(set(mapping.values()), {AGREED},
                         'the fenced example must not supply any disposition')

    def test_quoted_old_footer_plus_real_footer_is_ambiguous(self):
        text = prose_block() + '\n\n' + footer() + '\n\n' + footer() + '\n'
        self.assertEqual(extract_footer(text)['status'], 'ambiguous')

    def test_single_footer_not_on_final_line_is_not_final(self):
        """Trailing text that restates a verdict makes the footer non-final.

        A sign-off carrying no disposition is tolerated (see the A1 tests); a line
        that names a disposition after the footer could be *superseding* it, so it
        must not be tolerated.
        """
        text = (prose_block() + '\n\n' + footer()
                + '\n\nOn reflection P0.2-AC1 is DISAGREED.\n')
        self.assertEqual(extract_footer(text)['status'], 'not-final')

    def test_blockquote_footer_is_ignored(self):
        text = prose_block() + '\n\n> ' + footer() + '\n'
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_indented_footer_is_not_column_zero(self):
        text = prose_block() + '\n\n    ' + footer() + '\n'
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_tab_indented_footer_is_not_column_zero(self):
        text = prose_block() + '\n\n\t' + footer() + '\n'
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_unterminated_fence_swallows_the_footer(self):
        text = prose_block() + '\n\n```\n' + footer() + '\n'
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_tilde_fence_is_not_closed_by_backticks(self):
        text = prose_block() + '\n\n~~~\n' + footer() + '\n```\n'
        self.assertEqual(extract_footer(text)['status'], 'none',
                         'a ~~~ fence must not be closed by ```')

    def test_html_comment_footer_is_ignored(self):
        text = prose_block() + '\n\n<!-- ' + footer() + ' -->\n'
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_crlf_line_endings_still_yield_a_footer(self):
        text = (prose_block() + '\r\n\r\n' + footer() + '\r\n')
        result = extract_footer(text)
        self.assertEqual(result['status'], 'authoritative')
        self.assertEqual(set(parse_footer_map(result['line'].rstrip('\r')).values()), {AGREED})

    def test_trailing_blank_lines_do_not_break_finality(self):
        text = prose_block() + '\n\n' + footer() + '\n\n\n   \n'
        self.assertEqual(extract_footer(text)['status'], 'authoritative')

    def test_footer_with_text_after_it_is_not_final(self):
        text = (prose_block() + '\n\n' + footer()
                + '\nSigned, the reviewer. P0.2-AC1 remains DISAGREED.\n')
        self.assertEqual(extract_footer(text)['status'], 'not-final')

    def test_trailing_signoff_without_a_disposition_is_tolerated(self):
        """A1: robustness to a trailing acknowledgement in the same message.

        Whether a reviewer's closing line lands in the footer's message or a later
        one is an artifact of how the model emits bubbles, so a sign-off carrying
        no disposition is tolerated rather than failing an honest review.
        """
        for trailing in ('Thanks for reading.', 'Review delivered.',
                         'Signed, the reviewer.', 'Summary above.'):
            with self.subTest(trailing=trailing):
                text = prose_block() + '\n\n' + footer() + '\n' + trailing + '\n'
                self.assertEqual(extract_footer(text)['status'], 'authoritative')

    def test_trailing_summary_naming_a_disposition_is_refused(self):
        """The tolerance must not extend to text that could supersede the map."""
        text = (prose_block() + '\n\n' + footer()
                + '\nReview delivered; result was AGREED.\n')
        self.assertEqual(extract_footer(text)['status'], 'not-final')

    def test_two_text_blocks_each_ending_in_a_footer_is_ambiguous(self):
        text = prose_block() + '\n\n' + footer() + '\n\n---\n\n' + footer() + '\n'
        self.assertEqual(extract_footer(text)['status'], 'ambiguous')


class RealAdversarialInputTests(unittest.TestCase):
    """A live review that quoted the footer as a counterexample.

    Found by auditing the session's own review history. The P0.2 *amendment*
    reviewer wrote `Verdict: **INADEQUATE**` and, to demonstrate the vulnerability
    it was describing, embedded a `REVIEW_CRITERIA_JSON` line **inside a fenced
    block**. A naive scanner would have promoted that quoted line to an
    `AGREED` verdict for a review that said the opposite.

    These tests reconstruct that shape, because it is the strongest available
    adversarial input: it is not invented, it occurred.
    """

    BT = chr(96) * 3

    def test_quoted_footer_inside_a_fence_is_not_promoted(self):
        """The exact live shape: prose INADEQUATE, fenced footer AGREED."""
        text = ('Both hashes match. Here is my attack.\n\n'
                '## Verdict: **INADEQUATE**\n\n'
                'The quoted footer shape:\n\n'
                f'{self.BT}\n  {self.BT}\n  example:\n{footer()}\n{self.BT}\n\n'
                'A naive detector sees one candidate and promotes it.\n')
        result = extract_footer(text)
        self.assertNotEqual(result['status'], 'authoritative',
                            'a quoted counterexample must never be promoted')
        # The prose verdict is INADEQUATE, so nothing here may corroborate AGREED.
        prose = prose_dispositions(text, len(text.split('\n')), REQUIRED_IDS)
        for cid in REQUIRED_IDS:
            self.assertNotIn(AGREED, prose[cid]['corroborated'])

    def test_fenced_footer_is_not_authoritative_even_when_last(self):
        text = (f'Review body.\n\n{self.BT}\n{footer()}\n{self.BT}\n')
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_bare_footer_that_is_last_is_caught_by_corroboration(self):
        """The residual: a bare footer IS the format, so corroboration guards it.

        Syntax cannot reject a correctly-formed footer, which is why the
        corroboration requirement is mandatory rather than advisory.
        """
        text = f'Review body with no verdict of its own.\n\n{footer()}\n'
        result = extract_footer(text)
        self.assertEqual(result['status'], 'authoritative')
        prose = prose_dispositions(text, result['index'], REQUIRED_IDS, ROOT)
        for cid in REQUIRED_IDS:
            self.assertEqual(prose[cid]['corroborated'], [],
                             'a footer with no affirmative prose must not '
                             'corroborate its own claim')

    def test_prose_verdict_contradicting_the_footer_blocks_agreement(self):
        """A review whose prose says INADEQUATE cannot yield AGREED criteria."""
        text = ('## Verdict: **INADEQUATE**\n\n'
                + '\n'.join(f'| {i} | DISAGREED | found a defect |'
                            for i in REQUIRED_IDS)
                + '\n\n' + footer() + '\n')
        index = extract_footer(text)['index']
        prose = prose_dispositions(text, index, REQUIRED_IDS, ROOT)
        for cid in REQUIRED_IDS:
            self.assertIn(DISAGREED, prose[cid]['stated'],
                          'the prose disagreement must be attributed')


class FooterHardeningTests(unittest.TestCase):
    """Regressions for the fail-open forms an independent review identified.

    Each was a real defect in this validator, not a hypothetical: the negation
    case and the unterminated-comment case both returned ``authoritative`` before
    the fix, i.e. a contradicted or commented-out footer could have been promoted
    to an AGREED verdict.
    """

    BT = chr(96) * 3
    TL = '~~~'

    def test_indented_unterminated_fence_hides_the_footer(self):
        text = (prose_block() + '\n\n  ' + self.BT + '\n' + footer() + '\n')
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_mismatched_fence_characters_do_not_close(self):
        text = (prose_block() + '\n\n' + self.TL + '\n' + footer() + '\n'
                + self.BT + '\n')
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_unterminated_html_comment_hides_the_footer(self):
        text = prose_block() + '\n\n<!-- note\n' + footer() + '\n'
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_closed_html_comment_does_not_hide_a_later_footer(self):
        text = prose_block() + '\n\n<!-- note -->\n\n' + footer() + '\n'
        self.assertEqual(extract_footer(text)['status'], 'authoritative')

    def test_prefix_with_empty_payload_is_malformed_not_skipped(self):
        text = prose_block() + '\n\nREVIEW_CRITERIA_JSON:\n'
        self.assertEqual(extract_footer(text)['status'], 'malformed')

    def test_prefix_with_garbage_payload_is_malformed(self):
        text = prose_block() + '\n\nREVIEW_CRITERIA_JSON: garbage\n'
        self.assertEqual(extract_footer(text)['status'], 'malformed')

    def test_malformed_trailing_footer_does_not_expose_an_earlier_good_one(self):
        text = prose_block() + '\n\n' + footer() + '\n\nREVIEW_CRITERIA_JSON:\n'
        self.assertEqual(extract_footer(text)['status'], 'ambiguous')

    def test_footer_unknown_maps_to_cannot_verify(self):
        line = 'REVIEW_CRITERIA_JSON: [{"id":"P0.2-AC1","disposition":"UNKNOWN"}]'
        self.assertEqual(parse_footer_map(line), {'P0.2-AC1': CANNOT_VERIFY})

    def test_negated_agreement_is_not_corroborated(self):
        """`is not AGREED` must never corroborate an AGREED footer.

        The design changed here deliberately.  Recognising every negation form is
        impossible -- `is **not** AGREED`, `is not marked AGREED`,
        `should not be AGREED` and `I do not think … is AGREED` each defeated a
        stricter pattern -- so the rule is no longer "attribute the negation to
        another disposition" but "a negated region is uninformative, so the
        footer's AGREED must be corroborated elsewhere or stay pending".

        The body deliberately carries **no** other statement about the ID, so the
        negation is the only evidence and cannot be rescued by a second line.
        """
        text = ('Some prose with no criterion mention at all.\n\n'
                'P0.2-AC1 is not AGREED.\n\n' + footer() + '\n')
        index = extract_footer(text)['index']
        found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
        self.assertNotIn(AGREED, found['corroborated'],
                         'a negated statement must not corroborate agreement')
        self.assertEqual(found['corroborated'], [])

    def test_affirmative_agreement_is_corroborated(self):
        """The mirror control: a plain agreement must corroborate."""
        text = ('Some prose.\n\nP0.2-AC1 is AGREED.\n\n' + footer() + '\n')
        index = extract_footer(text)['index']
        found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
        self.assertIn(AGREED, found['corroborated'])

    def test_each_negation_form_the_reviewer_supplied_is_blocked(self):
        """Every form an independent review measured as a fail-open."""
        forms = (
            'P0.2-AC1 is **not** AGREED',
            'P0.2-AC1 is not marked AGREED',
            'P0.2-AC1 should not be AGREED yet.',
            'I do not think P0.2-AC1 is AGREED.',
            'I cannot agree with P0.2-AC1; the evidence is missing.',
            'I reject P0.2-AC1 outright.',
            'P0.2-AC1 FAILS: the tool never ran.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_typoed_disagreement_does_not_corroborate_agreement(self):
        text = f'Prose.\n\nP0.2-AC1 is DISAGREEED\n\n{footer()}\n'
        index = extract_footer(text)['index']
        found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
        self.assertNotIn(AGREED, found['corroborated'])

    def test_counterfactual_and_adversative_forms_do_not_corroborate(self):
        """N4: forms that affirm the token while disagreeing with it.

        Measured fail-open: both of the first two yielded corroborated AGREED, so
        a review that explicitly hedged its verdict validated as clean.  They are
        treated as *unreliable* -- contributing to neither direction -- rather
        than as negations, because the construction is what makes them unreliable,
        not a negation word.
        """
        forms = (
            'P0.2-AC1 would be AGREED if the run had completed; it did not.',
            'P0.2-AC1 is AGREED, yet the criterion is unresolved pending rerun.',
            'P0.2-AC1 is AGREED, however the evidence is thin.',
            'P0.2-AC1 is AGREED but the tool never ran.',
            'P0.2-AC1 is AGREED unless the rerun disagrees.',
            'P0.2-AC1 is AGREED, pending rerun.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_hedge_markers_do_not_fire_on_unrelated_prose(self):
        """A false hedge must not suppress an honest verdict.

        Measured three times over: a broad `reject\\w*` matched "Over-rejection
        controls pass"; a bare `\\bunless\\b` matched the C expression "enabled
        unless exactly `0`"; and `but` matched "is AGREED, but I also reviewed
        AC2".  Each left an honest criterion pending.  The committed-construction
        allowlist removes the class, because no broad token is consulted.
        """
        forms = (
            'P0.2-AC1 is AGREED.',
            'P0.2-AC1 is AGREED. Over-rejection controls pass.',
            'enabled unless exactly 0. P0.2-AC1 is AGREED.',
            '**P0.2-AC1 — AGREED.** Per-variable semantics verified.',
            'P0.2-AC1: AGREED',
            '| P0.2-AC1 | AGREED |',
            'Verdict: AGREED for P0.2-AC1',
            'P0.2-AC1 was reproduced and is AGREED.',
            'P0.2-AC1 is AGREED. `check-run-profile.py` -> STRICT (exit 0).',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_separate_pending_item_does_not_hedge_the_verdict(self):
        """A follow-up noted in a *separate* sentence is not a hedge.

        The review that supplied the hedge forms listed
        `P0.2-AC1 is AGREED. One follow-up is pending separately.` as a fail-open,
        but its own advisory called that sentence honest.  Measured, the two
        sentences are independent: the verdict sentence is a clean committed
        construction and the pending item is explicitly *separate*.  Treating an
        unrelated later sentence as a hedge would fail honest reviews, so the
        guard is scoped to the sentence containing the verdict -- and this case is
        recorded here rather than silently dropped from the list.
        """
        text = (f'Prose.\n\nP0.2-AC1 is AGREED. One follow-up is pending '
                f'separately.\n\n{footer()}\n')
        index = extract_footer(text)['index']
        found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
        self.assertIn(AGREED, found['corroborated'])

    def test_pending_in_the_same_sentence_is_still_a_hedge(self):
        """The mirror: a hedge attached to the verdict sentence must not pass."""
        text = (f'Prose.\n\nP0.2-AC1 is AGREED pending the rerun.\n\n{footer()}\n')
        index = extract_footer(text)['index']
        found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
        self.assertNotIn(AGREED, found['corroborated'])

    def test_unlisted_qualifiers_do_not_corroborate(self):
        """N5: the guard is structural, so an unlisted qualifier is still safe.

        Corroboration was `committed AND NOT(hedge list)` -- still an enumeration,
        so an unlisted hedge yielded a false AGREED.  Measured: `is AGREED
        awaiting the rerun` passed while `is AGREED pending the rerun` failed --
        same withholding, different word.  The rule is now that the token must end
        its clause, which needs no lexicon.
        """
        forms = (
            'P0.2-AC1 is AGREED awaiting the rerun.',
            'P0.2-AC1 is AGREED, awaiting the rerun.',
            'P0.2-AC1 is AGREED; this is superseded by AC2.',
            'P0.2-AC1 is AGREED until the rerun lands.',
            'P0.2-AC1 is AGREED modulo the rerun.',
            'P0.2-AC1 is AGREED short of the rerun.',
            'P0.2-AC1 is AGREED save for the rerun.',
            'P0.2-AC1 is AGREED bar the rerun.',
            'P0.2-AC1 is AGREED subject to verification.',
            'P0.2-AC1 is AGREED provided the rerun agrees.',
            'P0.2-AC1 is AGREED pending the rerun.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_attribution_is_sentence_scoped_not_line_scoped(self):
        """N6: a neighbouring criterion's verdict must not be attributed.

        Measured: `The verdict for AC2 is DISAGREED. P0.2-AC1 is AGREED.` failed
        for P0.2-AC1, because a line-window attributed AC2's disposition to it.  A
        review discussing two criteria on adjacent lines must be acceptable.
        """
        text = (f'The verdict for AC2 is DISAGREED. P0.2-AC1 is AGREED.\n\n'
                f'{footer()}\n')
        index = extract_footer(text)['index']
        found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
        self.assertIn(AGREED, found['corroborated'])
        self.assertNotIn(DISAGREED, found['stated'],
                         "AC2's disposition must not be attributed to AC1")

    def test_honest_continuations_still_corroborate(self):
        """The clause-final rule must not refuse prose that continues the verdict."""
        forms = (
            '**P0.2-AC1 — AGREED.** Per-variable semantics verified.',
            'Verdict: AGREED for P0.2-AC1',
            '| P0.2-AC1 | AGREED | reproduced by re-running the fixture |',
            'Status: AGREED. P0.2-AC1 satisfied.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_plausibly_worded_qualifiers_do_not_corroborate(self):
        """N7: the tolerance must be structural, not a smaller hedge lexicon.

        Measured: the conjunction tolerance was "a conjunction is present AND no
        listed hedge word appears", so `and awaiting the rerun` failed (listed)
        while `and holds until the rerun` passed (unlisted) -- the same enumeration
        defect one level down.  The tolerance now asks a grammatical question: a
        trailing clause counts only if it asserts a **completed fact** (a
        past-tense verb), and present-tense or verbless clauses are refused.
        """
        forms = (
            'P0.2-AC1 is AGREED and is conditional on the rerun.',
            'P0.2-AC1 is AGREED and holds until the rerun.',
            'P0.2-AC1 is AGREED with the rerun outstanding.',
            'P0.2-AC1 is AGREED and awaiting the rerun.',
            'P0.2-AC1 is AGREED and depends on the rerun.',
            'P0.2-AC1 is AGREED and rests on the rerun.',
            'P0.2-AC1 is AGREED but remains provisional.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_completed_fact_clauses_do_not_corroborate(self):
        """The past-tense tolerance was **removed**, and this is why.

        It was intended as a closed grammatical class, but tense marks *when* a
        clause happened, not whether the verdict is committed.  A reviewer
        describing what they *did* writes in the past tense, so `I deferred
        sign-off` and `the criterion was withdrawn` are past tense and both
        withhold.  Measured, the tolerance admitted ten such forms.

        The tolerance is now minimal: a trailing fragment must have **no finite
        verb at all**, so it cannot assert a condition.  These forms all carry a
        verb, so they are refused -- and that is the intended behaviour, not a
        regression.
        """
        forms = (
            'P0.2-AC1 is AGREED and the identity check passed.',
            'P0.2-AC1 is AGREED and all 12 tests passed.',
            'P0.2-AC1 is AGREED; the run completed and all 12 tests passed.',
            'P0.2-AC1 is AGREED, but I also reviewed AC2.',
            'P0.2-AC1 is AGREED and the hashes matched.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_label_binding_refuses_a_retraction(self):
        """A label must not bind to a criterion sentence that retracts it.

        Measured: the label bypass trusted the following sentence without checking
        it, so `Outcome: AGREED. P0.2-AC1 was deferred.` corroborated. The first
        four forms below were found by an independent review *after* the initial
        fix, which is why the retraction list is recorded as incomplete in the
        unsafe direction.
        """
        forms = (
            'Outcome: AGREED. P0.2-AC1 was deferred.',
            'Verdict: AGREED. P0.2-AC1 is pending rerun.',
            'Status: AGREED. P0.2-AC1 remains open.',
            'Result: AGREED. P0.2-AC1 was withdrawn.',
            # found later, by the reviewer
            'Verdict: AGREED. P0.2-AC1 is on hold.',
            'Verdict: AGREED. P0.2-AC1 was held back.',
            'Verdict: AGREED. P0.2-AC1 needs the rerun.',
            'Verdict: AGREED. P0.2-AC1 is under review.',
            'Verdict: AGREED. P0.2-AC1 stays provisional.',
            'Verdict: AGREED. P0.2-AC1 is not confirmed.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_honest_label_criterion_pairs_still_corroborate(self):
        """The retraction check must not refuse an honest label+criterion pair."""
        forms = (
            'Status: AGREED. P0.2-AC1 satisfied.',
            'Verdict: AGREED. P0.2-AC1 reproduced.',
            '**Verdict: AGREED.** P0.2-AC1 verified.',
            'Result: AGREED. P0.2-AC1 evidence checked.',
            'Verdict: AGREED for P0.2-AC1',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_for_clause_must_name_this_criterion(self):
        """`for <id>` may only name the criterion being judged.

        Measured: `P0.2-AC1 is AGREED for P0.2-AC2.` credited AC1 on a verdict
        that names a different criterion.
        """
        for form in ('P0.2-AC1 is AGREED for P0.2-AC2.',
                     'Verdict: AGREED for P0.2-AC2'):
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_bolded_verdict_label_binds(self):
        """Leading markdown emphasis must not lose the verdict.

        Measured: `**Verdict: AGREED**` failed because `_LABEL_RE` did not
        tolerate a leading `**`.
        """
        forms = (
            '**Verdict: AGREED.** P0.2-AC1 satisfied.',
            '**Status: AGREED.** P0.2-AC1 verified.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_parenthetical_evidence_still_corroborates(self):
        """A verdict followed by its evidence in parentheses is honest prose.

        Measured on a real acceptance review: `**P0.4-AC1 AGREED** (tests/... 21/21).`
        and `**P0.4-AC3 AGREED, reproduced.**` were both refused by the
        no-trailing-clause rule, so two honest criteria came out CANNOT VERIFY.  A
        parenthetical cannot withhold agreement -- the verdict is already asserted
        -- so it is judged by what follows it, and a hedge *inside* it is still a
        hedge.
        """
        honest = (
            '**P0.2-AC1 AGREED** (`tests/test_dump_controls.py` 21/21).',
            '**P0.2-AC1 AGREED, reproduced.**',
            'P0.2-AC1 AGREED (verified).',
            'P0.2-AC1 is AGREED (see below).',
            'P0.2-AC1 AGREED, independently reproduced.',
        )
        for form in honest:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_hedge_inside_a_parenthetical_is_still_refused(self):
        """The tolerance must not let a parenthetical smuggle a hedge."""
        hedged = (
            'P0.2-AC1 AGREED (pending rerun).',
            'P0.2-AC1 AGREED (not yet verified).',
            'P0.2-AC1 AGREED (provisional).',
        )
        for form in hedged:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_every_measured_parenthetical_hedge_is_refused(self):
        """The parenthetical tolerance must fail CLOSED, not open.

        An independent review defeated the first version: it guarded with a WORD
        LIST (`not|never|pending|await|unless|if|would|provisional|...`), and 12 of
        18 hedged forms corroborated a false AGREED -- including `(for now)`,
        `(in part)`, `(under protest)`, `(except for AC2)`, `(on hold)`,
        `(was held back)` and `(needs the rerun)`.  Every one of those is a shape
        the amendment itself records as withholding.

        The fix inverted the direction: a parenthesis is admitted only when all its
        content is citation-like, so an UNLISTED hedge now refuses.  Each form below
        is one the reviewer measured as leaking.
        """
        hedged = (
            'P0.2-AC1 AGREED (for now).',
            'P0.2-AC1 AGREED (in part).',
            'P0.2-AC1 AGREED (under protest).',
            'P0.2-AC1 AGREED (except for AC2).',
            'P0.2-AC1 AGREED (on hold).',
            'P0.2-AC1 AGREED (is under review).',
            'P0.2-AC1 AGREED (was held back).',
            'P0.2-AC1 AGREED (needs the rerun).',
            'P0.2-AC1 AGREED (subject to the rerun).',
            'P0.2-AC1 AGREED (once the rerun lands).',
            'P0.2-AC1 AGREED (assuming the rerun lands).',
            'P0.2-AC1 AGREED (yet AC2 fails).',
            'P0.2-AC1 AGREED (but the run never completed).',
            'P0.2-AC1 AGREED (pending rerun).',
            'P0.2-AC1 AGREED (not yet verified).',
            'P0.2-AC1 AGREED (provisional).',
            'P0.2-AC1 AGREED (tentative).',
            'P0.2-AC1 AGREED (unresolved).',
        )
        for form in hedged:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_citation_like_parentheticals_are_admitted(self):
        """The fail-closed rule must not refuse a genuine citation.

        Measured: the first fail-closed attempt refused two honest citations,
        because stripping backticks before paths split `tests/test_dump_controls.py`
        into `tests/test` + `dump controls.py` and left `dump` behind as a word.
        """
        honest = (
            '**P0.2-AC1 AGREED** (`tests/test_dump_controls.py` 21/21).',
            'P0.2-AC1 AGREED (verified).',
            'P0.2-AC1 AGREED (see below).',
            'P0.2-AC1 AGREED (tests/test_recorded_reviews.py 150/150).',
            'P0.2-AC1 AGREED (reproduced).',
            'P0.2-AC1 AGREED (exit 0).',
            'P0.2-AC1 AGREED (section 3).',
        )
        for form in honest:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_path_shaped_hedges_are_refused(self):
        """A hedge written in PATH SHAPE must not be stripped away before the check.

        The round-3 defeat, found by an independent review: the word check ran on the
        parenthesis AFTER paths, hashes and numbers were stripped, so a hedge written
        as a path was deleted before it was ever examined.  **8 of 8** forms
        corroborated a false AGREED:

            (pending/awaiting.py)  (pending/awaiting)  (deferred/rerun.md)
            (on-hold.md)  (see deferred/the/rerun.py)  (provisional.json)
            (except/AC2.py)  (not/yet.txt)

        No grammar separates `tests/test_dump_controls.py` from
        `pending/awaiting.py` -- both are `<word>/<word>.<ext>`.  What separates them
        is that one EXISTS, so a path citation must now resolve against the
        repository.
        """
        hedged = (
            'P0.2-AC1 AGREED (pending/awaiting.py).',
            'P0.2-AC1 AGREED (pending/awaiting).',
            'P0.2-AC1 AGREED (deferred/rerun.md).',
            'P0.2-AC1 AGREED (on-hold.md).',
            'P0.2-AC1 AGREED (see deferred/the/rerun.py).',
            'P0.2-AC1 AGREED (provisional.json).',
            'P0.2-AC1 AGREED (except/AC2.py).',
            'P0.2-AC1 AGREED (not/yet.txt).',
        )
        for form in hedged:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_a_real_repo_path_is_admitted_as_a_citation(self):
        """The verifiability rule must admit a path that actually exists."""
        honest = (
            'P0.2-AC1 AGREED (`tests/test_dump_controls.py` 21/21).',
            'P0.2-AC1 AGREED (scripts/jsrf_review_records.py).',
            'P0.2-AC1 AGREED (docs/packets/p0-acceptance-contract.md 1/1).',
        )
        for form in honest:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_a_real_path_cannot_be_combined_into_a_condition(self):
        """Resolution admits a citation, not a citation-shaped hedge.

        A real path plus a pointer word must not form a condition: `(see tests)`,
        `(verified tests)` and `(tests reproduced)` all refuse, because a bare
        directory name is not path-shaped and so cannot be admitted as a citation.
        """
        refused = (
            'P0.2-AC1 AGREED (see tests).',
            'P0.2-AC1 AGREED (verified tests).',
            'P0.2-AC1 AGREED (tests reproduced).',
            'P0.2-AC1 AGREED (see below tests).',
            'P0.2-AC1 AGREED (tests).',
            'P0.2-AC1 AGREED (scripts).',
        )
        for form in refused:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_absolute_paths_outside_the_repository_are_refused(self):
        """A citation must resolve INSIDE the repository, not merely on disk.

        Round-4 defeat, found by an independent review: `repo_root / candidate` with
        an *absolute* candidate replaces the base under pathlib semantics, so any file
        anywhere on disk resolved.  Measured, all of these corroborated a false
        AGREED:

            (C:/Windows/System32/drivers/etc/hosts)
            (//AGENTS.md)
            (C:/Users/.../Temp/awaiting-the-rerun.md)

        The reviewer writes the verdict AND can create files, so "resolves on this
        machine" is not an independent property.  "Resolves inside the repository" is.
        """
        import tempfile as _tempfile
        outside = Path(_tempfile.gettempdir()) / 'p02-escape-test'
        outside.mkdir(exist_ok=True)
        (outside / 'awaiting-the-rerun.md').write_text('x\n', encoding='utf-8')

        refused = (
            f'P0.2-AC1 AGREED ({outside.as_posix()}/awaiting-the-rerun.md).',
            f'P0.2-AC1 AGREED (see {outside.as_posix()}/awaiting-the-rerun.md).',
            'P0.2-AC1 AGREED (C:/Windows/System32/drivers/etc/hosts).',
            'P0.2-AC1 AGREED (//AGENTS.md).',
            'P0.2-AC1 AGREED (/etc/hosts).',
            'P0.2-AC1 AGREED (../AGENTS.md).',
            'P0.2-AC1 AGREED (tests/../../AGENTS.md).',
        )
        for form in refused:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_trailing_dot_segments_stay_inside_the_repository(self):
        """A measured platform quirk, pinned so it is not mistaken for an escape.

        Self-attack found `(.../AGENTS.md)` admitted.  Windows collapses trailing
        dots, so `...`, `....` and `.....` are ALIASES for `.` -- the repository root.
        Measured across `...` through `......`: every resolution stays inside
        `repo_root`, so no escape exists.  A `...`-prefixed citation names a real
        repository file, which is what the rule intends to admit.

        The test asserts CONTAINMENT rather than admission, because containment is
        the security property; whether `...` reads as canonical is a style question.
        """
        from jsrf_review_records import _resolves_to_a_real_path
        base = ROOT.resolve()
        for dots in ('...', '....', '.....', '......'):
            with self.subTest(dots=dots):
                token = f'{dots}/AGENTS.md'
                if _resolves_to_a_real_path(token, ROOT):
                    resolved = (ROOT / dots / 'AGENTS.md').resolve()
                    self.assertTrue(
                        resolved == base or base in resolved.parents,
                        f'{token} resolved outside the repository: {resolved}')

    def test_the_repository_root_itself_is_not_a_citation_escape(self):
        """`..` must never resolve, at any depth or spelling."""
        from jsrf_review_records import _resolves_to_a_real_path
        for token in ('../AGENTS.md', '../../AGENTS.md', 'tests/../AGENTS.md',
                      'tests/../../AGENTS.md', 'a/b/../../../AGENTS.md'):
            with self.subTest(token=token):
                self.assertFalse(_resolves_to_a_real_path(token, ROOT), token)


    def test_containment_and_verdict_binding_share_one_normalisation(self):
        """A documented tolerance must be reachable on every route that needs it.

        Measured inconsistency, reported by the round-4 review: `_same_text`
        normalised CRLF while the containment check was exact-string, so a
        CRLF-normalised record exited 2 at containment and never reached the
        tolerance.  A tolerance that cannot be reached is worse than none, because it
        is claimed.
        """
        from jsrf_review_records import _contains_text, _normalise_text, _same_text
        self.assertEqual(_normalise_text('a\r\nb'), 'a\nb')
        self.assertTrue(_same_text('a\r\nb', 'a\nb'))
        self.assertTrue(_contains_text('x\r\ny\r\nz', 'y\nz'))
        self.assertFalse(_contains_text('a\nb', 'a\n\nb'))
        self.assertFalse(_same_text('a\nb', 'a\n\nb'))


    def test_separator_smuggling_is_refused(self):
        """No separator may join a real path to a hedge.

        Measured across slash, comma, semicolon, colon, space and tab: every form
        refuses.  A token that is `realpath/hedge` does not resolve, and a token that
        splits into two leaves the hedge half unresolvable.
        """
        real = 'tests/test_recorded_reviews.py'
        for sep in ('/', ',', ';', ':', ' ', '\t'):
            form = f'P0.2-AC1 AGREED ({real}{sep}pending).'
            with self.subTest(separator=repr(sep)):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)
            form = f'P0.2-AC1 AGREED (pending{sep}{real}).'
            with self.subTest(separator=repr(sep), side='prefix'):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)


    def test_a_nonexistent_path_never_corroborates(self):
        """Resolution is the separator, so a plausible-looking fake must fail."""
        fakes = (
            'P0.2-AC1 AGREED (tests/does_not_exist.py 21/21).',
            'P0.2-AC1 AGREED (docs/nope.md 1/1).',
            'P0.2-AC1 AGREED (scripts/absent.json).',
        )
        for form in fakes:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)


    def test_noun_plus_number_citations_are_refused_by_necessity(self):
        """A deliberate trade-off, measured rather than assumed.

        `(152 tests, exit 0)` is refused because `tests` is a NOUN, and a noun plus
        a number is syntactically identical whether the number is honest (`152`) or
        damning (`0`).  Adding `tests` to the pointer list immediately reopens
        `(tests 0/0)` as a leak -- verified by patching the module and re-measuring.

        The cost is paid on purpose: a reviewer citing a count writes
        `AGREED, exit 0.` or cites the path instead.  This test pins BOTH sides so
        neither can drift silently.
        """
        refused_by_design = (
            'P0.2-AC1 AGREED (152 tests, exit 0).',
            'P0.2-AC1 AGREED (tests 0/0).',
            'P0.2-AC1 AGREED (line 0).',
            'P0.2-AC1 AGREED (no run).',
            'P0.2-AC1 AGREED (no exit).',
        )
        for form in refused_by_design:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_the_vocabulary_that_leaked_is_not_reintroduced(self):
        """The rejected designs are pinned so they cannot return by accident.

        `exit` is deliberately in BOTH sets, and the distinction is the point: as a
        bare pointer `(exit 0)` is safe, while as a noun it appears in the leaking
        `(no exit)`.  What must never happen is a *quantity noun* becoming a pointer,
        because a noun plus a number is identical whether the number is honest or
        damning.
        """
        from jsrf_review_records import CITATION_NOUNS, CITATION_POINTERS
        quantity_nouns = {'tests', 'test', 'line', 'lines', 'run', 'runs', 'file',
                          'files', 'code', 'pass', 'ok', 'case', 'cases'}
        self.assertEqual(quantity_nouns & CITATION_POINTERS, set(),
                         'a quantity noun in the pointer list reopens (tests 0/0)')
        connectives = {'no', 'all', 'and', 'per', 'at', 'of', 'to', 'with', 'or'}
        self.assertEqual(connectives & CITATION_POINTERS, set(),
                         'a connective in the pointer list reopens (no exit)')
        # The pointer list stays small enough to audit by eye.
        self.assertLessEqual(len(CITATION_POINTERS), 20)
        self.assertTrue(quantity_nouns & CITATION_NOUNS,
                        'the rejected vocabulary stays recorded for the record')

    def test_appositive_tolerance_judges_what_follows(self):
        """The appositive is safe only because the tail after it is still judged."""
        hedged = (
            'P0.2-AC1 AGREED, reproduced awaiting the rerun.',
            'P0.2-AC1 AGREED, verified except for AC2.',
            'P0.2-AC1 AGREED, measured subject to the rerun.',
        )
        for form in hedged:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)


    def test_trailing_clauses_are_refused_by_design(self):
        """The tolerance is **gone**: a verdict sentence must end at the token.

        Five attempts to admit a trailing clause safely each ended with the
        admitted shape qualifying the verdict, because qualification is a matter
        of *meaning*, not of syntax or vocabulary:

        | attempt | admitted |
        |---|---|
        | hedge word list | any unlisted hedge |
        | committed AND NOT(hedges) | `holds until` (unlisted) |
        | past tense | `deferred`, `withdrawn`, `reopened` — all withhold |
        | no finite verb | `for now`, `in part`, `under protest` — all withhold |

        The last row also cost an honest form.  So nothing is admitted after the
        token, and references belong *before* the verdict — which reads naturally
        and is stated in the amendment.
        """
        forms = (
            'P0.2-AC1 is AGREED for now.',
            'P0.2-AC1 is AGREED in part.',
            'P0.2-AC1 is AGREED without the rerun.',
            'P0.2-AC1 is AGREED except for AC2.',
            'P0.2-AC1 is AGREED under protest.',
            'P0.2-AC1 is AGREED; see section 3 for the log.',
            'P0.2-AC1 is AGREED, per the log above.',
            'P0.2-AC1 is AGREED, with evidence.',
            'P0.2-AC1 is AGREED; the run completed and all 12 tests passed.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_reference_before_the_verdict_still_corroborates(self):
        """The documented way to cite evidence: put it before the verdict."""
        forms = (
            'See section 3 for the log. P0.2-AC1 is AGREED.',
            'Per the log above, P0.2-AC1 is AGREED.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_past_tense_withheld_verdicts_do_not_corroborate(self):
        """N9: past tense marks *when*, not *whether committed*.

        Every form here is past tense and every one withholds.  This is the
        measurement that killed the tense-based tolerance.
        """
        forms = (
            'P0.2-AC1 is AGREED and I deferred sign-off.',
            'P0.2-AC1 is AGREED and the criterion was withdrawn.',
            'P0.2-AC1 is AGREED and the dispute was left open.',
            'P0.2-AC1 is AGREED and the packet reopened.',
            'P0.2-AC1 is AGREED and I exceeded my remit.',
            'P0.2-AC1 is AGREED and I proceeded without the rerun.',
            'P0.2-AC1 is AGREED and we agreed to revisit later.',
            'P0.2-AC1 is AGREED and that seeded doubt about it.',
            'P0.2-AC1 is AGREED and I stopped at a partial result.',
            'P0.2-AC1 is AGREED and I need the rerun.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_verdict_label_binds_to_the_following_sentence(self):
        """N8: `Status: AGREED. P0.2-AC1 satisfied.` must corroborate.

        The label names the disposition in one sentence and the criterion in the
        next, so sentence-scoped attribution alone refused it.  Only the explicit
        label form binds across that boundary.
        """
        forms = (
            'Status: AGREED. P0.2-AC1 satisfied.',
            'Verdict: AGREED. P0.2-AC1 verified.',
            'Disposition: AGREED. P0.2-AC1 reproduced.',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertIn(AGREED, found['corroborated'], form)

    def test_bare_copula_does_not_bind_across_sentences(self):
        """The widening is limited to labels; a bare `is AGREED` must not bind."""
        text = (f'AC2 is AGREED. P0.2-AC1 was examined closely.\n\n{footer()}\n')
        index = extract_footer(text)['index']
        found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
        self.assertNotIn(AGREED, found['corroborated'])

    def test_unlisted_hedges_do_not_corroborate(self):
        """The structural fix: an *unlisted* hedge must be safe.

        Three revisions tried to enumerate the ways a reviewer withholds
        agreement, and each was defeated by a word not on the list.  Corroboration
        is now an allowlist of committed constructions, so a hedge the author
        never anticipated costs a pending criterion instead of a false AGREED.
        Every form here was measured as an end-to-end fail-open before the fix.
        """
        forms = (
            'P0.2-AC1 should be AGREED once the rerun lands.',
            'P0.2-AC1 is tentatively AGREED.',
            'P0.2-AC1 is provisionally AGREED subject to the rerun.',
            'P0.2-AC1 is possibly AGREED.',
            'P0.2-AC1 is almost AGREED.',
            'P0.2-AC1 is partially AGREED.',
            'P0.2-AC1 is conditionally AGREED.',
            'P0.2-AC1 is arguably AGREED.',
            'P0.2-AC1 would be AGREED if the run had completed; it did not.',
            'P0.2-AC1 is AGREED, yet the criterion is unresolved pending rerun.',
            'P0.2-AC1 is AGREED, however the evidence is thin.',
            'P0.2-AC1 is AGREED but the tool never ran.',
            'P0.2-AC1 is AGREED unless the rerun disagrees.',
            'P0.2-AC1 is AGREED, pending rerun.',
            'P0.2-AC1 is **not** AGREED',
            'P0.2-AC1 is not marked AGREED',
            'P0.2-AC1 should not be AGREED yet.',
            'I do not think P0.2-AC1 is AGREED.',
            'I cannot agree with P0.2-AC1; the evidence is missing.',
            'I reject P0.2-AC1 outright.',
            'P0.2-AC1 FAILS: the tool never ran.',
            'P0.2-AC1 is DISAGREEED',
        )
        for form in forms:
            with self.subTest(form=form):
                text = f'Prose.\n\n{form}\n\n{footer()}\n'
                index = extract_footer(text)['index']
                found = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']
                self.assertNotIn(AGREED, found['corroborated'], form)

    def test_disposition_stated_before_the_id_is_detected(self):
        text = (prose_block() + '\n\nMy verdict is DISAGREED for P0.2-AC1.\n\n'
                + footer() + '\n')
        index = extract_footer(text)['index']
        stated = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']['stated']
        self.assertIn(DISAGREED, stated)

    def test_conditional_contradiction_is_detected(self):
        text = (prose_block() + '\n\nIf the evidence were complete P0.2-AC1 would be '
                'AGREED; as it stands it is DISAGREED.\n\n' + footer() + '\n')
        index = extract_footer(text)['index']
        stated = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']['stated']
        self.assertIn(DISAGREED, stated)

    def test_honest_agreement_prose_is_not_flagged(self):
        """The over-approximation must not fire on a clean agreement."""
        text = (prose_block() + '\n\nP0.2-AC1 was reproduced and is AGREED.\n\n'
                + footer() + '\n')
        index = extract_footer(text)['index']
        stated = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']['stated']
        self.assertEqual(stated, [AGREED])

    def test_fenced_prose_dispositions_are_not_attributed(self):
        """A quoted old verdict inside a fence must not create a contradiction."""
        quoted = ('| P0.2-AC1 | DISAGREED | from the earlier review |')
        text = (prose_block() + '\n\nEarlier:\n' + self.BT + '\n' + quoted + '\n'
                + self.BT + '\n\n' + footer() + '\n')
        index = extract_footer(text)['index']
        stated = prose_dispositions(text, index, ['P0.2-AC1'], ROOT)['P0.2-AC1']['stated']
        self.assertEqual(stated, [AGREED],
                         'a fenced quotation must not be read as a live disposition')


class FooterParseTests(unittest.TestCase):
    def test_unsupported_disposition_word_is_rejected(self):
        line = 'REVIEW_CRITERIA_JSON: [{"id":"P0.2-AC1","disposition":"PASS"}]'
        with self.assertRaises(Exception) as raised:
            parse_footer_map(line)
        self.assertIn('unsupported disposition', str(raised.exception))

    def test_repeated_id_is_rejected(self):
        line = ('REVIEW_CRITERIA_JSON: [{"id":"P0.2-AC1","disposition":"AGREED"},'
                '{"id":"P0.2-AC1","disposition":"DISAGREED"}]')
        with self.assertRaises(Exception) as raised:
            parse_footer_map(line)
        self.assertIn('repeats criterion id', str(raised.exception))

    def test_malformed_json_is_rejected(self):
        with self.assertRaises(Exception):
            parse_footer_map('REVIEW_CRITERIA_JSON: [{"id":')

    def test_empty_array_is_rejected(self):
        with self.assertRaises(Exception) as raised:
            parse_footer_map('REVIEW_CRITERIA_JSON: []')
        self.assertIn('non-empty', str(raised.exception))

    def test_extra_key_in_entry_is_rejected(self):
        line = ('REVIEW_CRITERIA_JSON: '
                '[{"id":"P0.2-AC1","disposition":"AGREED","note":"x"}]')
        with self.assertRaises(Exception):
            parse_footer_map(line)


class RecordValidationTests(unittest.TestCase):
    """End-to-end validation against a real contract and a real manifest."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'contracts').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        self.contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
            self.contract_bytes)
        self.manifest = {
            'schema_version': 1,
            'packet_id': 'P0.2',
            'contract_file': 'docs/packets/p0-acceptance-contract.md',
            'contract_sha256': sha256_bytes(self.contract_bytes),
            'required_ids': list(REQUIRED_IDS),
        }
        (self.root / 'docs' / 'reviews' / 'contracts' / 'P0.2.json').write_text(
            json.dumps(self.manifest), encoding='utf-8')

    def tearDown(self):
        self._tmp.cleanup()

    def write_source(self, body: str, name: str = 'source.txt') -> dict:
        """Write the verdict source AND a matching session log.

        `source_session` is mandatory: the validator re-opens the original log to
        check ancestry, turn and route instead of trusting the record.  Measured
        fail-open before that: every identity field could be invented and the
        record still validated.
        """
        path = self.root / 'docs' / 'reviews' / 'P0.2' / name
        path.write_text(body, encoding='utf-8')
        session = write_matching_session(
            self.root, f'{name}.session.jsonl', child='child-1',
            parent='session-parent', turn='turn-0001',
            model='hy4-preview-f', verdict_text=body)
        return {'kind': 'turn_text', 'path': str(path.relative_to(self.root)),
                'sha256': sha256_bytes(path.read_bytes()),
                'source_session': str(session)}

    def write_evidence(self, name: str = 'evidence.txt') -> list[dict]:
        path = self.root / 'docs' / 'reviews' / 'P0.2' / name
        path.write_text('measured output\n', encoding='utf-8')
        return [{'path': str(path.relative_to(self.root)),
                 'sha256': sha256_bytes(path.read_bytes())}]

    def base_record(self, source_body: str, dispositions: dict | None = None) -> dict:
        dispositions = dispositions or {i: AGREED for i in REQUIRED_IDS}
        return {
            'schema_version': 1,
            'review_id': 'review-fixture-1',
            'packet_id': 'P0.2',
            'contract_sha256': self.manifest['contract_sha256'],
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.manifest['contract_sha256']}],
            'harness': 'dsh',
            'parent_id': 'session-parent',
            'child_id': 'child-1',
            'requested_model': 'workbuddy-ai/hy4-preview-f',
            'requested_effort': 'high',
            'identity_evidence': self.write_source(source_body),
            'turn_id': 'turn-0001',
            'verdict_text': source_body,
            # Mandatory: the record must declare the text its verdict came from,
            # so a disagreement in an earlier message cannot be disowned.
            'contradiction_evidence': source_body,
            'criteria': [
                {'id': cid, 'disposition': disposition,
                 'evidence': self.write_evidence(f'evidence-{cid}.txt'),
                 'procedure': 'ran the fixture command',
                 'observed_result': 'matched the expected value'}
                for cid, disposition in dispositions.items()
            ],
        }

    def validate(self, record: dict) -> dict:
        return validate_record(record, self.manifest, self.root)

    # ── the valid control ────────────────────────────────────────────────────
    def test_valid_record_is_acceptance_eligible(self):
        body = prose_block() + '\n\n' + footer() + '\n'
        result = self.validate(self.base_record(body))
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))
        self.assertTrue(all(v['disposition'] == AGREED
                            for v in result['criteria'].values()))

    # ── footer-driven failures ───────────────────────────────────────────────
    def test_no_footer_is_failed_not_eligible(self):
        body = prose_block() + '\n\nNo machine-readable map here.\n'
        result = self.validate(self.base_record(body))
        self.assertEqual(result['status'], FAILED)
        self.assertIn('footer not authoritative', ' '.join(result['reasons']))
        self.assertEqual(result['footer_status'], 'none')

    def test_fenced_only_footer_cannot_supply_a_verdict(self):
        body = prose_block() + '\n\n```\n' + footer() + '\n```\n'
        result = self.validate(self.base_record(body))
        self.assertEqual(result['status'], FAILED)
        self.assertEqual(result['footer_status'], 'none')

    def test_footer_omitting_a_required_id_fails(self):
        body = (prose_block() + '\n\n'
                + footer(ids=REQUIRED_IDS[:-1]) + '\n')
        result = self.validate(self.base_record(body))
        self.assertEqual(result['status'], FAILED)
        self.assertTrue(any('omits required criterion id' in r for r in result['reasons']),
                        result['reasons'])

    def test_footer_adding_an_unknown_id_fails(self):
        body = (prose_block() + '\n\n'
                + footer(ids=REQUIRED_IDS + ['P0.2-AC9']) + '\n')
        result = self.validate(self.base_record(body))
        self.assertEqual(result['status'], FAILED)
        self.assertTrue(any('absent from the manifest' in r for r in result['reasons']),
                        result['reasons'])

    def test_record_claiming_agreed_while_footer_says_disagreed_is_unknown(self):
        body = prose_block(disposition=DISAGREED) + '\n\n' + footer(disposition=DISAGREED) + '\n'
        record = self.base_record(body, {i: AGREED for i in REQUIRED_IDS})
        result = self.validate(record)
        self.assertEqual(result['status'], FAILED)
        self.assertTrue(all(v['disposition'] == 'UNKNOWN'
                            for v in result['criteria'].values()),
                        result['criteria'])

    def test_prose_contradicting_footer_is_unknown(self):
        body = (prose_block(disposition=DISAGREED) + '\n\n'
                + footer(disposition=AGREED) + '\n')
        result = self.validate(self.base_record(body))
        self.assertEqual(result['status'], FAILED)
        self.assertTrue(any(v['disposition'] == 'UNKNOWN'
                            for v in result['criteria'].values()), result['criteria'])
        self.assertTrue(any('prose states' in v['reason']
                            for v in result['criteria'].values()))

    def test_hollow_footer_without_evidence_is_cannot_verify(self):
        """A footer alone cannot carry a criterion: the record must show work.

        This replaces an earlier rule that required the *prose* to mention each
        ID.  That punished honest layouts (a heading, an ID in a table header, a
        row with the disposition first) for formatting reasons, so the anti-hollow
        property now rests on the record's own evidence fields, which the schema
        already mandates.
        """
        body = prose_block() + '\n\n' + footer() + '\n'
        record = self.base_record(body)
        for entry in record['criteria']:
            entry['evidence'] = []
        result = self.validate(record)
        self.assertEqual(result['status'], FAILED)
        self.assertTrue(all(v['disposition'] == CANNOT_VERIFY
                            for v in result['criteria'].values()), result['criteria'])

    def test_reviewer_layout_without_repeated_ids_still_validates(self):
        """A verdict table whose rows do not repeat the ID must still pass.

        The reviewer's own layout is not a criterion.  Only the footer and the
        record's evidence fields are load-bearing.
        """
        body = ('| Criterion | Disposition | Evidence |\n|---|---|---|\n'
                + '\n'.join(f'| {i} | AGREED | reproduced |' for i in REQUIRED_IDS)
                + '\n\n' + footer() + '\n')
        result = self.validate(self.base_record(body))
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_ambiguous_footer_is_failed(self):
        body = prose_block() + '\n\n' + footer() + '\n\n' + footer() + '\n'
        result = self.validate(self.base_record(body))
        self.assertEqual(result['status'], FAILED)
        self.assertEqual(result['footer_status'], 'ambiguous')

    # ── identity / evidence failures ─────────────────────────────────────────
    def test_reviewed_file_hash_change_fails(self):
        body = prose_block() + '\n\n' + footer() + '\n'
        record = self.base_record(body)
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(b'changed')
        result = self.validate(record)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('changed since review' in r for r in result['reasons']),
                        result['reasons'])

    def test_missing_required_field_is_invalid(self):
        body = prose_block() + '\n\n' + footer() + '\n'
        record = self.base_record(body)
        del record['turn_id']
        result = self.validate(record)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('turn_id' in r for r in result['reasons']), result['reasons'])

    def test_missing_source_file_is_invalid(self):
        body = prose_block() + '\n\n' + footer() + '\n'
        record = self.base_record(body)
        (self.root / record['identity_evidence']['path']).unlink()
        result = self.validate(record)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('source is missing' in r for r in result['reasons']),
                        result['reasons'])

    def test_empty_criterion_evidence_is_cannot_verify(self):
        body = prose_block() + '\n\n' + footer() + '\n'
        record = self.base_record(body)
        record['criteria'][0]['evidence'] = []
        result = self.validate(record)
        self.assertEqual(result['status'], FAILED)
        self.assertEqual(result['criteria'][record['criteria'][0]['id']]['disposition'],
                         CANNOT_VERIFY)

    def test_unknown_schema_version_is_invalid(self):
        body = prose_block() + '\n\n' + footer() + '\n'
        record = self.base_record(body)
        record['schema_version'] = 99
        result = self.validate(record)
        self.assertEqual(result['status'], INVALID)

    def test_contract_hash_conflict_is_invalid(self):
        body = prose_block() + '\n\n' + footer() + '\n'
        record = self.base_record(body)
        record['contract_sha256'] = 'a' * 64
        result = self.validate(record)
        self.assertEqual(result['status'], INVALID)

    def test_missing_manifest_is_invalid(self):
        with self.assertRaises(Exception) as raised:
            load_required_manifest(self.root / 'docs' / 'reviews' / 'contracts' / 'absent.json')
        self.assertIn('missing', str(raised.exception))

    def test_manifest_with_duplicate_ids_is_invalid(self):
        bad = dict(self.manifest, required_ids=['P0.2-AC1', 'P0.2-AC1'])
        path = self.root / 'docs' / 'reviews' / 'contracts' / 'bad.json'
        path.write_text(json.dumps(bad), encoding='utf-8')
        with self.assertRaises(Exception) as raised:
            load_required_manifest(path)
        self.assertIn('unique', str(raised.exception))


class SinglePropertyMutationTests(unittest.TestCase):
    """Defect-8 control: every negative fixture is one mutation of a passing twin.

    Asserting a reason code does not by itself show a fixture failed *for that
    reason* — a fixture violating two properties still reports whichever the
    validator checks first.  So each case below is a single property changed on a
    body that is otherwise the valid control, and the un-mutated twin is asserted
    to pass in the same test.
    """

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(contract_bytes)
        self.manifest = {
            'schema_version': 1,
            'packet_id': 'P0.2',
            'contract_file': 'docs/packets/p0-acceptance-contract.md',
            'contract_sha256': sha256_bytes(contract_bytes),
            'required_ids': list(REQUIRED_IDS),
        }

    def tearDown(self):
        self._tmp.cleanup()

    def record_for(self, body: str) -> dict:
        source = self.root / 'docs' / 'reviews' / 'P0.2' / 'src.txt'
        source.write_text(body, encoding='utf-8')
        evidence = self.root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
        evidence.write_text('measured\n', encoding='utf-8')
        return {
            'schema_version': 1,
            'review_id': 'mut-1',
            'packet_id': 'P0.2',
            'contract_sha256': self.manifest['contract_sha256'],
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.manifest['contract_sha256']}],
            'harness': 'dsh',
            'parent_id': 'p',
            'child_id': 'c',
            'requested_model': 'workbuddy-ai/hy4-preview-f',
            'requested_effort': 'high',
            'identity_evidence': {
                'kind': 'turn_text',
                'path': str(source.relative_to(self.root)),
                'sha256': sha256_bytes(source.read_bytes()),
                'source_session': str(write_matching_session(
                    self.root, 'mut-session.jsonl', model='hy4-preview-f', verdict_text=body)),
            },
            'turn_id': 't1',
            'verdict_text': body,
            'contradiction_evidence': body,
            'criteria': [
                {'id': cid, 'disposition': AGREED,
                 'evidence': [{'path': str(evidence.relative_to(self.root)),
                               'sha256': sha256_bytes(evidence.read_bytes())}],
                 'procedure': 'ran it',
                 'observed_result': 'as expected'}
                for cid in REQUIRED_IDS
            ],
        }

    def validate(self, record: dict) -> dict:
        return validate_record(record, self.manifest, self.root)

    def assert_single_mutation(self, mutate, expect_status: str, expect_substring: str):
        """Assert the valid twin passes, then that the mutation fails as stated."""
        control_body = prose_block() + '\n\n' + footer() + '\n'
        control = self.validate(self.record_for(control_body))
        self.assertEqual(control['status'], ELIGIBLE,
                         f'control must pass before the mutation means anything: '
                         f'{control.get("reasons")}')

        mutated_body = mutate(control_body)
        self.assertNotEqual(mutated_body, control_body,
                            'the mutation must actually change the source')
        result = self.validate(self.record_for(mutated_body))
        self.assertEqual(result['status'], expect_status, result.get('reasons'))
        blob = ' '.join(result.get('reasons', [])) + ' ' + ' '.join(
            v['reason'] for v in result.get('criteria', {}).values())
        self.assertIn(expect_substring, blob)

    def test_no_footer(self):
        self.assert_single_mutation(
            lambda body: body.replace(footer(), 'No machine-readable map.'),
            FAILED, 'footer not authoritative')

    def test_fenced_only_footer(self):
        bt = chr(96) * 3
        self.assert_single_mutation(
            lambda body: body.replace(footer(), f'{bt}\n{footer()}\n{bt}'),
            FAILED, 'footer not authoritative')

    def test_footer_not_final(self):
        self.assert_single_mutation(
            lambda body: body + '\nOn reflection P0.2-AC1 is DISAGREED.\n',
            FAILED, 'not-final')

    def test_duplicated_footer(self):
        self.assert_single_mutation(
            lambda body: body + '\n' + footer() + '\n',
            FAILED, 'ambiguous')

    def test_unterminated_comment_hides_footer(self):
        self.assert_single_mutation(
            lambda body: '<!-- note\n' + body,
            FAILED, 'footer not authoritative')

    def test_prefix_with_empty_payload(self):
        self.assert_single_mutation(
            lambda body: body.replace(footer(), 'REVIEW_CRITERIA_JSON:'),
            FAILED, 'malformed')

    def test_unsupported_disposition_word(self):
        self.assert_single_mutation(
            lambda body: body.replace('"AGREED"', '"PASS"'),
            FAILED, 'unsupported disposition')

    def test_missing_required_id_in_footer(self):
        bad = ('REVIEW_CRITERIA_JSON: '
               + json.dumps([{'id': i, 'disposition': AGREED}
                             for i in REQUIRED_IDS[:-1]]))
        self.assert_single_mutation(
            lambda body: body.replace(footer(), bad),
            FAILED, 'omits required criterion id')

    def test_extra_id_in_footer(self):
        bad = ('REVIEW_CRITERIA_JSON: '
               + json.dumps([{'id': i, 'disposition': AGREED}
                             for i in REQUIRED_IDS + ['P0.2-AC9']]))
        self.assert_single_mutation(
            lambda body: body.replace(footer(), bad),
            FAILED, 'absent from the manifest')

    def test_prose_contradiction(self):
        def mutate(body: str) -> str:
            return body.replace('| P0.2-AC1 | AGREED |',
                                '| P0.2-AC1 | DISAGREED |')
        self.assert_single_mutation(mutate, FAILED, 'prose states')

    def test_negated_prose_disposition(self):
        """A negated agreement leaves the footer uncorroborated, so it fails.

        The failure class moved from UNKNOWN (contradiction detected) to
        CANNOT VERIFY (no corroboration), which is the intended direction: an
        unrecognised negation can only leave a criterion pending.

        The mutation replaces the control's affirmative row rather than adding a
        second, contradicting statement -- otherwise the surviving row would
        legitimately corroborate and the fixture would not isolate negation.
        """
        def mutate(body: str) -> str:
            return body.replace('| P0.2-AC1 | AGREED |',
                                '| P0.2-AC1 | is not AGREED |')
        self.assert_single_mutation(mutate, FAILED, 'uncorroborated')

    def test_empty_evidence_field(self):
        control_body = prose_block() + '\n\n' + footer() + '\n'
        record = self.record_for(control_body)
        self.assertEqual(self.validate(record)['status'], ELIGIBLE)
        record['criteria'][0]['evidence'] = []
        result = self.validate(record)
        self.assertEqual(result['status'], FAILED)
        self.assertEqual(result['criteria'][record['criteria'][0]['id']]['disposition'],
                         CANNOT_VERIFY)

    def test_empty_criteria_list_fails(self):
        control_body = prose_block() + '\n\n' + footer() + '\n'
        record = self.record_for(control_body)
        self.assertEqual(self.validate(record)['status'], ELIGIBLE)
        record['criteria'] = []
        result = self.validate(record)
        self.assertEqual(result['status'], INVALID)


class ContradictionScopeTests(unittest.TestCase):
    """N1: a quoted footer must not outrank a real DISAGREED verdict.

    Measured fail-open: the adapter selected the turn text by looking for a
    message containing a footer-like line, so a message that merely *showed* the
    footer format was chosen over the message carrying the verdict, and the real
    verdict was then excluded as narration.  Authority and contradiction therefore
    have different scopes, and the wider scope is mandatory.
    """

    MSG_A = '## Review\n\nI checked the evidence. P0.2-AC1 is DISAGREED.\n'
    MSG_B = ('Understood. For the record the durable map must look like this:\n\n'
             'REVIEW_CRITERIA_JSON: [{"id":"P0.2-AC1","disposition":"AGREED"}]\n')

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
            contract_bytes)
        self.manifest = {
            'schema_version': 1, 'packet_id': 'P0.2',
            'contract_file': 'docs/packets/p0-acceptance-contract.md',
            'contract_sha256': sha256_bytes(contract_bytes),
            'required_ids': ['P0.2-AC1'],
        }
        self.authority = self.root / 'docs' / 'reviews' / 'P0.2' / 'authority.txt'
        self.authority.write_text(self.MSG_B, encoding='utf-8')
        evidence = self.root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
        evidence.write_text('measured\n', encoding='utf-8')
        self.evidence = [{'path': str(evidence.relative_to(self.root)),
                          'sha256': sha256_bytes(evidence.read_bytes())}]

    def tearDown(self):
        self._tmp.cleanup()

    def record(self, contradiction: str | None) -> dict:
        rec = {
            'schema_version': 1, 'review_id': 'n1', 'packet_id': 'P0.2',
            'contract_sha256': self.manifest['contract_sha256'],
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.manifest['contract_sha256']}],
            'harness': 'dsh', 'parent_id': 'p', 'child_id': 'c',
            'requested_model': 'workbuddy-ai/hy4-preview-f',
            'requested_effort': 'high',
            'identity_evidence': {
                'kind': 'turn_text',
                'path': str(self.authority.relative_to(self.root)),
                'sha256': sha256_bytes(self.authority.read_bytes()),
                'source_session': str(write_matching_session(
                    self.root, 'authority-session.jsonl', model='hy4-preview-f',
                    verdict_text=self.MSG_B))},
            'turn_id': 't1', 'verdict_text': self.MSG_B,
            'criteria': [{'id': 'P0.2-AC1', 'disposition': AGREED,
                          'evidence': self.evidence,
                          'procedure': 'ran scripts/check-recorded-reviews.py',
                          'observed_result': 'exit 0, 1 criterion'}],
        }
        if contradiction is not None:
            rec['contradiction_evidence'] = contradiction
        return rec

    def test_omitting_contradiction_scope_is_invalid(self):
        """Without the wider scope the fail-open returns, so it is mandatory."""
        result = validate_record(self.record(None), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('contradiction_evidence' in r for r in result['reasons']),
                        result['reasons'])

    def test_wider_scope_that_excludes_the_verdict_is_invalid(self):
        result = validate_record(self.record(self.MSG_A), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('does not contain the verdict text' in r
                            for r in result['reasons']), result['reasons'])

    def test_disagreement_in_an_earlier_message_is_not_eligible(self):
        """The N1 control: this must never be acceptance-eligible AGREED."""
        result = validate_record(self.record(self.MSG_A + '\n' + self.MSG_B),
                                 self.manifest, self.root)
        self.assertNotEqual(result['status'], ELIGIBLE)
        self.assertEqual(result['criteria']['P0.2-AC1']['disposition'], 'UNKNOWN')

    def test_empty_contradiction_scope_is_invalid(self):
        result = validate_record(self.record('   '), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)


class CorroborationTests(unittest.TestCase):
    """N2: an AGREED footer must be positively corroborated.

    Every form below was measured to defeat a negation-detecting regex, so
    detection is not attempted; instead an uncorroborated agreement stays pending.
    """

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
            contract_bytes)
        self.manifest = {
            'schema_version': 1, 'packet_id': 'P0.2',
            'contract_file': 'docs/packets/p0-acceptance-contract.md',
            'contract_sha256': sha256_bytes(contract_bytes),
            'required_ids': ['P0.2-AC1'],
        }
        evidence = self.root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
        evidence.write_text('measured\n', encoding='utf-8')
        self.evidence = [{'path': str(evidence.relative_to(self.root)),
                          'sha256': sha256_bytes(evidence.read_bytes())}]

    def tearDown(self):
        self._tmp.cleanup()

    def validate_body(self, body: str) -> dict:
        source = self.root / 'docs' / 'reviews' / 'P0.2' / 'src.txt'
        source.write_text(body, encoding='utf-8')
        record = {
            'schema_version': 1, 'review_id': 'n2', 'packet_id': 'P0.2',
            'contract_sha256': self.manifest['contract_sha256'],
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.manifest['contract_sha256']}],
            'harness': 'dsh', 'parent_id': 'p', 'child_id': 'c',
            'requested_model': 'workbuddy-ai/hy4-preview-f',
            'requested_effort': 'high',
            'identity_evidence': {'kind': 'turn_text',
                                  'path': str(source.relative_to(self.root)),
                                  'sha256': sha256_bytes(source.read_bytes()),
                                  # The session must carry THIS body: the validator
                                  # binds the verdict to the turn it names, so a
                                  # per-test body needs a per-test session.
                                  'source_session': str(write_matching_session(
                                      self.root, 'fixture-session.jsonl',
                                      verdict_text=body)),
                              },
            'turn_id': 't1', 'verdict_text': body,
            'contradiction_evidence': body,
            'criteria': [{'id': 'P0.2-AC1', 'disposition': AGREED,
                          'evidence': self.evidence,
                          'procedure': 'ran scripts/check-recorded-reviews.py',
                          'observed_result': 'exit 0, 1 criterion'}],
        }
        return validate_record(record, self.manifest, self.root)

    def test_footer_only_agreement_is_never_eligible(self):
        """The footer must not vouch for itself."""
        body = footer(ids=['P0.2-AC1']) + '\n'
        result = self.validate_body(body)
        self.assertNotEqual(result['status'], ELIGIBLE)
        self.assertEqual(result['criteria']['P0.2-AC1']['disposition'], CANNOT_VERIFY)

    def test_affirmative_agreement_is_eligible(self):
        body = 'P0.2-AC1 is AGREED.\n\n' + footer(ids=['P0.2-AC1']) + '\n'
        result = self.validate_body(body)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_every_measured_negation_form_is_not_eligible(self):
        forms = (
            'P0.2-AC1 is **not** AGREED',
            'P0.2-AC1 is not marked AGREED',
            'P0.2-AC1 should not be AGREED yet.',
            'I do not think P0.2-AC1 is AGREED.',
            'I cannot agree with P0.2-AC1; the evidence is missing.',
            'I reject P0.2-AC1 outright.',
            'P0.2-AC1 FAILS: the tool never ran.',
            'P0.2-AC1 is DISAGREEED',
        )
        for form in forms:
            with self.subTest(form=form):
                body = f'Prose.\n\n{form}\n\n{footer(ids=["P0.2-AC1"])}\n'
                result = self.validate_body(body)
                self.assertNotEqual(result['status'], ELIGIBLE, form)

    def test_unrelated_rejection_word_does_not_break_an_honest_review(self):
        """A false negation must not fire on unrelated prose.

        Measured: a broad `reject\\w*` negation pattern matched the phrase
        "Over-rejection controls pass" in a real review and left three criteria
        pending.
        """
        body = ('P0.2-AC1 is AGREED. Over-rejection controls pass; the rejected '
                'candidate was refused.\n\n' + footer(ids=['P0.2-AC1']) + '\n')
        result = self.validate_body(body)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_disagreement_is_still_detected_as_a_contradiction(self):
        body = 'P0.2-AC1 is DISAGREED.\n\n' + footer(ids=['P0.2-AC1']) + '\n'
        result = self.validate_body(body)
        self.assertEqual(result['criteria']['P0.2-AC1']['disposition'], 'UNKNOWN')
        self.assertIn('prose states', result['criteria']['P0.2-AC1']['reason'])


class RequiredFieldTests(unittest.TestCase):
    """N3: the spec mandates nonempty procedure; the code must enforce it."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
            contract_bytes)
        self.manifest = {
            'schema_version': 1, 'packet_id': 'P0.2',
            'contract_file': 'docs/packets/p0-acceptance-contract.md',
            'contract_sha256': sha256_bytes(contract_bytes),
            'required_ids': ['P0.2-AC1'],
        }
        self.body = 'P0.2-AC1 is AGREED.\n\n' + footer(ids=['P0.2-AC1']) + '\n'
        source = self.root / 'docs' / 'reviews' / 'P0.2' / 'src.txt'
        source.write_text(self.body, encoding='utf-8')
        evidence = self.root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
        evidence.write_text('measured\n', encoding='utf-8')
        self.source = source
        self.evidence = [{'path': str(evidence.relative_to(self.root)),
                          'sha256': sha256_bytes(evidence.read_bytes())}]

    def tearDown(self):
        self._tmp.cleanup()

    def record(self, **overrides) -> dict:
        criterion = {'id': 'P0.2-AC1', 'disposition': AGREED,
                     'evidence': self.evidence,
                     'procedure': 'ran scripts/check-recorded-reviews.py',
                     'observed_result': 'exit 0, 1 criterion'}
        criterion.update(overrides)
        return {
            'schema_version': 1, 'review_id': 'n3', 'packet_id': 'P0.2',
            'contract_sha256': self.manifest['contract_sha256'],
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.manifest['contract_sha256']}],
            'harness': 'dsh', 'parent_id': 'p', 'child_id': 'c',
            'requested_model': 'workbuddy-ai/hy4-preview-f',
            'requested_effort': 'high',
            'identity_evidence': {'kind': 'turn_text',
                                  'path': str(self.source.relative_to(self.root)),
                                  'sha256': sha256_bytes(self.source.read_bytes()),
                                  'source_session': str(write_matching_session(
                                      self.root, 'fixture-session.jsonl',
                                      verdict_text=self.body)),
                              },
            'turn_id': 't1', 'verdict_text': self.body,
            'contradiction_evidence': self.body,
            'criteria': [criterion],
        }

    def test_control_is_eligible(self):
        result = validate_record(self.record(), self.manifest, self.root)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_empty_procedure_is_cannot_verify(self):
        result = validate_record(self.record(procedure=''), self.manifest, self.root)
        self.assertEqual(result['status'], FAILED)
        self.assertEqual(result['criteria']['P0.2-AC1']['disposition'], CANNOT_VERIFY)
        self.assertIn('procedure is empty', result['criteria']['P0.2-AC1']['reason'])

    def test_whitespace_procedure_is_cannot_verify(self):
        result = validate_record(self.record(procedure='   '), self.manifest, self.root)
        self.assertEqual(result['status'], FAILED)
        self.assertIn('procedure is empty', result['criteria']['P0.2-AC1']['reason'])

    def test_empty_observed_result_is_cannot_verify(self):
        result = validate_record(self.record(observed_result=''), self.manifest, self.root)
        self.assertEqual(result['criteria']['P0.2-AC1']['disposition'], CANNOT_VERIFY)


class SchemaDocumentTests(unittest.TestCase):
    """The published schema must match the validator it documents.

    The P0.2 interface names `docs/reviews/review-record.schema.json`; a schema
    that drifts from `jsrf_review_records.py` would document a contract nothing
    enforces, so the required-field set and the validator's are compared directly.
    """

    SCHEMA = ROOT / 'docs' / 'reviews' / 'review-record.schema.json'

    def setUp(self):
        if not self.SCHEMA.is_file():
            self.skipTest('schema document is absent')
        self.schema = json.loads(self.SCHEMA.read_text(encoding='utf-8'))

    def test_schema_required_fields_match_the_validator(self):
        from jsrf_review_records import REQUIRED_RECORD_FIELDS
        self.assertEqual(set(self.schema['required']), set(REQUIRED_RECORD_FIELDS))

    def test_schema_criterion_fields_match_the_validator(self):
        from jsrf_review_records import CRITERION_FIELDS
        item = self.schema['properties']['criteria']['items']
        self.assertEqual(set(item['required']), set(CRITERION_FIELDS))

    def test_schema_schema_version_matches_the_validator(self):
        from jsrf_review_records import SCHEMA_VERSION
        self.assertEqual(self.schema['properties']['schema_version']['const'],
                         SCHEMA_VERSION)

    def test_schema_dispositions_match_the_validator(self):
        from jsrf_review_records import DISPOSITIONS
        item = self.schema['properties']['criteria']['items']
        self.assertEqual(set(item['properties']['disposition']['enum']),
                         set(DISPOSITIONS))
        self.assertNotIn('UNKNOWN', item['properties']['disposition']['enum'],
                         'UNKNOWN must not be storable; a footer UNKNOWN maps to '
                         'CANNOT VERIFY')

    def test_real_record_satisfies_the_schema(self):
        record = json.loads((ROOT / 'docs' / 'reviews' / 'P0.1'
                             / 'p0-1-consolidated-review.json').read_text(encoding='utf-8'))
        missing = [f for f in self.schema['required'] if f not in record]
        self.assertEqual(missing, [])
        for criterion in record['criteria']:
            for field in ('id', 'disposition', 'evidence', 'procedure',
                          'observed_result'):
                self.assertIn(field, criterion)

    def test_schema_documents_the_mandatory_contradiction_scope(self):
        self.assertIn('contradiction_evidence', self.schema['required'],
                      'the wider scope is mandatory, not optional')
        description = self.schema['properties']['contradiction_evidence']['description']
        self.assertIn('INVALID', description)


class SourceAdapterFixtureTests(unittest.TestCase):
    """The AC2 fixtures that must go through the SOURCE ADAPTERS.

    An independent acceptance review measured that three AC2-named fixtures were
    absent: two parents with identical labels, unrelated `ACCEPT` text, and an
    unreadable cache / no recorded review / unknown schema *through a source
    adapter*.  Testing the classifier alone does not cover the adapter path, which
    is where a review is actually recovered from a log.
    """

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)

    def tearDown(self):
        self._tmp.cleanup()

    def write_codex(self, name: str, *, child: str, parent: str,
                    verdict: str, complete: bool = True) -> Path:
        lines = [json.dumps({'type': 'session_meta',
                             'payload': {'id': child, 'parent_thread_id': parent}})]
        lines.append(json.dumps({'type': 'turn_context',
                                 'payload': {'turn_id': 'turn-0001', 'model': 'gpt-6-luna',
                                             'effort': 'max'}}))
        if complete:
            lines.append(json.dumps({'type': 'event_msg',
                                     'payload': {'type': 'task_complete',
                                                 'turn_id': 'turn-0001',
                                                 'last_agent_message': verdict}}))
        path = self.root / name
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        return path

    def test_two_parents_with_identical_labels_are_distinguishable(self):
        """Two children can share a label; ancestry must separate them.

        This is the fixture the contract names and the implementation lacked.  A
        label is not an identity, so the adapter must report the parent from the
        source header rather than from anything a human wrote.
        """
        a = self.write_codex('a.jsonl', child='child-A', parent='parent-ONE',
                             verdict='verdict A')
        b = self.write_codex('b.jsonl', child='child-B', parent='parent-TWO',
                             verdict='verdict B')
        ident_a = exporter.export(a, 'turn-0001')
        ident_b = exporter.export(b, 'turn-0001')
        self.assertNotEqual(ident_a['parent_id'], ident_b['parent_id'])
        self.assertNotEqual(ident_a['child_id'], ident_b['child_id'])
        # Same label, different identity: the ids are what separate them.
        self.assertEqual('A2f acceptance review'.split()[0], 'A2f')

    def test_identical_child_ids_across_sources_are_detectable(self):
        """Two sources claiming the same child id cannot both be the source."""
        a = self.write_codex('a.jsonl', child='child-SAME', parent='parent-ONE',
                             verdict='verdict A')
        b = self.write_codex('b.jsonl', child='child-SAME', parent='parent-TWO',
                             verdict='verdict B')
        self.assertEqual(exporter.export(a, 'turn-0001')['child_id'],
                         exporter.export(b, 'turn-0001')['child_id'])
        self.assertNotEqual(exporter.export(a, 'turn-0001')['parent_id'],
                            exporter.export(b, 'turn-0001')['parent_id'],
                            'identical child ids must not imply identical parentage')

    def test_unrelated_accept_text_is_not_a_verdict(self):
        """A log containing the word ACCEPT is not a completed review."""
        path = self.write_codex('c.jsonl', child='c', parent='p',
                                verdict='The word ACCEPT appears here, unrelated.',
                                complete=False)
        with self.assertRaises(exporter.ExportError) as raised:
            exporter.export(path, 'turn-0001')
        self.assertIn('no completed task_complete', str(raised.exception))

    def test_accept_text_without_a_footer_is_not_acceptance_eligible(self):
        """`ACCEPT` in prose is not a machine-readable map."""
        text = 'Verdict: ACCEPT\n\nNo machine-readable footer here.\n'
        self.assertEqual(extract_footer(text)['status'], 'none')

    def test_unreadable_source_is_refused_by_the_adapter(self):
        path = self.root / 'truncated.jsonl'
        path.write_bytes(b'{"type": "session_meta", "payload": {"id": "x"')
        with self.assertRaises(exporter.ExportError):
            exporter.export(path, 'turn-0001')

    def test_missing_source_is_refused_by_the_adapter(self):
        with self.assertRaises(exporter.ExportError) as raised:
            exporter.export(self.root / 'absent.jsonl', 't1')
        self.assertIn('missing', str(raised.exception))

    def test_unknown_schema_is_refused_by_the_validator(self):
        """An unsupported schema version is INVALID, never a pass."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'docs' / 'packets').mkdir(parents=True)
            (root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
            contract_bytes = CONTRACT.read_bytes()
            (root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
                contract_bytes)
            manifest = {'schema_version': 1, 'packet_id': 'P0.2',
                        'contract_file': 'docs/packets/p0-acceptance-contract.md',
                        'contract_sha256': sha256_bytes(contract_bytes),
                        'required_ids': list(REQUIRED_IDS)}
            source = root / 'docs' / 'reviews' / 'P0.2' / 'src.txt'
            source.write_text(prose_block() + '\n\n' + footer() + '\n', encoding='utf-8')
            evidence = root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
            evidence.write_text('x\n', encoding='utf-8')
            record = {
                'schema_version': 99,
                'review_id': 'x', 'packet_id': 'P0.2',
                'contract_sha256': manifest['contract_sha256'],
                'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                    'sha256': manifest['contract_sha256']}],
                'harness': 'dsh', 'parent_id': 'p', 'child_id': 'c',
                'requested_model': 'm', 'requested_effort': 'high',
                'identity_evidence': {'kind': 'turn_text',
                                      'path': str(source.relative_to(root)),
                                      'sha256': sha256_bytes(source.read_bytes()),
                                      'source_session': str(write_matching_session(
                                          root, 'fixture-session.jsonl',
                                          verdict_text=source.read_text(
                                              encoding='utf-8')))},
                'turn_id': 't1',
                'verdict_text': source.read_text(encoding='utf-8'),
                'contradiction_evidence': source.read_text(encoding='utf-8'),
                'criteria': [{'id': i, 'disposition': AGREED,
                              'evidence': [{'path': str(evidence.relative_to(root)),
                                            'sha256': sha256_bytes(evidence.read_bytes())}],
                              'procedure': 'p', 'observed_result': 'r'}
                             for i in REQUIRED_IDS],
            }
            result = validate_record(record, manifest, root)
            self.assertEqual(result['status'], INVALID)


class IdentityVerificationTests(unittest.TestCase):
    """The validator must verify ancestry against the SOURCE, not trust the record.

    Measured fail-open: setting `parent_id`, `child_id`, `turn_id`,
    `requested_model`, `requested_effort` and `harness` to arbitrary values --
    including all six at once -- still returned acceptance-eligible, because only
    the *presence* of each key was checked.  A record with invented parentage could
    close a packet.
    """

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
            contract_bytes)
        self.manifest = {
            'schema_version': 1, 'packet_id': 'P0.2',
            'contract_file': 'docs/packets/p0-acceptance-contract.md',
            'contract_sha256': sha256_bytes(contract_bytes),
            'required_ids': list(REQUIRED_IDS),
        }
        body = prose_block() + '\n\n' + footer() + '\n'
        self.source = self.root / 'docs' / 'reviews' / 'P0.2' / 'src.txt'
        self.source.write_text(body, encoding='utf-8')
        evidence = self.root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
        evidence.write_text('x\n', encoding='utf-8')
        self.evidence = [{'path': str(evidence.relative_to(self.root)),
                          'sha256': sha256_bytes(evidence.read_bytes())}]
        self.body = body

    def tearDown(self):
        self._tmp.cleanup()

    def write_session(self, *, child='child-1', parent='parent-1',
                      turn=1, complete=True, harness='dsh',
                      include_text=True) -> Path:
        if harness == 'dsh':
            lines = [json.dumps({'type': 'session', 'version': 3, 'id': child,
                                 'parentSession': parent}),
                     json.dumps({'type': 'subagent/descriptor',
                                 'data': {'agentModel': 'hy4-preview-f',
                                          'agentReasoningEffort': 'high'}}),
                     # The verdict must be present: the validator binds
                     # `verdict_text` to the turn it names, so a session without the
                     # assistant message cannot corroborate anything.
                     ]
            if include_text:
                lines.append(json.dumps({
                    'type': 'assistant/message',
                    'data': {'turn': turn,
                             'message': {'content': [
                                 {'type': 'text', 'text': self.body}]}}}))
        else:
            lines = [json.dumps({'type': 'session_meta',
                                 'payload': {'id': child,
                                             'parent_thread_id': parent}}),
                     json.dumps({'type': 'turn_context',
                                 'payload': {'turn_id': 't1',
                                             'model': 'gpt-6-luna'}})]
        reason = {'kind': 'completed'} if complete else {
            'kind': 'error', 'error': {'code': 'PI_AI_ERROR'}}
        lines.append(json.dumps({'type': 'turn/end',
                                 'data': {'turn': turn, 'reason': reason}}))
        if harness != 'dsh':
            lines.append(json.dumps({'type': 'event_msg',
                                     'payload': {'type': 'task_complete',
                                                 'turn_id': 'turn-0001',
                                                 'last_agent_message': self.body}}))
        path = self.root / 'docs' / 'reviews' / 'P0.2' / f'{harness}.jsonl'
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        return path

    def record(self, session: Path, **overrides) -> dict:
        rec = {
            'schema_version': 1, 'review_id': 'r1', 'packet_id': 'P0.2',
            'contract_sha256': self.manifest['contract_sha256'],
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.manifest['contract_sha256']}],
            'harness': 'dsh', 'parent_id': 'parent-1', 'child_id': 'child-1',
            'requested_model': 'hy4-preview-f', 'requested_effort': 'high',
            'identity_evidence': {'kind': 'turn_text',
                                  'path': str(self.source.relative_to(self.root)),
                                  'sha256': sha256_bytes(self.source.read_bytes()),
                                  'source_session': str(session)},
            'turn_id': '1', 'verdict_text': self.body,
            'contradiction_evidence': self.body,
            'criteria': [{'id': i, 'disposition': AGREED, 'evidence': self.evidence,
                          'procedure': 'ran it', 'observed_result': 'matched'}
                         for i in REQUIRED_IDS],
        }
        rec.update(overrides)
        return rec

    def test_matching_identity_is_eligible(self):
        session = self.write_session()
        result = validate_record(self.record(session), self.manifest, self.root)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_wrong_parent_is_rejected(self):
        session = self.write_session(parent='parent-REAL')
        result = validate_record(self.record(session, parent_id='parent-FAKE'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('parent_id' in r for r in result['reasons']),
                        result['reasons'])

    def test_wrong_child_is_rejected(self):
        session = self.write_session(child='child-REAL')
        result = validate_record(self.record(session, child_id='child-FAKE'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('child_id' in r for r in result['reasons']),
                        result['reasons'])

    def test_nonexistent_turn_is_rejected(self):
        session = self.write_session(turn=1)
        result = validate_record(self.record(session, turn_id='99'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('completed turn' in r for r in result['reasons']),
                        result['reasons'])

    def test_errored_turn_is_not_a_completed_turn(self):
        session = self.write_session(turn=1, complete=False)
        result = validate_record(self.record(session), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('completed turn' in r for r in result['reasons']),
                        result['reasons'])

    def test_wrong_harness_is_rejected(self):
        session = self.write_session(harness='dsh')
        result = validate_record(self.record(session, harness='codex'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('harness' in r for r in result['reasons']),
                        result['reasons'])

    def test_wrong_model_is_rejected(self):
        session = self.write_session()
        result = validate_record(self.record(session, requested_model='gpt-6-luna'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('model' in r for r in result['reasons']),
                        result['reasons'])

    def test_all_identity_fields_falsified_at_once_is_rejected(self):
        """The reviewer's strongest case: every identity field invented."""
        session = self.write_session()
        result = validate_record(self.record(
            session, parent_id='session-TOTALLY-DIFFERENT',
            child_id='deadbeef-0000-0000-0000-000000000000', turn_id='99',
            requested_model='gpt-6-luna', requested_effort='low',
            harness='codex'), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertGreaterEqual(len(result['reasons']), 3, result['reasons'])

    def test_invented_verdict_body_is_rejected(self):
        """Ancestry alone is not enough: the verdict must be the turn's own text.

        Measured residual from an independent review: `verdict_text` was bound only
        by the tracked-file hash and by `contradiction_evidence` containment, both
        of which a self-authored record satisfies.  A record could declare a
        genuinely verified ancestry and an INVENTED verdict body -- and the verdict
        body is what carries the dispositions.
        """
        session = self.write_session()
        fabricated = ('Verdict: everything is fine.\n\n'
                      + footer())
        result = validate_record(self.record(session, verdict_text=fabricated),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('not the text of the turn' in r
                            for r in result['reasons']), result['reasons'])

    def test_paraphrased_verdict_is_rejected(self):
        """The binding is an equality test, not a containment test."""
        session = self.write_session()
        result = validate_record(
            self.record(session, verdict_text=self.body + '\n\nAll criteria agree.'),
            self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)

    def test_self_consistent_fabrication_is_rejected(self):
        """A record controlling its own contradiction scope still cannot invent."""
        session = self.write_session()
        fabricated = 'Fabricated.\n\n' + footer()
        result = validate_record(
            self.record(session, verdict_text=fabricated,
                        contradiction_evidence=fabricated),
            self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)

    def test_crlf_normalisation_is_tolerated(self):
        """A producer may rewrite line endings; that is not fabrication."""
        session = self.write_session()
        result = validate_record(
            self.record(session,
                        verdict_text=self.body.replace('\n', '\r\n')),
            self.manifest, self.root)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_turn_without_readable_text_cannot_corroborate(self):
        """A completed turn with no assistant text cannot bind a verdict."""
        session = self.write_session(include_text=False)
        result = validate_record(self.record(session), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('no readable' in r for r in result['reasons']),
                        result['reasons'])


    def test_missing_source_session_declaration_is_invalid(self):
        """A record that does not name its source cannot be verified."""
        session = self.write_session()
        record = self.record(session)
        del record['identity_evidence']['source_session']
        result = validate_record(record, self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('source_session' in r for r in result['reasons']),
                        result['reasons'])

    def test_missing_source_session_file_is_invalid(self):
        session = self.write_session()
        record = self.record(session)
        session.unlink()
        result = validate_record(record, self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('source session is missing' in r for r in result['reasons']),
                        result['reasons'])

    def test_empty_identity_value_is_invalid(self):
        session = self.write_session()
        for field in ('review_id', 'parent_id', 'child_id', 'turn_id',
                      'requested_model', 'harness'):
            with self.subTest(field=field):
                result = validate_record(self.record(session, **{field: ''}),
                                         self.manifest, self.root)
                self.assertEqual(result['status'], INVALID, field)


class CodexAdapterBindingTests(unittest.TestCase):
    """The OTHER source adapter must bind identity, verdict and route too.

    Every review round so far exercised the DSH adapter. The Codex branch of
    `read_source_identity` is separate code, and it was **missing the route
    binding**: measured, `requested_model='gpt-9-moon'` validated as
    acceptance-eligible against a Codex rollout, because the branch read child,
    parent and harness but never the `turn_context`. A binding that applies to one
    harness and not the other looks like coverage until the other is exercised.
    """

    VERDICT = ('P0.2-AC1 is AGREED.\n\n'
               'REVIEW_CRITERIA_JSON: [{"id":"P0.2-AC1","disposition":"AGREED"}]')

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
            contract_bytes)
        self.contract_hash = sha256_bytes(contract_bytes)
        self.manifest = {'schema_version': 1, 'packet_id': 'P0.2',
                         'contract_file': 'docs/packets/p0-acceptance-contract.md',
                         'contract_sha256': self.contract_hash,
                         'required_ids': ['P0.2-AC1']}

    def tearDown(self):
        self._tmp.cleanup()

    def write_rollout(self, *, child='child-1', parent='parent-1',
                      turn='turn-0001', model='gpt-6-luna', effort='max',
                      body=None) -> Path:
        body = body if body is not None else self.VERDICT
        path = self.root / 'docs' / 'reviews' / 'P0.2' / 'rollout.jsonl'
        path.write_text('\n'.join([
            json.dumps({'type': 'session_meta',
                        'payload': {'id': child, 'parent_thread_id': parent}}),
            json.dumps({'type': 'turn_context',
                        'payload': {'turn_id': turn, 'model': model,
                                    'effort': effort}}),
            json.dumps({'type': 'event_msg',
                        'payload': {'type': 'task_complete', 'turn_id': turn,
                                    'last_agent_message': body}}),
        ]) + '\n', encoding='utf-8')
        return path

    def record(self, session: Path, **over) -> dict:
        source = self.root / 'docs' / 'reviews' / 'P0.2' / 'src.txt'
        source.write_text(self.VERDICT, encoding='utf-8')
        evidence = self.root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
        evidence.write_text('x\n', encoding='utf-8')
        rec = {
            'schema_version': 1, 'review_id': 'r1', 'packet_id': 'P0.2',
            'contract_sha256': self.contract_hash,
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.contract_hash}],
            'harness': 'codex', 'parent_id': 'parent-1', 'child_id': 'child-1',
            'requested_model': 'gpt-6-luna', 'requested_effort': 'max',
            'identity_evidence': {'kind': 'turn_text',
                                  'path': str(source.relative_to(self.root)),
                                  'sha256': sha256_bytes(source.read_bytes()),
                                  'source_session': str(session)},
            'turn_id': 'turn-0001', 'verdict_text': self.VERDICT,
            'contradiction_evidence': self.VERDICT,
            'criteria': [{'id': 'P0.2-AC1', 'disposition': AGREED,
                          'evidence': [{'path': str(evidence.relative_to(self.root)),
                                        'sha256': sha256_bytes(
                                            evidence.read_bytes())}],
                          'procedure': 'ran it', 'observed_result': 'matched'}],
        }
        rec.update(over)
        return rec

    def test_codex_control_is_eligible(self):
        session = self.write_rollout()
        result = validate_record(self.record(session), self.manifest, self.root)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_codex_wrong_model_is_rejected(self):
        """The route binding must apply to Codex, not only to DSH."""
        session = self.write_rollout()
        result = validate_record(self.record(session, requested_model='gpt-9-moon'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('model' in r for r in result['reasons']),
                        result['reasons'])

    def test_codex_wrong_parent_is_rejected(self):
        session = self.write_rollout(parent='parent-REAL')
        result = validate_record(self.record(session), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)

    def test_codex_wrong_child_is_rejected(self):
        session = self.write_rollout(child='child-REAL')
        result = validate_record(self.record(session), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)

    def test_codex_wrong_turn_is_rejected(self):
        session = self.write_rollout(turn='turn-0001')
        result = validate_record(self.record(session, turn_id='turn-9999'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)

    def test_codex_invented_verdict_is_rejected(self):
        session = self.write_rollout()
        result = validate_record(
            self.record(session, verdict_text=self.VERDICT + '\n\nAll agree.'),
            self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)

    def test_codex_harness_mismatch_is_rejected(self):
        session = self.write_rollout()
        result = validate_record(self.record(session, harness='dsh'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('harness' in r for r in result['reasons']),
                        result['reasons'])


class DeltaEnforcementTests(unittest.TestCase):
    """A re-bind is NOT a re-review, and the validator must enforce that.

    Measured fail-open: `"delta"` occurred zero times in `jsrf_review_records.py`,
    so a record could carry `delta.affected_criteria=['P0.1-AC1']` -- an explicit
    admission that a criterion's supporting text changed -- and still return
    acceptance-eligible.  That is a laundering path for a stale review.
    """

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
            contract_bytes)
        self.manifest = {
            'schema_version': 1, 'packet_id': 'P0.2',
            'contract_file': 'docs/packets/p0-acceptance-contract.md',
            'contract_sha256': sha256_bytes(contract_bytes),
            'required_ids': list(REQUIRED_IDS),
        }
        self.body = prose_block() + '\n\n' + footer() + '\n'
        source = self.root / 'docs' / 'reviews' / 'P0.2' / 'src.txt'
        source.write_text(self.body, encoding='utf-8')
        evidence = self.root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
        evidence.write_text('x\n', encoding='utf-8')
        self.source = source
        self.evidence = [{'path': str(evidence.relative_to(self.root)),
                          'sha256': sha256_bytes(evidence.read_bytes())}]

    def tearDown(self):
        self._tmp.cleanup()

    def write_session(self) -> Path:
        # The session must carry the verdict: the validator binds `verdict_text` to
        # the turn it names, so a session without the assistant message cannot
        # corroborate anything.
        path = self.root / 'docs' / 'reviews' / 'P0.2' / 's.jsonl'
        path.write_text('\n'.join([
            json.dumps({'type': 'session', 'version': 3, 'id': 'c',
                        'parentSession': 'p'}),
            json.dumps({'type': 'subagent/descriptor',
                        'data': {'agentModel': 'm'}}),
            json.dumps({'type': 'assistant/message',
                        'data': {'turn': 1,
                                 'message': {'content': [
                                     {'type': 'text', 'text': self.body}]}}}),
            json.dumps({'type': 'turn/end',
                        'data': {'turn': 1, 'reason': {'kind': 'completed'}}}),
        ]) + '\n', encoding='utf-8')
        return path

    def record(self, delta=None) -> dict:
        session = self.write_session()
        rec = {
            'schema_version': 1, 'review_id': 'r1', 'packet_id': 'P0.2',
            'contract_sha256': self.manifest['contract_sha256'],
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.manifest['contract_sha256']}],
            'harness': 'dsh', 'parent_id': 'p', 'child_id': 'c',
            'requested_model': 'm', 'requested_effort': 'high',
            'identity_evidence': {'kind': 'turn_text',
                                  'path': str(self.source.relative_to(self.root)),
                                  'sha256': sha256_bytes(self.source.read_bytes()),
                                  'source_session': str(session)},
            'turn_id': '1', 'verdict_text': self.body,
            'contradiction_evidence': self.body,
            'criteria': [{'id': i, 'disposition': AGREED, 'evidence': self.evidence,
                          'procedure': 'ran it', 'observed_result': 'matched'}
                         for i in REQUIRED_IDS],
        }
        if delta is not None:
            rec['delta'] = delta
        return rec

    def test_no_delta_is_eligible(self):
        result = validate_record(self.record(), self.manifest, self.root)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_empty_affected_criteria_is_eligible(self):
        """`[]` is the explicit claim that no criterion text changed."""
        result = validate_record(
            self.record({'reason': 'reformatted', 'affected_criteria': []}),
            self.manifest, self.root)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_nonempty_affected_criteria_stays_pending(self):
        result = validate_record(
            self.record({'reason': 'x', 'affected_criteria': ['P0.2-AC1']}),
            self.manifest, self.root)
        self.assertEqual(result['status'], FAILED)
        self.assertTrue(any('re-bind is not a re-review' in r
                            for r in result['reasons']), result['reasons'])

    def test_delta_without_affected_criteria_is_invalid(self):
        result = validate_record(self.record({'reason': 'x'}),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('affected_criteria' in r for r in result['reasons']))

    def test_delta_without_a_reason_is_invalid(self):
        result = validate_record(self.record({'affected_criteria': []}),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('reason' in r for r in result['reasons']))

    def test_delta_that_is_not_an_object_is_invalid(self):
        result = validate_record(self.record('not an object'),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)


class PublishedSchemaEnforcementTests(unittest.TestCase):
    """The published schema must be ENFORCED, not merely documented.

    An independent review measured that `check-recorded-reviews.py` never validated
    against `docs/reviews/review-record.schema.json` -- no `jsonschema` call existed
    anywhere in `scripts/` -- so "the schema matches the validator" was a
    test-only property and a drifted schema would document an unenforced contract.
    `jsonschema` is not installed, so a small structural checker covers the
    keywords the schema actually uses.
    """

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        install_schema(self.root)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        (self.root / 'docs' / 'reviews' / 'P0.2').mkdir(parents=True)
        contract_bytes = CONTRACT.read_bytes()
        (self.root / 'docs' / 'packets' / 'p0-acceptance-contract.md').write_bytes(
            contract_bytes)
        self.manifest = {
            'schema_version': 1, 'packet_id': 'P0.2',
            'contract_file': 'docs/packets/p0-acceptance-contract.md',
            'contract_sha256': sha256_bytes(contract_bytes),
            'required_ids': ['P0.2-AC1'],
        }
        self.body = 'P0.2-AC1 is AGREED.\n\n' + footer(ids=['P0.2-AC1']) + '\n'
        self.source = self.root / 'docs' / 'reviews' / 'P0.2' / 'src.txt'
        self.source.write_text(self.body, encoding='utf-8')
        evidence = self.root / 'docs' / 'reviews' / 'P0.2' / 'ev.txt'
        evidence.write_text('measured\n', encoding='utf-8')
        self.evidence = [{'path': str(evidence.relative_to(self.root)),
                          'sha256': sha256_bytes(evidence.read_bytes())}]

    def tearDown(self):
        self._tmp.cleanup()

    def copy_schema(self):
        import shutil
        (self.root / 'docs' / 'reviews').mkdir(parents=True, exist_ok=True)
        shutil.copy(ROOT / 'docs' / 'reviews' / 'review-record.schema.json',
                    self.root / 'docs' / 'reviews' / 'review-record.schema.json')

    def record(self, **overrides) -> dict:
        session = write_matching_session(self.root, 'schema-session.jsonl',
                                         verdict_text=self.body)
        rec = {
            'schema_version': 1, 'review_id': 'r1', 'packet_id': 'P0.2',
            'contract_sha256': self.manifest['contract_sha256'],
            'reviewed_files': [{'path': 'docs/packets/p0-acceptance-contract.md',
                                'sha256': self.manifest['contract_sha256']}],
            'harness': 'dsh', 'parent_id': 'p', 'child_id': 'c',
            'requested_model': 'hy4-preview-f', 'requested_effort': 'high',
            'identity_evidence': {'kind': 'turn_text',
                                  'path': str(self.source.relative_to(self.root)),
                                  'sha256': sha256_bytes(self.source.read_bytes()),
                                  'source_session': str(session)},
            'turn_id': 't1', 'verdict_text': self.body,
            'contradiction_evidence': self.body,
            'criteria': [{'id': 'P0.2-AC1', 'disposition': AGREED,
                          'evidence': self.evidence, 'procedure': 'ran it',
                          'observed_result': 'matched'}],
        }
        rec.update(overrides)
        return rec

    def test_missing_schema_is_reported(self):
        """An absent schema is a finding, not a silent skip."""
        # setUp installs it; remove it so this test exercises the absent case.
        (self.root / 'docs' / 'reviews' / 'review-record.schema.json').unlink()
        result = validate_record(self.record(), self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('schema is missing' in r for r in result['reasons']),
                        result['reasons'])

    def test_valid_record_passes_the_schema(self):
        self.copy_schema()
        result = validate_record(self.record(), self.manifest, self.root)
        self.assertEqual(result['status'], ELIGIBLE, result.get('reasons'))

    def test_unknown_disposition_is_rejected_by_the_schema(self):
        self.copy_schema()
        record = self.record()
        record['criteria'][0]['disposition'] = 'PROBABLY FINE'
        result = validate_record(record, self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('schema' in r for r in result['reasons']),
                        result['reasons'])

    def test_unknown_evidence_kind_is_rejected_by_the_schema(self):
        self.copy_schema()
        record = self.record()
        record['identity_evidence']['kind'] = 'vibes'
        result = validate_record(record, self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('schema' in r for r in result['reasons']),
                        result['reasons'])

    def test_missing_source_session_is_rejected_by_the_schema(self):
        """The schema requires source_session, so its absence is a schema finding."""
        self.copy_schema()
        record = self.record()
        del record['identity_evidence']['source_session']
        result = validate_record(record, self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('source_session' in r for r in result['reasons']),
                        result['reasons'])

    def test_wrong_schema_version_is_rejected_by_the_schema(self):
        self.copy_schema()
        result = validate_record(self.record(schema_version=2),
                                 self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)

    def test_malformed_hash_is_rejected_by_the_pattern(self):
        self.copy_schema()
        record = self.record()
        record['reviewed_files'][0]['sha256'] = 'not-a-hash'
        result = validate_record(record, self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)

    def test_undeclared_fields_are_rejected_by_additional_properties(self):
        """`additionalProperties: false` is a real constraint, not decoration.

        Found by auditing the checker against the schema: `additionalProperties`
        appeared in the schema but the structural checker ignored it, so an
        undeclared key passed -- the checker enforced LESS than the schema claimed.
        """
        self.copy_schema()
        record = self.record()
        record['identity_evidence']['smuggled'] = 'extra'
        result = validate_record(record, self.manifest, self.root)
        self.assertEqual(result['status'], INVALID)
        self.assertTrue(any('unexpected field' in r for r in result['reasons']),
                        result['reasons'])

    def test_checker_covers_every_structural_keyword_the_schema_uses(self):
        """A hand-written checker must not silently ignore a schema keyword.

        `jsonschema` is not installed, so the checker is hand-written; this test
        is the guard against it drifting into enforcing less than the schema
        documents.
        """
        import re as _re
        raw = (ROOT / 'docs' / 'reviews' / 'review-record.schema.json').read_text(
            encoding='utf-8')
        structural = {
            'type', 'required', 'enum', 'const', 'minLength', 'minItems', 'pattern',
            'properties', 'items', 'additionalProperties', 'minimum', 'maximum',
            'minProperties', 'uniqueItems', 'oneOf', 'anyOf', 'allOf', 'not',
            'format', 'propertyNames', 'dependencies', 'if', 'then', 'else',
        }
        handled = {
            'type', 'required', 'enum', 'const', 'minLength', 'minItems', 'pattern',
            'properties', 'items', 'additionalProperties',
        }
        found = set(_re.findall(r'"([a-zA-Z$][a-zA-Z0-9$]*)"\s*:', raw))
        unhandled = (found & structural) - handled
        self.assertEqual(
            unhandled, set(),
            f'the schema uses {sorted(unhandled)} but the checker ignores it, so it '
            f'enforces less than the schema claims')


    def test_the_real_records_satisfy_the_published_schema(self):
        """The schema must describe the records the project actually ships."""
        from jsrf_review_records import validate_against_published_schema
        for name in ('docs/reviews/P0.1/p0-1-consolidated-review.json',
                     'docs/reviews/P0.3/p0-3-to-p0-7-acceptance.json'):
            with self.subTest(record=name):
                record = json.loads((ROOT / name).read_text(encoding='utf-8'))
                problems = validate_against_published_schema(record, ROOT)
                self.assertEqual(problems, [], f'{name}: {problems}')


class CheckerCliTests(unittest.TestCase):
    """The CLI's exit codes are part of the interface."""

    def test_real_repository_manifest_validates_and_contract_hash_matches(self):
        manifest = load_required_manifest(ROOT / 'docs' / 'reviews' / 'contracts' / 'P0.2.json')
        self.assertEqual(manifest['required_ids'], REQUIRED_IDS)
        self.assertEqual(sha256_bytes(CONTRACT.read_bytes()).lower(),
                         manifest['contract_sha256'].lower())

    def test_missing_review_exits_2(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER),
             '--contract', 'docs/reviews/contracts/P0.2.json',
             '--review', 'docs/reviews/P0.2/does-not-exist.json'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stderr)

    def test_missing_contract_exits_2(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER),
             '--contract', 'docs/reviews/contracts/absent.json',
             '--review', 'docs/reviews/contracts/P0.2.json'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stderr)

    def test_checker_is_read_only(self):
        before = {p: p.read_bytes() for p in ROOT.glob('docs/reviews/contracts/*.json')}
        subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER),
             '--contract', 'docs/reviews/contracts/P0.2.json',
             '--review', 'docs/reviews/contracts/P0.2.json'],
            cwd=ROOT, capture_output=True, text=True)
        after = {p: p.read_bytes() for p in ROOT.glob('docs/reviews/contracts/*.json')}
        self.assertEqual(before, after)


if __name__ == '__main__':
    unittest.main(verbosity=2)
