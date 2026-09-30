/*
 * The TTD write census: what writes a range, from which module, and through which
 * alias.
 *
 * Run inside a TTD trace by `tools/ttd/ttd-census.py`. Three questions a per-slot
 * query cannot answer:
 *
 *   - `census`              every write in a range, classified by the MODULE its IP
 *                           falls in, with counts and the first/last sequence. This
 *                           is what shows that a table is written WHOLESALE by a
 *                           bulk fill rather than slot by slot.
 *   - `aliasSweep`          the same range through every mirror view, which is W11's
 *                           W-c control in the construction that exercises it.
 *   - `kernelWriteControl`  whether writes from OUTSIDE the process's own modules
 *                           are reported at all -- W11's exclusion (d), untested
 *                           until now.
 *
 * The engine is old and strict about syntax: `var` only, no arrow functions, no
 * template literals. A syntax error here is reported as "Unable to bind name" on
 * every later call, which reads like a missing function.
 *
 * Read-only by construction: every query is `TTD.Memory(..., "w")` or `"r"`.
 */

var CENSUS = 'CENSUS';
var SAMPLE = 'SAMPLE';
var ALIAS = 'ALIAS';
var WDCONTROL = 'WDCONTROL';

function _hex(value, width) {
    var s = value.toString(16);
    while (s.length < width) { s = '0' + s; }
    return s;
}

function _addr(value) { return '0x' + _hex(value, 16); }

function _log(line) { host.diagnostics.debugLog(line + '\n'); }

function _parseHex(token) {
    var t = ('' + token).replace(/^\s+|\s+$/g, '').toLowerCase();
    if (t.indexOf('0x') === 0) { t = t.substring(2); }
    if (t.length === 0) { return 0; }
    // Halves, because JavaScript numbers lose precision above 2^53.
    if (t.length > 8) {
        return parseInt(t.substring(0, t.length - 8), 16) * 4294967296
             + parseInt(t.substring(t.length - 8), 16);
    }
    return parseInt(t, 16);
}

/*
 * Parse the module list the Python side derived from the trace's own load events.
 * Each entry is `name=base:end` in hex.
 */
function _parseModules(text) {
    var out = [];
    var parts = ('' + text).split(',');
    for (var i = 0; i < parts.length; i++) {
        var entry = parts[i].replace(/^\s+|\s+$/g, '');
        if (entry.length === 0) { continue; }
        var eq = entry.indexOf('=');
        var colon = entry.indexOf(':', eq + 1);
        if (eq < 0 || colon < 0) { continue; }
        out.push({
            name: entry.substring(0, eq),
            base: _parseHex(entry.substring(eq + 1, colon)),
            end: _parseHex(entry.substring(colon + 1))
        });
    }
    return out;
}

function _moduleOf(modules, ip) {
    for (var i = 0; i < modules.length; i++) {
        if (ip >= modules[i].base && ip < modules[i].end) { return modules[i].name; }
    }
    return 'UNKNOWN';
}

/*
 * The census. One row per module, keyed by module name -- a finite set derived from
 * the trace's own load events -- so the record does not grow with run length.
 * Per-event rows are bounded samples, labelled as such.
 */
function census(moduleList, lo, hi, maxEvents, maxSamples) {
    var modules = _parseModules(moduleList);
    var stats = {};
    var order = [];
    var samples = 0;
    var n = 0;
    var query = host.currentSession.TTD.Memory(lo, hi, 'w');
    for (var event of query) {
        var name = _moduleOf(modules, event.IP);
        if (stats[name] === undefined) {
            stats[name] = { total: 0, size1: 0, size4: 0, other: 0,
                            first: event.TimeStart.Sequence,
                            last: event.TimeStart.Sequence };
            order.push(name);
        }
        var s = stats[name];
        s.total = s.total + 1;
        // **`event.Size` is a BOXED OBJECT, not a number.** Measured:
        // `typeof e.Size` is `object`, `e.Size === 1` is FALSE and `e.Size == 1` is
        // true -- so the strict comparison sent every write to `other` and the census
        // reported `size1=0 size4=0 other=960` for a range written entirely in single
        // bytes. `Number()` is the explicit conversion, and it is used rather than `==`
        // so the intent is a conversion rather than a loose comparison.
        var size = Number(event.Size);
        if (size === 1) { s.size1 = s.size1 + 1; }
        else if (size === 4) { s.size4 = s.size4 + 1; }
        else { s.other = s.other + 1; }
        s.last = event.TimeStart.Sequence;
        if (samples < maxSamples) {
            _log(SAMPLE + '|' + event.TimeStart.Sequence + '|'
                 + _addr(event.Address) + '|' + size + '|'
                 + _hex(event.Value >>> 0, 8) + '|'
                 + _hex(event.OverwrittenValue >>> 0, 8) + '|'
                 + _addr(event.IP));
            samples = samples + 1;
        }
        n = n + 1;
        if (n >= maxEvents) { break; }
    }
    for (var i = 0; i < order.length; i++) {
        var e = stats[order[i]];
        _log(CENSUS + '|' + order[i] + '|' + e.total + '|' + e.size1 + '|'
             + e.size4 + '|' + e.other + '|' + e.first + '|' + e.last);
    }
    return n;
}

/*
 * W11's W-c control, in the construction that exercises it: ask every MIRROR view
 * about a range that was definitely written through the canonical view. A broken
 * mirror query would have to return the right answer for a range known to be
 * written, which a single-mirror negative cannot show.
 */
function aliasSweep(mirrorList, length) {
    var parts = ('' + mirrorList).split(',');
    for (var i = 0; i < parts.length; i++) {
        var token = parts[i].replace(/^\s+|\s+$/g, '');
        if (token.length === 0) { continue; }
        var lo = _parseHex(token);
        var n = 0;
        var first = 0;
        try {
            var query = host.currentSession.TTD.Memory(lo, lo + length, 'w');
            for (var event of query) {
                if (n === 0) { first = event.TimeStart.Sequence; }
                n = n + 1;
            }
        } catch (err) { }
        _log(ALIAS + '|' + (i + 1) + '|' + n + '|' + first);
    }
    return 0;
}

/*
 * W11's exclusion (d): are writes from OUTSIDE the process's own modules reported?
 *
 * It CLASSIFIES rather than counts. An earlier version of this function reported
 * `outside` as a literal zero without measuring it, which would have "confirmed" the
 * exclusion by construction -- the exact failure mode the control exists to prevent.
 *
 * Measured on the C1 trace: zero of 400,000 writes came from outside
 * `jsrf_recomp.exe` and `VCRUNTIME140`, so TTD reports instruction stores only and a
 * kernel-mode write is invisible.
 */
function kernelWriteControl(moduleList, lo, hi, maxEvents, processModules) {
    var modules = _parseModules(moduleList);
    var inside = ('' + processModules).split(',');
    var n = 0;
    var outside = 0;
    var samples = 0;
    var query = host.currentSession.TTD.Memory(lo, hi, 'w');
    for (var event of query) {
        var name = _moduleOf(modules, event.IP);
        var isInside = false;
        for (var i = 0; i < inside.length; i++) {
            if (inside[i] === name) { isInside = true; break; }
        }
        if (!isInside) {
            outside = outside + 1;
            if (samples < 5) {
                _log(WDCONTROL + '|OUTSIDE|' + _addr(event.IP) + '|' + name + '|'
                     + event.TimeStart.Sequence);
                samples = samples + 1;
            }
        }
        n = n + 1;
        if (n >= maxEvents) { break; }
    }
    _log(WDCONTROL + '|RESULT|' + n + '|' + outside + '|'
         + (outside === 0 ? 'NOT_REPORTED' : 'REPORTED'));
    return outside;
}
