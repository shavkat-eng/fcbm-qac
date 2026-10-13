import sys
from wordfreq import top_n_list, word_frequency
langs = ['en','de','ru','tr']
union = {}
for l in langs:
    ws = top_n_list(l, 2000000)
    with open(f'data/{l}.tsv','w') as f:
        for w in ws:
            if '\t' in w or '\n' in w: continue
            s = max(1, int(round(word_frequency(w, l) * 1e9)))
            f.write(f'{w}\t{s}\n')
            union[w] = union.get(w, 0) + s
with open('data/all.tsv','w') as f:
    for w, s in union.items(): f.write(f'{w}\t{s}\n')
print(len(union))
