"""Structural guard: markdown tables in the durable documents must be well-formed.

Why this exists
---------------
The `A2h-oom-causal-slice-r1` acceptance took THREE rounds, and every round failed on the same
family of mistake -- a correction applied to the sentence in front of the author without
checking the structure it sits in:

  round 1: correction BANNERS were added but the document BODIES were never swept.
  round 2: the sweep's PATTERN was too narrow -- it searched `no-trap` while the survivor
           spelled it `NO trap`.
  round 3: the sentence and the ledger beneath it were fixed, and the fix left a stray `|`
           that malformed the table row above them.

The third round's defect was caught by a human-style reader noticing a pipe count. It is
mechanical, so a machine should catch it. This test does exactly that: every markdown table
row in the durable documents must have the same number of cells as its header.

It is deliberately narrow: it checks CELL COUNT, not alignment, padding, escaping, or
rendering. A row with a different cell count from its header is malformed in every CommonMark
renderer and is always a defect; anything subtler is left to review.

Run:  python -X utf8 -m unittest tests.test_markdown_tables
"""
from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# The durable documents whose tables a reader depends on.
DOCUMENTS = [
    ROOT / "plan-jsrf-bare-minimum.md",
    ROOT / "AGENTS.md",
    ROOT / "docs" / "agent-workflow.md",
    ROOT / "docs" / "jsrf-run-profiles.md",
]

# A table row starts and ends with a pipe (after optional indentation/blockquote markers).
ROW = re.compile(r"^\s*(?:>\s*)*\|.*\|\s*$")


def cells(row: str) -> int:
    """Count cells in a table row, honouring escaped pipes and backtick code spans.

    A leading and trailing pipe delimit the row rather than delimiting cells, so the cell
    count is (number of DELIMITING pipes - 1).

    Two constructs must not be counted as delimiters, and both are common in this
    repository's register and toolkit-sync tables:

      * an ESCAPED pipe (`\\|`), which renders as a literal pipe inside a cell;
      * a pipe inside a BACKTICK code span (`` `GS |= 1` ``), which renders literally.

    An earlier version of this function was `return body.count("|") - 1` with no detection
    branch at all, while its docstring promised that escaped pipes and backtick spans were
    handled. That mismatch was caught in acceptance: the guard reported CORRECT rows as
    malformed, and it passed only because none of the four guarded documents happened to
    contain an escaped pipe yet. A guard that fails on correct input gets deleted under
    pressure, so the counting is now correct by construction rather than by luck.
    """
    body = re.sub(r"^(?:>\s*)+", "", row.strip())

    delimiters = 0
    in_code = False
    i = 0
    while i < len(body):
        ch = body[i]
        if ch == "\\" and i + 1 < len(body):
            # An escaped character is literal; skip both characters.
            i += 2
            continue
        if ch == "`":
            in_code = not in_code
            i += 1
            continue
        if ch == "|" and not in_code:
            delimiters += 1
        i += 1
    return max(delimiters - 1, 0)


def has_unescaped_pipe_ambiguity(row: str) -> bool:
    """True when a row contains a construct whose cell count could be disputed.

    Kept as a named predicate so the guard can REPORT such rows rather than silently
    guessing -- the behaviour the docstring always promised. Currently the counter handles
    both constructs, so this is a diagnostic aid, not a gate.
    """
    body = re.sub(r"^(?:>\s*)+", "", row.strip())
    return "\\|" in body or "`" in body


def table_blocks(lines: list[str]) -> list[tuple[int, int]]:
    """Yield (start_index, end_index_exclusive) for each run of consecutive table rows."""
    blocks = []
    start = None
    for i, L in enumerate(lines):
        if ROW.match(L):
            if start is None:
                start = i
        else:
            if start is not None:
                blocks.append((start, i))
                start = None
    if start is not None:
        blocks.append((start, len(lines)))
    return blocks


class MarkdownTableTests(unittest.TestCase):
    def test_durable_documents_have_consistent_table_rows(self):
        problems = []
        for path in DOCUMENTS:
            if not path.is_file():
                continue
            lines = path.read_text(encoding="utf-8", errors="replace").split("\n")
            for start, end in table_blocks(lines):
                block = lines[start:end]
                if len(block) < 2:
                    # A lone pipe line is not a table.
                    continue
                header = cells(block[0])
                # block[1] is the delimiter row (---); it has the same shape as the header.
                for offset, row in enumerate(block):
                    n = cells(row)
                    if n != header:
                        problems.append(
                            f"{path.name}:{start + offset + 1}: row has {n} cell(s) "
                            f"but the table header has {header}")
        self.assertEqual(
            problems, [],
            "malformed markdown table row(s):\n  " + "\n  ".join(problems))

    def test_guard_detects_a_stray_pipe(self):
        """Positive control: the guard must actually catch the defect it exists for."""
        lines = [
            "| a | b |",
            "|---|---|",
            "| 1 | 2 |",
            "| 1 | 2 |; stray |",
        ]
        blocks = table_blocks(lines)
        self.assertEqual(len(blocks), 1)
        header = cells(lines[0])
        bad = [i for i in range(blocks[0][0], blocks[0][1]) if cells(lines[i]) != header]
        self.assertEqual(bad, [3], "the guard must flag the row with the stray pipe")

    def test_guard_accepts_a_well_formed_table(self):
        lines = [
            "| a | b |",
            "|---|---|",
            "| 1 | 2 |",
            "| 3 | 4 |",
        ]
        blocks = table_blocks(lines)
        header = cells(lines[0])
        bad = [i for i in range(blocks[0][0], blocks[0][1]) if cells(lines[i]) != header]
        self.assertEqual(bad, [])

    def test_guard_handles_blockquoted_tables(self):
        lines = [
            "> | a | b |",
            "> |---|---|",
            "> | 1 | 2 |",
        ]
        blocks = table_blocks(lines)
        self.assertEqual(len(blocks), 1)
        header = cells(lines[0])
        self.assertEqual([cells(L) for L in lines], [header] * 3)

    def test_guard_ignores_non_table_pipes(self):
        lines = [
            "prose with a | pipe",
            "| a | b |",
            "|---|---|",
            "| 1 | 2 |",
            "more prose | here",
        ]
        blocks = table_blocks(lines)
        self.assertEqual(len(blocks), 1)
        self.assertEqual(blocks[0], (1, 4))

    def test_cell_count_arithmetic(self):
        self.assertEqual(cells("| a | b |"), 2)
        self.assertEqual(cells("| a | b | c |"), 3)
        self.assertEqual(cells("> | a | b |"), 2)

    def test_escaped_pipe_inside_a_cell_is_not_a_delimiter(self):
        """`\\|` renders as a literal pipe, so it must not split the cell.

        The acceptance reviewer demonstrated that the earlier counter reported these
        CORRECT rows as malformed -- a guard that fails on correct input.
        """
        self.assertEqual(cells(r"| `A\|B` | clears it |"), 2)
        self.assertEqual(cells(r"| x \| y | z |"), 2)
        self.assertEqual(cells(r"| a | b \| c | d |"), 3)

    def test_pipe_inside_a_backtick_span_is_not_a_delimiter(self):
        self.assertEqual(cells("| `GS |= 1` | sets it |"), 2)
        self.assertEqual(cells("| a | `b | c` | d |"), 3)

    def test_guard_accepts_rows_with_escaped_pipes_and_code_spans(self):
        """A well-formed table using either construct must pass the whole-document check."""
        lines = [
            "| Override | Effect | Why |",
            "|---|---|---|",
            r"| `A\|B` | clears it | cannot pass |",
            "| `GS |= 1` | sets it | cannot pass |",
        ]
        blocks = table_blocks(lines)
        self.assertEqual(len(blocks), 1)
        header = cells(lines[0])
        bad = [i for i in range(blocks[0][0], blocks[0][1]) if cells(lines[i]) != header]
        self.assertEqual(bad, [], "correct rows using `\\|` or `|=` must not be flagged")

    def test_guard_still_catches_a_real_defect_alongside_escaped_pipes(self):
        """The positive control must survive the counting fix."""
        lines = [
            "| Override | Effect | Why |",
            "|---|---|---|",
            r"| `A\|B` | clears it | cannot pass |",
            "| `GS |= 1` | sets it |",          # genuinely missing a cell
        ]
        blocks = table_blocks(lines)
        header = cells(lines[0])
        bad = [i for i in range(blocks[0][0], blocks[0][1]) if cells(lines[i]) != header]
        self.assertEqual(bad, [3])

    def test_ambiguity_predicate_reports_both_constructs(self):
        """The diagnostic the docstring promised: say so rather than guess."""
        self.assertTrue(has_unescaped_pipe_ambiguity(r"| a \| b | c |"))
        self.assertTrue(has_unescaped_pipe_ambiguity("| `x | y` | z |"))
        self.assertFalse(has_unescaped_pipe_ambiguity("| a | b |"))

    def test_unterminated_code_span_does_not_crash(self):
        """A malformed row must still produce a number, not an exception."""
        self.assertIsInstance(cells("| `unclosed | b |"), int)


if __name__ == "__main__":
    unittest.main()
