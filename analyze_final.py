import pandas as pd, numpy as np, json
F='results/final/'
pd.set_option('display.width',250)
import os
UZ=os.path.exists(F+'uz_main_r1.csv')
m=pd.concat([pd.read_csv(F+f'{p}main_r{i}.csv').assign(run=i) for i in (1,2,3) for p in (['','uz_'] if UZ else [''])])
assert m.mismatches.sum()==0, 'mismatch!'
g=m.groupby(['dataset','method','param','k'],sort=False).agg(n=('n','first'),bytes=('bytes','first'),build=('build_s','median'),mean=('mean_us','median'),mn=('mean_us','min'),mx=('mean_us','max'),p99=('p99_us','median')).reset_index()
mar=pd.concat([pd.read_csv(F+f) for f in (['marisa_r1.csv','uz_marisa_r1.csv'] if UZ else ['marisa_r1.csv'])]); assert mar.mismatches.sum()==0
mar=mar.rename(columns={'build_s':'build','mean_us':'mean','p99_us':'p99'}); mar['mn']=mar['mean']; mar['mx']=mar['mean']
mar=mar[['dataset','method','param','k','n','bytes','build','mean','mn','mx','p99']]
gog=pd.concat([pd.read_csv(F+f'{p}gog_r{i}.csv') for i in (1,2,3) for p in (['','uz_'] if UZ else [''])]); assert gog.mismatches.sum()==0
gg=gog.groupby(['dataset','method','k'],sort=False).agg(n=('n','first'),heap=('heap_bytes','first'),pre=('prefix_bytes','first'),build=('build_s','median'),mean=('mean_us','median'),mn=('mean_us','min'),mx=('mean_us','max'),p99=('p99_us','median')).reset_index()
allm=pd.concat([g,mar])
info=pd.concat([pd.read_csv(F+f) for f in (['datasets.csv','uz_datasets.csv'] if UZ else ['datasets.csv'])]).drop_duplicates('dataset').set_index('dataset')
allm['Bps']=allm.bytes/allm.n; allm['pct']=allm.apply(lambda r:100*r.bytes/info.loc[r.dataset,'raw_bytes'],axis=1)
allm.to_csv(F+'all_agg.csv',index=False); gg.to_csv(F+'gog_agg.csv',index=False)
order=['Trie-DFS','Trie-Max','Trie-TopK','SA-RMQ','Marisa','FC-BM']
def name(r): return r.method+('' if r.param=='-' else f' ({r.param})')
lines=['| Dataset | Method | Bytes / string | Size (MB) | % of raw strings | Build (s) | Mean (µs) [min–max] | p99 (µs) |','| --- | --- | --- | --- | --- | --- | --- | --- |']
for ds in (['tr','uz','en','de','ru','all'] if UZ else ['tr','en','de','ru','all']):
    sub=allm[(allm.dataset==ds)&(allm.k==10)].copy(); sub['o']=sub.method.map(order.index); sub=sub.sort_values(['o','param'],key=lambda c: c if c.name=='o' else c.map(lambda p: {'-':0,'K=10':0,'B=8':1,'B=32':2}.get(p,0)))
    for _,r in sub.iterrows():
        rng='one run' if r.method=='Marisa' else f'{r.mn:.2f}–{r.mx:.2f}'
        lines.append(f'| {ds} | {name(r)} | {r.Bps:.2f} | {r.bytes/1e6:.2f} | {r.pct:.0f} | {r.build:.2f} | {r["mean"]:.2f} [{rng}] | {r.p99:.2f} |')
open(F+'table_main.md','w').write('\n'.join(lines))
# k table (all)
kt=['| Method | k = 1 | k = 5 | k = 10 | k = 20 | k = 50 |','| --- | --- | --- | --- | --- | --- |']
for meth,param,label in [('Trie-DFS','-','Trie-DFS'),('Trie-Max','-','Trie-Max'),('Trie-TopK','K=10','Trie-TopK (K=10)'),('SA-RMQ','-','SA-RMQ'),('Marisa','-','Marisa (one run)'),('GOG','','Gog et al. type 1 [13]'),('FC-BM','B=8','FC-BM (B=8)'),('FC-BM','B=32','FC-BM (B=32)')]:
    vals=[]
    for k in (1,5,10,20,50):
        if meth=='GOG': x=gg[(gg.dataset=='all')&(gg.method=='Gog-type1')&(gg.k==k)]['mean']
        else: x=allm[(allm.dataset=='all')&(allm.method==meth)&(allm.param==param)&(allm.k==k)]['mean']
        vals.append(f'{x.iloc[0]:.2f}' if len(x) else '—')
    kt.append(f'| {label} | '+' | '.join(vals)+' |')
open(F+'table_k.md','w').write('\n'.join(kt))
# Gog table
gt=['| Dataset | Gog type 1: prefix part (B/string) | Gog type 1: full heap (B/string) | FC-BM B=8 (B/string) | Gog type 1 mean (µs) | FC-BM B=8 mean (µs) |','| --- | --- | --- | --- | --- | --- |']
for ds in (['tr','uz','en','de','ru','all'] if UZ else ['tr','en','de','ru','all']):
    a=gg[(gg.dataset==ds)&(gg.method=='Gog-type1')&(gg.k==10)].iloc[0]; f=allm[(allm.dataset==ds)&(allm.method=='FC-BM')&(allm.param=='B=8')&(allm.k==10)].iloc[0]
    gt.append(f'| {ds} | {a.pre/a.n:.2f} | {a.heap/a.n:.2f} | {f.Bps:.2f} | {a["mean"]:.2f} | {f["mean"]:.2f} |')
open(F+'table_gog.md','w').write('\n'.join(gt))
print('\n'.join(lines)); print('\n'.join(kt)); print('\n'.join(gt))
t2=gg[(gg.method=='Gog-type2')&(gg.k==10)]; print(t2[['dataset','pre','n','mean']].assign(Bps=t2.pre/t2.n))
# bylen (input of Figure 2: python3 figs.py)
by=pd.concat([pd.read_csv(F+f'main_bylen_r{i}.csv') for i in (1,2,3)]); mb=pd.read_csv(F+'marisa_bylen_r1.csv')
b=by[by.dataset=='all'].groupby(['method','param','prefix_chars']).mean_us.median().round(2)
print(b.unstack(2)); print(mb[mb.dataset=='all'][['prefix_chars','mean_us']].round(2).T)
