# T10 control: known-good record — MUST NOT BE FLAGGED

This is the known-good control for `scripts/cite.py check`. Every `0x` literal below is
present in the cited output, so the lint must report zero findings and exit 0. Without
this half, a lint that flags everything would pass the transposition control.

The cited output is the verbatim stdout of:

    python -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0

Run:

    python -X utf8 scripts/cite.py check tools/citations/controls/t10-known-good.md

Expected: exit 0, `0 unsupported`.

CITE: ../outputs/t10-disasm-00193D50-00193DA0.txt

`0x00193D96` is the instruction start of `mov esi, ecx`, which is the head of the helper
`0x00193D90`. `0x00193D61` is the instruction that contains the misread address, and
`0x00193D50` begins the region this disassembly covers.
