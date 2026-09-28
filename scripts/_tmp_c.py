"""Apply the four acceptance corrections to the repeat evidence record."""
from pathlib import Path

p = Path(r"C:\Users\logic\Repos\my_xbox_game\docs\reviews\a2h-arming-coverage-repeat-execution-evidence.md")
t = p.read_text(encoding="utf-8")

# --- C1: relabel the zero-hit statement as INSTRUMENT STATUS, not coverage ---
OLD1 = "**Install control positive** (`80000115 \u2192 FE000104`, `ok=1`) in all. **Zero `GUEST_DR_HIT`** in all."
NEW1 = ("**Install control positive** (`80000115 \u2192 FE000104`, `ok=1`) in all. "
        "**Zero `GUEST_DR_HIT` in all \u2014 recorded as INSTRUMENT STATUS, not as coverage.** "
        "**Per the DR0 positive control failure below, the canonical-write channel is NOT CERTIFIED:** "
        "the watch was armed (DR7 read back `0x000D0001`) and **never fired**, so zero hits certify "
        "nothing about writes. See the `PREMISE_CHANGED` section.")
if OLD1 not in t:
    raise SystemExit("C1 pattern not found")
t = t.replace(OLD1, NEW1, 1)

# --- C3: scope the 17/17 sentence to anchored runs ---
OLD3 = "**All anchored runs coverage-complete:** 17/17 arms, overflow 0, census 28/28 zero touches, install control"
NEW3 = ("**All ANCHORED runs coverage-complete (17/17 arms; run 1, which is NOT anchored, shows 18/18 \u2014 the "
        "scope is anchored runs only):** overflow 0, census 28/28 zero touches, install control")
if OLD3 not in t:
    # the phrase may sit in the plan, not this file; check
    print("  C3 pattern not in this file (may be plan-only)")
else:
    t = t.replace(OLD3, NEW3, 1)

p.write_text(t, encoding="utf-8")
print("  C1 applied; C3 handled above")
