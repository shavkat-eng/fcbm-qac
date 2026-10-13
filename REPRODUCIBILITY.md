# Reproducibility record

This file separates what was re-run from scratch from what relies on archived raw measurements.

## Fresh-copy check (author, 2026-10-11)
Environment: Linux x86-64 cloud VM (Intel Xeon 2.80 GHz, 2 vCPUs), g++ 13.3.0, cmake 3.28.3, Python 3.13.16 in a new
virtual environment with `requirements.txt`; external libraries at the commits in `results/setup.log`.

| Stage | What it does | Result | Time |
|---|---|---|---|
| `setup` | clone + build Marisa-trie, Gog et al. autocomplete (patched), CoCo-trie; build our harnesses | exit 0 | 198 s |
| `data` (run 1) | rebuild the five wordfreq lists and en-ascii with wordfreq 3.1.1; **Uzbek list restored** from the released `data/uz.tsv.gz` (LEIPZIG_UZ not set); regenerate 600,000 queries, 600,000 reference top-50 answers, Gog input files | exit 0; 19/19 files match `MANIFEST.sha256` | 339 s |
| `data` (run 2) | as run 1, but the **Uzbek list rebuilt from the original Leipzig file** (`LEIPZIG_UZ=…/uzb_wikipedia_2021_100K-words.txt`, SHA-256 d4ab528a…0e4644) | exit 0; 19/19 files match `MANIFEST.sha256`; route recorded in `results/data_provenance.txt` | 333 s |
| `verify` | hash check; sanitizer test harness; independent Python oracle | exit 0; 3,211,880 checks, 0 failures; oracle 600,000 queries, 0 mismatches | 66 s |
| `tables` | derived tables and figures from the raw measurements in `results/` | exit 0; tables, `ci_table_s.csv`, `controlled.csv`, `fig_data.csv` byte-identical to the archived ones | 40 s |
| `bench` | all timing measurements | **not re-run in this check** (several hours); the archived raw CSV files in `results/` come from the runs described in the paper (machine: `results/final/machine.txt`) | — |

Log: `results/reproduce_check.log` (run 1) and `results/data_provenance.txt` (run 2). Each `data` run writes which route it used for the Uzbek list to `results/data_provenance.txt`. The `tables` stage was also run under Python 3.12.3 with identical outputs.
PNG/PDF figure bytes can differ between environments because of fonts; the plotted values (`results/fig_data.csv`) do not.

## What depends on archived raw measurements
`tables` never measures anything. Tables 5–9, 12, the figures and the blocks-scanned statistics are computed from the
raw CSV/TXT files in `results/` and `results/final/`. To re-measure them, run `setup`, `data` and `bench` first;
absolute latencies will then reflect the new machine.

## Independent checks
- `oracle.py`: Python, no shared code with the C++ implementation; recomputes all 600,000 top-50 answers.
- `MANIFEST.sha256`: SHA-256 of the 19 data files (7 dictionaries, 6 query sets, 6 reference-answer sets).
- `requirements-lock.txt`: complete Python environment with wheel hashes (Linux x86-64, Python 3.13).
