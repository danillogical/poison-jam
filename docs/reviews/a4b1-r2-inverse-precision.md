# A4b1-r2: the inverse is already documented in the toolkit — Device semantics 3's precision

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Supports:** `docs/reviews/a4b1-r2-premise-recheck.md` (premise P-F) and the `A4b1-r2` planning brief.
**Script:** `logs/a4b1/inverse-precision.py`. **Source read, not an execution.**

## The constants

```
kernel.h:395   #define XBOX_CONTIG_BASE 0x80000000u
kernel.h:396   #define XBOX_CONTIG_SIZE (64u * 1024u * 1024u)
```

So the **contiguous arena IS the `0x80000000` window** — which is exactly A4b1 premise P-E, now
confirmed numerically rather than by assertion.

## What `bridge_MmGetPhysicalAddress` does at `M`

```
va in [0x80000000, 0x84000000)  ->  va - 0x80000000     (the low-RAM physical offset)
otherwise                       ->  va                  (identity)
```

So the **inverse** maps a physical address back **into** the window.

## The toolkit already states the inverse, and calls it documented rather than a guess

`src/kernel/xbox_memory_layout.c:843-848`:

```c
                     * The contiguous window IS the physical-address view, so
                     * OR-ing its base is the documented round trip, not a
                     * guess. */
                    if (last_put && put > last_put)
                        nv2a_pb_scan(XBOX_CONTIG_BASE | (last_put & 0x0FFFFFFFu),
                                     XBOX_CONTIG_BASE | (put      & 0x0FFFFFFFu));
```

**This is the precedent `A4b1` Device semantics 3 needs.** The toolkit's own established round trip is:

```
physical P  ->  guest VA  =  XBOX_CONTIG_BASE | (P & 0x0FFFFFFF)
```

and `kernel_bridge.c:629` states the guest's own assumption in the same terms:

> *"entitled to assume `(VA & 0x0FFFFFFF) | 0x80000000 == VA`, because that …"*

So the inverse of `bridge_MmGetPhysicalAddress` is **already implemented, already commented, and already
justified in-tree** — `A4b1` does not need to invent it, only to cite it.

## The `& 0x03FFFFFF` hazard A4b1 names is REAL and still present

`A4b1` Device semantics 3 says the translation function *"does not use `& 0x03FFFFFF`; a comment
explains why"*, because xemu's `addr & 0x03FFFFFF` reads the wrong bytes. Measured at `M`, **8 live
occurrences** of that pattern, and they are in **A4b1's own write scope**:

| Site | Code |
|---|---|
| `src/apu/apu_shim.h:101,105,109` | `*(uint32_t/16/8 *)(g_apu_ram_ptr + (addr & 0x03FFFFFF))` — reads |
| `src/apu/apu_shim.h:115,119,123` | same, writes |
| `src/apu/apu_vp.c:846` | `memcpy(adpcm_block, &d->ram_ptr[addr & 0x03FFFFFF], …)` |
| `src/kernel/README.md:44` | a memory-map table row, not code |

**This matters for `A4b1-r2`:** the shim's masking is a *different* mask from the toolkit's documented
`& 0x0FFFFFFF` round trip. `apu_shim.h` masks to 26 bits (`0x03FFFFFF`), while the window round trip uses
26 bits of the *physical* offset — `0x0FFFFFFF` is 28 bits. These are **not the same operation**, and the
distinction is exactly the defect A4b1's Device semantics 3 exists to prevent. The Planner should confirm
which sites fall inside the port's write scope and whether the comment requirement covers them.

`src/apu/apu_shim.h` and `src/apu/apu_vp.c` are **byte-identical** between `0d7929c` and `M` (they are not
in A4s's changed set), so this hazard is **pre-existing and unchanged** by the baseline move — it is a
premise that *holds*, not one that moved.

## What this establishes

- `A4b1`'s address premise **holds at `M`**, and the inverse it needs is **already documented in-tree**
  (`xbox_memory_layout.c:843-845`, corroborated by `kernel_bridge.c:629`).
- Device semantics 3's wording correction is therefore **small and citable**: name
  `XBOX_CONTIG_BASE | (P & 0x0FFFFFFF)` as the inverse, per the existing round trip.
- The `& 0x03FFFFFF` sites are **real, pre-existing, and inside the port's neighbourhood**, so the
  "comment explains why" requirement has concrete sites to point at.

## Limits

- Source read only; no execution and no guest-run claim.
- It does not decide whether the shim's masking is *reachable* on the JSRF path — that is `A4b2`'s
  behaviour question, and it belongs in the packet, not in planning.
