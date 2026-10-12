"""Decide a Packet 001 graffiti acceptance run against its control run.

Reads `result.json` and `peeks.jsonl` from each run and applies fixed, named checks. A
graffiti acceptance is only credible with a control that performs the same inputs except the
spray: the witness must move in the acceptance runs and stay put in the control.

Usage: python -X utf8 scripts/check-graffiti-run.py <accept-run-dir> <control-run-dir>
Exit:  0 PASS, 1 FAIL (every failed check is listed), 2 unreadable input.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

REQUIRED_SNAPS = ("gameplay-start", "cans-collected", "pre-spray", "spray-during",
                  "post-spray", "post-spray-late")
SPRAY_VK = 0x33  # the R trigger, per toolkit src/input/keyboard_pad.c ('3')


class CheckError(Exception):
    pass


def load(run_dir):
    run_dir = Path(run_dir)
    result_path = run_dir / "result.json"
    peeks_path = run_dir / "peeks.jsonl"
    if not result_path.is_file():
        raise CheckError("%s: no result.json" % run_dir)
    result = json.loads(result_path.read_text(encoding="utf-8"))
    peeks = []
    if peeks_path.is_file():
        for line in peeks_path.read_text(encoding="utf-8").splitlines():
            if line.strip():
                peeks.append(json.loads(line))
    by_name = {}
    for p in peeks:
        by_name[p["name"]] = p
    return {"dir": run_dir, "result": result, "peeks": peeks, "by_name": by_name}


def peek_value(run, name):
    p = run["by_name"].get(name)
    if p is None:
        return None
    return p.get("value")


def steps_of(run):
    """The (kind, options) sequence of the run, as recorded.

    A peek's `expect` may legitimately differ between acceptance and control for any peek at
    or after `cans-pre-spray` (the witness is expected to move), so those expects are
    normalized away and compared by name only."""
    steps = run["result"].get("steps", [])
    marker = None
    for i, s in enumerate(steps):
        if s.get("step") == "peek" and s.get("options", {}).get("name") == "cans-pre-spray":
            marker = i
            break
    out = []
    for i, s in enumerate(steps):
        opts = dict(s.get("options") or {})
        if (marker is not None and i >= marker and s.get("step") == "peek"):
            opts.pop("expect", None)
        out.append((s.get("step"), json.dumps(opts, sort_keys=True)))
    return out


def spray_block_bounds(run):
    """(start, end) indices of the steps between the pre- and post-spray peeks."""
    steps = run["result"].get("steps", [])
    start = end = None
    for i, s in enumerate(steps):
        if s.get("step") == "peek" and s.get("options", {}).get("name") == "cans-pre-spray":
            start = i
        if s.get("step") == "peek" and s.get("options", {}).get("name") == "cans-post-spray":
            end = i
    return start, end


def check(accept, control):
    """Return (failures, summary)."""
    fails = []

    for label, run in (("acceptance", accept), ("control", control)):
        r = run["result"]
        if not r.get("script_ok"):
            fails.append("%s: script_ok is false" % label)
        for s in r.get("steps", []):
            if "satisfied" in s and not s["satisfied"]:
                fails.append("%s: step %s (%s) not satisfied"
                             % (label, s.get("n"), s.get("step")))
        for p in run["peeks"]:
            if p.get("matches") is False:
                fails.append("%s: peek %s did not match (value=%s expect=%s)"
                             % (label, p.get("name"), p.get("value"), p.get("expect")))
            if p.get("unreadable"):
                fails.append("%s: peek %s hit an unreadable page" % (label, p.get("name")))
        if "ctl-xbe-text" not in run["by_name"]:
            fails.append("%s: no ctl-xbe-text peek" % label)

    # Step sequences must agree outside the spray block; inside it, each acceptance R press
    # pairs with a control sleep of the same duration.
    a_steps, c_steps = steps_of(accept), steps_of(control)
    a_start, a_end = spray_block_bounds(accept)
    c_start, c_end = spray_block_bounds(control)
    if a_start is None or a_end is None:
        fails.append("acceptance: no spray block (cans-pre-spray/cans-post-spray peeks)")
    if c_start is None or c_end is None:
        fails.append("control: no spray block (cans-pre-spray/cans-post-spray peeks)")
    if not fails:
        outside_a = a_steps[:a_start] + a_steps[a_end + 1:]
        outside_c = c_steps[:c_start] + c_steps[c_end + 1:]
        if outside_a != outside_c:
            for i, (x, y) in enumerate(zip(outside_a, outside_c)):
                if x != y:
                    fails.append("steps differ outside the spray block at %d: %s vs %s"
                                 % (i, x, y))
                    break
            else:
                fails.append("step counts differ outside the spray block: %d vs %d"
                             % (len(outside_a), len(outside_c)))
        else:
            pairs = 0
            a_block = accept["result"]["steps"][a_start + 1:a_end]
            c_block = control["result"]["steps"][c_start + 1:c_end]
            if len(a_block) != len(c_block):
                fails.append("spray block lengths differ: %d vs %d" % (len(a_block), len(c_block)))
            else:
                for x, y in zip(a_block, c_block):
                    if x.get("step") == "key" and x.get("options", {}).get("vk") == SPRAY_VK:
                        if not (y.get("step") == "sleep"
                                and abs(y.get("options", {}).get("seconds", -1)
                                        - x.get("options", {}).get("hold", -2)) < 1e-6):
                            fails.append("acceptance R press at step %s has no matching control "
                                         "sleep of the same duration" % x.get("n"))
                        pairs += 1
                    elif x.get("step") != y.get("step"):
                        fails.append("spray block step %s differs: %s vs %s"
                                     % (x.get("n"), x.get("step"), y.get("step")))
            if pairs == 0:
                fails.append("no R press inside the spray block: nothing distinguishes "
                             "acceptance from control")

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
