# T10 control: the recorded historical transposition — MUST BE FLAGGED

This is the known-bad control for `scripts/cite.py check`. It reproduces the project's
recorded extraction error (plan `T10`, plan section 3): the guest address `0x00193D62`
was written where `0x00193D96` belonged, and the wrong value was then relied on.

`0x00193D96` is the instruction start of `mov esi, ecx` at the head of `sub_00193D90`.
`0x00193D62` is **inside** the instruction that begins at `0x00193D61`
(`mov dword ptr [ebx + 0x40071c], eax`, 6 bytes), so it is never an instruction start and
appears in no disassembly of the region.

The cited output is the verbatim stdout of:

    python -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0

Run:

    python -X utf8 scripts/cite.py check tools/citations/controls/t10-transposition.md

Expected: exit 1, exactly one `unsupported` finding for `0x00193D62`, and the supported
literal `0x00193D90` below must NOT be reported — a lint that flags everything would pass
a one-sided control and is useless.

CITE: ../outputs/t10-disasm-00193D50-00193DA0.txt

The service helper `sub_00193D90` was recorded as beginning at `0x00193D62`.
