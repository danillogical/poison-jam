# Advisor ruling — the four-edge collapse and the completeness-method rule

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e896-97b6-7000-ad66-99dd7662fba7`.
**Raised because:** the four-edge trace collapsed four edges to one deciding quantity, **and** the Worker
measured that linear section decoding drifts — **which may qualify prior accepted rows.**
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: inventory-first as committed analysis, re-refer with it in one turn; packets only for flagged load-bearing items; method rule to §6.1; ONE bounded terminal successor on the two named gaps.**

**1. Neither audit-packet-now nor bare qualification.** The Session commits an inventory as analysis (no packet): every accepted "exactly ONE / complete sweep / N sites" claim + its enumeration method + whether a *later sound-method result already supersedes it* (much will — e.g. slot uniqueness re-verified by recursive descent + raw scan in four-edges needs no action). Re-refer with the inventory; I rule close (recorded qualification) or packet per flagged item, where flagged = linear-derived **and** load-bearing downstream (row disposition or inherited premise). **No accepted row is reopened by this ruling** — this is hygiene review, not §5.4(2); reopening requires an individually ruled finding.

**2. Confirmed with one refinement, and yes to standing rule.** Refinement: byte scans prove EXISTENCE alignment-free, but UNIQUENESS only with stated encoding coverage (same reference can wear different bytes; aligned-only scans inadmissible for uniqueness — demonstrated 0-vs-3). Linear sweep inadmissible for completeness without a drift control (known addresses reached — demonstrated miss). Rule text for §6.1, appended as a bullet alongside the existing enumeration rule: *"Completeness and uniqueness claims state their enumeration method. Linear-sweep decode is inadmissible for completeness without a drift control (known instruction addresses demonstrably reached); use recursive descent or equivalent control-flow-following enumeration. Byte-pattern scans are alignment-independent for existence; for uniqueness ('exactly one') state the encoding coverage over all instruction forms that could carry the pattern."*

**3. ONE bounded successor, terminal.** The gaps are finite and concrete (seed descent to cover `0x000D4DA0`/`0x000D4684`; read `0x122E8..0x12320` through the arg slot) — no new methods, no new enumeration. Binding scope: direct paths only (edge-1/direct-store on accepted device identity); do NOT premise the alias; read-side linkage recorded as downstream dependency, not part of this packet. Terminality: closes writer → mechanism consideration; opens another gap → PARK with precise edge + re-refer for scope (pivot/retire/other), never auto-chain. Tractability was never the issue; the bound is discipline against the L1 pattern. — And recorded: the Worker's self-falsification (`0x0018E03E` withdrawn) is why these gaps are trustworthy inputs; adversarial self-check is the highest-value evidence behavior on this line.

**BASIS:** observed — evidence §§0–9 (method probes with positive controls, edge fates, gap statements); packet rows/outcomes as read. Inferred — inventory-before-packet proportionality. Uncertain — full accepted-claim inventory (the Session's deliverable, not an assumption).

**REVERSED BY:** inventory showing a linear-derived load-bearing claim with no sound superseder (→ bounded re-derivation packet for that item); successor evidence of a new evidence-class gap (→ scope ruling, not auto-chain).

**RECORD IN:** verbatim in the line's review record; §6.1 amendment placed by Session; inventory committed then re-referred in one turn.

---

## The inventory result, which answers the Advisor's one uncertainty

**The Advisor's *"Uncertain"* line was:** *"full accepted-claim inventory (the Session's deliverable, not an
assumption)."*

**The Session delivered it:** `docs/reviews/a2h-completeness-claim-inventory.md`.

> ## **ZERO items flagged.** **No accepted row carries a linear-derived, load-bearing completeness claim.**

**Seven accepted rows; only TWO make any completeness-flavoured claim, and neither is flagged:**

| Row | Claim | Method | Why not flagged |
|---|---|---|---|
| **`a2h-rdata-call-target-r1`** | *"4 sites, exactly one with `ecx−esp = 0x20`"* | **RAW-BYTE REGEX** over every executable section | **alignment-independent**, and the reviewer **stated the encoding coverage** (`disp8` AND `disp32`) |
| **`a2h-named-producer-frame-r1`** | *"all 13 sites"* | re-walk of the **same enumerated set** | **a WITHIN-SET agreement, not an image-completeness claim** |

**The other five rows make no completeness claim at all** — **their inputs are logs, live-hook records, or
`loc_`-anchored static facts.**

**So the Advisor's own reversal condition — *"inventory showing a linear-derived load-bearing claim with no
sound superseder"* — is NOT met, and the Session recommends CLOSE with a recorded qualification.**

## The refinement the Session had not drawn, and it is the sharper half

**The Advisor wrote:** *"byte scans prove EXISTENCE alignment-free, but UNIQUENESS only with stated encoding
coverage (same reference can wear different bytes; aligned-only scans inadmissible for uniqueness —
demonstrated 0-vs-3)."*

**The Session's own framing had been simpler — *"raw-byte scans are sound, linear decodes are not"* — and the
Advisor's version is strictly better.** **A byte scan proves a pattern EXISTS somewhere; it does not prove
it exists ONLY there unless you have enumerated every ENCODING that could carry it.**

**And the `0-vs-3` demonstration is exactly that failure:** an `--aligned` dword scan reported **ZERO**
references to the vtable base where the truth is **THREE**, because `C7 06 70 12 1E 00` puts the immediate
**off** a 4-byte boundary. **An existence scan with unstated coverage would have REFUTED a correct finding.**

**The Session records this because it is the more general trap, and the rule now names it.**

## The successor: ONE bounded, TERMINAL packet

**Per point 3, the Session must write ONE successor with:**
- **binding scope: DIRECT PATHS ONLY** — edge-1 / the direct store, **on the accepted device identity**;
- **do NOT premise the alias** — **EDGE 4 was REFUTED**, so it is not a premise;
- **read-side linkage recorded as a DOWNSTREAM DEPENDENCY**, not part of the packet;
- **the two named gaps only:** (a) seed descent to cover `0x000D4DA0`/`0x000D4684`; (b) read
  `0x122E8..0x12320` through the argument slot.
- **TERMINAL:** **closes the writer \u21d2 mechanism consideration; opens another gap \u21d2 PARK with the precise
  edge and re-refer \u2014 NEVER auto-chain.**

**The Advisor's reason for the bound is worth recording verbatim:** *"Tractability was never the issue; the
bound is discipline against the L1 pattern."*

## The behavior the Advisor singled out

> *"the Worker's self-falsification (`0x0018E03E` withdrawn) is why these gaps are trustworthy inputs; **adversarial self-check is the highest-value evidence behavior on this line.**"*

**The Session endorses this and records it as the standard.** **The Worker found a candidate writer, then
falsified it against its own tool's unsoundness, and recorded the falsification rather than the hit.** **That
is why its two stated gaps can be trusted as inputs.**
