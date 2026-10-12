"""Shared decisions for an acceptance/control pair of `key-script-run.py` runs.

A pair is credible only when both runs read guest memory through a proven translation, come
from the same executable and XBE on verified disposable saves, and perform the same inputs
except the action under test. `check-graffiti-run.py` and `check-progression-run.py` apply
these rules and add their own witness checks; neither verdict is proof of a game behaviour
without the frames and game-owned witness it names.
"""
from __future__ import annotations

import json
from pathlib import Path

# AGENTS.md mapping control: guest VA 0x00011000 begins with the bytes 8b 51 2c 85, so its
# little-endian u32 is 0x852C518B. A run that cannot read exactly this has no working guest
# memory translation, and every other peek from it is untrustworthy.
XBE_CONTROL_NAME = "ctl-xbe-text"
XBE_CONTROL_VA = 0x00011000
XBE_CONTROL_SIZE = 4
XBE_CONTROL_VALUE = 0x852C518B


class CheckError(Exception):
    pass


def load(run_dir):
    run_dir = Path(run_dir)
    result_path = run_dir / "result.json"
    peeks_path = run_dir / "peeks.jsonl"
    metadata_path = run_dir / "metadata.json"
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
    metadata = None
    if metadata_path.is_file():
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    return {"dir": run_dir, "result": result, "peeks": peeks, "by_name": by_name,
            "metadata": metadata}


def peek_value(run, name):
    p = run["by_name"].get(name)
    if p is None:
        return None
    return p.get("value")


def _va(value):
    """A VA as the runner records it: int in result.json, "0x..." string in peeks.jsonl."""
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError:
            return None
    return None


def check_basics(label, run):
    """Script completion, satisfied steps and every peek's read state."""
    fails = []
    r = run["result"]
    if not r.get("script_ok"):
        fails.append("%s: script_ok is false" % label)
    for s in r.get("steps", []):
        if "satisfied" in s and not s["satisfied"]:
            fails.append("%s: step %s (%s) not satisfied" % (label, s.get("n"), s.get("step")))
    for p in run["peeks"]:
        if p.get("matches") is False:
            fails.append("%s: peek %s did not match (value=%s expect=%s)"
                         % (label, p.get("name"), p.get("value"), p.get("expect")))
        if p.get("unreadable"):
            fails.append("%s: peek %s hit an unreadable page" % (label, p.get("name")))
        if "error" in p:
            fails.append("%s: peek %s failed to read (%s)" % (label, p.get("name"), p.get("error")))
    return fails


def check_xbe_control(label, run):
    """The mapping control must prove the reader works, not merely exist."""
    records = [p for p in run["peeks"] if p.get("name") == XBE_CONTROL_NAME]
    if not records:
        return ["%s: no ctl-xbe-text peek" % label]
    if len(records) != 1:
        return ["%s: ctl-xbe-text recorded %d times" % (label, len(records))]
    rec = records[0]
    fails = []
    if _va(rec.get("va")) != XBE_CONTROL_VA or rec.get("size") != XBE_CONTROL_SIZE:
        fails.append("%s: ctl-xbe-text read VA %s size %s, not 0x00011000 size 4"
                     % (label, rec.get("va"), rec.get("size")))
    if "error" in rec or rec.get("value") != XBE_CONTROL_VALUE:
        fails.append("%s: ctl-xbe-text value %s is not the XBE control 0x852C518B"
                     % (label, rec.get("value")))
    if rec.get("matches") is not True:
        fails.append("%s: ctl-xbe-text matches is %s, not true" % (label, rec.get("matches")))
    steps = [s for s in run["result"].get("steps", [])
             if s.get("step") == "peek"
             and (s.get("options") or {}).get("name") == XBE_CONTROL_NAME]
    agrees = False
    if len(steps) == 1:
        o = steps[0].get("options") or {}
        agrees = (_va(o.get("va")) == XBE_CONTROL_VA and o.get("size") == XBE_CONTROL_SIZE
                  and o.get("expect") == XBE_CONTROL_VALUE
                  and steps[0].get("value") == XBE_CONTROL_VALUE)
    if not agrees:
        fails.append("%s: ctl-xbe-text result.json step disagrees with peeks.jsonl" % label)
    return fails


def check_identity(accept, control):
    """A control from another build or an unverified save root is not a control."""
    fails = []
    metas = {}
    for label, run in (("acceptance", accept), ("control", control)):
        meta = run.get("metadata")
        if meta is None:
            fails.append("%s: no metadata.json" % label)
            continue
        metas[label] = meta
        if (meta.get("save_root") or {}).get("verified") is not True:
            fails.append("%s: save root not verified" % label)
    if len(metas) == 2:
        a, c = metas["acceptance"], metas["control"]
        if a.get("exe_sha256") != c.get("exe_sha256"):
            fails.append("acceptance and control executables differ (%s vs %s)"
                         % (a.get("exe_sha256"), c.get("exe_sha256")))
        if a.get("xbe_sha256") != c.get("xbe_sha256"):
            fails.append("acceptance and control XBE images differ (%s vs %s)"
                         % (a.get("xbe_sha256"), c.get("xbe_sha256")))
    return fails


def block_bounds(run, start_name, end_name):
    """(start, end) step indices of the first peeks named `start_name` and `end_name`."""
    start = end = None
    for i, s in enumerate(run["result"].get("steps", [])):
        if s.get("step") != "peek":
            continue
        name = (s.get("options") or {}).get("name")
        if name == start_name and start is None:
            start = i
        if name == end_name and end is None:
            end = i
    return start, end


def step_signature(step, witness_names):
    """(kind, options) with nothing normalized except a declared witness peek's `expect`."""
    options = dict(step.get("options") or {})
    if step.get("step") == "peek" and options.get("name") in witness_names:
        options.pop("expect", None)
    return step.get("step"), json.dumps(options, sort_keys=True)


def compare_steps(accept, control, *, start_name, end_name, action_vk, witness_names,
                  action_label, block_label):
    """The runs must perform the same steps, except that each acceptance action key inside
    the block pairs with a control sleep of the same duration."""
    fails = []
    a_steps = accept["result"].get("steps", [])
    c_steps = control["result"].get("steps", [])
    a_start, a_end = block_bounds(accept, start_name, end_name)
    c_start, c_end = block_bounds(control, start_name, end_name)
    for label, (s, e) in (("acceptance", (a_start, a_end)), ("control", (c_start, c_end))):
        if s is None or e is None or e <= s:
            fails.append("%s: no %s block (%s/%s peeks)" % (label, block_label, start_name, end_name))
    if fails:
        return fails

    # Outside the block, the boundary peeks included: identical signatures.
    outside_a = [step_signature(s, witness_names) for s in a_steps[:a_start + 1] + a_steps[a_end:]]
    outside_c = [step_signature(s, witness_names) for s in c_steps[:c_start + 1] + c_steps[c_end:]]
    if outside_a != outside_c:
        for i, (x, y) in enumerate(zip(outside_a, outside_c)):
            if x != y:
                fails.append("steps differ outside the %s block at %d: %s vs %s"
                             % (block_label, i, x, y))
                break
        else:
            fails.append("step counts differ outside the %s block: %d vs %d"
                         % (block_label, len(outside_a), len(outside_c)))

    a_block = a_steps[a_start + 1:a_end]
    c_block = c_steps[c_start + 1:c_end]
    if len(a_block) != len(c_block):
        fails.append("%s block lengths differ: %d vs %d" % (block_label, len(a_block), len(c_block)))
        return fails
    pairs = 0
    for x, y in zip(a_block, c_block):
        xo = x.get("options") or {}
        if x.get("step") == "key" and xo.get("vk") == action_vk:
            yo = y.get("options") or {}
            seconds, hold = yo.get("seconds"), xo.get("hold")
            if not (y.get("step") == "sleep" and isinstance(seconds, (int, float))
                    and isinstance(hold, (int, float)) and abs(seconds - hold) < 1e-6):
                fails.append("acceptance %s at step %s has no matching control sleep of the "
                             "same duration" % (action_label, x.get("n")))
            pairs += 1
            continue
        sx, sy = step_signature(x, witness_names), step_signature(y, witness_names)
        if sx != sy:
            fails.append("%s block step %s differs: %s vs %s" % (block_label, x.get("n"), sx, sy))
    if pairs == 0:
        fails.append("no %s inside the %s block: nothing distinguishes acceptance from control"
                     % (action_label, block_label))
    return fails
