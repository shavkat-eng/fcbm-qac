// Harness for Gog, Pibiri, Venturini (SIGIR 2020) autocomplete: prefix_topk on our queries.
#include <bits/stdc++.h>
#include <malloc.h>
#define private public
#define protected public
#include "types.hpp"
#include "probe.hpp"
#undef private
#undef protected
using namespace autocomplete;
using namespace std;
static string env_or(const char* k, const char* d) { const char* v = getenv(k); return v ? v : d; }
static size_t heap_used() { auto m = mallinfo2(); return m.uordblks + m.hblkhd; }

template <class Index>
void run(const string& ds, const string& type, const string& base, ofstream& csv) {
  vector<string> qs; { ifstream f(base + ".queries"); string l; while (getline(f, l)) qs.push_back(l); }
  vector<vector<uint32_t>> ref; { ifstream f(base + ".ref50"); string l; while (getline(f, l)) { istringstream is(l); vector<uint32_t> v; uint32_t x; while (is >> x) v.push_back(x); ref.push_back(v); } }
  vector<uint32_t> doc2lex; { ifstream f(env_or("GOG_DATA", "gogdata") + "/" + ds + ".doc2lex"); uint32_t x; while (f >> x) doc2lex.push_back(x); }
  parameters params; params.collection_basename = env_or("GOG_DATA", "gogdata") + "/" + ds + ".completions"; params.load();
  size_t before = heap_used(); auto t0 = chrono::steady_clock::now();
  Index* idx = new Index(params);
  double bt = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
  size_t heap = heap_used() - before, total = idx->bytes();
  size_t pre = idx->m_completions.bytes() + idx->m_unsorted_docs_list.bytes() + idx->m_dictionary.bytes();
  if constexpr (requires { idx->m_forward_index; }) pre += idx->m_forward_index.bytes();
  if constexpr (requires { idx->m_docid_to_lexid; }) pre += idx->m_docid_to_lexid.bytes();
  nop_probe probe; size_t n = doc2lex.size();
  vector<int> klist = {1, 5, 10, 20, 50};
  if (getenv("GOG_STAT")) klist.assign(10, 10);   // 10 timing runs at k = 10 on one built index
  for (int k : klist) {
    for (size_t i = 0; i < 20000; ++i) idx->prefix_topk(qs[i], k, probe);
    vector<double> ns(qs.size()); size_t mism = 0, notfound = 0; uint64_t chk = 0;
    for (size_t i = 0; i < qs.size(); ++i) {
      auto a = chrono::steady_clock::now(); auto it = idx->prefix_topk(qs[i], k, probe); auto b = chrono::steady_clock::now();
      ns[i] = chrono::duration<double, nano>(b - a).count();
      // verify: ids and strings against the exhaustive reference
      size_t want = min<size_t>(k, ref[i].size());
      if (it.size() != want) { ++mism; if (it.size() == 0) ++notfound; continue; }
      for (size_t j = 0; j < it.size(); ++j, ++it) {
        auto sbr = *it; uint32_t lex = doc2lex[sbr.score];
        if (lex != ref[i][j]) { ++mism; break; }
        chk += sbr.string.end - sbr.string.begin;
      }
    }
    vector<double> s = ns; sort(s.begin(), s.end());
    double mean = accumulate(ns.begin(), ns.end(), 0.0) / ns.size();
    auto pct = [&](double q) { return s[size_t(q * (s.size() - 1))] / 1000; };
    csv << ds << ",Gog-" << type << ",-," << k << ',' << n << ',' << heap << ',' << total << ',' << pre << ',' << bt << ','
        << mean / 1000 << ',' << pct(0.5) << ',' << pct(0.95) << ',' << pct(0.99) << ',' << mism << ',' << notfound << ',' << chk << '\n';
    csv.flush();
    cerr << ds << " " << type << " k=" << k << " heapMB=" << heap / 1e6 << " totalMB=" << total / 1e6 << " prefixMB=" << pre / 1e6
         << " mean=" << mean / 1000 << " p99=" << pct(0.99) << " mism=" << mism << " notfound=" << notfound << '\n';
  }
  delete idx; malloc_trim(0);
}
int main(int argc, char** argv) {
  string out = argv[1]; ofstream csv(out);
  csv << "dataset,method,param,k,n,heap_bytes,lib_bytes,prefix_bytes,build_s,mean_us,p50_us,p95_us,p99_us,mismatches,notfound,checksum\n";
  for (int i = 2; i < argc; ++i) {
    string ds = argv[i], base = env_or("QAC_DATA", "data") + "/" + ds;
    run<ef_autocomplete_type1>(ds, "type1", base, csv);
    run<ef_autocomplete_type2>(ds, "type2", base, csv);
  }
}
