"""Compare two directory trees by relative path and SHA-256; print counts, sizes and differences."""
import hashlib, os, sys
def walk(root):
    out = {}
    for d, _, files in os.walk(root):
        for f in files:
            p = os.path.join(d, f)
            h = hashlib.sha256()
            with open(p, 'rb') as fh:
                for c in iter(lambda: fh.read(1 << 20), b''):
                    h.update(c)
            out[os.path.relpath(p, root)] = (os.path.getsize(p), h.hexdigest())
    return out
a, b = walk(sys.argv[1]), walk(sys.argv[2])
for name, t in (('A', a), ('B', b)):
    print(f'{name}: {len(t)} files, {sum(s for s, _ in t.values())} bytes')
only_a = sorted(set(a) - set(b)); only_b = sorted(set(b) - set(a))
diff = sorted(k for k in set(a) & set(b) if a[k] != b[k])
print('only in A:', only_a); print('only in B:', only_b); print('differ:', diff)
print('IDENTICAL' if not (only_a or only_b or diff) else 'DIFFERENT')
