"""Independent brute-force oracle, written separately from the C++ code (Python standard library only).
For every benchmark query of a dataset it computes the top-50 completions by sorting the completion range
by (score desc, lexicographic index asc) and compares them with data/<ds>.ref50, the reference answers
written by the C++ harness (`./qac dump`), against which every method in the paper was checked."""
import bisect, heapq, sys, time
def run(ds):
    t = time.time(); d = {}
    for line in open(f'data/{ds}.tsv', 'rb'):
        w, s = line.rstrip(b'\n').split(b'\t'); d[w] = int(s)   # duplicate strings: last wins, as in qac.cpp load()
    ws = sorted(d); sc = [d[w] for w in ws]
    qs = open(f'data/{ds}.queries', 'rb').read().split(b'\n')[:-1]
    refs = open(f'data/{ds}.ref50').read().split('\n')[:-1]
    assert len(qs) == len(refs), (len(qs), len(refs))
    cache = {}; bad = 0
    for q, ref in zip(qs, refs):
        if q not in cache:
            L = bisect.bisect_left(ws, q)
            R = L
            # upper end of the prefix range: first string not starting with q
            lo, hi = L, len(ws)
            while lo < hi:
                mid = (lo + hi) // 2
                if ws[mid][:len(q)] <= q: lo = mid + 1
                else: hi = mid
            R = lo
            cache[q] = ' '.join(map(str, heapq.nsmallest(50, range(L, R), key=lambda i: (-sc[i], i))))
        if cache[q] != ref: bad += 1
    print(f'{ds}: queries={len(qs)} distinct_prefixes={len(cache)} mismatches={bad} time={time.time()-t:.0f}s', flush=True)
    return bad
if __name__ == '__main__':
    sys.exit(1 if sum(run(ds) for ds in (sys.argv[1:] or ['tr', 'uz', 'en', 'de', 'ru', 'all'])) else 0)
