"""Order-preserving byte remapping for CoCo-trie, whose BIG_ALPHABET build accepts a byte span
(max - min + 1) of at most 96. Each byte value used in the word list is replaced by 0x21 + its rank
among the used values; lexicographic order and string lengths are unchanged."""
import sys
src, dst = sys.argv[1], sys.argv[2]
rows = [l.rstrip(b'\n').split(b'\t') for l in open(src, 'rb')]
used = sorted({c for w, _ in rows for c in w})
assert len(used) <= 95, len(used)
m = {c: 0x21 + i for i, c in enumerate(used)}
with open(dst, 'wb') as f:
    for w, s in rows: f.write(bytes(m[c] for c in w) + b'\t' + s + b'\n')
print(src, 'distinct bytes', len(used), 'span before', used[-1] - used[0] + 1, 'after', len(used))
