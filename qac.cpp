// Top-k query auto-completion: baselines and the proposed FC-BM index.
// Build: g++ -O3 -march=native -std=c++20 -o qac qac.cpp
#include <bits/stdc++.h>
#include <malloc.h>
#include <marisa.h>
using namespace std;
using u8 = uint8_t; using u32 = uint32_t; using u64 = uint64_t;

static size_t heap_used() { return mallinfo2().uordblks + mallinfo2().hblkhd; }

static u64 g_blocks = 0;
struct Dict { vector<string> w; vector<u32> s; };          // lexicographically sorted
static inline u64 mkkey(u32 score, u32 id) { return (u64(score) << 32) | u64(~id); }

struct Result { vector<u32> ids; vector<string> strs; };

// ---------------------------------------------------------------- tries
// BFS-numbered array trie: children of v are nodes [cb[v], cb[v+1]).
struct ArrayTrie {
  vector<u32> cb, parent; vector<u8> label; vector<int32_t> term;
  void build(const Dict& d) {
    // temporary pointer trie
    struct T { vector<pair<u8,u32>> ch; int32_t term = -1; };
    vector<T> t(1);
    for (u32 i = 0; i < d.w.size(); ++i) {
      u32 v = 0;
      for (unsigned char c : d.w[i]) {
        auto& ch = t[v].ch;
        if (!ch.empty() && ch.back().first == c) { v = ch.back().second; continue; }
        u32 nv = t.size(); ch.push_back({c, nv}); t.emplace_back(); v = nv;
      }
      t[v].term = i;
    }
    size_t N = t.size();
    vector<u32> order; order.reserve(N); order.push_back(0);
    vector<u32> newid(N);
    cb.assign(N + 1, 0); parent.assign(N, 0); label.assign(N, 0); term.assign(N, -1);
    for (size_t h = 0; h < order.size(); ++h) {
      u32 v = order[h]; newid[v] = h; cb[h] = order.size();
      for (auto& [c, u] : t[v].ch) { label[order.size()] = c; parent[order.size()] = h; order.push_back(u); }
      term[h] = t[v].term;
    }
    cb[N] = N;
    vector<T>().swap(t);
  }
  size_t nodes() const { return term.size(); }
  inline int64_t child(u32 v, u8 c) const {
    u32 a = cb[v], b = cb[v + 1];
    while (a < b) { u32 m = (a + b) >> 1; if (label[m] < c) a = m + 1; else b = m; }
    return (a < cb[v + 1] && label[a] == c) ? int64_t(a) : -1;
  }
  int64_t locate(const string& p) const {
    int64_t v = 0;
    for (unsigned char c : p) { v = child(u32(v), c); if (v < 0) return -1; }
    return v;
  }
  void spell(u32 v, string& out) const {
    out.clear(); while (v) { out.push_back(char(label[v])); v = parent[v]; }
    reverse(out.begin(), out.end());
  }
};

struct TrieDFS {                       // B1: exhaustive subtree enumeration
  ArrayTrie t; vector<u32> score;
  void build(const Dict& d) { t.build(d); score = d.s; }
  void query(const string& p, int k, Result& r) {
    if (k <= 0) { r.ids.clear(); r.strs.clear(); return; }
    r.ids.clear(); int64_t v0 = t.locate(p); if (v0 < 0) { r.strs.resize(0); return; }
    priority_queue<pair<u64,u32>, vector<pair<u64,u32>>, greater<>> h;  // min-heap (key,node)
    static vector<u32> st; st.clear(); st.push_back(u32(v0));
    while (!st.empty()) {
      u32 v = st.back(); st.pop_back();
      if (t.term[v] >= 0) {
        u64 key = mkkey(score[t.term[v]], t.term[v]);
        if ((int)h.size() < k) h.push({key, v}); else if (key > h.top().first) { h.pop(); h.push({key, v}); }
      }
      for (u32 c = t.cb[v]; c < t.cb[v + 1]; ++c) st.push_back(c);
    }
    vector<pair<u64,u32>> o; while (!h.empty()) { o.push_back(h.top()); h.pop(); }
    reverse(o.begin(), o.end()); r.strs.resize(o.size());
    for (size_t i = 0; i < o.size(); ++i) { r.ids.push_back(t.term[o[i].second]); t.spell(o[i].second, r.strs[i]); }
  }
};

struct TrieMax {                       // B2: subtree-max pruning, best-first search
  ArrayTrie t; vector<u32> score, best;   // best[v] = id of best completion under v
  inline u64 key(u32 id) const { return mkkey(score[id], id); }
  void build(const Dict& d) {
    t.build(d); score = d.s; size_t N = t.nodes(); best.assign(N, 0);
    vector<u64> bk(N, 0);
    for (size_t v = N; v-- > 0;) {
      u64 b = 0; u32 bi = 0;
      if (t.term[v] >= 0) { b = key(t.term[v]); bi = t.term[v]; }
      for (u32 c = t.cb[v]; c < t.cb[v + 1]; ++c) if (bk[c] > b) { b = bk[c]; bi = best[c]; }
      bk[v] = b; best[v] = bi;
    }
  }
  void query(const string& p, int k, Result& r) {
    if (k <= 0) { r.ids.clear(); r.strs.clear(); return; }
    r.ids.clear(); int64_t v0 = t.locate(p); if (v0 < 0) { r.strs.resize(0); return; }
    // entries: (key, node, isResult)
    static vector<tuple<u64,u32,bool>> pq; pq.clear();
    auto push = [&](u64 kk, u32 v, bool res) { pq.push_back({kk, v, res}); push_heap(pq.begin(), pq.end()); };
    push(key(best[v0]), u32(v0), false);
    static vector<u32> outn; outn.clear();
    while (!pq.empty() && (int)outn.size() < k) {
      pop_heap(pq.begin(), pq.end()); auto [kk, v, isres] = pq.back(); pq.pop_back();
      if (isres) { outn.push_back(v); continue; }
      if (t.term[v] >= 0) push(key(t.term[v]), v, true);
      for (u32 c = t.cb[v]; c < t.cb[v + 1]; ++c) push(key(best[c]), c, false);
    }
    r.strs.resize(outn.size());
    for (size_t i = 0; i < outn.size(); ++i) { r.ids.push_back(t.term[outn[i]]); t.spell(outn[i], r.strs[i]); }
  }
};

struct TrieCache {                     // B3: per-node precomputed top-K lists
  static constexpr int K = 10;
  ArrayTrie t; vector<u32> off, ids, termnode;
  void build(const Dict& d) {
    t.build(d); size_t N = t.nodes();
    auto key = [&](u32 id) { return mkkey(d.s[id], id); };
    vector<vector<u32>> L(N);
    for (size_t v = N; v-- > 0;) {
      vector<u32> c; if (t.term[v] >= 0) c.push_back(t.term[v]);
      for (u32 u = t.cb[v]; u < t.cb[v + 1]; ++u) { c.insert(c.end(), L[u].begin(), L[u].end()); }
      size_t m = min<size_t>(K, c.size());
      partial_sort(c.begin(), c.begin() + m, c.end(), [&](u32 a, u32 b) { return key(a) > key(b); });
      c.resize(m); c.shrink_to_fit(); L[v] = move(c);
      for (u32 u = t.cb[v]; u < t.cb[v + 1]; ++u) if (u > v) {}  // children lists kept until parent processed
    }
    off.assign(N + 1, 0);
    for (size_t v = 0; v < N; ++v) off[v + 1] = off[v] + L[v].size();
    ids.resize(off[N]);
    for (size_t v = 0; v < N; ++v) copy(L[v].begin(), L[v].end(), ids.begin() + off[v]);
    vector<vector<u32>>().swap(L);
    termnode.assign(d.w.size(), 0);
    for (size_t v = 0; v < N; ++v) if (t.term[v] >= 0) termnode[t.term[v]] = v;
  }
  void query(const string& p, int k, Result& r) {
    if (k <= 0) { r.ids.clear(); r.strs.clear(); return; }
    r.ids.clear(); int64_t v0 = t.locate(p); if (v0 < 0) { r.strs.resize(0); return; }
    u32 a = off[v0], b = min<u32>(off[v0 + 1], a + k); r.strs.resize(b - a);
    for (u32 i = a; i < b; ++i) { r.ids.push_back(ids[i]); t.spell(termnode[ids[i]], r.strs[i - a]); }
  }
};

// ---------------------------------------------------------------- B4: sorted array + RMQ
struct SARMQ {
  static constexpr u32 RB = 64;
  vector<char> chars; vector<u32> off, score; vector<vector<u32>> sp;  // sparse table over block argmax
  vector<u32> bmax;
  inline u64 key(u32 i) const { return mkkey(score[i], i); }
  inline u32 better(u32 a, u32 b) const { return key(a) >= key(b) ? a : b; }
  void build(const Dict& d) {
    size_t n = d.w.size(), tot = 0; for (auto& w : d.w) tot += w.size();
    chars.reserve(tot); off.reserve(n + 1); off.push_back(0);
    for (auto& w : d.w) { chars.insert(chars.end(), w.begin(), w.end()); off.push_back(chars.size()); }
    score = d.s; size_t nb = (n + RB - 1) / RB; bmax.resize(nb);
    for (size_t b = 0; b < nb; ++b) { u32 m = b * RB; for (u32 i = b * RB; i < min<size_t>(n, (b + 1) * RB); ++i) m = better(m, i); bmax[b] = m; }
    sp.push_back(bmax);
    for (size_t j = 1; (size_t(1) << j) <= nb; ++j) {
      auto& pr = sp[j - 1]; vector<u32> cur(nb - (size_t(1) << j) + 1);
      for (size_t i = 0; i < cur.size(); ++i) cur[i] = better(pr[i], pr[i + (size_t(1) << (j - 1))]);
      sp.push_back(move(cur));
    }
  }
  u32 scan(u32 a, u32 b) const { u32 m = a; for (u32 i = a + 1; i <= b; ++i) m = better(m, i); return m; }
  u32 rmq(u32 a, u32 b) const {             // inclusive
    u32 ba = a / RB, bb = b / RB;
    if (bb - ba <= 1) return scan(a, b);
    u32 m = better(scan(a, (ba + 1) * RB - 1), scan(bb * RB, b));
    u32 l = ba + 1, r = bb - 1, j = 31 - __builtin_clz(r - l + 1);
    return better(m, better(sp[j][l], sp[j][r - (1u << j) + 1]));
  }
  inline string_view str(u32 i) const { return string_view(chars.data() + off[i], off[i + 1] - off[i]); }
  void query(const string& p, int k, Result& r) {
    if (k <= 0) { r.ids.clear(); r.strs.clear(); return; }
    r.ids.clear(); u32 n = score.size();
    u32 lo = 0, hi = n;                                  // first >= p
    while (lo < hi) { u32 m = (lo + hi) >> 1; if (str(m) < string_view(p)) lo = m + 1; else hi = m; }
    u32 L = lo; hi = n;                                  // first whose |p|-prefix > p
    while (lo < hi) { u32 m = (lo + hi) >> 1; if (str(m).substr(0, p.size()) <= string_view(p)) lo = m + 1; else hi = m; }
    u32 R = lo; if (L >= R) { r.strs.resize(0); return; }
    static vector<tuple<u64,u32,u32,u32>> pq; pq.clear();
    auto push = [&](u32 a, u32 b) { u32 x = rmq(a, b); pq.push_back({key(x), x, a, b}); push_heap(pq.begin(), pq.end()); };
    push(L, R - 1);
    while (!pq.empty() && (int)r.ids.size() < k) {
      pop_heap(pq.begin(), pq.end()); auto [kk, m, a, b] = pq.back(); pq.pop_back(); r.ids.push_back(m);
      if (m > a) push(a, m - 1);
      if (m < b) push(m + 1, b);
    }
    r.strs.resize(r.ids.size());
    for (size_t i = 0; i < r.ids.size(); ++i) r.strs[i].assign(str(r.ids[i]));
  }
};

// ---------------------------------------------------------------- proposed: FC-BM
// Front-coded blocks of B strings with in-stream VByte scores; a max segment tree over
// per-block maximum scores drives best-first, lazy block decoding.
static inline void vb_put(vector<u8>& o, u32 x) { while (x >= 128) { o.push_back(u8(x | 128)); x >>= 7; } o.push_back(u8(x)); }
static inline u32 vb_get(const u8*& p) { u32 x = 0; int s = 0; while (*p & 128) { x |= u32(*p++ & 127) << s; s += 7; } x |= u32(*p++) << s; return x; }

struct FCBM {
  u32 B = 32; bool vbyteScores = true; bool prune = true;
  u32 n = 0, nb = 0, S = 1;
  vector<u8> data; vector<u32> boff, tree;
  // query-time scratch (not part of the index footprint)
  string cur;
  void put_score(u32 s) { if (vbyteScores) vb_put(data, s); else { for (int i = 0; i < 4; ++i) data.push_back(u8(s >> (8 * i))); } }
  inline u32 get_score(const u8*& p) const { if (vbyteScores) return vb_get(p); u32 s; memcpy(&s, p, 4); p += 4; return s; }
  void build(const Dict& d) {
    n = d.w.size(); nb = (n + B - 1) / B; boff.reserve(nb + 1);
    while (S < nb) S <<= 1; tree.assign(2 * S, 0);
    for (u32 b = 0; b < nb; ++b) {
      boff.push_back(data.size()); u32 mx = 0;
      for (u32 i = b * B; i < min(n, (b + 1) * B); ++i) {
        const string& w = d.w[i];
        if (i == b * B) { vb_put(data, w.size()); }
        else {
          const string& pw = d.w[i - 1]; u32 l = 0; while (l < pw.size() && l < w.size() && pw[l] == w[l]) ++l;
          vb_put(data, l); vb_put(data, w.size() - l); data.insert(data.end(), w.begin() + l, w.end()); put_score(d.s[i]); mx = max(mx, d.s[i]); continue;
        }
        data.insert(data.end(), w.begin(), w.end()); put_score(d.s[i]); mx = max(mx, d.s[i]);
      }
      tree[S + b] = mx;
    }
    boff.push_back(data.size()); data.shrink_to_fit();
    for (u32 v = S - 1; v >= 1; --v) tree[v] = max(tree[2 * v], tree[2 * v + 1]);
  }
  string_view header(u32 b) const { const u8* p = data.data() + boff[b]; u32 len = vb_get(p); return string_view((const char*)p, len); }
  // decode block b; feed entries with prefix p into the candidate heap
  template <class F> void decode(u32 b, const string& p, F&& emit) {
    const u8* q = data.data() + boff[b]; u32 end = min(n, (b + 1) * B);
    bool seen = false;
    for (u32 i = b * B; i < end; ++i) {
      if (i == b * B) { u32 len = vb_get(q); cur.assign((const char*)q, len); q += len; }
      else { u32 l = vb_get(q), sl = vb_get(q); cur.resize(l); cur.append((const char*)q, sl); q += sl; }
      u32 s = get_score(q);
      bool m = cur.size() >= p.size() && memcmp(cur.data(), p.data(), p.size()) == 0;
      if (m) { seen = true; emit(i, s, cur); } else if (seen) return;     // sorted: matches are contiguous
    }
  }
  void query(const string& p, int k, Result& r) {
    if (k <= 0) { r.ids.clear(); r.strs.clear(); return; }
    r.ids.clear(); string_view pv(p);
    u32 lo = 0, hi = nb;   // first block with header >= p
    while (lo < hi) { u32 m = (lo + hi) >> 1; if (header(m) < pv) lo = m + 1; else hi = m; }
    u32 bl = lo ? lo - 1 : 0; hi = nb;
    while (lo < hi) { u32 m = (lo + hi) >> 1; if (header(m).substr(0, p.size()) <= pv) lo = m + 1; else hi = m; }
    if (lo == 0) { r.strs.resize(0); return; }
    u32 br = lo - 1; if (br < bl) { r.strs.resize(0); return; }
    // candidate min-heap over (key, slot)
    static vector<string> slot; if ((int)slot.size() < k) slot.resize(k);
    vector<pair<u64,int>> h; h.reserve(k); int used = 0;
    auto cmp = [](const pair<u64,int>& a, const pair<u64,int>& b) { return a.first > b.first; };
    auto emit = [&](u32 id, u32 s, const string& w) {
      u64 key = mkkey(s, id);
      if ((int)h.size() < k) { int sl = used++; slot[sl] = w; h.push_back({key, sl}); push_heap(h.begin(), h.end(), cmp); }
      else if (key > h.front().first) { pop_heap(h.begin(), h.end(), cmp); int sl = h.back().second; slot[sl] = w; h.back() = {key, sl}; push_heap(h.begin(), h.end(), cmp); }
    };
    if (!prune) { for (u32 b = bl; b <= br; ++b) decode(b, p, emit); }
    else {
      // best-first over segment-tree nodes intersecting [bl, br]
      priority_queue<tuple<u32,u32,u32,u32>> pq;   // (bound, node, lo, hi)
      pq.push({tree[1], 1, 0, S - 1});
      while (!pq.empty()) {
        auto [bd, v, a, b] = pq.top(); pq.pop();
        if ((int)h.size() == k && h.front().first >= mkkey(bd, 0)) break;
        if (v >= S) { decode(v - S, p, emit); continue; }
        u32 m = (a + b) >> 1;
        if (!(m < bl || a > br)) pq.push({tree[2 * v], 2 * v, a, m});
        if (!(b < bl || m + 1 > br)) pq.push({tree[2 * v + 1], 2 * v + 1, m + 1, b});
      }
    }
    sort(h.begin(), h.end(), [](auto& a, auto& b) { return a.first > b.first; });
    r.strs.resize(h.size());
    for (size_t i = 0; i < h.size(); ++i) { r.ids.push_back(~u32(h[i].first)); r.strs[i] = slot[h[i].second]; }
  }
};


// FC-BM (final): split block layout [VByte scores of the B entries][front-coded strings].
// Queries scan only scores of candidate blocks and materialise strings for the k winners at the end.
// Usage contract of this research implementation: build() is called once, on a default-constructed
// object. The implementation is single-threaded: query() uses the member scratch string `cur`,
// function-static scratch vectors (shared by ALL FCBM2 instances) and the global counter g_blocks.
// Concurrent calls are therefore unsafe even on separate FCBM2 objects. Concurrent use requires
// query-local or thread_local scratch storage and removing g_blocks; this was not benchmarked.
struct FCBM2 {
  u32 B = 32; bool prune = true;
  u32 n = 0, nb = 0, S = 1;
  vector<u8> data; vector<u32> boff, tree;
  string cur;                                   // scratch
  void build(const Dict& d) {
    n = d.w.size(); nb = (n + B - 1) / B; boff.reserve(nb + 1);
    while (S < nb) S <<= 1; tree.assign(2 * S, 0);
    for (u32 b = 0; b < nb; ++b) {
      boff.push_back(data.size()); u32 e = min(n, (b + 1) * B), mx = 0;
      { const string& w = d.w[b * B]; vb_put(data, w.size()); data.insert(data.end(), w.begin(), w.end()); }
      for (u32 i = b * B; i < e; ++i) { vb_put(data, d.s[i]); mx = max(mx, d.s[i]); }
      for (u32 i = b * B + 1; i < e; ++i) {
        const string& w = d.w[i];
        const string& pw = d.w[i - 1]; u32 l = 0; while (l < pw.size() && l < w.size() && pw[l] == w[l]) ++l;
        vb_put(data, l); vb_put(data, w.size() - l); data.insert(data.end(), w.begin() + l, w.end());
      }
      tree[S + b] = mx;
    }
    boff.push_back(data.size()); data.shrink_to_fit();
    for (u32 v = S - 1; v >= 1; --v) tree[v] = max(tree[2 * v], tree[2 * v + 1]);
  }
  inline u32 bsize(u32 b) const { return min(n, (b + 1) * B) - b * B; }
  // index of the block with maximum score in [a, b] (inclusive), leftmost on ties
  inline u32 argmax(u32 a, u32 b) const {
    u32 l = a + S, r = b + S + 1, best = l, bv = tree[l];
    static u32 rs[64]; int nr = 0;
    for (; l < r; l >>= 1, r >>= 1) {
      if (l & 1) { if (tree[l] > bv) { bv = tree[l]; best = l; } ++l; }
      if (r & 1) rs[nr++] = --r;
    }
    while (nr) { u32 v = rs[--nr]; if (tree[v] > bv) { bv = tree[v]; best = v; } }
    while (best < S) best = tree[2 * best] == bv ? 2 * best : 2 * best + 1;
    return best - S;
  }
  // block layout: [header string][VByte scores x bsize][front-coded entries 1..bsize-1]
  inline const u8* scores_of(u32 b) const { const u8* q = data.data() + boff[b]; u32 len = vb_get(q); return q + len; }
  inline const u8* skip_scores(const u8* q, u32 c) const { while (c) { if (!(*q & 128)) --c; ++q; } return q; }
  string_view header(u32 b) const { const u8* p = data.data() + boff[b]; u32 len = vb_get(p); return string_view((const char*)p, len); }
  // decode block b's strings; call f(j, cur) for j = 0..; stop when f returns false
  template <class F> void walk(u32 b, F&& f) {
    const u8* q = data.data() + boff[b]; u32 c = bsize(b);
    for (u32 j = 0; j < c; ++j) {
      if (j == 0) { u32 len = vb_get(q); cur.assign((const char*)q, len); q += len; q = skip_scores(q, c); }
      else { u32 l = vb_get(q), sl = vb_get(q); cur.resize(l); cur.append((const char*)q, sl); q += sl; }
      if (!f(j, cur)) return;
    }
  }
  // first global index whose string satisfies !pred; pred monotone (true ... false)
  template <class P> u32 bound(P&& pred) {
    u32 lo = 0, hi = nb;   // first block whose header fails pred
    while (lo < hi) { u32 m = (lo + hi) >> 1; if (pred(header(m))) lo = m + 1; else hi = m; }
    if (lo == 0) return 0;
    u32 b = lo - 1, res = b * B + bsize(b);
    walk(b, [&](u32 j, const string& w) { if (!pred(string_view(w))) { res = b * B + j; return false; } return true; });
    return res;
  }
  void query(const string& p, int k, Result& r) {
    if (k <= 0) { r.ids.clear(); r.strs.clear(); return; }
    r.ids.clear(); string_view pv(p);
    u32 L = bound([&](string_view w) { return w < pv; });
    u32 R = bound([&](string_view w) { return w.substr(0, pv.size()) <= pv; });
    if (L >= R) { r.strs.resize(0); return; }
    u32 bl = L / B, br = (R - 1) / B;
    static vector<u64> h; h.clear();                        // min-heap of exact keys
    auto scan = [&](u32 b) {
      ++g_blocks; const u8* q = scores_of(b); u32 base = b * B, c = bsize(b);
      u32 a = L > base ? L - base : 0, e = min(c, R - base);
      for (u32 j = 0; j < c; ++j) {
        u32 s = vb_get(q); if (j < a) continue; if (j >= e) break;
        u64 key = mkkey(s, base + j);
        if ((int)h.size() < k) { h.push_back(key); push_heap(h.begin(), h.end(), greater<>()); }
        else if (key > h.front()) { pop_heap(h.begin(), h.end(), greater<>()); h.back() = key; push_heap(h.begin(), h.end(), greater<>()); }
      }
    };
    if (!prune) { for (u32 b = bl; b <= br; ++b) scan(b); }
    else {
      // best-first over block intervals (Hsu-Ottaviano style), RMQ answered by the segment tree
      // Max-heap of block intervals ordered by (block maximum desc, argmax block index asc).
      // argmax() returns the leftmost maximal block, so every block left of m in [a, b] has a
      // strictly smaller maximum, and every entry of [m, b] has index >= m*B. Hence
      // (M[m], max(L, m*B)) bounds the key of every completion in the interval (Lemma 1).
      static vector<tuple<u64,u32,u32,u32>> pq; pq.clear();      // (order key, argmax block, lo, hi)
      auto push = [&](u32 a, u32 b) { u32 m = argmax(a, b); pq.push_back({mkkey(tree[S + m], m), m, a, b}); push_heap(pq.begin(), pq.end()); };
      push(bl, br);
      while (!pq.empty()) {
        pop_heap(pq.begin(), pq.end()); auto [ok, m, a, b] = pq.back(); pq.pop_back();
        if ((int)h.size() == k && h.front() >= mkkey(tree[S + m], max(L, m * B))) continue;
        scan(m);
        if (m > a) push(a, m - 1);
        if (m < b) push(m + 1, b);
      }
    }
    sort(h.begin(), h.end(), greater<>());
    r.strs.resize(h.size());
    for (auto key : h) r.ids.push_back(~u32(key));
    // materialise: group winners by block, decode each block once up to its last winner
    static vector<pair<u32,u32>> ord; ord.clear();
    for (u32 i = 0; i < r.ids.size(); ++i) ord.push_back({r.ids[i], i});
    sort(ord.begin(), ord.end());
    for (size_t x = 0; x < ord.size();) {
      u32 b = ord[x].first / B; size_t y = x;
      walk(b, [&](u32 j, const string& w) {
        while (y < ord.size() && ord[y].first == b * B + j) { r.strs[ord[y].second] = w; ++y; }
        return y < ord.size() && ord[y].first / B == b;
      });
      x = y;
    }
  }
};


// B5: Marisa-trie (published library) + score array; exhaustive predictive search
struct MarisaDFS {
  marisa::Trie trie; vector<u32> score, lexid;    // lexid only for result verification (excluded from footprint)
  size_t exclude_bytes() const { return lexid.capacity() * 4; }
  void build(const Dict& d) {
    { marisa::Keyset ks; for (u32 i = 0; i < d.w.size(); ++i) ks.push_back(d.w[i].data(), d.w[i].size(), float(d.s[i])); trie.build(ks); }
    score.assign(d.w.size(), 0); lexid.assign(d.w.size(), 0);
    marisa::Agent ag;
    for (u32 i = 0; i < d.w.size(); ++i) { ag.set_query(d.w[i].data(), d.w[i].size()); trie.lookup(ag); score[ag.key().id()] = d.s[i]; lexid[ag.key().id()] = i; }
  }
  void query(const string& p, int k, Result& r) {
    if (k <= 0) { r.ids.clear(); r.strs.clear(); return; }
    r.ids.clear(); static marisa::Agent ag; ag.set_query(p.data(), p.size());
    static vector<pair<u64,u32>> h; h.clear(); auto cmp = greater<pair<u64,u32>>();
    while (trie.predictive_search(ag)) {
      u32 mid = ag.key().id(); u64 key = mkkey(score[mid], lexid[mid]);
      if ((int)h.size() < k) { h.push_back({key, mid}); push_heap(h.begin(), h.end(), cmp); }
      else if (key > h.front().first) { pop_heap(h.begin(), h.end(), cmp); h.back() = {key, mid}; push_heap(h.begin(), h.end(), cmp); }
    }
    sort(h.begin(), h.end(), greater<>()); r.strs.resize(h.size());
    static marisa::Agent rv;
    for (size_t i = 0; i < h.size(); ++i) { r.ids.push_back(lexid[h[i].second]); rv.set_query(size_t(h[i].second)); trie.reverse_lookup(rv); r.strs[i].assign(rv.key().ptr(), rv.key().length()); }
  }
};

// ---------------------------------------------------------------- driver
static Dict load(const string& f) {
  ifstream in(f); string line; vector<pair<string,u32>> v;
  while (getline(in, line)) { auto t = line.find('\t'); v.push_back({line.substr(0, t), u32(stoul(line.substr(t + 1)))}); }
  sort(v.begin(), v.end());
  v.erase(unique(v.begin(), v.end(), [](auto& a, auto& b) { return a.first == b.first; }), v.end());
  Dict d; for (auto& [w, s] : v) { d.w.push_back(w); d.s.push_back(s); } return d;
}
static vector<string> make_queries(const Dict& d, size_t q, u64 seed) {
  mt19937_64 rng(seed); discrete_distribution<u32> pick(d.s.begin(), d.s.end());
  vector<string> out; out.reserve(q);
  while (out.size() < q) {
    const string& w = d.w[pick(rng)];
    vector<size_t> cut; for (size_t i = 1; i <= w.size(); ++i) if (i == w.size() || (u8(w[i]) & 0xC0) != 0x80) cut.push_back(i);
    size_t L = min<size_t>(cut.size(), 8); uniform_int_distribution<size_t> u(1, L);
    out.push_back(w.substr(0, cut[u(rng) - 1]));
  }
  return out;
}
static size_t nchars(const string& s) { size_t c = 0; for (u8 x : s) if ((x & 0xC0) != 0x80) ++c; return c; }

template <class M> struct Bench {
  static void run(const string& ds, const string& name, const string& param, M& m, const Dict& d,
                  const vector<string>& qs, const vector<int>& ks, vector<vector<u32>>* ref, int refk,
                  ofstream& csv, ofstream& bylen, size_t& bytes_out, double& build_out, bool doBuild = true) {
    size_t before = heap_used(); auto t0 = chrono::steady_clock::now();
    if (doBuild) m.build(d);
    double bt = doBuild ? chrono::duration<double>(chrono::steady_clock::now() - t0).count() : build_out;
    size_t bytes = doBuild ? heap_used() - before : bytes_out;
    if constexpr (requires { m.exclude_bytes(); }) bytes -= m.exclude_bytes(); bytes_out = bytes; build_out = bt;
    Result r;
    for (int k : ks) {
      for (size_t i = 0; i < min<size_t>(qs.size(), 20000); ++i) m.query(qs[i], k, r);  // warm-up
      vector<double> ns(qs.size()); u64 chk = 0; size_t mism = 0; g_blocks = 0;
      for (size_t i = 0; i < qs.size(); ++i) {
        auto a = chrono::steady_clock::now(); m.query(qs[i], k, r); auto b = chrono::steady_clock::now();
        ns[i] = chrono::duration<double, nano>(b - a).count();
        for (auto id : r.ids) chk = chk * 1000003 + id;
        for (auto& s : r.strs) chk += s.size();
        if (ref && k == refk) { if (r.ids != (*ref)[i]) ++mism; }
        for (size_t j = 0; j < r.ids.size(); ++j) if (r.strs[j] != d.w[r.ids[j]]) ++mism;
      }
      vector<double> srt = ns; sort(srt.begin(), srt.end());
      double mean = accumulate(ns.begin(), ns.end(), 0.0) / ns.size();
      auto pct = [&](double q) { return srt[size_t(q * (srt.size() - 1))] / 1000.0; };
      csv << ds << ',' << name << ',' << param << ',' << k << ',' << d.w.size() << ',' << bytes << ','
          << fixed << setprecision(4) << bt << ',' << mean / 1000 << ',' << pct(0.5) << ',' << pct(0.95) << ',' << pct(0.99)
          << ',' << mism << ',' << chk << ',' << double(g_blocks) / qs.size() << '\n';
      csv.flush();
      cerr << ds << ' ' << name << ' ' << param << " k=" << k << " MB=" << bytes / 1e6 << " mean_us=" << mean / 1000
           << " p99=" << pct(0.99) << " mism=" << mism << '\n';
      if (k == 10) {
        map<size_t, pair<double,size_t>> agg;
        for (size_t i = 0; i < qs.size(); ++i) { auto& a = agg[nchars(qs[i])]; a.first += ns[i]; a.second++; }
        for (auto& [L, a] : agg) bylen << ds << ',' << name << ',' << param << ',' << L << ',' << a.first / a.second / 1000 << ',' << a.second << '\n';
      }
    }
  }
};

#ifndef QAC_NO_MAIN
int main(int argc, char** argv) {
  string mode = argc > 1 ? argv[1] : "main";
  vector<string> dsets = {"tr", "en", "de", "ru", "all"}; if (argc > 2) { dsets.clear(); for (int i = 2; i < argc; ++i) dsets.push_back(argv[i]); }
  ofstream csv("results/" + mode + ".csv"), bylen("results/" + mode + "_bylen.csv");
  csv << "dataset,method,param,k,n,bytes,build_s,mean_us,p50_us,p95_us,p99_us,mismatches,checksum,blocks_per_query\n";
  bylen << "dataset,method,param,prefix_chars,mean_us,count\n";
  ofstream info("results/" + mode + "_datasets.csv");
  info << "dataset,n,raw_bytes,trie_nodes,queries,empty_queries,mean_matches\n";
  const size_t Q = 100000;
  for (auto& ds : dsets) {
    Dict d = load("data/" + ds + ".tsv");
    auto qs = make_queries(d, Q, 20261009);
    size_t raw = 0; for (auto& w : d.w) raw += w.size();
    size_t B; double bt;
    // reference answers from the exhaustive baseline at k=10
    vector<vector<u32>> ref(qs.size());
    TrieDFS* dfs = new TrieDFS();
    { Result r; size_t before = heap_used(); auto t0 = chrono::steady_clock::now(); dfs->build(d); B = heap_used() - before; bt = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
      size_t empty = 0;
      for (size_t i = 0; i < qs.size(); ++i) { dfs->query(qs[i], 10, r); ref[i] = r.ids; if (r.ids.empty()) ++empty; }
      info << ds << ',' << d.w.size() << ',' << raw << ',' << dfs->t.nodes() << ',' << qs.size() << ',' << empty << ",\n"; info.flush(); }
    if (mode == "main") {
      vector<int> ks = {1, 5, 10, 20, 50};
      Bench<TrieDFS>::run(ds, "Trie-DFS", "-", *dfs, d, qs, ks, &ref, 10, csv, bylen, B, bt, false);
      delete dfs; malloc_trim(0);
      { auto* m = new TrieMax(); Bench<TrieMax>::run(ds, "Trie-Max", "-", *m, d, qs, ks, &ref, 10, csv, bylen, B, bt); delete m; malloc_trim(0); }
      { auto* m = new TrieCache(); Bench<TrieCache>::run(ds, "Trie-TopK", "K=10", *m, d, qs, {1, 5, 10}, &ref, 10, csv, bylen, B, bt); delete m; malloc_trim(0); }
      { auto* m = new SARMQ(); Bench<SARMQ>::run(ds, "SA-RMQ", "-", *m, d, qs, ks, &ref, 10, csv, bylen, B, bt); delete m; malloc_trim(0); }
      if (!getenv("SKIP_MARISA")) { auto* m = new MarisaDFS(); Bench<MarisaDFS>::run(ds, "Marisa", "-", *m, d, qs, ks, &ref, 10, csv, bylen, B, bt); delete m; malloc_trim(0); }
      for (u32 b : {8u, 32u}) { auto* m = new FCBM2(); m->B = b; Bench<FCBM2>::run(ds, "FC-BM", "B=" + to_string(b), *m, d, qs, ks, &ref, 10, csv, bylen, B, bt); delete m; malloc_trim(0); }
    } else if (mode == "fcbm") {
      vector<int> ks = {1, 5, 10, 20, 50};
      delete dfs; malloc_trim(0);
      for (u32 b : {8u, 32u}) { auto* m = new FCBM2(); m->B = b; Bench<FCBM2>::run(ds, "FC-BM", "B=" + to_string(b), *m, d, qs, ks, &ref, 10, csv, bylen, B, bt); delete m; malloc_trim(0); }
    } else if (mode == "stat") {
      // 10 interleaved timing runs at k = 10 (mean latency per run), method order rotated each run
      delete dfs; malloc_trim(0);
      SARMQ sa; sa.build(d); FCBM2 f8; f8.B = 8; f8.build(d); FCBM2 f32; f32.B = 32; f32.build(d);
      auto timeit = [&](auto& m) { Result r; for (size_t i = 0; i < 20000; ++i) m.query(qs[i], 10, r);
        auto a = chrono::steady_clock::now(); u64 c = 0; for (auto& q : qs) { m.query(q, 10, r); c += r.ids.size(); }
        auto b = chrono::steady_clock::now(); if (c == 0) cerr << "?"; return chrono::duration<double, micro>(b - a).count() / qs.size(); };
      for (int run = 0; run < 10; ++run) {
        double t[3];
        for (int j = 0; j < 3; ++j) { int w = (j + run) % 3; t[w] = w == 0 ? timeit(sa) : w == 1 ? timeit(f8) : timeit(f32); }
        csv << ds << ",stat,run" << run << ",10," << d.w.size() << "," << t[0] << "," << t[1] << "," << t[2] << ",0,0,0,0,0,0\n"; csv.flush();
        cerr << ds << " run " << run << " SA-RMQ " << t[0] << " FC-BM8 " << t[1] << " FC-BM32 " << t[2] << "\n";
      }
    } else if (mode == "dump") {
      // queries + reference top-50 ids for external harnesses
      ofstream fq("data/" + ds + ".queries"), fr("data/" + ds + ".ref50");
      Result r;
      for (size_t i = 0; i < qs.size(); ++i) { fq << qs[i] << '\n'; dfs->query(qs[i], 50, r); for (size_t j = 0; j < r.ids.size(); ++j) fr << (j ? " " : "") << r.ids[j]; fr << '\n'; }
      delete dfs; malloc_trim(0);
    } else if (mode == "marisa") {
      delete dfs; malloc_trim(0);
      vector<int> ks = {1, 5, 10, 20, 50};
      { auto* m = new MarisaDFS(); Bench<MarisaDFS>::run(ds, "Marisa", "-", *m, d, qs, ks, &ref, 10, csv, bylen, B, bt); delete m; malloc_trim(0); }
    } else if (mode == "beta") {
      delete dfs; malloc_trim(0);
      for (u32 b : {8u, 32u}) for (int k : {1, 10, 50}) {
        FCBM2 m; m.B = b; m.build(d); Result r; vector<u64> bs; size_t over = 0, interior_over = 0;
        for (auto& q : qs) { u64 g0 = g_blocks; m.query(q, k, r); u64 x = g_blocks - g0; bs.push_back(x); if (x > u64(k) + 2) ++over; }
        sort(bs.begin(), bs.end());
        double mean = accumulate(bs.begin(), bs.end(), 0.0) / bs.size();
        csv << ds << ",FC-BM,B=" << b << ',' << k << ',' << mean << ',' << bs[bs.size() / 2] << ',' << bs[size_t(0.99 * (bs.size() - 1))] << ',' << bs.back() << ',' << over << '\n';
        cerr << ds << " B=" << b << " k=" << k << " mean=" << mean << " max=" << bs.back() << " over=" << over << '\n';
      }
    } else if (mode == "ablation") {
      delete dfs; malloc_trim(0);
      for (u32 b : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
        for (int var = 0; var < 4; ++var) {
          string nm = var == 0 ? "FC-BM" : var == 1 ? "FC-BM/no-prune" : var == 2 ? "FC-BM/interleaved" : "FC-BM/interleaved+fixed32";
          if (var <= 1) { auto* m = new FCBM2(); m->B = b; m->prune = var == 0; Bench<FCBM2>::run(ds, nm, "B=" + to_string(b), *m, d, qs, {10}, &ref, 10, csv, bylen, B, bt); delete m; }
          else { auto* m = new FCBM(); m->B = b; m->vbyteScores = var == 2; Bench<FCBM>::run(ds, nm, "B=" + to_string(b), *m, d, qs, {10}, &ref, 10, csv, bylen, B, bt); delete m; }
          malloc_trim(0);
        }
    }
  }
}
#endif
