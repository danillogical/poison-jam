"""Decide a graffiti acceptance run against its control run.

Reads `result.json`, `peeks.jsonl` and `metadata.json` from each run and applies fixed, named
checks. A graffiti acceptance is only credible with a control that performs the same inputs
except the spray: the witness must move in the acceptance run and stay put in the control.

The verdict decides tooling conditions only -- the mapping control, build identity, input
equivalence and the declared can-counter witness. A PASS is not proof of painted graffiti
without the frames and game-owned witness it names.

Usage: python -X utf8 scripts/check-graffiti-run.py <accept-run-dir> <control-run-dir>
Exit:  0 PASS, 1 FAIL (every failed check is listed), 2 unreadable input.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import keyscript_pair as kp  # noqa: E402

REQUIRED_SNAPS = ("gameplay-start", "cans-collected", "pre-spray", "spray-during",
                  "post-spray", "post-spray-late")
SPRAY_VK = 0x33  # the R trigger, per toolkit src/input/keyboard_pad.c ('3')
# Peeks whose `expect` may legitimately differ between acceptance and control: the witness.
GRAFFITI_WITNESS_PEEKS = ("cans-pre-spray", "cans-post-spray", "cans-late",
                          "tag-pre-spray", "tag-post-spray")

CheckError = kp.CheckError
load = kp.load
peek_value = kp.peek_value


def check(accept, control):
    """Return (failures, summary)."""
    fails = []
    for label, run in (("acceptance", accept), ("control", control)):
        fails += kp.check_basics(label, run)
        fails += kp.check_xbe_control(label, run)
    fails += kp.check_identity(accept, control)
    fails += kp.compare_steps(accept, control, start_name="cans-pre-spray",
                              end_name="cans-post-spray", action_vk=SPRAY_VK,
                              witness_names=GRAFFITI_WITNESS_PEEKS, action_label="R press",
                              block_label="spray")

    # The witness: cans fall on the spray in acceptance, and never in control.
    a_collected = peek_value(accept, "cans-collected")
    a_pre = peek_value(accept, "cans-pre-spray")
    a_post = peek_value(accept, "cans-post-spray")
    a_late = peek_value(accept, "cans-late")
    c_pre = peek_value(control, "cans-pre-spray")
    c_post = peek_value(control, "cans-post-spray")
    c_late = peek_value(control, "cans-late")

    for name, v in (("cans-collected", a_collected), ("cans-pre-spray", a_pre),
                    ("cans-post-spray", a_post), ("cans-late", a_late),
                    ("control cans-pre-spray", c_pre), ("control cans-post-spray", c_post),
                    ("control cans-late", c_late)):
        if v is None:
            fails.append("no %s peek value" % name)
    if None not in (a_collected, a_pre, a_post, a_late, c_pre, c_post, c_late):
        if not a_collected > 0:
            fails.append("acceptance: cans-collected is not > 0 (got %s)" % a_collected)
        if not (a_pre == a_collected):
            fails.append("acceptance: cans-pre-spray %s != cans-collected %s"
                         % (a_pre, a_collected))
        if not a_post < a_pre:
            fails.append("acceptance: cans-post-spray %s did not fall below cans-pre-spray %s"
                         % (a_post, a_pre))
        if not a_late == a_post:
            fails.append("acceptance: cans-late %s != cans-post-spray %s" % (a_late, a_post))
        if not (c_post == c_pre and c_late == c_pre):
            fails.append("control: cans changed without a spray (%s -> %s -> %s)"
                         % (c_pre, c_post, c_late))

    # The tag-state flag, when the acceptance run carries it.
    a_tpre, a_tpost = peek_value(accept, "tag-pre-spray"), peek_value(accept, "tag-post-spray")
    if a_tpre is not None and a_tpost is not None:
        c_tpre, c_tpost = peek_value(control, "tag-pre-spray"), peek_value(control, "tag-post-spray")
        if not a_tpre != a_tpost:
            fails.append("acceptance: tag state did not change (%s -> %s)" % (a_tpre, a_tpost))
        if c_tpre is not None and c_tpost is not None and c_tpre != c_tpost:
            fails.append("control: tag state changed without a spray (%s -> %s)"
                         % (c_tpre, c_tpost))

    for label, run in (("acceptance", accept), ("control", control)):
        for name in REQUIRED_SNAPS:
            if not (run["dir"] / (name + ".png")).is_file():
                fails.append("%s: missing snapshot %s.png" % (label, name))

    summary = {
        "acceptance": str(accept["dir"]),
        "control": str(control["dir"]),
        "acceptance_peeks": {n: peek_value(accept, n) for n in
                             ("cans-collected", "cans-pre-spray", "cans-post-spray", "cans-late")},
        "control_peeks": {n: peek_value(control, n) for n in
                          ("cans-pre-spray", "cans-post-spray", "cans-late")},
    }
    return fails, summary


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 2:
        print(__doc__.strip().splitlines()[-2], file=sys.stderr)
        return 2
    try:
        accept = load(argv[0])
        control = load(argv[1])
    except (CheckError, OSError, ValueError) as e:
        print("error: %s" % e, file=sys.stderr)
        return 2
    fails, summary = check(accept, control)
    print(json.dumps({"summary": summary, "failures": fails}, indent=2))
    if fails:
        print("FAIL (%d)" % len(fails), file=sys.stderr)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
