"""FC-BM footprint by component (Section 4.3, manuscript Table 2; string parts of Table 9)."""
import sys
from os.path import commonprefix
def vb(x):
    n=1
    while x>=128: x>>=7; n+=1
    return n
import time
# usage: python3 breakdown.py [dataset ...]   (default: all seven; progress goes to stderr)
for ds in (sys.argv[1:] or ['tr','uz','en','de','ru','all','en_ascii']):
    t0=time.time()
    rows=[]
    for line in open(f'data/{ds}.tsv','rb'):
        w,s=line.rstrip(b'\n').split(b'\t'); rows.append((w,int(s)))
    rows.sort(); 
    d={}
    for i,(w,s) in enumerate(rows):
        if i==0 or w!=rows[i-1][0]: d[w]=s
    ws=sorted(d); sc=[d[w] for w in ws]; n=len(ws)
    for B in (8,32):
        hdr=scores=meta=suf=0
        for i,w in enumerate(ws):
            scores+=vb(sc[i])
            if i%B==0: hdr+=vb(len(w))+len(w)
            else:
                l=len(commonprefix((ws[i-1],w)))      # byte-wise longest common prefix
                meta+=vb(l)+vb(len(w)-l); suf+=len(w)-l
        nb=(n+B-1)//B; S=1
        while S<nb: S*=2
        off=4*(nb+1); tree=8*S
        tot=hdr+scores+meta+suf+off+tree
        print(f"{ds},{B},{n},{hdr},{scores},{meta},{suf},{off},{tree},{tot},{tot/n:.3f},{scores/n:.3f},{(hdr+meta+suf)/n:.3f}")
    print(f"{ds}: done in {time.time()-t0:.1f} s", file=sys.stderr)
