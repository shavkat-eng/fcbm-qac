// Brute-force correctness and block-count tests for FC-BM and the baselines.
#define QAC_NO_MAIN
#include "qac.cpp"
static vector<u32> brute(const Dict& d, const string& p, int k) {
  vector<u32> c; for (u32 i = 0; i < d.w.size(); ++i) if (d.w[i].compare(0, p.size(), p) == 0 && d.w[i].size() >= p.size()) c.push_back(i);
  sort(c.begin(), c.end(), [&](u32 a, u32 b) { return mkkey(d.s[a], a) > mkkey(d.s[b], b); });
  if (k < 0) k = 0; if ((int)c.size() > k) c.resize(k); return c;
}
static Dict mkdict(vector<string> w, mt19937_64& rng, int scoreMode) {
  sort(w.begin(), w.end()); w.erase(unique(w.begin(), w.end()), w.end());
  Dict d; d.w = w; uniform_int_distribution<u32> small(0, 3), any(0, 0xFFFFFFFFu);
  for (size_t i = 0; i < w.size(); ++i) d.s.push_back(scoreMode == 0 ? 7u : scoreMode == 1 ? small(rng) : scoreMode == 2 ? any(rng) : u32(i % 5 == 0 ? 0xFFFFFFFFu : 0u));
  return d;
}
int main() {
  mt19937_64 rng(12345); size_t checks = 0, fails = 0, overBound = 0, maxOver = 0;
  auto run = [&](const string& name, auto& m, const Dict& d, const vector<string>& qs, const vector<int>& ks, bool countBlocks) {
    Result r;
    for (auto& q : qs) for (int k : ks) {
      u64 g0 = g_blocks; m.query(q, k, r); u64 used = g_blocks - g0;
      auto want = brute(d, q, k); ++checks;
      bool ok = r.ids == want && r.strs.size() == r.ids.size();
      for (size_t j = 0; ok && j < r.ids.size(); ++j) ok = r.strs[j] == d.w[r.ids[j]];
      if (!ok) { if (++fails < 10) cerr << "FAIL " << name << " q='" << q << "' k=" << k << " got " << r.ids.size() << " want " << want.size() << '\n'; }
      if (countBlocks && k > 0 && used > u64(k) + 2) { ++overBound; maxOver = max<size_t>(maxOver, used - k); }
    }
  };
  for (int trial = 0; trial < 300; ++trial) {
    // dictionary: small alphabet for heavy prefix sharing, plus UTF-8 and long strings
    int n = trial < 5 ? trial : uniform_int_distribution<int>(1, 600)(rng);
    vector<string> w; const string alpha = trial % 3 == 0 ? "ab" : trial % 3 == 1 ? "abc" : "abcdz";
    for (int i = 0; i < n; ++i) {
      int len = uniform_int_distribution<int>(0, 9)(rng); string s;
      for (int j = 0; j < len; ++j) s += alpha[rng() % alpha.size()];
      if (rng() % 17 == 0) s += "\xd0\xbe\xd2\x9b";          // Cyrillic 'оқ' bytes
      if (rng() % 97 == 0) s += string(1000, 'q');               // very long string
      w.push_back(s);
    }
    Dict d = mkdict(w, rng, trial % 4);
    vector<string> qs = {"", "zzzz", "a", "ab", "\xd0", string(2000, 'q')};
    for (auto& x : d.w) for (size_t l = 0; l <= min<size_t>(x.size(), 4); ++l) qs.push_back(x.substr(0, l));
    sort(qs.begin(), qs.end()); qs.erase(unique(qs.begin(), qs.end()), qs.end());
    if (qs.size() > 60) { shuffle(qs.begin(), qs.end(), rng); qs.resize(60); qs.push_back(""); }
    vector<int> ks = {0, 1, 2, 3, 5, 10, 50, (int)d.w.size() + 5};
    for (u32 B : {1u, 2u, 3u, 4u, 8u, 32u, 256u}) {
      FCBM2 f; f.B = B; f.build(d); run("FC-BM B=" + to_string(B), f, d, qs, ks, true);
      FCBM2 g; g.B = B; g.prune = false; g.build(d); run("FC-BM/no-prune", g, d, qs, ks, false);
      FCBM h; h.B = B; h.build(d); run("FC-BM/interleaved", h, d, qs, ks, false);
    }
    if (n > 0) {
      SARMQ s; s.build(d); run("SA-RMQ", s, d, qs, ks, false);
      TrieMax t; t.build(d); run("Trie-Max", t, d, qs, ks, false);
      TrieDFS u; u.build(d); run("Trie-DFS", u, d, qs, ks, false);
      TrieCache c; c.build(d); vector<int> kc = {0, 1, 5, 10}; run("Trie-TopK", c, d, qs, kc, false);
    }
  }
  printf("checks=%zu fails=%zu blocks>k+2: %zu (max excess over k: %zu)\n", checks, fails, overBound, maxOver);
  return fails ? 1 : 0;
}
