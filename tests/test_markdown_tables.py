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
    """Count cells in a table row.

    A leading and trailing pipe delimit the row rather than delimiting cells, so the cell
    count is (number of pipes - 1). Escaped pipes (\\|) and pipes inside backtick spans are
    NOT treated specially -- the durable documents avoid both in table cells, and pretending
    to parse them would give false confidence. If either appears, the test says so rather
    than guessing.
    """
    body = row.strip()
    # strip blockquote markers
    body = re.sub(r"^(?:>\s*)+", "", body)
    return body.count("|") - 1


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


if __name__ == "__main__":
    unittest.main()
