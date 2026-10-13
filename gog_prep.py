"""Input files for the Gog, Pibiri & Venturini library (github.com/jermp/autocomplete, commit b3e9da7).
Writes <out>/<ds>.completions ("docid string", lexicographic order) with docid = rank of the string by
(score desc, lexicographic index asc), so that the library's docid order equals our ranking order, and
<out>/<ds>.doc2lex (lexicographic index of each docid). Then run, from the library's test_data directory:
  python3 extract_dict.py F; python3 map_dataset.py F; python3 build_stats.py F.mapped;
  python3 build_inverted_and_forward.py F        (F = <out>/<ds>.completions)"""
import sys
ds, out = sys.argv[1], sys.argv[2]
d = {}
for line in open(f'data/{ds}.tsv', 'rb'):
    w, s = line.rstrip(b'\n').split(b'\t'); d[w] = int(s)
ws = sorted(d)
order = sorted(range(len(ws)), key=lambda i: (-d[ws[i]], i))
docid = [0] * len(ws)
for r, i in enumerate(order): docid[i] = r
with open(f'{out}/{ds}.completions', 'wb') as f:
    for i, w in enumerate(ws): f.write(str(docid[i]).encode() + b' ' + w + b'\n')
with open(f'{out}/{ds}.doc2lex', 'w') as f:
    for i in order: f.write(f'{i}\n')
