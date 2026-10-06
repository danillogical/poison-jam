"""Controls for `scripts/check-stop-chain.py`.

The gate exists because the stop chain has twice claimed progression its archived
runs did not establish: `0x00032610` was credited to a run that never dispatched
it (f24/f25, Turn Reviewer finding B3), and `0xB5EB0` was repaired and then
credited by two runs that never executed it.

A checker that cannot fail is not a checker. Every control here builds a scratch
record and a scratch runs directory in a temporary directory and runs the **real
script** against them, so nothing in the live tree is touched and the failure path
is exercised end to end.

The deciding control is `test_confirmed_without_a_return_line_fails`: it takes a
genuine confirming row and cites a run that does not contain the ABI-verified
return. That is the exact false-continuation claim the gate exists to reject.
"""
from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts' / 'check-stop-chain.py'

ADDRESS = '0x000B5EB0'
REPAIR = '64945a3'
# Real revisions, because the ancestry half of the gate can only be exercised
# with commits that exist in this repository: a synthetic revision is reported
# UNKNOWN (not refuted), which is correct but would test nothing here.
#
# Both of these are genuine archive revisions from this session, and the pair is
# itself the finding the gate exists for: g07 (6f2e4e2) *discovered* 0xB5EB0 and
# does NOT descend from its repair 64945a3, while g08 (82a2c40) does. So g07 can
# only ever be cited as `discovered`, and only a later run could confirm it.
REVISION = '82a2c406ba84b383324d4fd0f78aea7ebe5aebe1'             # descends from 64945a3
PRE_REPAIR_REVISION = '6f2e4e2662e144556b2096dc0d9bf66f9da9bfbf'  # does not

RETURN_LINE = f'[RECOVERED] {ADDRESS} returned; ABI verified (ESP/EBX/ESI/EDI)\n'
DEFECT_LINE = '[ICALL] Failed to resolve VA 0x000B5F82 (thread calls: 1, tid=1, ms=1)\n'
HISTORY_BLOCK = f'  [ 0] 0x00190FB0\n  [15] {ADDRESS}\n'


class Scratch:
    """A scratch record plus scratch run archives."""

    def __init__(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix='jsrf-stopchain-'))
        self.runs = self.root / 'logs' / 'runs'
        self.runs.mkdir(parents=True)
        self.record = self.root / 'stop-chain.json'

    def close(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)

    def run(self, name: str, log: str, revision: str = REVISION,
            with_metadata: bool = True, with_result: bool = True,
            break_log_hash: bool = False) -> None:
        archive = self.runs / name
        archive.mkdir(parents=True, exist_ok=True)
        log_path = archive / 'jsrf_run.log'
        # Write bytes, then hash the bytes on disk. Hashing the source string
        # instead would disagree on any host that translates newlines, and the
        # gate would then be testing the host rather than the record.
        log_path.write_bytes(log.encode('utf-8'))
        if with_result:
            (archive / 'result.json').write_text('{"outcome": "test"}', encoding='utf-8')
        if with_metadata:
            digest = hashlib.sha256(log_path.read_bytes()).hexdigest()
            if break_log_hash:
                digest = 'f' * 64
            (archive / 'metadata.json').write_text(json.dumps({
                'repository_identities': {'project': {'revision': revision}},
                'run_log_sha256': digest,
            }), encoding='utf-8')

    def write(self, rows: list[dict]) -> None:
        self.record.write_text(json.dumps({'schema': 1, 'stops': rows}),
                               encoding='utf-8')

    def check(self) -> subprocess.CompletedProcess:
        return subprocess.run(
            [sys.executable, '-X', 'utf8', str(SCRIPT),
             '--record', str(self.record), '--runs', str(self.runs)],
            capture_output=True, text=True)


def row(state: str, evidence: list[dict], address: str = ADDRESS,
        repair: str | None = REPAIR) -> dict:
    out = {'id': 1, 'address': address, 'state': state, 'evidence': evidence}
    if repair is not None:
        out['repair_commit'] = repair
    return out


class StopChainControls(unittest.TestCase):
    def setUp(self) -> None:
        self.scratch = Scratch()
        self.addCleanup(self.scratch.close)

    # -- the gate must be able to fail ------------------------------------

    def test_confirmed_without_a_return_line_fails(self):
        """The exact false-continuation claim this gate exists to reject.

        The row says RUNTIME_CONFIRMED and cites a run whose log has no
        ABI-verified return for the address -- the `0x00032610`/f25 defect.
        """
        self.scratch.run('r1', 'unrelated output only\n')
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'r1', 'role': 'confirmed'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('no ABI-verified return', r.stderr)
        self.assertIn('NOT EXERCISED, not a pass', r.stderr)

    def test_state_unsupported_by_roles_fails(self):
        """A row may not claim a confirmed state with only negative evidence."""
        self.scratch.run('r1', 'nothing here\n')
        self.scratch.write([row('RUNTIME_CONFIRMED',
                                [{'run': 'r1', 'role': 'not_exercised'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('false-continuation claim', r.stderr)

    def test_not_exercised_role_that_did_return_fails(self):
        """The inverse: claiming a run did NOT reach it when it did."""
        self.scratch.run('r1', RETURN_LINE)
        self.scratch.write([row('NOT_EXERCISED',
                                [{'run': 'r1', 'role': 'not_exercised'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('DOES contain', r.stderr)

    def test_confirmation_from_a_run_that_predates_the_repair_fails(self):
        """The ancestry half: the return proves the path ran, not that the fix ran.

        g05 found `0x00154540` and does not descend from its own repair commit,
        which is exactly why that row cites g05 as `discovered` and not as a
        confirmation. Both revisions here are real commits, so this control
        exercises the ancestry comparison rather than the unprovable case.
        """
        self.scratch.run('r1', RETURN_LINE, revision=PRE_REPAIR_REVISION)
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'r1', 'role': 'confirmed'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('does not descend from the repair', r.stderr)

    def test_an_unprovable_revision_is_unknown_not_refuted(self):
        """A revision this repository cannot resolve must not read as refuted."""
        self.scratch.run('r1', RETURN_LINE, revision='0' * 40)
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'r1', 'role': 'confirmed'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('refusing to assume', r.stderr)
        self.assertNotIn('does not descend', r.stderr)

    def test_confirmation_without_a_repair_commit_fails(self):
        self.scratch.run('r1', RETURN_LINE)
        self.scratch.write([row('RUNTIME_CONFIRMED',
                                [{'run': 'r1', 'role': 'confirmed'}], repair=None)])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('no repair_commit', r.stderr)

    def test_confirmation_without_a_recorded_revision_fails(self):
        self.scratch.run('r1', RETURN_LINE, with_metadata=False)
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'r1', 'role': 'confirmed'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('records no project revision', r.stderr)

    def test_citing_a_run_that_does_not_exist_fails(self):
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'nope', 'role': 'confirmed'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('is not an archived run', r.stderr)

    def test_a_cited_run_missing_result_json_fails(self):
        self.scratch.run('r1', RETURN_LINE, with_result=False)
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'r1', 'role': 'confirmed'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('missing result.json', r.stderr)

    def test_a_log_that_does_not_match_its_own_metadata_hash_fails(self):
        """A citation must be bound to the archived bytes, not to a later edit."""
        self.scratch.run('r1', RETURN_LINE, break_log_hash=True)
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'r1', 'role': 'confirmed'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('hash does not match', r.stderr)

    def test_an_empty_evidence_list_may_not_claim_runtime_state(self):
        self.scratch.write([row('RUNTIME_CONFIRMED', [])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('NO evidence rows', r.stderr)

    def test_an_unknown_role_fails(self):
        self.scratch.run('r1', RETURN_LINE)
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'r1', 'role': 'vibes'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('unknown role', r.stderr)

    def test_a_discovery_citation_with_no_signal_at_all_fails(self):
        self.scratch.run('r1', 'nothing relevant\n')
        self.scratch.write([row('NOT_EXERCISED', [{'run': 'r1', 'role': 'discovered'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('ICALL-history frame', r.stderr)

    def test_a_missing_record_is_not_a_pass(self):
        r = subprocess.run(
            [sys.executable, '-X', 'utf8', str(SCRIPT),
             '--record', str(self.scratch.root / 'absent.json'),
             '--runs', str(self.scratch.runs)],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn('has no record', r.stderr)

    # -- ...and must pass on the real shapes ------------------------------

    def test_a_genuine_confirmation_passes(self):
        self.scratch.run('r1', RETURN_LINE)
        self.scratch.write([row('RUNTIME_CONFIRMED', [{'run': 'r1', 'role': 'confirmed'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('PASS', r.stdout)

    def test_a_genuine_not_exercised_row_passes(self):
        """The honest `0xB5EB0` shape: found by one run, never executed by it."""
        self.scratch.run('r1', DEFECT_LINE + HISTORY_BLOCK)
        self.scratch.run('r2', RETURN_LINE.replace(ADDRESS, '0x00000000'))
        self.scratch.write([row('NOT_EXERCISED', [
            {'run': 'r1', 'role': 'discovered'},
            {'run': 'r2', 'role': 'not_exercised'},
        ])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_discovery_by_icall_history_passes(self):
        """`0xB5EB0` was found as an ICALL-history frame, not as a resolve line.

        The guest trapped on the out-of-span table arm `0xB5F82`; the history
        named `0xB5EB0` as the body whose own switch table had been cut.
        """
        self.scratch.run('r1', DEFECT_LINE + HISTORY_BLOCK)
        self.scratch.write([row('NOT_EXERCISED', [{'run': 'r1', 'role': 'discovered'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_discovery_by_abi_failure_line_passes(self):
        self.scratch.run('r1', f'[RECOVERED] ABI FAILURE {ADDRESS} esp 1->2 expected +4\n')
        self.scratch.write([row('REPAIRED', [{'run': 'r1', 'role': 'discovered'}])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_a_static_only_row_with_no_evidence_passes(self):
        """`0xB06E0` is a proved static defect no run has reached."""
        self.scratch.write([row('STATIC_ONLY', [])])
        r = self.scratch.check()
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    # -- the live record -------------------------------------------------

    def test_the_live_record_passes(self):
        r = subprocess.run([sys.executable, '-X', 'utf8', str(SCRIPT)],
                           capture_output=True, text=True, cwd=str(ROOT))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_the_live_record_does_not_overstate_stop_21(self):
        """`0xB5EB0` may only be confirmed by a run that actually executed it.

        This test originally asserted the *state* was NOT_EXERCISED, because two
        consecutive runs had failed to reach it. Run f9 then genuinely did, so the
        frozen value became wrong -- and the test failed, which is the correct
        behaviour for a pin that has been overtaken. It now asserts the durable
        invariant instead of a snapshot: if the row says RUNTIME_CONFIRMED, at
        least one cited run must actually contain the ABI-verified return line,
        checked against the archives rather than against the record's own claim.
        """
        record = json.loads((ROOT / 'config' / 'stop-chain.json').read_text(encoding='utf-8'))
        stops = record['stops'] if isinstance(record, dict) else record
        row21 = next(s for s in stops if s['address'].lower() == '0x000b5eb0')

        confirming = [e for e in row21['evidence']
                      if e['role'] in {'confirmed', 'exercised', 'returned'}]
        if row21['state'] != 'RUNTIME_CONFIRMED':
            self.assertFalse(confirming,
                             'stop 21 is not RUNTIME_CONFIRMED but cites a confirming role')
            return
        self.assertTrue(confirming, 'RUNTIME_CONFIRMED with no confirming evidence')
        for item in confirming:
            log = ROOT / 'logs' / 'runs' / item['run'] / 'jsrf_run.log'
            self.assertTrue(log.is_file(), f"{item['run']} has no archived log")
            text = log.read_text(encoding='utf-8', errors='replace')
            self.assertIn('0x000B5EB0 returned; ABI verified', text,
                          f"{item['run']} is cited as confirming 0xB5EB0 but its log has no "
                          f"ABI-verified return for it")


if __name__ == '__main__':
    unittest.main(verbosity=2)
