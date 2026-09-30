"""Controls for W15's override-drift check.

Plan W15's failure, verbatim: "`AGENTS.md:92` and `docs/jsrf-run-profiles.md:51,54`
still describe `RECOMP_AC97_READY`/`RECOMP_APU_DSP_ACK` as live (removed in toolkit
`c97ce2c`/`8f8f6e4`)". The delivered check found exactly that line on its first run,
so the control below is the recorded defect rather than an invented one.

The check has three verdicts and each needs its own control, because the two
false-positive directions are the ones that get a lint disabled:

  * a retired name described as live must FAIL;
  * a name the runtime reads must NOT fail, however the document describes it; and
  * a name the runtime does not read, that no policy calls synthetic completion,
    must NOT fail -- most documented overrides are observation or feature
    enablement, and `jsrf_run_profile.py` says a name the runtime does not read
    cannot make a run exploratory.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_override_drift', ROOT / 'scripts' / 'check-override-drift.py')
assert _spec and _spec.loader
drift = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(drift)


class SourceScanTests(unittest.TestCase):
    """Which names count as 'read by the runtime'."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'src').mkdir(parents=True)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def write(self, relative: str, body: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(body, encoding='utf-8')

    def scan(self) -> dict:
        saved = drift.TOOLKIT
        drift.TOOLKIT = self.root
        try:
            return drift.read_names(drift.source_files())
        finally:
            drift.TOOLKIT = saved

    def test_a_quoted_getenv_name_is_read(self) -> None:
        self.write('src/a.c', 'const char *v = getenv("RECOMP_THING");\n')
        self.assertIn('RECOMP_THING', self.scan())

    def test_a_name_only_in_a_comment_is_not_read(self) -> None:
        """The exact confusion that produced W15.

        A comment documenting a removal must not count as evidence the name is
        live; only a quoted literal does, because that is what `getenv` takes.
        """
        self.write('src/a.c', '/* RECOMP_THING was removed; see history */\n')
        self.assertNotIn('RECOMP_THING', self.scan())

    def test_a_non_source_file_is_not_scanned(self) -> None:
        """A doc, a test or an unbuilt template cannot make a name live."""
        self.write('src/notes.md', 'RECOMP_THING = "RECOMP_THING"\n')
        self.assertNotIn('RECOMP_THING', self.scan())

    def test_a_real_name_in_a_header_is_read(self) -> None:
        self.write('include/a.h', 'if (getenv("JSRF_FLAG")) { }\n')
        self.assertIn('JSRF_FLAG', self.scan())


class DriftVerdictTests(unittest.TestCase):
    """The three verdicts, on the real document corpus."""

    def test_the_recorded_w15_defect_is_detected(self) -> None:
        """A document calling an unread name synthetic completion must FAIL."""
        self.assertTrue(drift.claims_synthetic_completion(
            '`RECOMP_APU_DSP_ACK` and `RECOMP_AC97_READY` are synthetic completion.'))
        self.assertTrue(drift.claims_synthetic_completion(
            'This is a bypass switch.'))

    def test_a_neutral_mention_is_not_a_completion_claim(self) -> None:
        """A document may mention an unread name without asserting what it does."""
        self.assertFalse(drift.claims_synthetic_completion(
            '`RECOMP_PB_WRAP_TRACE` is no longer read.'))
        self.assertFalse(drift.claims_synthetic_completion(
            '| `RECOMP_APU_TRAP` | Routes the APU range to the emulated APU. |'))

    def test_a_historical_marker_is_recognised(self) -> None:
        for phrase in ('historical', 'retired', 'removed', 'no longer', 'inert'):
            self.assertTrue(any(marker in phrase
                                for marker in drift.HISTORICAL_MARKERS))

    def test_the_delivered_documents_pass(self) -> None:
        """The real corpus must be clean; that is the property the check is for."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-override-drift.py')],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('no findings', result.stdout)

    def test_agents_md_no_longer_asserts_the_removed_overrides(self) -> None:
        """The specific stale text, which must not come back.

        `AGENTS.md` is loaded automatically on a byte budget, so a second copy of
        the override list drifts silently -- which is how it came to describe two
        removed overrides as live.
        """
        text = (ROOT / 'AGENTS.md').read_text(encoding='utf-8')
        for removed in ('RECOMP_AC97_READY', 'RECOMP_APU_DSP_ACK'):
            for line in text.splitlines():
                if removed not in line:
                    continue
                lowered = line.casefold()
                self.assertTrue(
                    any(marker in lowered for marker in drift.HISTORICAL_MARKERS),
                    f'AGENTS.md names {removed} without marking it historical: '
                    f'{line.strip()!r}')


if __name__ == '__main__':
    unittest.main(verbosity=2)
