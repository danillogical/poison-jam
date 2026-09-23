# Original DSH review sources — read-only reconnaissance

Measured by Luna Max `/root/audit_dsh_session`, 2026-09-23. Originals decoded
using Python313 `zstandard==0.25.0` stream_reader without writing them. This is
provenance reconnaissance, not P0.2 implementation or historical reacceptance.
Base: `C:\Users\logic\.dsh\sessions\--C-Users-logic-Repos-my_xbox_game--\`.

## A2f

File `2aa4b25a-25c7-45a0-a196-ad38cc1a8926/session.v3.jsonl.zstd`.
Header ID equals directory UUID; parentSession is
`session-237565f1-5b30-45ff-b6df-058195974de8`, schema v3. Descriptor: continuable,
provider spawn, label A2f acceptance review, agent route
`workbuddy-ai / hy4-preview-f / high`.

- Turn 1 start ordinal 7 / seq 6.
- Final assistant ordinal 306 / seq 305, message ID
  `fc04fe47-654c-4f77-8f4f-1410c31120f9`.
- Replay response ID `cmb-f249951db70811f1a7cf5276ee41026d`, stopReason stop.
- Single text block: 8,993 characters, UTF-8 SHA256
  `3ed9701b218f0da554e7f743227accfac4616b3a1263606a16ea4d4298e2951a`.
- Turn end ordinal 308 / seq 307, reason.kind completed.
- Compressed: 357,930 bytes, SHA256
  `189c733737c2bb7f5a3b464b3733cfea80615b3e2d375cf6863e79befbbbc618`.
- Decompressed: 1,335,217 bytes / 309 records, SHA256
  `10706d587ab57e47c404ebf167babc2278ad8763fa872875ba35207a06d2e767`.

The final response contains nine AGREED occurrences in prose, without explicit
criterion IDs or a structured verdict map. Do not infer a machine-readable
criterion assignment by counting these words.

## Independent A2f+A2g

File `1415063e-fa7d-4c0a-8e89-f827d1a43fba/session.v3.jsonl.zstd`.
Header ID equals directory UUID; parentSession is
`session-2db160de-af91-4d24-83fe-1c9211d83eca`, schema v3. Descriptor: continuable,
provider spawn, label Independent A2f+A2g review, agent route
`workbuddy-ai / hy4-preview-f / high`.

- Turn 1 start ordinal 7 / seq 6.
- Final assistant ordinal 300 / seq 299, message ID
  `686c34e1-08cc-4231-bf55-06bf29bc0b75`.
- Replay response ID `cmb-f3ba7016b71411f184d1faa8a138c971`, stopReason stop.
- Single text block: 3,586 characters, UTF-8 SHA256
  `a16283b149481dfbac84c8d618c5898aac5804e92fcc5c36814a2fc8ccffc415`.
- Turn end ordinal 302 / seq 301, reason.kind completed.
- Compressed: 277,030 bytes, SHA256
  `3c670e10cc306e3ce5c691f3631045ef017d022235c17c4b40e702c97597fc78`.
- Decompressed: 997,593 bytes / 303 records, SHA256
  `d5ea7547e60b6ec0a0c967a110070fb0f687aa7910b5b0ee517243746dfe6643`.

The final response has eight numbered agreed items, two PASS tokens, one
UNVERIFIED note and a criterion 7 reference, but no structured map. Preserve full
historical prose rather than inventing criterion assignments.

## Adapter observations

For both sources, final text is in `assistant/message.data.message.content`.
Route/stop metadata is under `message.source.replayState.response`.
Matching `turn/end` uses the same `data.turn` as the final message (1 here) and
confirms reason.kind completed. The completion is the last record, so the listed
full decompressed byte length/hash also identifies that completed-turn prefix.
Ordinals above are the worker's reported record indexes; verify indexes when
implementing, and bind by identities as well as positions. Neither source is a
live DSH readiness check. The two reviews have different parents.
