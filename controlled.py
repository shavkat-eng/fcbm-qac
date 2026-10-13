"""Matched-size comparison of the five languages (Section 6.8, manuscript Table 11).
For each language the n0 = 63,261 highest-scoring words are kept (ties broken by byte order), n0 being the
size of the smallest list (tr). Reported per list: mean UTF-8 bytes, share of bytes in prefixes shared with
the predecessor, FC-BM B=32 string part (headers, lcp/length codes, suffixes) as % of raw bytes, VByte score
bytes per string with the original scores, and the whole FC-BM B=32 index as % of raw bytes when every list
uses the same rank-based scores (score = n0 - rank), which removes differences in score distribution."""
def vb(x):
    n = 1
    while x >= 128: x >>= 7; n += 1
    return n
N0 = 63261; B = 32
print('dataset,n,mean_bytes,shared_prefix_pct,fc_string_pct_raw,score_bytes_orig,score_bytes_rank,fcbm32_pct_raw_orig,fcbm32_pct_raw_rank')
for ds in ['tr', 'uz', 'en', 'de', 'ru']:
    rows = [l.rstrip(b'\n').split(b'\t') for l in open(f'data/{ds}.tsv', 'rb')]
    rows = [(w, int(s)) for w, s in rows]
    rows.sort(key=lambda r: (-r[1], r[0])); top = rows[:N0]
    rank = {w: N0 - i for i, (w, _) in enumerate(top)}
    top.sort(key=lambda r: r[0]); ws = [w for w, _ in top]; sc = [s for _, s in top]
    raw = sum(map(len, ws)); shared = 0; fcs = 0
    for i, w in enumerate(ws):
        if i % B == 0: fcs += vb(len(w)) + len(w)
        if i:
            p = ws[i - 1]; l = 0
            while l < len(p) and l < len(w) and p[l] == w[l]: l += 1
            shared += l
            if i % B: fcs += vb(l) + vb(len(w) - l) + len(w) - l
    nb = (N0 + B - 1) // B; S = 1
    while S < nb: S *= 2
    nav = 4 * (nb + 1) + 8 * S
    so = sum(vb(s) for s in sc); sr = sum(vb(rank[w]) for w in ws)
    print(f"{ds},{N0},{raw/N0:.2f},{100*shared/raw:.1f},{100*fcs/raw:.1f},{so/N0:.2f},{sr/N0:.2f},{100*(fcs+so+nav)/raw:.0f},{100*(fcs+sr+nav)/raw:.0f}")
