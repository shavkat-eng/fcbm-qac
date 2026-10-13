# Data dictionary (Supporting Information S2)

All text files are UTF-8, one record per line, lines end with `\n`. Indices are **0-based**.

| File | Records | Format |
|---|---|---|
| `data/<ds>.tsv.gz` (`ds` = tr, uz, en, de, ru, all, en_ascii) | one per word | `word<TAB>score`. `word`: UTF-8 string in the normalised, case-folded form supplied by wordfreq (tr, en, de, ru, all, en_ascii) or produced by `export_uz.py` (uz, NFC). wordfreq output is not always NFC: 6 entries of `en` (and therefore of `all`) and 3 of `de` are not in NFC and are kept as supplied. No TAB or newline. `score`: integer frequency per 10^9 running tokens, rounded, minimum 1. |
| `data/<ds>.queries.gz` (`ds` = tr, uz, en, de, ru, all) | 100,000 | one query prefix per line (raw UTF-8 bytes, cut at a code-point boundary, 1–8 code points). |
| `data/<ds>.ref50.xz` | 100,000 (line i answers query i) | up to 50 space-separated integers: indices of the answers in the dictionary sorted by byte order (0-based), ranked by score descending and, for equal scores, index ascending. Fewer than 50 numbers when the prefix has fewer completions; an empty line when it has none. |
| `MANIFEST.sha256` | 19 | SHA-256 of the **uncompressed** `.tsv`, `.queries` and `.ref50` files. |

**Dictionaries.** `tr`, `en`, `de`, `ru`: wordfreq 3.1.1 `top_n_list(lang, 2000000)` with `word_frequency`, normalised by wordfreq (NFC/NFKC, language-aware case folding). `all`: union of these four, scores summed for shared words. `en_ascii`: the `en` words made only of printable ASCII (used for CoCo-trie). `uz`: Leipzig Corpora Collection `uzb_wikipedia_2021_100K`, words file; NFC, the apostrophe characters U+0027/0060/00B4/02B9/02BB/02BC/2018/2019 become U+02BB directly after o/O/g/G (also when the apostrophe ends the token, e.g. `so'` → `soʻ`), U+02BC elsewhere inside a token, and are dropped at the start of a token and at its end when not preceded by o/O/g/G; case-folded; Latin letters only (see `export_uz.py`).

**Queries.** For each dataset, `./qac dump <ds>` draws 100,000 words with probability proportional to score (`std::mt19937_64`, seed 20261009, `std::discrete_distribution`) and a prefix length uniform in 1..min(8, code points). The reference answers come from the exhaustive Trie-DFS and were checked independently by `oracle.py`.

**Sorting.** Dictionary order is plain byte order of the UTF-8 strings (equal to code-point order). Duplicate words do not occur.

**Licences.** See `DATA_LICENSE.md`.
