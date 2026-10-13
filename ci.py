"""Ten-run latency statistics of Section 6.9 (manuscript Table 12; called Table S in the draft).

Input : results/stat.csv     (10 interleaved runs per dataset: SA-RMQ, FC-BM B=8, FC-BM B=32;
                              columns bytes/build_s/mean_us hold the three per-run mean latencies)
        results/gog_stat.csv (10 runs per dataset of the Gog et al. library, separate process)
        results/final/uz_stat.csv, results/final/uz_gog_stat.csv (Uzbek)
Method: the resampling unit is a run (its mean latency over all 100,000 queries).
        Percentile bootstrap, 10,000 replicates, seed 20261010.
        Median latency: resample the 10 runs with replacement, take the median.
        FC-BM/SA-RMQ ("paired"): both timed in the same run, so whole runs are resampled and the
        median of the per-run ratios FC-BM/SA-RMQ is taken.
        FC-BM/Gog ("independent"): the two sets of runs are resampled independently.
        The CIs are descriptive; no correction for multiple comparisons is applied.
"""
import csv, random, statistics as st, sys
R = 10000; random.seed(20261010)
def rows(f): return list(csv.DictReader(open(f)))
fc = {}; gog = {}
for f in ['results/stat.csv', 'results/final/uz_stat.csv']:
    for r in rows(f):
        fc.setdefault(r['dataset'], []).append((float(r['bytes']), float(r['build_s']), float(r['mean_us'])))
for f in ['results/gog_stat.csv', 'results/final/uz_gog_stat.csv']:
    for r in rows(f):
        if r['method'] == 'Gog-type1': gog.setdefault(r['dataset'], []).append(float(r['mean_us']))
def ci(v): v = sorted(v); return v[int(0.025 * len(v))], v[int(0.975 * len(v)) - 1]
def cv(x): return 100 * st.stdev(x) / st.mean(x)
out = csv.writer(sys.stdout)
out.writerow(['dataset', 'SA-RMQ', 'Gog', 'FC-BM8', 'FC-BM32', 'FC8/SA', 'FC8/Gog', 'cv_sa%', 'cv_f8%', 'cv_f32%', 'cv_gog%'])
for ds in ['tr', 'uz', 'en', 'de', 'ru', 'all']:
    runs = fc[ds]; g = gog[ds]; n = len(runs)
    assert n == 10 and len(g) == 10, (ds, n, len(g))
    bs = {k: [] for k in ['sa', 'f8', 'f32', 'g', 'rs', 'rg']}
    for _ in range(R):
        s = [runs[random.randrange(n)] for _ in range(n)]
        gg = [g[random.randrange(n)] for _ in range(n)]
        sa = st.median(x[0] for x in s); f8 = st.median(x[1] for x in s); f32 = st.median(x[2] for x in s); gm = st.median(gg)
        rs = st.median(x[1] / x[0] for x in s)
        for k, v in zip(bs, [sa, f8, f32, gm, rs, f8 / gm]): bs[k].append(v)
    med = [st.median(x[0] for x in runs), st.median(x[1] for x in runs), st.median(x[2] for x in runs), st.median(g)]
    fmt = lambda m, k, d=2: f"{m:.{d}f} [{ci(bs[k])[0]:.{d}f}, {ci(bs[k])[1]:.{d}f}]"
    out.writerow([ds, fmt(med[0], 'sa'), fmt(med[3], 'g'), fmt(med[1], 'f8'), fmt(med[2], 'f32'),
                  fmt(st.median(x[1] / x[0] for x in runs), 'rs'), fmt(med[1] / med[3], 'rg'),
                  f"{cv([x[0] for x in runs]):.1f}", f"{cv([x[1] for x in runs]):.1f}", f"{cv([x[2] for x in runs]):.1f}", f"{cv(g):.1f}"])
