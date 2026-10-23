// CCC-26 level 6 solver: simulated annealing over emitter walks.
//
// Model: every step applies the active rays; a cell's final content after a
// sequence of steps depends only on its LAST "significant" event
// (count==2 -> matter, count>=3 -> empty). So a walk s_1..s_k produces the
// pattern from an empty board iff, for every cell, the last significant event
// matches the target. Played backwards (ping-pong) the FIRST event decides.
//
// Modes per case:
//   chain   : copies are consecutive walks, each starting where the previous
//             one ended (first one from the initial state). Only "last" matters.
//   pingpong: one walk W (k steps) + setup S; copies alternate W, reverse(W).
//             Total = S + k*req. Requires first AND last event to be right.
//
// usage: solver INPUT CASE_FROM CASE_TO SECONDS_PER_CASE SEED OUTDIR
#include <bits/stdc++.h>
using namespace std;

static mt19937_64 rng;
static inline uint64_t rnd() { return rng(); }
static inline int rndi(int n) { return (int)(rnd() % (uint64_t)n); }
static inline double rndf() { return (rnd() >> 11) * (1.0 / 9007199254740992.0); }

struct Case {
    int R, rows, cols, req;
    vector<string> pat;
};

struct Geo {
    int R, rad, rows, cols, N, P;
    vector<int> cellId;                 // r*cols+c -> interior idx or -1
    vector<pair<int,int>> rc;           // idx -> (r,c)
    vector<vector<int>> ray[6];         // ray[t][p+R] interior cells, p in [-R,R]
    vector<vector<int>> rayAll[6];      // full ray (overlay coords) per perimeter index, for exact sim
    vector<pair<int,int>> perim;
    vector<string> base;
    vector<char> target;                // per interior cell
    int nTarget;
};

static const int DC[6] = {2, 1, -1, -2, -1, 1};
static const int DR[6] = {0, 1, 1, 0, -1, -1};

Geo buildGeo(const Case& cs) {
    Geo g;
    g.R = cs.R; g.rad = cs.R + 1; g.rows = 2 * g.rad - 1; g.cols = 4 * g.rad - 3;
    int rad = g.rad, cols = g.cols;
    int pd[6] = {5, 0, 1, 2, 3, 4};
    int c = 0, r = rad - 1;
    for (int k = 0; k < 6; k++)
        for (int i = 0; i < rad - 1; i++) { g.perim.push_back({c, r}); c += DC[pd[k]]; r += DR[pd[k]]; }
    g.P = g.perim.size();
    auto inGrid = [&](int c, int r) {
        if (r < 0 || r >= 2 * rad - 1) return false;
        if (((r + c + rad + 1) % 2 + 2) % 2) return false;
        if (r < rad) return c + r >= rad - 1 && c - r <= cols - rad + 1;
        return c + (2 * rad - 1 - r) >= rad - 1 && c + r - 2 * rad + 1 < cols - rad + 1;
    };
    g.base.assign(g.rows, string(cols, '-'));
    for (int r = 0; r < g.rows; r++) {
        bool rRail = r == 0 || r == g.rows - 1;
        for (int c = 0; c < cols; c++) {
            if (((r + c + rad + 1) % 2 + 2) % 2) { g.base[r][c] = '-'; continue; }
            char ch;
            if (r < rad) {
                bool cRail = c == rad - 1 - r || c == cols - 1 - rad + 1 + r;
                if (c + r >= rad - 1 && c - r <= cols - rad + 1) ch = (rRail || cRail) ? '#' : 'O'; else ch = ':';
            } else {
                bool cRail = c == rad - 1 + r - 2 * rad + 2 || c == cols - 1 + rad - 1 - r;
                if (c + 2 * rad - 1 - r >= rad - 1 && c + r - 2 * rad + 1 < cols - rad + 1) ch = (rRail || cRail) ? '#' : 'O'; else ch = ':';
            }
            g.base[r][c] = ch;
        }
    }
    g.cellId.assign(g.rows * cols, -1);
    for (int r = 0; r < g.rows; r++)
        for (int c = 0; c < cols; c++)
            if (g.base[r][c] == 'O') { g.cellId[r * cols + c] = g.rc.size(); g.rc.push_back({r, c}); }
    g.N = g.rc.size();
    for (int t = 0; t < 6; t++) {
        g.rayAll[t].resize(g.P);
        for (int i = 0; i < g.P; i++) {
            int c0 = g.perim[i].first + DC[t], r0 = g.perim[i].second + DR[t];
            while (inGrid(c0, r0)) { g.rayAll[t][i].push_back(r0 * cols + c0); c0 += DC[t]; r0 += DR[t]; }
        }
        g.ray[t].resize(2 * g.R + 1);
        for (int p = -g.R; p <= g.R; p++) {
            int idx = ((t * g.R + p) % g.P + g.P) % g.P;
            for (int rc : g.rayAll[t][idx]) if (g.cellId[rc] >= 0) g.ray[t][p + g.R].push_back(g.cellId[rc]);
        }
    }
    g.target.assign(g.N, 0);
    g.nTarget = 0;
    for (int i = 0; i < g.N; i++) {
        char ch = cs.pat[g.rc[i].first][g.rc[i].second];
        if (ch == 'M') { g.target[i] = 1; g.nTarget++; }
    }
    return g;
}

// exact simulation (full mask) of a program; returns accepted count; stops when req reached and sets usedLines
int simulateExact(const Geo& g, const vector<array<char,6>>& prog, int req, int& usedLines) {
    vector<int> idx(6); vector<char> act(6, 0);
    for (int t = 0; t < 6; t++) idx[t] = t * g.R;
    vector<char> grid(g.rows * g.cols);
    for (int r = 0; r < g.rows; r++) for (int c = 0; c < g.cols; c++) grid[r * g.cols + c] = g.base[r][c];
    vector<int> hit(g.rows * g.cols, 0);
    int acc = 0; usedLines = prog.size();
    for (size_t s = 0; s < prog.size(); s++) {
        for (auto& ch : grid) { if (ch == 'x' || ch == 'd') ch = 'O'; else if (ch == 'm') ch = 'M'; }
        for (int t = 0; t < 6; t++) {
            char in = prog[s][t];
            if (in == 'F') idx[t] = (idx[t] + 1) % g.P;
            else if (in == 'B') idx[t] = (idx[t] + g.P - 1) % g.P;
            else if (in == 'T') act[t] = !act[t];
        }
        vector<int> touched;
        for (int t = 0; t < 6; t++) if (act[t]) for (int rc : g.rayAll[t][idx[t]]) { if (!hit[rc]) touched.push_back(rc); hit[rc]++; }
        for (int rc : touched) {
            char v = grid[rc]; int n = hit[rc]; hit[rc] = 0;
            if (v != 'O' && v != 'M') continue;
            if (n == 2) grid[rc] = (v == 'O') ? 'm' : 'M';
            else if (n >= 3) grid[rc] = 'd';
        }
        bool ok = true;
        for (int i = 0; i < g.N && ok; i++) {
            int rc = g.rc[i].first * g.cols + g.rc[i].second;
            bool m = grid[rc] == 'M' || grid[rc] == 'm';
            if (m != (bool)g.target[i]) ok = false;
        }
        if (ok) {
            for (int i = 0; i < g.N; i++) if (g.target[i]) grid[g.rc[i].first * g.cols + g.rc[i].second] = 'x';
            acc++;
            if (acc >= req) { usedLines = s + 1; return acc; }
        }
    }
    return acc;
}

struct St { int8_t p, a; };
static inline bool adj(St x, St y) { return abs(x.p - y.p) + abs(x.a - y.a) <= 1; }

struct Walk {
    int k;
    vector<St> s[6]; // index 0..k ; 0 = start state
};

struct Solver {
    const Geo& g;
    bool ping;    // free start + setup cost (PING or CYCLE)
    bool cyc;     // cyclic walk: step K is followed by step 1 (forward-only objective)
    bool firstEv; // also require first event (PING)
    bool fixedEnd = false; // chain mode: state at step K is frozen
    St endSt[6];
    St start[6];  // fixed start (chain mode)
    int K;
    vector<St> s[6];
    vector<uint8_t> cnt;   // cnt[cell*KS + i], i in 1..K
    int KS;
    vector<int> viol;      // per cell
    long long V;           // total violations
    int S;                 // setup cost (ping)
    double wS;
    vector<int> stamp; int stampId = 0;
    vector<int> touched;

    // mode: 0 chain, 1 ping-pong, 2 cycle
    Solver(const Geo& g_, int mode) : g(g_), ping(mode != 0), cyc(mode == 2), firstEv(mode == 1) {}
    inline St* prevS(int t, int i) { if (i > 1) return &s[t][i - 1]; if (!ping) return &s[t][0]; if (cyc) return &s[t][K]; return nullptr; }
    inline St* nextS(int t, int i) { if (i < K) return &s[t][i + 1]; if (cyc) return &s[t][1]; return nullptr; }

    inline const vector<int>* line(int t, St x) const {
        if (!x.a || abs(x.p) >= g.R) return nullptr;
        return &g.ray[t][x.p + g.R];
    }
    int cellViol(int c) const {
        const uint8_t* a = &cnt[(size_t)c * KS];
        int last = 0, first = 0;
        for (int i = K; i >= 1; i--) if (a[i] >= 2) { last = a[i]; break; }
        int v = 0;
        bool tg = g.target[c];
        if (tg ? last != 2 : last == 2) v++;
        if (firstEv) {
            for (int i = 1; i <= K; i++) if (a[i] >= 2) { first = a[i]; break; }
            if (tg ? first != 2 : first == 2) v++;
        }
        return v;
    }
    int setupCost() const {
        int m = 0;
        for (int t = 0; t < 6; t++) { int d = abs(s[t][1].p); int need = s[t][1].a ? d : d - 1; m = max(m, need); }
        return m;
    }
    double cost() const { return V + (ping ? wS * S : 0.0); }
    void rebuild() {
        KS = K + 1;
        cnt.assign((size_t)g.N * KS, 0);
        for (int t = 0; t < 6; t++) for (int i = 1; i <= K; i++) { auto L = line(t, s[t][i]); if (L) for (int c : *L) cnt[(size_t)c * KS + i]++; }
        viol.assign(g.N, 0); V = 0;
        for (int c = 0; c < g.N; c++) { viol[c] = cellViol(c); V += viol[c]; }
        S = ping ? setupCost() : 0;
        stamp.assign(g.N, 0);
    }
    void init(int k, const St* st) {
        K = k;
        for (int t = 0; t < 6; t++) { s[t].assign(K + 1, st ? st[t] : St{0, 0}); }
        if (st) for (int t = 0; t < 6; t++) start[t] = st[t];
        rebuild();
    }
    // apply change of emitter t at step i to state ns, updating cnt; collects touched cells
    inline void setState(int t, int i, St ns) {
        St os = s[t][i];
        auto Lo = line(t, os), Ln = line(t, ns);
        if (Lo != Ln) {
            if (Lo) for (int c : *Lo) { cnt[(size_t)c * KS + i]--; if (stamp[c] != stampId) { stamp[c] = stampId; touched.push_back(c); } }
            if (Ln) for (int c : *Ln) { cnt[(size_t)c * KS + i]++; if (stamp[c] != stampId) { stamp[c] = stampId; touched.push_back(c); } }
        }
        s[t][i] = ns;
    }
    long long recompute(vector<int>& oldViol) {
        long long dv = 0;
        oldViol.resize(touched.size());
        for (size_t j = 0; j < touched.size(); j++) { int c = touched[j]; oldViol[j] = viol[c]; int nv = cellViol(c); dv += nv - viol[c]; viol[c] = nv; }
        V += dv;
        return dv;
    }

    bool validBounds(St x) const { return x.p >= -g.R && x.p <= g.R; }

    // check adjacency of emitter t's trajectory when steps i..j are replaced by ch (in order)
    bool segValid(int t, int i, int j, const vector<tuple<int,int,St>>& ch) {
        auto get = [&](int q) -> St { if (q >= i && q <= j) return std::get<2>(ch[q - i]); return s[t][q]; };
        // transitions into q for q in [i, j+1]
        for (int q = i; q <= j + 1; q++) {
            int a, b;
            if (q == K + 1) { if (!cyc || j != K) continue; a = K; b = 1; }
            else if (q == 1) { if (cyc) { a = K; b = 1; } else if (!ping) { if (!adj(s[t][0], get(1))) return false; continue; } else continue; }
            else { a = q - 1; b = q; }
            if (q > K) continue;
            if (!adj(get(a), get(b))) return false;
        }
        if (cyc && j == K && i > 1) { if (!adj(get(K), get(1))) return false; }
        return true;
    }

    // run SA for given time; returns true if V==0 reached (keeps best)
    bool anneal(double seconds, double T0, double T1) {
        auto t0 = chrono::steady_clock::now();
        vector<St> best[6]; for (int t = 0; t < 6; t++) best[t] = s[t];
        double bestCost = cost();
        vector<int> oldViol;
        vector<tuple<int,int,St>> changes;
        long long it = 0;
        double T = T0, frac = 0;
        while (true) {
            if ((it & 255) == 0) {
                frac = chrono::duration<double>(chrono::steady_clock::now() - t0).count() / seconds;
                if (frac >= 1) break;
                T = T0 * pow(T1 / T0, frac);
                if (bestCost == 0 && !ping) break;
                if (ping && V == 0 && bestCost <= 0) break;
            }
            it++;
            int t = rndi(6);
            changes.clear();
            int mt = rndi(10);
            if (mt < 5) {
                int i = 1 + rndi(K);
                St cur = s[t][i];
                St cand[6]; int nc = 0;
                St* pv = prevS(t, i);
                St* nx = nextS(t, i);
                if (K == 1 && cyc) pv = nx = nullptr;
                St opts[5] = {{(int8_t)(cur.p + 1), cur.a}, {(int8_t)(cur.p - 1), cur.a}, {cur.p, (int8_t)(1 - cur.a)}, cur, cur};
                int no = 3;
                if (pv && nx) { opts[3] = {pv->p, nx->a}; opts[4] = {nx->p, pv->a}; no = 5; }
                for (int o = 0; o < no; o++) {
                    St x = opts[o];
                    if (x.p == cur.p && x.a == cur.a) continue;
                    if (!validBounds(x)) continue;
                    if (pv && !adj(*pv, x)) continue;
                    if (nx && !adj(x, *nx)) continue;
                    cand[nc++] = x;
                }
                if (!nc) continue;
                changes.push_back({t, i, cand[rndi(nc)]});
            } else if (mt < 8) {
                // segment shift / flip
                int i = 1 + rndi(K);
                int len = 1 + rndi(min(K - i + 1, 1 + rndi(K)));
                int j = i + len - 1;
                int kind = rndi(3);
                bool ok = true;
                for (int q = i; q <= j && ok; q++) {
                    St x = s[t][q];
                    if (kind == 0) x.p++; else if (kind == 1) x.p--; else x.a = 1 - x.a;
                    if (!validBounds(x)) ok = false;
                    changes.push_back({t, q, x});
                }
                if (!ok || !segValid(t, i, j, changes)) continue;
            } else {
                // drag: new[q] = old[q+1] or old[q-1]
                int i = 1 + rndi(K);
                int len = 1 + rndi(min(K - i + 1, 6));
                int j = i + len - 1;
                int dir = rndi(2);
                bool ok = true;
                for (int q = i; q <= j; q++) {
                    int src = dir == 0 ? q + 1 : q - 1;
                    St x;
                    if (src > K) { if (!cyc) { ok = false; break; } x = s[t][1]; }
                    else if (src < 1) { if (cyc) x = s[t][K]; else if (!ping) x = s[t][0]; else { ok = false; break; } }
                    else x = s[t][src];
                    changes.push_back({t, q, x});
                }
                if (!ok || !segValid(t, i, j, changes)) continue;
            }
            if (fixedEnd) { bool bad = false; for (auto& ch : changes) if (get<1>(ch) == K) bad = true; if (bad) continue; }
            // apply
            stampId++; touched.clear();
            vector<St>& tr = s[t];
            static vector<St> saved; saved.clear();
            for (auto& ch : changes) { saved.push_back(tr[get<1>(ch)]); setState(get<0>(ch), get<1>(ch), get<2>(ch)); }
            double oldCost = cost();
            int oldS = S;
            long long dv = recompute(oldViol);
            if (ping) S = setupCost();
            double nc = cost();
            double delta = nc - oldCost;
            if (delta <= 0 || rndf() < exp(-delta / T)) {
                if (nc < bestCost) { bestCost = nc; for (int q = 0; q < 6; q++) best[q] = s[q]; }
            } else {
                // revert
                stampId++; touched.clear();
                for (int q = (int)changes.size() - 1; q >= 0; q--) setState(get<0>(changes[q]), get<1>(changes[q]), saved[q]);
                for (size_t q = 0; q < touched.size(); q++) { int c = touched[q]; int nv = cellViol(c); V += nv - viol[c]; viol[c] = nv; }
                S = oldS;
                (void)dv;
            }
        }
        for (int t = 0; t < 6; t++) s[t] = best[t];
        rebuild();
        return V == 0;
    }

    // remove step i (1..K), repairing adjacency by chasing
    bool removeStep(int i) {
        for (int t = 0; t < 6; t++) {
            vector<St> old = s[t];
            vector<St> nw(K);
            for (int q = 0; q < i; q++) nw[q] = old[q];
            for (int q = i; q <= K - 1; q++) {
                St want = old[q + 1];
                if (q == 1 && ping && i == 1) { nw[q] = want; continue; }
                St pv = nw[q - 1];
                if (adj(pv, want)) nw[q] = want;
                else {
                    St x = pv;
                    if (x.p != want.p) x.p += (want.p > x.p) ? 1 : -1;
                    else x.a = want.a;
                    nw[q] = x;
                }
            }
            s[t] = nw;
        }
        K--;
        bool ok = true;
        if (fixedEnd) for (int t = 0; t < 6; t++) if (s[t][K].p != endSt[t].p || s[t][K].a != endSt[t].a) ok = false;
        if (cyc && K >= 1) for (int t = 0; t < 6; t++) if (!adj(s[t][K], s[t][1])) ok = false;
        rebuild();
        return ok;
    }
    void insertStep(int i) { // duplicate state at i (1..K)
        for (int t = 0; t < 6; t++) s[t].insert(s[t].begin() + i, s[t][i]);
        K++;
        rebuild();
    }
};


// exact search for a single configuration whose count-2 set equals the target.
// returns best setup cost S (max |p| over active emitters) or -1.
int exactSingle(const Geo& g, St* out, long long budget) {
    vector<vector<int>> opts(6);
    for (int t = 0; t < 6; t++) {
        opts[t].push_back(INT_MIN); // inactive
        for (int p = -(g.R - 1); p <= g.R - 1; p++) {
            bool hit = false;
            for (int c : g.ray[t][p + g.R]) if (g.target[c]) { hit = true; break; }
            if (hit) opts[t].push_back(p);
        }
    }
    vector<int> cnt(g.N, 0);
    int best = -1; long long evals = 0;
    int cur[6];
    function<void(int, int)> rec = [&](int t, int mx) {
        if (best >= 0 && mx >= best) return;
        if (++evals > budget) return;
        if (t == 6) {
            for (int c = 0; c < g.N; c++) if ((cnt[c] == 2) != (bool)g.target[c]) return;
            best = mx;
            for (int q = 0; q < 6; q++) out[q] = cur[q] == INT_MIN ? St{0, 0} : St{(int8_t)cur[q], 1};
            return;
        }
        for (int p : opts[t]) {
            cur[t] = p;
            if (p == INT_MIN) { rec(t + 1, mx); continue; }
            for (int c : g.ray[t][p + g.R]) cnt[c]++;
            rec(t + 1, max(mx, abs(p)));
            for (int c : g.ray[t][p + g.R]) cnt[c]--;
        }
    };
    rec(0, 0);
    return best;
}

static vector<array<char,6>> walkToProg(const vector<St>* s, int from, int to, const St* prevState) {
    // states s[t][from..to]; prevState = state before 'from'
    vector<array<char,6>> out;
    St prev[6]; for (int t = 0; t < 6; t++) prev[t] = prevState[t];
    int dir = from <= to ? 1 : -1;
    for (int i = from;; i += dir) {
        array<char,6> line;
        for (int t = 0; t < 6; t++) {
            St x = s[t][i];
            char c = '-';
            if (x.p == prev[t].p + 1) c = 'F';
            else if (x.p == prev[t].p - 1) c = 'B';
            else if (x.a != prev[t].a) c = 'T';
            line[t] = c; prev[t] = x;
        }
        out.push_back(line);
        if (i == to) break;
    }
    return out;
}

static vector<array<char,6>> setupProg(const St* target, int S) {
    vector<array<char,6>> out(S + 1);
    for (auto& l : out) l.fill('-');
    for (int t = 0; t < 6; t++) {
        int d = abs(target[t].p);
        char mv = target[t].p > 0 ? 'F' : 'B';
        for (int q = 0; q < d; q++) out[q][t] = mv;
        if (target[t].a) out[S][t] = 'T';
    }
    return out; // last line is step 1 of the walk
}

string progToStr(const vector<array<char,6>>& prog) {
    string o;
    for (auto& l : prog) { for (int t = 0; t < 6; t++) { if (t) o += ' '; o += l[t]; } o += '\n'; }
    return o;
}

vector<Case> readInput(const string& path) {
    ifstream f(path);
    vector<Case> cs; int n; f >> n;
    for (int i = 0; i < n; i++) {
        Case c; f >> c.R >> c.rows >> c.cols >> c.req;
        c.pat.resize(c.rows);
        for (int r = 0; r < c.rows; r++) f >> c.pat[r];
        cs.push_back(c);
    }
    return cs;
}

// minimize k for a walk under a given mode; returns best walk with V==0
// seconds: total budget
bool shrinkWalk(Solver& sv, double seconds, double perTry);

// seed a chain-mode solver with a line-pair sweep along axis a (emitters a, a+3)
void seedSweep(Solver& sv, int a, const St* start) {
    const Geo& g = sv.g;
    int R = g.R;
    auto partner = [&](int p) {
        auto A = g.ray[a][p + R]; sort(A.begin(), A.end());
        for (int q = -(R - 1); q <= R - 1; q++) { auto B = g.ray[a + 3][q + R]; sort(B.begin(), B.end()); if (A == B) return q; }
        return 0;
    };
    vector<vector<St>> tr(6);
    for (int t = 0; t < 6; t++) {
        St c = start[t];
        tr[t].push_back(c);
        if (t != a && t != a + 3) { if (c.a) { c.a = 0; tr[t].push_back(c); } continue; }
    }
    int p0 = -(R - 1);
    int tgt[2] = {p0, partner(p0)};
    int ems[2] = {a, a + 3};
    for (int e = 0; e < 2; e++) {
        int t = ems[e]; St c = start[t];
        if (c.a) { c.a = 0; tr[t].push_back(c); }
        while (c.p != tgt[e]) { c.p += c.p < tgt[e] ? 1 : -1; tr[t].push_back(c); }
        c.a = 1; tr[t].push_back(c);
    }
    size_t L1 = max(tr[a].size(), tr[a + 3].size());
    // align: pad the shorter pair member at the front (wait before acting) so both switch on together
    for (int e = 0; e < 2; e++) { int t = ems[e]; while (tr[t].size() < L1) tr[t].insert(tr[t].begin() + 1, tr[t][0]); }
    for (int p = p0 + 1; p <= R - 1; p++) {
        tr[a].push_back({(int8_t)p, 1});
        tr[a + 3].push_back({(int8_t)partner(p), 1});
    }
    int K = tr[a].size() - 1;
    sv.init(K, start);
    for (int t = 0; t < 6; t++) for (int i = 1; i <= K; i++) sv.s[t][i] = i < (int)tr[t].size() ? tr[t][i] : tr[t].back();
    sv.rebuild();
}
bool minimizeWalk(Solver& sv, int k0, const St* start, double seconds, double perTry, int seedAxis = -1) {
    auto t0 = chrono::steady_clock::now();
    auto elapsed = [&] { return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); };
    if (seedAxis >= 0) {
        seedSweep(sv, seedAxis, start);
        int add = sv.K / 3 + 2;
        for (int q = 0; q < add; q++) sv.insertStep(1 + rndi(sv.K));
    } else sv.init(k0, start);
    // find a feasible solution, increasing k if needed
    bool ok = false;
    while (!ok) {
        ok = sv.anneal(perTry * 2, 2.0, 0.05);
        if (!ok) {
            if (elapsed() > seconds * 3) return false;
            int add = max(1, sv.K / 4);
            for (int q = 0; q < add; q++) sv.insertStep(1 + rndi(sv.K));
        }
    }
    return shrinkWalk(sv, max(0.0, seconds - elapsed()), perTry);
}

bool shrinkWalk(Solver& sv, double seconds, double perTry) {
    auto t0 = chrono::steady_clock::now();
    auto elapsed = [&] { return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); };
    vector<St> best[6]; int bestK = sv.K;
    for (int t = 0; t < 6; t++) best[t] = sv.s[t];
    int fails = 0;
    while (elapsed() < seconds && sv.K > 1) {
        // choose step whose removal gives smallest violation
        int bi = -1; long long bv = LLONG_MAX;
        vector<St> keep[6]; for (int t = 0; t < 6; t++) keep[t] = sv.s[t];
        int keepK = sv.K;
        vector<int> order(sv.K); iota(order.begin(), order.end(), 1);
        shuffle(order.begin(), order.end(), rng);
        for (int i : order) {
            bool okr = sv.removeStep(i);
            long long v = okr ? sv.V * 4 + (sv.ping ? sv.S : 0) + rndi(3) : LLONG_MAX;
            if (v < bv) { bv = v; bi = i; }
            for (int t = 0; t < 6; t++) sv.s[t] = keep[t];
            sv.K = keepK;
        }
        sv.rebuild();
        if (bi < 0) break;
        sv.removeStep(bi);
        bool r = sv.V == 0 || sv.anneal(perTry, 1.0, 0.05);
        if (r) {
            bestK = sv.K; for (int t = 0; t < 6; t++) best[t] = sv.s[t];
            fails = 0;
        } else {
            fails++;
            for (int t = 0; t < 6; t++) sv.s[t] = best[t];
            sv.K = bestK; sv.rebuild();
            if (fails >= 3) break;
        }
    }
    for (int t = 0; t < 6; t++) sv.s[t] = best[t];
    sv.K = bestK; sv.rebuild();
    // polish setup in ping mode
    if (sv.ping && elapsed() < seconds) sv.anneal(min(perTry, seconds - elapsed() + 0.01), 0.5, 0.05);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 7) { fprintf(stderr, "usage: solver INPUT FROM TO SECONDS SEED OUTDIR\n"); return 1; }
    auto cases = readInput(argv[1]);
    int from = atoi(argv[2]), to = min((int)cases.size() - 1, atoi(argv[3]));
    double secs = atof(argv[4]);
    rng.seed(atoll(argv[5]));
    string outdir = argv[6];
    for (int ci = from; ci <= to; ci++) {
        auto& cs = cases[ci];
        Geo g = buildGeo(cs);
        vector<array<char,6>> bestProg; int bestLen = INT_MAX; string bestMode;
        St init[6]; for (int t = 0; t < 6; t++) init[t] = {0, 0};
        int kGuess = max(2, g.nTarget / 3 + 2);
        {
            St one[6];
            int S1 = exactSingle(g, one, 20000000);
            if (S1 >= 0) {
                auto prog = setupProg(one, S1);
                for (int c = 1; c < cs.req; c++) { array<char,6> l; l.fill('-'); prog.push_back(l); }
                int used; int acc = simulateExact(g, prog, cs.req, used);
                if (acc >= cs.req) { prog.resize(used); bestLen = prog.size(); bestProg = prog; bestMode = "single"; }
                else fprintf(stderr, "case %d single sim failed\n", ci);
            }
        }
        // --- chain mode
        bool doChain = cs.req <= 3;
        bool doPing = cs.req >= 2 && bestMode != "single";
        double share = (doChain && doPing) ? 0.5 : 1.0;
        if (doChain) {
            vector<array<char,6>> prog;
            St cur[6]; for (int t = 0; t < 6; t++) cur[t] = init[t];
            bool ok = true;
            for (int c = 0; c < cs.req && ok; c++) {
                bool dense = g.nTarget * 10 >= g.N * 4 && g.R >= 6;
                int nv = dense ? 4 : 1;
                double tsh = secs * share / cs.req / nv;
                vector<St> bestS[6]; int bestK = INT_MAX;
                for (int v = 0; v < nv; v++) {
                    Solver sv(g, false);
                    bool r = minimizeWalk(sv, kGuess + g.R, cur, tsh, tsh / 8, v - 1);
                    if (r && sv.K < bestK) { bestK = sv.K; for (int t = 0; t < 6; t++) bestS[t] = sv.s[t]; }
                }
                if (bestK == INT_MAX) { ok = false; break; }
                auto part = walkToProg(bestS, 1, bestK, cur);
                prog.insert(prog.end(), part.begin(), part.end());
                for (int t = 0; t < 6; t++) cur[t] = bestS[t][bestK];
            }
            if (ok) {
                int used; int acc = simulateExact(g, prog, cs.req, used);
                if (acc >= cs.req) { prog.resize(used); if ((int)prog.size() < bestLen) { bestLen = prog.size(); bestProg = prog; bestMode = "chain"; } }
                else fprintf(stderr, "case %d chain sim failed acc=%d\n", ci, acc);
            }
        }
        for (int mode = 1; mode <= 2 && doPing; mode++) {
            Solver sv(g, mode);
            sv.wS = 1.0 / (cs.req + 1);
            bool ok = minimizeWalk(sv, kGuess, nullptr, secs * share / 3, secs * share / 24);
            if (!ok) continue;
            St s1[6]; for (int t = 0; t < 6; t++) s1[t] = sv.s[t][1];
            int S = sv.setupCost();
            auto prog = setupProg(s1, S);
            St cur[6]; for (int t = 0; t < 6; t++) cur[t] = s1[t];
            if (sv.K >= 2) { auto part = walkToProg(sv.s, 2, sv.K, cur); prog.insert(prog.end(), part.begin(), part.end()); }
            for (int c = 1; c < cs.req; c++) {
                vector<array<char,6>> part;
                if (mode == 1) {
                    St last[6]; for (int t = 0; t < 6; t++) last[t] = (c % 2 == 1) ? sv.s[t][sv.K] : sv.s[t][1];
                    part = (c % 2 == 1) ? walkToProg(sv.s, sv.K, 1, last) : walkToProg(sv.s, 1, sv.K, last);
                } else {
                    St last[6]; for (int t = 0; t < 6; t++) last[t] = sv.s[t][sv.K];
                    part = walkToProg(sv.s, 1, sv.K, last);
                }
                prog.insert(prog.end(), part.begin(), part.end());
            }
            // hybrid: re-optimise the first copy (setup + W) as a chain walk from init with frozen end state
            {
                int L0 = S + sv.K;
                Solver hv(g, 0);
                hv.init(L0, init);
                St cur2[6]; for (int t = 0; t < 6; t++) cur2[t] = init[t];
                for (int i = 1; i <= L0; i++) for (int t = 0; t < 6; t++) {
                    char c = prog[i - 1][t];
                    if (c == 'F') cur2[t].p++; else if (c == 'B') cur2[t].p--; else if (c == 'T') cur2[t].a = 1 - cur2[t].a;
                    hv.s[t][i] = cur2[t];
                }
                hv.fixedEnd = true;
                for (int t = 0; t < 6; t++) hv.endSt[t] = hv.s[t][L0];
                hv.rebuild();
                if (hv.V == 0) {
                    shrinkWalk(hv, secs * share / 6, secs * share / 48);
                    if (hv.K < L0) {
                        auto first = walkToProg(hv.s, 1, hv.K, init);
                        vector<array<char,6>> np(first);
                        np.insert(np.end(), prog.begin() + L0, prog.end());
                        prog = np;
                    }
                } else fprintf(stderr, "case %d hybrid init V=%lld\n", ci, hv.V);
            }
            int used; int acc = simulateExact(g, prog, cs.req, used);
            const char* nm = mode == 1 ? "ping" : "cycle";
            if (acc >= cs.req) { prog.resize(used); if ((int)prog.size() < bestLen) { bestLen = prog.size(); bestProg = prog; bestMode = nm; } }
            else fprintf(stderr, "case %d %s sim failed acc=%d\n", ci, nm, acc);
        }
        if (bestLen == INT_MAX) { fprintf(stderr, "case %d: NO SOLUTION\n", ci); continue; }
        char fn[512]; snprintf(fn, sizeof fn, "%s/case_%03d.txt", outdir.c_str(), ci);
        ofstream f(fn); f << progToStr(bestProg);
        printf("case %d R=%d req=%d M=%d -> %d lines (%s)\n", ci, cs.R, cs.req, g.nTarget, bestLen, bestMode.c_str());
        fflush(stdout);
    }
}
