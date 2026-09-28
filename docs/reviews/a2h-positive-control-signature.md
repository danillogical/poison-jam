# `A2h` — the positive control's exact signature, and a page-granularity note

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the approved preflight makes the **installer trap** the REQUIRED positive control. **The
Session established its exact signature offline so the packet's control is precise, and found a
page-granularity detail the packet must handle.**

## The installer trap's exact signature

**Decoded from the declared boundary `0x0018CE30`:**

```
0018CE30  mov eax, [esp + 4]
0018CE34  mov ecx, [0x19dce0]        ; ecx = software_device
0018CE3A  mov [ecx + 0x242c], eax    <== THE WRITE THE WATCH MUST CATCH
0018CE40  ret 4
```

> **The control FIRES when the watch sees a WRITE to the slot with `RIP = 0x0018CE3A` and value
> `0x0015F9D0`.** **If it does not fire, that is INFRA FAILURE and the packet fails closed.**

**And the CANDIDATE writer's contrasting signature:** **if the store at `0x00199F45` wrote the slot, the watch
would see `RIP = 0x00199F45` with the packed tuple `0x001D5078`.**

**So the two signatures are DISTINCT in both RIP and VALUE** — **the control and the target cannot be
confused, which is what makes the trap a clean positive control.**

## ⚠ The page-granularity note

**The slot's absolute VA (with the observed base `0x0019B200`) is `0x0019D62C`, so:**

| Property | Value |
|---|---|
| **page base** | **`0x0019D000`** |
| **offset within page** | **`0x62C`** |
| **section** | **D3D** (`0x0018CB40..0x0019E338`) |

> **Page protection is PAGE-GRANULAR, so protecting `0x0019D000` catches EVERY write to that page — not only
> writes to the slot.**

**That is acceptable and even useful** — **more hits, filtered by address** — **but the packet must state it
so a reader does not expect a slot-only watch.** **And it means the address filter is LOAD-BEARING: a hit is
attributable to the slot only if its effective address equals the re-derived slot VA.**

**The Session also notes the slot and the installer are BOTH in D3D**, so the page being protected is inside
the section that holds the code doing the writing — **which is exactly the case where a page-guard and a
code-fetch could interact.** **The packet's fixtures should cover a write to a page that is also being
EXECUTED FROM, if the implementation protects a code-bearing page.** **Recorded as a fixture concern, not a
finding.**

## What this does NOT establish

- **Whether the watch will fire live.** **That is the trial's question.**
- **The slot's base at run time.** **The packet must RE-READ `MEM32(0x19DCE0)` at arm AND terminal and
  re-derive the slot VA — the `0x0019D62C` above uses the OBSERVED base and must not be inherited.**
- **Whether the installer fires on every run.** **The trap is the control precisely because it SHOULD.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**No DR record cited.** **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**;
**`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was not reopened.**
