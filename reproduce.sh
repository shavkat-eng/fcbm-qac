#!/usr/bin/env bash
# One-command reproduction of every number in the paper.
#   bash reproduce.sh layout   # check that S1, S2 and S3 were unpacked into one folder
#   bash reproduce.sh setup    # fetch and build the three external libraries and our harnesses
#   bash reproduce.sh data     # rebuild the wordfreq lists, the Uzbek list (from LEIPZIG_UZ, else restored from data/uz.tsv.gz),
#                              # queries and reference answers; check all 19 files against MANIFEST.sha256
#   bash reproduce.sh verify   # sanitizer test harness + independent Python oracle (minutes)
#   bash reproduce.sh bench    # all measurements of Sections 6.1-6.9 (several hours, Marisa dominates)
#   bash reproduce.sh tables   # tables of the paper from results/final
#   bash reproduce.sh all      # everything above, in order
# Requirements: Linux, g++ >= 13, cmake, git, python3 >= 3.12 (numpy 2.5.3 needs it; tested on 3.13.16) with the packages of requirements.txt
# (python3 -m venv .venv && . .venv/bin/activate && python3 -m pip install -r requirements.txt), ~8 GiB RAM.
# PORTABLE=1 builds without $ARCH (binaries run on any x86-64 CPU; the paper's timings used -march=native).
# The Uzbek list needs the Leipzig archive uzb_wikipedia_2021_100K (https://corpora.uni-leipzig.de);
# set LEIPZIG_UZ to the path of its file uzb_wikipedia_2021_100K-words.txt.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"; cd "$ROOT"
EXT="${EXT:-$ROOT/ext}"; F=results/final
DATASETS="tr en de ru all"
if [ "${PORTABLE:-0}" = 1 ]; then ARCH=""; else ARCH="-march=native"; fi

# the Python packages of requirements.txt must be installed at exactly the listed versions
check_python() {
  python3 - <<'PY' || { echo "ERROR: install the Python packages with: python3 -m pip install -r requirements.txt" >&2; exit 1; }
import importlib.metadata as m, sys
if sys.version_info < (3, 12):
    print('ERROR: Python >= 3.12 is required (found ' + sys.version.split()[0] + ')', file=sys.stderr); sys.exit(1)
bad = []
for line in open('requirements.txt'):
    line = line.split('#')[0].strip()
    if not line: continue
    name, ver = line.split('==')
    try: got = m.version(name)
    except m.PackageNotFoundError: got = None
    if got != ver: bad.append(f'{name}: need {ver}, found {got}')
print('python', sys.version.split()[0], '; packages OK' if not bad else '; ' + '; '.join(bad))
sys.exit(1 if bad else 0)
PY
}

# clone (if needed) and check out an exact commit; fail if HEAD is not that commit afterwards
checkout() {  # dir url commit
  [ -d "$1" ] || git clone -q "$2" "$1"
  (cd "$1" && git checkout -q "$3" && git submodule update --init --recursive -q)
  [ "$(cd "$1" && git rev-parse HEAD)" = "$3" ] || { echo "ERROR: $1 is not at commit $3" >&2; exit 1; }
}

setup() {
  check_python
  mkdir -p "$EXT" "$ROOT/results"; cd "$EXT"
  { echo "date: $(date -u +%FT%TZ)"; echo "build profile: ${ARCH:-portable}"; g++ --version | head -1; cmake --version | head -1; python3 --version
    (cd "$ROOT" && python3 -c "import importlib.metadata as m; [print(p, m.version(p)) for p in ('wordfreq','pandas','numpy','matplotlib')]"); } > "$ROOT/results/setup.log"
  checkout marisa-trie https://github.com/s-yata/marisa-trie.git 38180f6d3653cd2c88dd7047711e75f48f84527c
  (cd marisa-trie && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build -j2 >/dev/null)
  checkout autocomplete https://github.com/jermp/autocomplete.git b3e9da7cec586881add11d55c956ebd0c5b83c0e
  (cd autocomplete
   if git apply --reverse --check "$ROOT/gog_autocomplete.patch" 2>/dev/null; then echo "Gog patch already applied"
   else git apply "$ROOT/gog_autocomplete.patch"; fi)                       # fails (and stops) if the patch does not apply
  checkout CoCo-trie https://github.com/aboffa/CoCo-trie.git ee860d4b1e62075b8c80e992e9c32de94ee4caa6
  (cd CoCo-trie && bash ./lib/adapted_code/move.sh >/dev/null \
     && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build -j2 >/dev/null)   # move.sh: CoCo-trie's own setup step (README)
  for r in marisa-trie autocomplete CoCo-trie; do echo "$r $(cd $r && git rev-parse HEAD)"; done >> "$ROOT/results/setup.log"
  cd "$ROOT"
  M="$EXT/marisa-trie"; A="$EXT/autocomplete"; C="$EXT/CoCo-trie"
  g++ -O3 $ARCH -std=c++20 -I"$M/include" -o qac qac.cpp "$M/build/libmarisa.a"
  g++ -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -std=c++20 -DQAC_NO_MAIN -I"$M/include" -o tests tests.cpp "$M/build/libmarisa.a"
  g++ -O3 $ARCH -std=c++20 -DNDEBUG -I"$A/include" -I"$A/external/essentials/include" -I"$A/external/cmd_line_parser/include" -o gog_bench gog_bench.cpp
  g++ -O3 $ARCH -std=gnu++20 -DBIG_ALPHABET -DNDEBUG -I"$C/include" -I"$C/lib/sdsl-lite/include" -I"$C/build/lib/sdsl-lite/include" \
      -I"$C/lib/ds2i" -I"$C/lib/sux" -I"$C/build/lib/sdsl-lite/external/libdivsufsort/include" -o coco_bench coco_bench.cpp "$C/build/lib/sdsl-lite/lib/libsdsl.a"
}

data() {
  mkdir -p data results "$F" gogdata
  check_python
  python3 export.py                                            # en, de, ru, tr, all
  python3 -c "
out=[l for l in open('data/en.tsv','rb') if all(0x20<c<0x7f for c in l.split(b'\t')[0])]
open('data/en_ascii.tsv','wb').writelines(out)"                 # en-ascii (CoCo-trie)
  if [ -n "${LEIPZIG_UZ:-}" ]; then python3 -I export_uz.py "$LEIPZIG_UZ" data/uz.tsv; UZ_ROUTE="rebuilt from Leipzig source $LEIPZIG_UZ";
  elif [ -f data/uz.tsv.gz ]; then gunzip -kf data/uz.tsv.gz; UZ_ROUTE="restored from the released data/uz.tsv.gz (LEIPZIG_UZ not set)";
  else UZ_ROUTE="skipped (no LEIPZIG_UZ, no data/uz.tsv.gz)"; fi
  echo "Uzbek list: $UZ_ROUTE"
  { echo "date: $(date -u +%FT%TZ)"; echo "wordfreq lists (tr, en, de, ru, all, en_ascii): rebuilt with wordfreq $(python3 -c 'import importlib.metadata as m; print(m.version("wordfreq"))')";
    echo "uz: $UZ_ROUTE"; echo "queries, reference answers, Gog input: regenerated by ./qac dump and gog_prep.py"; } > results/data_provenance.txt
  [ -f data/uz.tsv ] && DATASETS="tr uz en de ru all"
  if [ -f data/uz.tsv ]; then python3 remap_bytes.py data/uz.tsv data/uz_remap.tsv; fi   # Uzbek is optional; a failing remap stops the script
  for d in $DATASETS; do ./qac dump "$d"; python3 gog_prep.py "$d" gogdata
    (cd "$EXT/autocomplete/test_data" && G="$ROOT/gogdata/$d.completions" && python3 extract_dict.py "$G" && python3 map_dataset.py "$G" \
       && python3 build_stats.py "$G.mapped" && python3 build_inverted_and_forward.py "$G") >/dev/null; done
  check_hashes
}

# strict check of every file listed in MANIFEST.sha256: a missing or different file stops the script
check_hashes() {
  local bad=0
  while read -r h f; do
    if [ ! -f "data/$f" ]; then echo "MISSING data/$f" >&2; bad=1
    elif [ "$(sha256sum "data/$f" | cut -d' ' -f1)" != "$h" ]; then echo "HASH MISMATCH data/$f" >&2; bad=1; fi
  done < MANIFEST.sha256
  [ $bad = 0 ] || { echo "ERROR: data differ from the paper's (see above)" >&2; exit 1; }
  echo "all $(wc -l < MANIFEST.sha256) files match MANIFEST.sha256"
}

verify() {
  check_hashes
  ./tests | tee results/tests_rerun.log
  python3 oracle.py | tee results/oracle_rerun.log
}

bench() {
  export GOG_DATA="$ROOT/gogdata" QAC_DATA="$ROOT/data"
  lscpu > $F/machine.txt; uname -a >> $F/machine.txt; g++ --version | head -1 >> $F/machine.txt
  for i in 1 2 3; do
    SKIP_MARISA=1 ./qac main tr en de ru all > $F/main_r$i.log 2>&1
    mv results/main.csv $F/main_r$i.csv; mv results/main_bylen.csv $F/main_bylen_r$i.csv; cp results/main_datasets.csv $F/datasets.csv
    ./gog_bench $F/gog_r$i.csv tr en de ru all > $F/gog_r$i.log 2>&1
    if [ -f data/uz.tsv ]; then
      SKIP_MARISA=1 ./qac main uz > $F/uz_main_r$i.log 2>&1
      mv results/main.csv $F/uz_main_r$i.csv; mv results/main_bylen.csv $F/uz_main_bylen_r$i.csv; cp results/main_datasets.csv $F/uz_datasets.csv
      ./gog_bench $F/uz_gog_r$i.csv uz > $F/uz_gog_r$i.log 2>&1
    fi
  done
  ./qac marisa tr en de ru all > $F/marisa.log 2>&1; mv results/marisa.csv $F/marisa_r1.csv; mv results/marisa_bylen.csv $F/marisa_bylen_r1.csv
  ./qac main en_ascii > $F/ascii.log 2>&1; mv results/main.csv $F/ascii_main.csv      # SKIP_MARISA unset: Marisa included
  ./coco_bench data/en_ascii.tsv > $F/coco_en_ascii.txt 2>&1
  ./qac beta tr en de ru all > results/beta2.log 2>&1; mv results/beta.csv results/beta2.csv
  ./qac ablation all > results/ablation2.log 2>&1; mv results/ablation.csv results/ablation2.csv
  if [ -f data/uz.tsv ]; then
    ./qac marisa uz > $F/uz_marisa.log 2>&1; mv results/marisa.csv $F/uz_marisa_r1.csv
    ./qac beta uz > $F/uz_beta.log 2>&1; mv results/beta.csv $F/uz_beta.csv
    ./qac stat uz > $F/uz_stat.log 2>&1; mv results/stat.csv $F/uz_stat.csv
    GOG_STAT=1 ./gog_bench $F/uz_gog_stat.csv uz > $F/uz_gog_stat.log 2>&1
    ./coco_bench data/uz_remap.tsv > $F/coco_uz_remap.txt 2>&1
  fi
  ./qac stat tr en de ru all > results/stat.log 2>&1
  GOG_STAT=1 ./gog_bench results/gog_stat.csv tr en de ru all > results/gog_stat.log 2>&1
}

# tables: derived tables and figures from the raw measurements already in results/ (archived or produced by bench);
# it does not re-run any measurement.
tables() {
  check_python
  for f in results/final/main_r1.csv results/final/main_r2.csv results/final/main_r3.csv results/final/main_bylen_r1.csv \
           results/final/main_bylen_r2.csv results/final/main_bylen_r3.csv results/final/gog_r1.csv results/final/gog_r2.csv \
           results/final/gog_r3.csv results/final/marisa_r1.csv results/final/datasets.csv results/ablation2.csv \
           results/stat.csv results/gog_stat.csv results/final/coco_en_ascii.txt \
           results/final/uz_main_r1.csv results/final/uz_main_r2.csv results/final/uz_main_r3.csv results/final/uz_gog_r1.csv \
           results/final/uz_gog_r2.csv results/final/uz_gog_r3.csv results/final/uz_marisa_r1.csv results/final/uz_datasets.csv \
           results/final/uz_stat.csv results/final/uz_gog_stat.csv results/final/coco_uz_remap.txt; do
    test -s "$f" || { echo "ERROR: raw measurement $f is missing; restore it from the archive or run: bash reproduce.sh bench" >&2; exit 1; }
  done
  for d in tr uz en de ru all en_ascii; do                      # unpack the released lists if the data stage was skipped
    if [ ! -f data/$d.tsv ] && [ -f data/$d.tsv.gz ]; then gunzip -k data/$d.tsv.gz; fi
    [ -f data/$d.tsv ] || { [ "$d" = uz ] && continue; echo "ERROR: data/$d.tsv missing; run: bash reproduce.sh data" >&2; exit 1; }
  done
  python3 analyze_final.py > results/final/analyze_final.log    # Tables 5, 6, 8 (table_main.md, table_k.md, table_gog.md)
  python3 figs.py                                                # Figures 1 and 2 (results/fig1.*, results/fig2.*)
  python3 ci.py > results/ci_table_s.csv
  python3 langstats.py; python3 breakdown.py > results/breakdown.csv; python3 controlled.py > results/controlled.csv
  for f in results/final/table_main.md results/final/table_k.md results/final/table_gog.md results/ci_table_s.csv \
           results/controlled.csv results/langstats.csv results/breakdown.csv results/fig1.png results/fig1.pdf results/fig2.png results/fig2.pdf results/fig_data.csv; do
    test -s "$f" || { echo "ERROR: $f was not produced" >&2; exit 1; }
  done
  echo "Tables written to results/ and results/final/"
}

# layout: check that S1 (code), S2 (data) and S3 (results) were unpacked into one folder
layout() {
  # RELEASE_MANIFEST.sha256 (in S1) lists the SHA-256 of every released file of S1, S2 and S3 except outputs that the
  # stages rewrite; a file from another release, or a missing file, fails the check. Run it on the unpacked release.
  [ -f RELEASE_MANIFEST.sha256 ] || { echo "ERROR: RELEASE_MANIFEST.sha256 (part of S1) is missing" >&2; exit 1; }
  local rel; rel=$(head -1 RELEASE_MANIFEST.sha256 | sed 's/^# release: //')
  if sha256sum --quiet -c <(grep -v '^#' RELEASE_MANIFEST.sha256) ; then
    echo "layout OK: S1, S2 and S3 all belong to release $rel ($(grep -vc '^#' RELEASE_MANIFEST.sha256) files checked)"
  else
    echo "ERROR: the files above do not belong to release $rel; unpack S1, S2 and S3 of the same release into one folder" >&2; exit 1
  fi
}

case "${1:-all}" in
  layout) layout;; setup) setup;; data) data;; verify) verify;; bench) bench;; tables) tables;;
  all) layout; setup; data; verify; bench; tables;;
  *) echo "usage: $0 [layout|setup|data|verify|bench|tables|all]"; exit 1;;
esac
