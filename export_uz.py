"""Uzbek scored word list from the Leipzig Corpora Collection (uzb_wikipedia_2021_100K, words file).
Steps: NFC; apostrophe variants -> U+02BB after o/g (oʻ, gʻ) and U+02BC elsewhere (tutuq belgisi);
casefold; keep tokens made only of Latin letters and the two modifier letters; merge; score per 10^9 tokens."""
import sys, unicodedata, collections
src, dst = sys.argv[1], sys.argv[2]
APOS = set("‘’'`ʼʻ´ʹ")
def norm(w):
    w = unicodedata.normalize('NFC', w)
    out = []
    for i, ch in enumerate(w):
        if ch in APOS:
            if 0 < i < len(w) - 1 or (i == len(w) - 1 and i > 0 and w[i-1] in 'oOgG'):
                out.append('ʻ' if w[i-1] in 'oOgG' else 'ʼ')
            # leading or trailing quote marks are punctuation: drop
        else:
            out.append(ch)
    return ''.join(out).casefold()
def latin_word(w):
    if not w: return False
    for ch in w:
        if ch in 'ʻʼ': continue
        if not ch.isalpha() or 'LATIN' not in unicodedata.name(ch, ''): return False
    return w[0] not in 'ʻʼ'
total = 0; counts = collections.Counter(); dropped = collections.Counter(); variants = collections.Counter()
for line in open(src, encoding='utf-8'):
    p = line.rstrip('\n').split('\t')
    if len(p) < 3: continue
    tok, c = p[1], int(p[2]); total += c
    if any(ch in APOS for ch in tok): variants[next(ch for ch in tok if ch in APOS)] += c
    w = norm(tok)
    if latin_word(w): counts[w] += c
    else:
        kind = 'cyrillic' if any('CYRILLIC' in unicodedata.name(ch, '') for ch in w) else ('digit/punct/other')
        dropped[kind] += c
with open(dst, 'w', encoding='utf-8') as f:
    for w, c in counts.items(): f.write(f"{w}\t{max(1, round(c / total * 1e9))}\n")
kept = sum(counts.values())
print(f"running tokens={total} kept tokens={kept} ({100*kept/total:.1f}%) types={len(counts)}")
print('dropped:', dict(dropped)); print('apostrophe variants (token counts):', {hex(ord(k)): v for k, v in variants.items()})
