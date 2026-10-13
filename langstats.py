"""Per-language properties of the word lists (Section 6.8, manuscript Table 10)."""
import unicodedata, statistics, csv
rows=[]
for ds in ['en','de','ru','tr','uz','all']:
    d={}; dup=0; nonnfc=0
    for line in open(f'data/{ds}.tsv', encoding='utf-8'):
        w,s=line.rstrip('\n').split('\t')
        if w in d: dup+=1
        d[w]=int(s)
        if unicodedata.normalize('NFC',w)!=w: nonnfc+=1
    ws=sorted(d, key=lambda x: x.encode())
    n=len(ws); bs=[x.encode() for x in ws]
    chars=sum(len(x) for x in ws)/n; byts=sum(len(b) for b in bs)/n
    lcp_tot=0
    for i in range(1,n):
        a,b=bs[i-1],bs[i]; l=0; m=min(len(a),len(b))
        while l<m and a[l]==b[l]: l+=1
        lcp_tot+=l
    lcp=lcp_tot/(n-1); share=100*lcp_tot/sum(len(b) for b in bs)
    sc=sorted(d.values(), reverse=True); one=sum(1 for v in sc if v<128)/n*100
    top1=sum(sc[:n//100])/sum(sc)*100
    rows.append(dict(dataset=ds,n=n,dups=dup,non_nfc=nonnfc,mean_chars=round(chars,2),mean_bytes=round(byts,2),bytes_per_char=round(byts/chars,2),mean_lcp_bytes=round(lcp,2),lcp_share_pct=round(share,1),scores_1byte_pct=round(one,1),top1pct_mass=round(top1,1)))
w=csv.DictWriter(open('results/langstats.csv','w'),fieldnames=rows[0].keys()); w.writeheader(); w.writerows(rows)
for r in rows: print(r)
