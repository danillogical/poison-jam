# `A2h-arming-coverage-attribution-r2` — fresh OFF evidence: **record-level inertness CONFIRMED**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-arming-coverage-attribution-r2`, frozen
**`80E9425977547BC1EE44DD95645F72ACF4F7941857A39B6DB67B427E7DD81D84`**.
**Run:** `logs/runs/20260928-030751-407-a2h-arming-coverage-inert-off`, `--profile strict --seconds 8`,
`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000`, **no A2h or DR gates**.

**Why a FRESH OFF was required:** the packet's carry rule holds only for an **unchanged build**, and the
reporting change is a code change. **The previous OFF control could not be inherited.**

---

## The check the packet demands — record-level, not log silence

> *"require record-level inertness (no gated trace AND zero frozen arming/slot/census latches, records,
> counters and collector side effects), not merely log silence."*

### 1. No gates in the effective environment

`JSRF_COLLECTED`, `JSRF_LOG_PATH`, `RECOMP_APU_TRAP`, `RECOMP_GPU_ACK`, `RECOMP_KERNEL_LOG_BUDGET` only.
**No `JSRF_TRACE_A2H_DR`, no `JSRF_TRACE_A2H_SLOT`, no `RECOMP_KERNEL_WATCH*`.** Strict profile confirmed.

### 2. Every record family is absent — **from the log AND from `stacks.txt`**

| Family | log | `stacks.txt` |
|---|---|---|
| `[A2HSLOT]` | 0 | 0 |
| `GUEST_DR_ARM` / `_OK` / `_TERMINAL` / `_RECONCILE` / `_PRE_MAPPING_BOUND` | 0 | 0 |
| `GUEST_DR_BIRTH` / `_ROW` / `_DROPPED` | 0 | 0 |
| `GUEST_DR_ARM_TID_TERMINAL` | 0 | 0 |
| `GUEST_DR_HIT` / `GUEST_DR_DISARM` | 0 | 0 |
| `alias census` | 0 | 0 |

**The new reporting code emits nothing when gated off** — which is the point, since the whole defect was
records that were **silent when they should have spoken.**

### 3. The frozen registry is all-zero — **including the watch**

| Field | Value |
|---|---|
| registry `version` | 3 |
| `install_record_present` | **False** |
| `install_seen` / `install_ok` / `claimed` / `overflow` / `partial` / transitions | **0 / 0 / 0 / 0 / 0 / 0** |
| **watch `armed`** | **0 — never armed** |
| watch `alias_count` / `mapped_mask` / `protect_mask` / `touched_count` / `publish_failed` / `handshake_seen` | **all 0** |

**The gate-off run leaves no trace in the archived decision record** — not merely no log lines.

---

## A false positive the Session caught in its own check, recorded because the check was wrong

**My first inertness script reported FAILED** because it found one occurrence of the string `handshake` in
`stacks.txt`. **That was my check being too crude, not a defect.** The line is:

```
GUEST_SLOT_WATCH armed=0 alias_count=0 mapped_mask=00000000 protect_mask=00000000 touched=0
                 publish_failed=0 handshake_seen=0 …
```

**It is the pre-existing `GUEST_SLOT_WATCH` summary — present in the accepted pre-change OFF run as well**
(`…014526-583`, line 14044) — and **it correctly reads all zeros.** My substring match on `handshake` caught
the field name `handshake_seen`, which is exactly the kind of **grep-one-spelling** error this project has
recorded before. **The Session corrected the check rather than the artifact.**

**Recorded rather than quietly fixed**, because the near-miss is instructive: a record-level inertness check
must match **record prefixes**, not arbitrary substrings that appear inside field names.

---

## Outcome

**Record-level inertness: CONFIRMED.** The OFF control is valid for this build, and the ON runs may proceed.

**No synthetic completion.** No guest semantics changed; the APU trap and `0x80` untouched; no allocation
faked; arena not widened; NULL call not bypassed; guest error handling not edited. **No `src/recomp/gen/*.c`
edit.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**.
