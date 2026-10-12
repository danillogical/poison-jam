"""Decide a tutorial-progression acceptance run against its no-action control run.

The pair must perform the same inputs except the action under test: inside the block between
the `action-pre` and `action-post` peeks, each acceptance key with the action vk pairs with a
control sleep of the same duration, and every other step matches exactly. The game-owned
witness peeks named with `--witness` must each declare an explicit `expect` and match it in
both runs, and at least one of them must expect a different value in acceptance than in the
control -- otherwise the control cannot discriminate.

The verdict decides tooling conditions only. It is not proof of a game behaviour without the
frames and the validated witness it names.

Usage: python -X utf8 scripts/check-progression-run.py <accept-run-dir> <control-run-dir>
           --action-vk INT --witness NAME [--witness NAME ...]
Exit:  0 PASS, 1 FAIL (every failed check is listed), 2 unreadable input or bad arguments.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import keyscript_pair as kp  # noqa: E402

BLOCK = ("action-pre", "action-post")
REQUIRED_SNAPS = ("pre-action", "post-action", "witness-talk")


def check(accept, control, action_vk, witness_names):
    """Return (failures, summary)."""
    witness_names = tuple(witness_names)
    fails = []
    for label, run in (("acceptance", accept), ("control", control)):
        fails += kp.check_basics(label, run)
        fails += kp.check_xbe_control(label, run)
    fails += kp.check_identity(accept, control)
    fails += kp.compare_steps(accept, control, start_name=BLOCK[0], end_name=BLOCK[1],
                              action_vk=action_vk, witness_names=witness_names,
                              action_label="action press", block_label="action")

    expects = {"acceptance": {}, "control": {}}
    for label, run in (("acceptance", accept), ("control", control)):
        for name in witness_names:
            rec = run["by_name"].get(name)
            if rec is None:
                fails.append("%s: no witness peek %s" % (label, name))
                continue
            if rec.get("expect") is None:
                fails.append("%s: witness peek %s has no expect" % (label, name))
                continue
            expects[label][name] = rec.get("expect")
            if rec.get("matches") is not True:
                fails.append("%s: witness peek %s did not match its expect (value=%s, expect=%s)"
                             % (label, name, rec.get("value"), rec.get("expect")))
    common = set(expects["acceptance"]) & set(expects["control"])
    if not any(expects["acceptance"][n] != expects["control"][n] for n in common):
        fails.append("witness expectations are identical in acceptance and control: "
                     "the control cannot discriminate")

    for label, run in (("acceptance", accept), ("control", control)):
        for name in REQUIRED_SNAPS:
            if not (run["dir"] / (name + ".png")).is_file():
                fails.append("%s: missing snapshot %s.png" % (label, name))

    summary = {
        "acceptance": str(accept["dir"]),
        "control": str(control["dir"]),
        "witness": {label: {n: {"value": kp.peek_value(run, n),
                                "expect": (run["by_name"].get(n) or {}).get("expect")}
                            for n in witness_names}
                    for label, run in (("acceptance", accept), ("control", control))},
    }
    return fails, summary


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("accept")
    ap.add_argument("control")
    ap.add_argument("--action-vk", required=True, type=lambda s: int(s, 0))
    ap.add_argument("--witness", action="append", required=True, metavar="NAME")
    try:
        args = ap.parse_args(argv)
    except SystemExit:
        return 2
    try:
        accept = kp.load(args.accept)
        control = kp.load(args.control)
    except (kp.CheckError, OSError, ValueError) as e:
        print("error: %s" % e, file=sys.stderr)
        return 2
    fails, summary = check(accept, control, args.action_vk, args.witness)
    print(json.dumps({"summary": summary, "failures": fails}, indent=2))
    if fails:
        print("FAIL (%d)" % len(fails), file=sys.stderr)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
