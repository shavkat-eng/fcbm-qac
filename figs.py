import matplotlib
# Deterministic output: no timestamps or version strings in the files; byte identity still depends on the
# matplotlib version and the fonts available (requirements.txt pins matplotlib).
matplotlib.use('Agg')
import matplotlib.pyplot as plt
plt.rcParams.update({'font.family':'serif','font.size':9,'axes.spines.top':False,'axes.spines.right':False,'pdf.fonttype':42})
blue='#1f5fbf'; grey='#8a8a8a'
# Fig 1: space-latency, all dataset, k=10
# All values are read from the result files (run `python3 analyze_final.py` first).
import pandas as pd
F='results/final/'
ab=pd.read_csv('results/ablation2.csv'); ab=ab[(ab.dataset=='all')&(ab.method=='FC-BM')&(ab.k==10)]
fc=[(int(r.param[2:]),r.bytes/r.n,r.mean_us) for r in ab.itertuples()]
agg=pd.read_csv(F+'all_agg.csv'); agg=agg[(agg.dataset=='all')&(agg.k==10)]
def pick(m,p): r=agg[(agg.method==m)&(agg.param==p)].iloc[0]; return r.Bps, r['mean']
gg=pd.read_csv(F+'gog_agg.csv'); g1=gg[(gg.dataset=='all')&(gg.method=='Gog-type1')&(gg.k==10)].iloc[0]
base=[('SA-RMQ',*pick('SA-RMQ','-')),('Trie-Max',*pick('Trie-Max','-')),('Trie-TopK',*pick('Trie-TopK','K=10')),('Trie-DFS',*pick('Trie-DFS','-')),('Marisa',*pick('Marisa','-')),('Gog et al., prefix part',g1.pre/g1.n,g1['mean'])]
info=pd.read_csv(F+'datasets.csv').set_index('dataset'); RAW=info.loc['all','raw_bytes']/info.loc['all','n']
print('fig1 FC-BM',[(b,round(x,3),round(y,2)) for b,x,y in fc]); print('fig1 base',[(n,round(x,3),round(y,2)) for n,x,y in base])
fig,ax=plt.subplots(figsize=(6.2,3.6))
ax.set_xscale('log'); ax.set_yscale('log')
ax.plot([x for _,x,_ in fc],[y for *_,y in fc],'-o',color=blue,ms=4,lw=1.6,label='FC-BM (B = 4 … 256)')
for b,x,y in fc:
    if x<7.5: ax.annotate(f'B={b}',(x,y),xytext=(-5,0),textcoords='offset points',ha='right',va='center',fontsize=7.5)
    else: ax.annotate(f'B={b}',(x,y),xytext=(0,-11),textcoords='offset points',ha='center',fontsize=7.5)
for n,x,y in base:
    ax.plot(x,y,'o',color=grey,ms=4.5)
    if n.startswith('Gog'): ax.annotate(n,(x,y),xytext=(-4,9),textcoords='offset points',ha='left',va='bottom',fontsize=7.5,color='#444')
    elif n=='SA-RMQ': ax.annotate(n,(x,y),xytext=(0,-10),textcoords='offset points',ha='center',va='top',fontsize=7.5,color='#444')
    else: ax.annotate(n,(x,y),xytext=(5,0),textcoords='offset points',va='center',fontsize=7.5,color='#444')
ax.axvline(RAW,ls='--',lw=0.8,color='#999'); ax.text(RAW*1.03,1.15,f'raw strings\nalone ({RAW:.1f} B)',fontsize=7,color='#666',va='bottom')
ax.set_xlim(4.3,110); ax.set_ylim(1,1500)
ax.set_xticks([5,10,20,50,100]); ax.set_xticklabels(['5','10','20','50','100'])
ax.set_yticks([1,3,10,30,100,300,1000]); ax.set_yticklabels(['1','3','10','30','100','300','1000'])
ax.set_xlabel('Index size (bytes per string, log scale)'); ax.set_ylabel('Mean latency at k = 10 (µs, log scale)')
ax.grid(True,which='major',lw=0.4,color='#e3e3e3'); ax.legend(frameon=False,fontsize=8,loc='upper right')
fig.tight_layout(); fig.savefig('results/fig1.pdf',metadata={'CreationDate':None,'Creator':None,'Producer':None}); fig.savefig('results/fig1.png',dpi=300,metadata={'Software':None})
# Fig 2: prefix length
L=list(range(1,9))
by=pd.concat([pd.read_csv(F+f'main_bylen_r{i}.csv') for i in (1,2,3)]); by=by[by.dataset=='all']
med=by.groupby(['method','param','prefix_chars']).mean_us.median()
S={lab:[float(med[(m,p,l)]) for l in L] for lab,m,p in [('FC-BM (B=8)','FC-BM','B=8'),('SA-RMQ','SA-RMQ','-'),('Trie-Max','Trie-Max','-'),('Trie-TopK','Trie-TopK','K=10')]}
print('fig2',{k:[round(x,2) for x in v] for k,v in S.items()})
fig,ax=plt.subplots(figsize=(6.2,3.2))
sty={'FC-BM (B=8)':dict(color=blue,lw=2),'SA-RMQ':dict(color='#555',lw=1.4),'Trie-Max':dict(color='#aaa',lw=1.2),'Trie-TopK':dict(color='#aaa',lw=1.2,ls='--')}
for k,v in S.items():
    ax.plot(L,v,marker='o',ms=3,**sty[k]); ax.annotate(f'{k}',(8,v[-1]),xytext=(6,0),textcoords='offset points',va='center',fontsize=7.5,color=sty[k]['color'])
ax.set_xlim(0.8,9.6); ax.set_ylim(0,14); ax.set_xticks(L)
ax.set_xlabel('Prefix length (characters)'); ax.set_ylabel('Mean latency at k = 10 (µs)')
ax.grid(True,axis='y',lw=0.4,color='#e3e3e3')
fig.tight_layout(); fig.savefig('results/fig2.pdf',metadata={'CreationDate':None,'Creator':None,'Producer':None}); fig.savefig('results/fig2.png',dpi=300,metadata={'Software':None})

# plotted values, for a numerical (not byte-wise) comparison of the figures
with open('results/fig_data.csv', 'w') as f:
    f.write('figure,series,x,y\n')
    for b, x, y in fc: f.write(f'1,FC-BM B={b},{x:.4f},{y:.4f}\n')
    for n, x, y in base: f.write(f'1,{n},{x:.4f},{y:.4f}\n')
    for k, v in S.items():
        for l, y in zip(L, v): f.write(f'2,{k},{l},{y:.4f}\n')
