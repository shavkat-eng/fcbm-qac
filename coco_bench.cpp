// CoCo-trie space and exact-lookup latency on a scored dictionary (strings only).
#include <bits/stdc++.h>
#include <malloc.h>
#include "uncompacted_trie.hpp"
#include "utils.hpp"
#include "CoCo-trie_v2.hpp"
using namespace std;
int main(int argc, char** argv) {
  ifstream in(argv[1]); string line; vector<pair<string,uint32_t>> v;
  while (getline(in, line)) { auto t = line.find('\t'); v.push_back({line.substr(0, t), (uint32_t)stoul(line.substr(t + 1))}); }
  sort(v.begin(), v.end());
  vector<string> w; vector<uint32_t> s; for (auto& [a, b] : v) { w.push_back(a); s.push_back(b); }
  datasetStats ds = dataset_stats_from_vector(w);
  MIN_CHAR = ds.get_min_char(); ALPHABET_SIZE = ds.get_alphabet_size();
  size_t before = mallinfo2().uordblks + mallinfo2().hblkhd;
  auto t0 = chrono::steady_clock::now();
  auto* coco = new CoCo_v2<>(w);
  double bt = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
  size_t heap = mallinfo2().uordblks + mallinfo2().hblkhd - before;
  size_t bits = coco->size_in_bits();
  // CoCo-trie ids follow node order, not lexicographic order. We check that every key is found and that
  // the ids form a set of n distinct values; non_lex_ids counts keys whose id differs from rank + 1
  // (expected, not an error), and id_order_monotone reports whether ids increase with lexicographic rank.
  size_t non_lex_ids = 0, notfound = 0, mono = 1, prev = 0; set<size_t> ids;
  for (size_t i = 0; i < w.size(); ++i) {
    size_t r = coco->look_up(w[i]);
    if (r == size_t(-1)) { ++notfound; continue; }
    ids.insert(r); if (r != i + 1) ++non_lex_ids; if (i && r <= prev) mono = 0; prev = r;
  }
  for (int i = 0; i < 5; ++i) printf("%s -> %zu\n", w[i].c_str(), coco->look_up(w[i]));
  printf("notfound=%zu distinct_ids=%zu all_ids_distinct=%d id_order_monotone=%zu non_lex_ids=%zu\n",
         notfound, ids.size(), int(ids.size() == w.size() - notfound), mono, non_lex_ids);
  // latency: 100k frequency-weighted exact lookups
  mt19937_64 rng(20261009); discrete_distribution<uint32_t> pick(s.begin(), s.end());
  vector<string> q; for (int i = 0; i < 100000; ++i) q.push_back(w[pick(rng)]);
  size_t chk = 0; for (int i = 0; i < 20000; ++i) chk += coco->look_up(q[i]);
  vector<double> ns; ns.reserve(q.size());
  for (auto& x : q) { auto a = chrono::steady_clock::now(); chk += coco->look_up(x); auto b = chrono::steady_clock::now(); ns.push_back(chrono::duration<double, nano>(b - a).count()); }
  double mean = accumulate(ns.begin(), ns.end(), 0.0) / ns.size(); sort(ns.begin(), ns.end());
  printf("n=%zu alphabet=%zu size_bytes=%.0f heap_bytes=%zu build_s=%.3f non_lex_ids=%zu lookup_mean_us=%.3f p99_us=%.3f chk=%zu\n",
         w.size(), ALPHABET_SIZE, bits / 8.0, heap, bt, non_lex_ids, mean / 1000, ns[size_t(0.99 * (ns.size() - 1))] / 1000, chk);
}
