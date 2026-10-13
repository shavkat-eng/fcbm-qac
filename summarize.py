import pandas as pd, sys
pd.set_option('display.width',250)
f = sys.argv[1]
d=pd.read_csv(f)
info=pd.read_csv('results/datasets.csv').set_index('dataset')
d['MB']=d.bytes/1e6
d['Bps']=d.bytes/d.n
d['pct_raw']=d.apply(lambda r: 100*r.bytes/info.loc[r.dataset,'raw_bytes'],axis=1)
ks = [int(x) for x in sys.argv[2].split(',')] if len(sys.argv)>2 else [10]
print(d[d.k.isin(ks)][['dataset','method','param','k','MB','Bps','pct_raw','build_s','mean_us','p50_us','p95_us','p99_us','blocks_per_query']].round(3).to_string())
