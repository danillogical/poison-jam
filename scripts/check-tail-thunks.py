#!/usr/bin/env python3
"""Find recovered entries whose declared stack_args disagrees with their tail target.

A recovered entry's wrapper measures the net simulated-ESP change of the whole
body and compares it against ``4 + stack_args``.  When the body ends in a tail
``jmp`` the emitted code is ``target(); return;``, so the net change is
``4 + target_stack_args`` -- the thunk inherits the target's cleanup.  If the
manifest declares a smaller value the wrapper aborts at run time with

    [RECOVERED] ABI FAILURE <va> esp A->B expected +N

which is what stopped run 20260922-085326-672-fix-addref at 0x00168FF0
(``sub dword ptr [esp+4], 4; jmp 0x174C20`` -- a this-adjusting thunk).

This script finds the whole class statically instead of waiting for each one to
be hit at run time.  Ground truth is the *generated* code, because that is what
the wrapper actually executes:

* a pure thunk body is ``...; sub_TARGET(); return;`` and nothing else, and
* the target's net cleanup is the last ``esp += K; return; /* ret N */`` in the
  target's own generated body (``K`` is what the nested call really does).

A body that reaches a ``return`` before the tail call is not a thunk -- the tail
call is unreachable alignment padding, which is common after a real ``ret N``.
That distinction matters: 0x00154130 ends ``ret 0xc`` at 0x0015416F and is
followed by ``jmp 0x154180`` padding, which is dead code, not a thunk.

Usage:
    python scripts/check-tail-thunks.py [--json]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RECOVERED_C = ROOT / "src" / "recomp" / "recovered" / "recovered.c"
GEN_DIR = ROOT / "src" / "recomp" / "gen"
MANIFEST = ROOT / "config" / "recovered-functions.json"

# ``sub_00174C20(); return; /* tail jmp 0x00174C20 */``
TAIL_RE = re.compile(r"sub_([0-9A-Fa-f]{8})\(\);\s*return;\s*/\* tail jmp 0x([0-9A-Fa-f]{8}) \*/")
# ``static void body_00168FF0(void)``
BODY_RE = re.compile(r"^static void body_([0-9A-Fa-f]{8})\(void\)\s*$", re.MULTILINE)
# ``void sub_000667D0(void)``
FUNC_RE = re.compile(r"^void (sub_[0-9A-Fa-f]{8})\(void\)\s*$", re.MULTILINE)
# ``esp += 16; return; /* ret 12 */`` -- the translator's own statement of the cleanup
RET_RE = re.compile(r"esp \+= (\d+); return; /\* ret(?: (\d+))? \*/")


def load_declared() -> dict[str, int]:
    """``0x00168ff0`` -> declared stack_args from the recovered manifest."""
    entries = json.loads(MANIFEST.read_text(encoding="utf-8"))
    return {e["start"].lower(): int(e.get("stack_args", 0) or 0) for e in entries}


def body_spans(text: str, pattern: re.Pattern[str], group: int = 1):
    """Yield ``(key, body_text)`` for each definition matching *pattern*."""
    marks = [(m.start(), m.group(group).lower()) for m in pattern.finditer(text)]
    if not marks:
        return
    marks.append((len(text), None))
    for i in range(len(marks) - 1):
        start, name = marks[i]
        yield name, text[start : marks[i + 1][0]]


def last_cleanup(body: str) -> int | None:
    """The cleanup of the body's final ``esp += K; return;``, in bytes."""
    hits = RET_RE.findall(body)
    if not hits:
        return None
    return int(hits[-1][0])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true", help="emit machine-readable output")
    args = ap.parse_args()

    declared = load_declared()
    rec_text = RECOVERED_C.read_text(encoding="utf-8", errors="replace")

    # 1. Net cleanup of every generated function, straight from its emitted body.
    cleanup: dict[str, int] = {}
    for path in sorted(GEN_DIR.glob("recomp_*.c")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for name, body in body_spans(text, FUNC_RE, group=1):
            value = last_cleanup(body)
            if value is not None:
                cleanup[name] = value
    # The recovered wrappers are not generated, so their cleanup is the declared
    # contract.  Cross-check the recovered *bodies* against it: if they disagree,
    # the manifest itself is wrong and that is worth reporting on its own.
    body_cleanup = {n: last_cleanup(b) for n, b in body_spans(rec_text, BODY_RE, group=1)}
    # A tail call into a recovered address reaches the *wrapper*, whose net ESP
    # change is exactly its body's -- so the recovered bodies are the ground
    # truth for those targets too.  Generated definitions win where both exist.
    for name, value in body_cleanup.items():
        if value is None:
            continue
        cleanup.setdefault("sub_" + name.upper(), value)
        cleanup.setdefault("sub_" + name, value)

    # 2. Every body that is a pure thunk: a tail call and nothing reachable before it.
    thunks = []
    for name, body in body_spans(rec_text, BODY_RE, group=1):
        if not TAIL_RE.search(body):
            continue
        code = body.split("loc_", 1)[1] if "loc_" in body else body
        code = code.split("\n", 1)[1] if "\n" in code else code
        tail = TAIL_RE.search(code)
        if tail is None:
            continue
        # Anything that returns before the tail call makes it unreachable padding.
        if "return;" in code[: tail.start()]:
            continue
        thunks.append((name, tail.group(2).lower()))

    rows = []
    for src, tgt in thunks:
        want = declared.get("0x" + src, 0)
        target_delta = cleanup.get("sub_" + tgt.upper())
        if target_delta is None:
            target_delta = cleanup.get("sub_" + tgt)
        if target_delta is None:
            rows.append(
                {
                    "from": "0x" + src,
                    "to": "0x" + tgt,
                    "declared": want,
                    "expected": 4 + want,
                    "target_cleanup": None,
                    "verdict": "unresolved target (stub, import or fragment)",
                }
            )
            continue
        expected = 4 + want
        rows.append(
            {
                "from": "0x" + src,
                "to": "0x" + tgt,
                "declared": want,
                "expected": expected,
                "target_cleanup": target_delta,
                "verdict": "ok" if expected == target_delta else "MISMATCH",
            }
        )

    # 3. Recovered bodies that disagree with their own declared contract.
    self_inconsistent = []
    for name, value in body_cleanup.items():
        if value is None:
            continue
        want = 4 + declared.get("0x" + name, 0)
        if value != want:
            self_inconsistent.append(
                {"va": "0x" + name, "body_cleanup": value, "declared_expects": want}
            )

    bad = [r for r in rows if r["verdict"] == "MISMATCH"]

    if args.json:
        print(
            json.dumps(
                {
                    "pure_thunks": len(thunks),
                    "mismatches": bad,
                    "unresolved": [r for r in rows if r["target_cleanup"] is None],
                    "self_inconsistent": self_inconsistent,
                },
                indent=2,
            )
        )
        return 0

    print(f"generated functions with a known cleanup: {len(cleanup)}")
    print(f"pure tail thunks among the recovered entries: {len(thunks)}")
    print(f"thunks whose wrapper expectation is wrong: {len(bad)}")
    print()
    for r in bad:
        print(
            f"  {r['from']} -> {r['to']}  declared={r['declared']} "
            f"expects +{r['expected']} but the nested call does +{r['target_cleanup']}"
        )
    unresolved = [r for r in rows if r["target_cleanup"] is None]
    print()
    print(f"thunks with an unresolved target (cannot be checked statically): {len(unresolved)}")
    for r in unresolved:
        print(f"  {r['from']} -> {r['to']}")
    print()
    print(f"recovered bodies disagreeing with their own declaration: {len(self_inconsistent)}")
    for r in self_inconsistent:
        print(f"  {r['va']}  body does +{r['body_cleanup']}, declaration expects +{r['declared_expects']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
