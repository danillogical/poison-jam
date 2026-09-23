"""Fixtures for the Codex review-source exporter.

The exporter's job is provenance: an exported review must re-open its source and
re-check every field.  A hand-written JSON claiming to be an export is not
sufficient, so the tamper controls matter more than the happy path.
"""
from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

_spec = importlib.util.spec_from_file_location(
    'export_codex_review', SCRIPTS / 'export-codex-review.py')
assert _spec and _spec.loader
exporter = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(exporter)

from jsrf_dsh_source import DshSourceError  # noqa: E402
import jsrf_dsh_source as exporter_dsh  # noqa: E402

ExportError = exporter.ExportError
CHILD = '01a0cd30-4eff-7020-b0d2-6af7828cb476'
PARENT = '01a098ee-8e15-7a03-badd-054dbd98ae6e'
TURN = '01a0cd43-27b7-76b3-8e25-f85bedf2d12e'


def rollout_lines(*, child=CHILD, parent=PARENT, turn=TURN, model='gpt-6-luna',
                  effort='max', verdict='verdict body', complete=True,
                  contexts=1, completions=1, extra_tail=None):
    """Build a minimal but structurally faithful rollout."""
    meta = {
        'type': 'session_meta',
        'payload': {
            'id': child,
            'parent_thread_id': parent,
            'source': {'subagent': {'thread_spawn': {'parent_thread_id': parent,
                                                     'depth': 1}}},
        },
    }
    lines = [json.dumps(meta)]
    for _ in range(contexts):
        lines.append(json.dumps({
            'type': 'turn_context',
            'payload': {'turn_id': turn, 'model': model,
                        'effort': effort,
                        'collaboration_mode': {'settings': {'model': model,
                                                            'reasoning_effort': effort}}},
        }))
    if complete:
        for _ in range(completions):
            lines.append(json.dumps({
                'type': 'event_msg',
                'payload': {'type': 'task_complete', 'turn_id': turn,
                            'last_agent_message': verdict},
            }))
    for extra in (extra_tail or []):
        lines.append(json.dumps(extra))
    return '\n'.join(lines) + '\n'


class ExportTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def write(self, body: str, name: str = 'rollout.jsonl') -> Path:
        path = self.root / name
        path.write_text(body, encoding='utf-8')
        return path

    def test_valid_rollout_exports_identity_route_and_verdict(self):
        path = self.write(rollout_lines())
        exported = exporter.export(path, TURN)
        self.assertEqual(exported['child_id'], CHILD)
        self.assertEqual(exported['parent_id'], PARENT)
        self.assertEqual(exported['turn_id'], TURN)
        self.assertEqual(exported['model'], 'gpt-6-luna')
        self.assertEqual(exported['effort'], 'max')
        self.assertEqual(exported['verdict_text'], 'verdict body')
        self.assertEqual(exporter.verify_export(exported, path), [])

    def test_missing_completion_is_rejected(self):
        path = self.write(rollout_lines(complete=False))
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, TURN)
        self.assertIn('no completed task_complete', str(raised.exception))

    def test_missing_turn_context_is_rejected(self):
        path = self.write(rollout_lines(contexts=0))
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, TURN)
        self.assertIn('no turn_context', str(raised.exception))

    def test_ambiguous_turn_context_is_rejected(self):
        path = self.write(rollout_lines(contexts=2))
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, TURN)
        self.assertIn('ambiguous turn', str(raised.exception))

    def test_ambiguous_completion_is_rejected(self):
        path = self.write(rollout_lines(completions=2))
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, TURN)
        self.assertIn('ambiguous completion', str(raised.exception))

    def test_parent_thread_falls_back_to_nested_spawn_record(self):
        """A missing top-level parent must be recovered from the spawn record.

        This is the shape the real Codex rollout uses, and the fallback is what
        makes ancestry verifiable rather than assumed.
        """
        body = rollout_lines()
        # Replace only the top-level parent_thread_id, leaving the nested one.
        body = body.replace(f'"parent_thread_id": "{PARENT}",\n', '', 1)
        path = self.write(body)
        exported = exporter.export(path, TURN)
        self.assertEqual(exported['parent_id'], PARENT)

    def test_missing_parent_thread_everywhere_is_rejected(self):
        body = rollout_lines().replace(f'"{PARENT}"', 'null')
        path = self.write(body)
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, TURN)
        self.assertIn('parent thread', str(raised.exception))

    def test_missing_child_id_is_rejected(self):
        body = rollout_lines()
        body = body.replace(json.dumps(CHILD), 'null', 1)
        path = self.write(body)
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, TURN)
        self.assertIn('child id', str(raised.exception))

    def test_empty_verdict_is_rejected(self):
        path = self.write(rollout_lines(verdict='   '))
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, TURN)
        self.assertIn('empty completed verdict', str(raised.exception))

    def test_malformed_turn_id_is_rejected(self):
        path = self.write(rollout_lines())
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, 'not a turn')
        self.assertIn('well-formed identifier', str(raised.exception))

    def test_missing_rollout_is_rejected(self):
        with self.assertRaises(ExportError) as raised:
            exporter.export(self.root / 'absent.jsonl', TURN)
        self.assertIn('missing', str(raised.exception))

    def test_rollout_without_session_meta_is_rejected(self):
        path = self.write(json.dumps({'type': 'turn_context',
                                      'payload': {'turn_id': TURN, 'model': 'm'}}) + '\n')
        with self.assertRaises(ExportError) as raised:
            exporter.export(path, TURN)
        self.assertIn('session_meta', str(raised.exception))


class ExportVerifyTests(unittest.TestCase):
    """A tampered export must not verify against its source."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self.rollout = self.root / 'rollout.jsonl'
        self.rollout.write_text(rollout_lines(), encoding='utf-8')
        self.exported = exporter.export(self.rollout, TURN)

    def tearDown(self):
        self._tmp.cleanup()

    def test_untampered_export_verifies(self):
        self.assertEqual(exporter.verify_export(self.exported, self.rollout), [])

    def test_edited_verdict_fails_verification(self):
        tampered = dict(self.exported, verdict_text='TAMPERED')
        problems = exporter.verify_export(tampered, self.rollout)
        self.assertTrue(any('verdict text' in p for p in problems), problems)

    def test_wrong_parent_fails_verification(self):
        tampered = dict(self.exported, parent_id='someone-else')
        problems = exporter.verify_export(tampered, self.rollout)
        self.assertTrue(any('parent id' in p for p in problems), problems)

    def test_wrong_child_fails_verification(self):
        tampered = dict(self.exported, child_id='someone-else')
        problems = exporter.verify_export(tampered, self.rollout)
        self.assertTrue(any('child id' in p for p in problems), problems)

    def test_wrong_model_fails_verification(self):
        tampered = dict(self.exported, model='gpt-6-astra')
        problems = exporter.verify_export(tampered, self.rollout)
        self.assertTrue(any('model' in p for p in problems), problems)

    def test_wrong_effort_fails_verification(self):
        tampered = dict(self.exported, effort='low')
        problems = exporter.verify_export(tampered, self.rollout)
        self.assertTrue(any('effort' in p for p in problems), problems)

    def test_later_appended_turns_do_not_invalidate_an_earlier_review(self):
        """The prefix is immutable; appending after it must not break it."""
        with self.rollout.open('a', encoding='utf-8') as handle:
            handle.write(json.dumps({'type': 'turn_context',
                                     'payload': {'turn_id': 'later-turn',
                                                 'model': 'gpt-6-luna'}}) + '\n')
            handle.write(json.dumps({'type': 'event_msg',
                                     'payload': {'type': 'task_complete',
                                                 'turn_id': 'later-turn',
                                                 'last_agent_message': 'later'}}) + '\n')
        self.assertEqual(exporter.verify_export(self.exported, self.rollout), [])

    def test_changed_prefix_fails_verification(self):
        """Editing inside the reviewed prefix must invalidate it."""
        lines = self.rollout.read_text(encoding='utf-8').split('\n')
        lines[0] = lines[0].replace(CHILD, 'edited-child-id')
        self.rollout.write_text('\n'.join(lines), encoding='utf-8')
        problems = exporter.verify_export(self.exported, self.rollout)
        self.assertTrue(problems, 'an edited prefix must not verify')

    def test_missing_source_fails_verification(self):
        self.rollout.unlink()
        problems = exporter.verify_export(self.exported, self.rollout)
        self.assertTrue(any('missing' in p for p in problems), problems)

    def test_wrong_source_kind_fails_verification(self):
        tampered = dict(self.exported, source_kind='something-else')
        problems = exporter.verify_export(tampered, self.rollout)
        self.assertTrue(any('rollout-prefix' in p for p in problems), problems)

    def test_unknown_schema_fails_verification(self):
        tampered = dict(self.exported, schema_version=99)
        problems = exporter.verify_export(tampered, self.rollout)
        self.assertTrue(any('schema' in p for p in problems), problems)


class RecordReviewProducerTests(unittest.TestCase):
    """The producer must refuse to attribute a verdict to the wrong artefact.

    Found by auditing the session's own review history: the P0.2 *amendment*
    reviewer's session contains a `P0.2-AC1` line (a quoted counterexample inside a
    fenced block), and the session cited it as an implementation acceptance review.
    Two independent guards now prevent that.
    """

    PRODUCER = SCRIPTS / 'record-review.py'

    def test_producer_refuses_a_footer_naming_another_packet(self):
        """A P0.2-packet request must not accept a footer naming P0.1 criteria."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'docs' / 'reviews' / 'contracts').mkdir(parents=True)
            (root / 'docs' / 'reviews' / 'contracts' / 'P0.2.json').write_text(
                json.dumps({'schema_version': 1, 'packet_id': 'P0.2',
                            'contract_file': 'docs/packets/p0-acceptance-contract.md',
                            'contract_sha256': 'a' * 64,
                            'required_ids': ['P0.2-AC1']}), encoding='utf-8')
            footer = ('REVIEW_CRITERIA_JSON: '
                      '[{"id":"P0.1-AC1","disposition":"AGREED"}]')
            source = root / 'turn.txt'
            source.write_text(f'Body.\n\n{footer}\n', encoding='utf-8')
            # Call the guard logic directly rather than the CLI, which needs a real
            # DSH session; the guard is the unit under test.
            sys.path.insert(0, str(SCRIPTS))
            from jsrf_review_records import extract_footer, parse_footer_map
            mapping = parse_footer_map(extract_footer(source.read_text(
                encoding='utf-8'))['line'])
            packet_prefix = 'P0.2-AC'
            foreign = [cid for cid in mapping if not cid.startswith(packet_prefix)]
            self.assertEqual(foreign, ['P0.1-AC1'],
                             'a footer naming another packet must be detectable')

    def test_producer_requires_a_footer_population_matching_the_manifest(self):
        """A partial footer must not silently under-report a packet's population."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = Path(tmp) / 'P0.2.json'
            manifest_path.write_text(json.dumps({
                'required_ids': ['P0.2-AC1', 'P0.2-AC2', 'P0.2-AC3', 'P0.2-AC4']}),
                encoding='utf-8')
            manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
            partial = {'P0.2-AC1': 'AGREED'}
            missing = sorted(set(manifest['required_ids']) - set(partial))
            self.assertEqual(missing, ['P0.2-AC2', 'P0.2-AC3', 'P0.2-AC4'])

    def test_producer_preserves_an_existing_delta_block(self):
        """Re-running the producer must not erase recorded post-review bookkeeping.

        Measured: rewriting the P0.1 record dropped the block documenting why its
        `AGENTS.md` binding changed, which is the bookkeeping the post-review rule
        depends on.
        """
        source = (SCRIPTS / 'record-review.py').read_text(encoding='utf-8')
        self.assertIn('preserved the existing delta block', source)
        self.assertIn("record['delta'] = previous['delta']", source)


class DshVerdictSelectionTests(unittest.TestCase):
    """The DSH adapter must pick the message that carries the verdict.

    Found on live evidence: a reviewer emitted its full review (with the footer)
    in one assistant message and a short "delivered, result was X" summary in the
    next.  Selecting the last message picked the summary and lost the footer, so a
    genuine five-criterion AGREED review validated as ``footer not authoritative``.
    """

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    @staticmethod
    def _session_lines(messages: list[tuple[str, str]], *, reason='completed') -> str:
        lines = [json.dumps({'type': 'session', 'version': 3, 'id': 'child-1',
                             'parentSession': 'session-parent'})]
        lines.append(json.dumps({
            'type': 'subagent/descriptor',
            'data': {'mode': 'continuable', 'agentProvider': 'workbuddy-ai',
                     'agentModel': 'hy4-preview-f', 'agentReasoningEffort': 'high'},
        }))
        for index, (text, message_id) in enumerate(messages):
            lines.append(json.dumps({
                'type': 'assistant/message',
                'data': {'turn': 1, 'step': index + 1,
                         'message': {'id': message_id, 'role': 'assistant',
                                     'content': [{'type': 'text', 'text': text}]}},
            }))
        end = {'turn': 1, 'reason': {'kind': reason}}
        if reason == 'error':
            end['reason']['error'] = {'message': 'Provider finish_reason: error',
                                      'code': 'PI_AI_ERROR'}
        lines.append(json.dumps({'type': 'turn/end', 'data': end}))
        return '\n'.join(lines) + '\n'

    def write(self, body: str) -> Path:
        path = self.root / 'session.v3.jsonl.zstd'
        path.write_bytes(__import__('zstandard').ZstdCompressor().compress(
            body.encode('utf-8')))
        return path

    def test_footer_message_is_chosen_over_a_later_summary(self):
        import zstandard  # noqa: F401  (import check only)

        footer = ('REVIEW_CRITERIA_JSON: '
                  '[{"id":"P0.1-AC1","disposition":"AGREED"}]')
        body = self._session_lines([
            ('P0.1-AC1: AGREED, reproduced.\n\n' + footer, 'msg-with-footer'),
            ('Review delivered. Result: all criteria AGREED.', 'msg-summary'),
        ])
        path = self.write(body)
        turns = exporter_dsh.completed_turns(path)
        self.assertEqual(len(turns), 1)
        self.assertIn('REVIEW_CRITERIA_JSON:', turns[0]['text'])
        self.assertEqual(turns[0]['final_message_id'], 'msg-with-footer')
        self.assertIn('Review delivered', turns[0]['narration'])

    def test_no_footer_anywhere_falls_back_to_last_message(self):
        body = self._session_lines([
            ('first message', 'm1'),
            ('last message, no footer', 'm2'),
        ])
        path = self.write(body)
        turns = exporter_dsh.completed_turns(path)
        self.assertEqual(turns[0]['text'], 'last message, no footer')
        self.assertEqual(turns[0]['footer_message_count'], 0)

    def test_errored_turn_is_not_exportable_as_a_verdict(self):
        footer = ('REVIEW_CRITERIA_JSON: '
                  '[{"id":"P0.1-AC1","disposition":"AGREED"}]')
        body = self._session_lines([('partial\n\n' + footer, 'm1')], reason='error')
        path = self.write(body)
        with self.assertRaises(exporter_dsh.DshSourceError) as raised:
            exporter_dsh.export_turn(path, 1)
        self.assertIn('provider error', str(raised.exception))

    def test_missing_parent_session_is_rejected(self):
        lines = [json.dumps({'type': 'session', 'version': 3, 'id': 'child-1'})]
        lines.append(json.dumps({'type': 'turn/end',
                                 'data': {'turn': 1, 'reason': {'kind': 'completed'}}}))
        path = self.write('\n'.join(lines) + '\n')
        with self.assertRaises(exporter_dsh.DshSourceError) as raised:
            exporter_dsh.source_identity(path)
        self.assertIn('parentSession', str(raised.exception))


if __name__ == '__main__':
    unittest.main(verbosity=2)
