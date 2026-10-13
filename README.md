# FC-BM: Front-Coded Block-Max top-k query auto-completion — reproducibility package

Paper: S. Abdunasimov, "Space- and Time-Efficient Top-k Query Auto-Completion with Front-Coded Block-Max Indexes"
(prepared for submission to *Data Intelligence*). Code: MIT licence (LICENSE). Data: see "Data and licences".

## Package layout
The journal submission splits this repository into three Supporting Information files: **S1** code and scripts,
**S2** data (`data/`, `MANIFEST.sha256`, `DATA_LICENSE.md`), **S3** raw measurements and derived outputs (`results/`).
Unzip all three into one folder to obtain the complete repository, then run `bash reproduce.sh layout` first: it checks every released file against `RELEASE_MANIFEST.sha256` (in S1), so packages from different releases cannot be mixed unnoticed. The SHA-256 of the three zip files themselves are published with the release (`SHA256SUMS.txt`). Data formats are described in `DATA_README.md` (in S2).

**What reproduction means here.** The scripts recompute every data file, answer and table exactly (byte-identical, checked by hashes). Latencies are re-measured, not reproduced byte for byte: they depend on the CPU, caches, compiler flags (`-march=native`) and allocator, so expect different absolute times and similar ratios.

## Uzbek list: rebuild or restore
- **Rebuild from source:** download `uzb_wikipedia_2021_100K` from the Leipzig Corpora Collection and run
  `LEIPZIG_UZ=/path/to/uzb_wikipedia_2021_100K-words.txt bash reproduce.sh data`.
- **Restore archived data:** without `LEIPZIG_UZ`, `data/uz.tsv.gz` is unpacked. In both cases the result is checked against
  `MANIFEST.sha256`, and the route used is written to `results/data_provenance.txt`. Both routes give byte-identical files.

## What each stage checks
| Stage | Checks | Does not do |
|---|---|---|
| `layout` | every released file of S1–S3 against `RELEASE_MANIFEST.sha256` (same release, nothing missing or altered) | build, run or measure anything |
| `setup` | external libraries at the exact commits; builds everything | run tests |
| `data` | rebuilds all data files and checks the 19 hashes of `MANIFEST.sha256` | measure anything |
| `verify` | data hashes; C++ sanitizer test harness; independent Python oracle on all 600,000 queries | measure time |
| `bench` | re-measures all sizes and latencies on your machine | reproduce the original absolute times |
| `tables` | recomputes all tables and figures from the raw measurements in `results/` | re-measure anything |

## Quick start
    bash reproduce.sh layout   # checks that S1, S2 and S3 were unpacked into one folder
    bash reproduce.sh setup    # clones Marisa-trie, Gog et al. autocomplete and CoCo-trie at the paper's commits, builds everything (~10 min)
    bash reproduce.sh data     # six scored dictionaries, 600,000 queries, reference top-50 answers, Gog input files (~6 min)
    bash reproduce.sh verify   # sanitizer test harness (3.2 M checks) + independent Python oracle (~2 min)
    bash reproduce.sh bench    # all measurements (several hours; Marisa's exhaustive enumeration dominates)
    bash reproduce.sh tables   # tables of the paper
`data` and `verify` check every file listed in `MANIFEST.sha256` after it has been generated; a missing or different file stops the script with a non-zero exit status. `setup` stops if a library is not at the stated commit or the Gog patch does not apply, and records the toolchain and commits in `results/setup.log`.
`tables` reconstructs the derived tables and figures listed below from the archived raw measurements in `results/`; it does not re-run any measurement and stops if a raw file is missing. To re-measure (ablation, external libraries, all timings), run `setup`, `data` and `bench` first. `REPRODUCIBILITY.md` records which stages were re-run from scratch and with what result.

## Where each manuscript table and figure comes from
| Manuscript | Generated file | Script |
|---|---|---|
| Table 2 (footprint by component), string parts in Table 9 | `results/breakdown.csv` | `breakdown.py` |
| Table 5 (footprint and latency, six datasets) | `results/final/table_main.md` | `analyze_final.py` |
| Table 6 (latency for k = 1…50) | `results/final/table_k.md` | `analyze_final.py` |
| Table 7 (ablation) | `results/ablation2.csv` | `./qac ablation` |
| Table 8 (FC-BM vs Gog et al.) | `results/final/table_gog.md` | `analyze_final.py` |
| Table 9 (string parts, CoCo-trie) | `results/final/coco_*.txt`, `results/breakdown.csv` | `coco_bench`, `breakdown.py` |
| Table 10 (language properties; "Table L" in the draft) | `results/langstats.csv` | `langstats.py` |
| Table 11 (matched-size comparison; "Table C") | `results/controlled.csv` | `controlled.py` |
| Table 12 (ten-run statistics; "Table S") | `results/ci_table_s.csv` | `ci.py` |
| Figures 1 and 2 | `results/fig1.*`, `results/fig2.*`, values in `results/fig_data.csv` | `figs.py` |
Blocks scanned (Section 6.5) are in `results/beta2.csv` and `results/final/uz_beta.csv`.
Requirements: Linux, g++ >= 13, cmake, git, ~8 GiB RAM, and Python >= 3.12 (numpy 2.5.3 requires it; tested on Python 3.13.16 and 3.12.3) with the exact package versions of `requirements.txt`:

    python3 -m venv .venv && . .venv/bin/activate && python3 -m pip install -r requirements.txt

Every stage checks the Python version and these package versions and stops if they differ. `requirements-lock.txt` lists every Python package of the environment in which the results were checked, with wheel hashes for Linux x86-64 and Python 3.13 (`python3 -m pip install --require-hashes -r requirements-lock.txt`); it does not pin the OS, compiler or external C++ libraries (those are recorded in `results/setup.log`). `PORTABLE=1 bash reproduce.sh setup` builds without `-march=native` (runs on any x86-64 CPU; the paper's timings used `-march=native`).
If you run `tables` without `data`, it unpacks the released `data/*.tsv.gz` first and stops with a message if a list is missing.

## Files
| File | Purpose |
|---|---|
| `qac.cpp` | FC-BM, Trie-DFS, Trie-Max, Trie-TopK, SA-RMQ, Marisa wrapper and benchmark driver (`./qac <mode> [datasets]`, modes: main, marisa, beta, stat, ablation, dump) |
| `tests.cpp` | randomised brute-force harness (300 dictionaries, edge cases, ties, k = 0, UTF-8, long strings); asserts blocks scanned <= k + 2 |
| `oracle.py` | independent Python oracle (no shared code): recomputes the top-50 answers of all 600,000 queries |
| `gog_bench.cpp`, `gog_prep.py`, `gog_autocomplete.patch` | harness, input files and the two-line patch (`<cstdint>`/`<cstddef>`, MAX_K 15 -> 64) for Gog et al. |
| `coco_bench.cpp`, `remap_bytes.py` | CoCo-trie space/lookup harness; order-preserving byte remap for its 96-code alphabet limit |
| `export.py` | wordfreq 3.1.1 lists en, de, ru, tr and their union `all` |
| `export_uz.py` | Uzbek list from Leipzig `uzb_wikipedia_2021_100K` (NFC; apostrophes U+0027/0060/00B4/02B9/02BB/02BC/2018/2019 -> U+02BB after o/g, U+02BC elsewhere inside a token, dropped at token edges; casefold; Latin letters only) |
| `ci.py` | ten-run medians, percentile bootstrap CIs and ratios (Table 12); resampling unit = run |
| `controlled.py` | matched-size language comparison (Table 11) |
| `langstats.py`, `breakdown.py`, `analyze_final.py` | Table 10, footprint by component, Tables 5, 6 and 8 |
| `results/final/` | raw CSV/logs of the final runs (machine.txt); `uz_*` = Uzbek; `coco_*` = CoCo-trie |
| `results/stat.csv`, `results/gog_stat.csv`, `results/ci_table_s.csv` | ten-run measurements and Table 12 |
| `results/sanitizer.log`, `results/oracle.log` | test harness and independent-oracle output |
| `results/main_r*.csv`, `results/gog_r*.csv` | second machine (Xeon 2.30 GHz, 260 MiB L3), earlier tie-handling variant |
| `data/*.tsv.gz`, `data/*.queries.gz`, `data/*.ref50.xz` | the exact dictionaries, queries and reference top-50 answers (Supporting Information S2); `MANIFEST.sha256` lists the hashes of the uncompressed files, which `reproduce.sh data` regenerates and checks |
| `figs.py` | Figures 1 and 2 from `results/ablation2.csv` and `results/final/`; the plotted values are also written to `results/fig_data.csv` for numerical comparison (PNG/PDF bytes are stable for a given matplotlib version and font set) |
| `requirements.txt`, `requirements-lock.txt` | direct Python dependencies (checked by `reproduce.sh`) and the hash-pinned Python environment |

## Data and licences
- wordfreq 3.1.1 (R. Speer): library code Apache-2.0; word-frequency data CC BY-SA 4.0. The derived en, de, ru, tr, all and en-ascii lists are released under CC BY-SA 4.0.
- Uzbek: Leipzig Corpora Collection, `uzb_wikipedia_2021_100K` (Goldhahn, Eckart & Quasthoff, LREC 2012). Downloadable corpora are licensed CC BY per the collection's terms of usage (https://wortschatz.uni-leipzig.de/en/usage). SHA-256 of `uzb_wikipedia_2021_100K-words.txt`: d4ab528acc57f067c8b0c77ecee82fce4145d308a23da1de5f552406360e4644. The derived `uz.tsv` is released under CC BY 4.0 with attribution to the collection.
- External libraries keep their own licences and are fetched, not redistributed: Marisa-trie 38180f6 (BSD-2-Clause or LGPL-2.1-or-later), autocomplete b3e9da7 (MIT), CoCo-trie ee860d4 (GPL-3.0; a `coco_bench` binary built against it falls under GPL-3.0).

## Limitations of this implementation
- The implementation is single-threaded. `query()` uses function-static scratch vectors shared by all FCBM2 objects and a global counter, so concurrent calls are unsafe even on separate objects (see the comment above `struct FCBM2` in `qac.cpp`). Concurrent use would need query-local or thread_local buffers; this was not implemented or benchmarked.
- `build()` is meant to be called once on a default-constructed object.
- All timings in the paper are single-threaded; absolute latencies depend on the machine (see `results/final/machine.txt`).
