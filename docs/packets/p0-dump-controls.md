# P0.4-AC3 concrete observation amendment

Revision `P0.4-observations-r1`, proposed for adequacy review. No P0.4 implementation
or acceptance precedes P0.3. This supplies the exact artifact/read oracle missing
from `P0-AC-r1`; all its other controls remain mandatory.

Artifact: `logs/runs/20260922-224429-003-a2g-304f0-span/`.
SHA256 identities:

- process.dmp: `C73C5E2DDC119E5970831111324ACB248BE865A343F720864CAA5D9B5C25C8BF`
- jsrf_run.log: `B9631EE5AF44B40213F84C8645BDC6FB31F23A7462B41866CA9CFBE5F1176A04`
- stacks.txt: `715AB30BBF3EA08900D012F2E89FE5F7DD9702E5755353A9E8CBCB322C447137`

From the game root, independently reproduce these existing commands:

```powershell
C:\Python313\python.exe -X utf8 scripts\inspect-jsrf.py memory logs/runs/20260922-224429-003-a2g-304f0-span 0x001C4064 4
C:\Python313\python.exe -X utf8 scripts\inspect-jsrf.py memory logs/runs/20260922-224429-003-a2g-304f0-span 0x0018CA5C 4
C:\Python313\python.exe -X utf8 scripts\inspect-jsrf.py memory logs/runs/20260922-224429-003-a2g-304f0-span 0x00F7FD00 4
```

Parent reproduced these during planning, all exit 0: respective dwords are
`00000000`, `FE000104`, `0014982E`. The independent runtime log records
`[ICALL] invalid target 0x00000000 ... esp=00F7FD00 return=0014982E`.
The reader must return bytes at those actual VAs. The second read observes the
relocated slot, not a corrected interpretation of the first. Never globally shift
the stack or image reads. One matching stack word supports that location only.

PASS for AC3 requires matching artifact hashes, all three observed dwords, the
matching runtime ICALL/ESP/return witness, and independent reviewer reproduction.
Mismatch is FAIL; absent/truncated/unreadable input is UNKNOWN. The artifact is
exploratory and establishes these captured observations only, not strict execution,
global capture validity, cause of corruption or a completed fix. Fixture positive,
displaced-content, malformed and missing controls in AC1/AC2 remain required.

The required separate result fields for this artifact are
`structural=SUPPORTED_AT_LOGGED_ESP` (only the checked stack word) and
`image_content=CONTENT_MISMATCH`. Also run the existing memory command above at
`0x00011000 16`: parent observed dwords `5614C483 EB1050FF 24448B5D 6AC0850C`,
or bytes `83c41456ff5010eb5d8b44240c85c06a`, unlike original XBE control bytes
`8b512c85d28b4130c70190431c00741c`.
Run `C:\Python313\python.exe -X utf8 scripts\check-dump-mapping.py logs/runs/20260922-224429-003-a2g-304f0-span`.
Its expected nonzero mismatch exit is the intended finding and satisfies this
observation oracle; it is not itself AC3 failure. A different result, absent
control or unreadable input needs reconciliation rather than a shifted read.
