// dijkstra_td.cpp
// Experimento: Dijkstra dependente do tempo (earliest arrival) sobre um grafo DIMACS,
// variando SOMENTE a estrutura que guarda os pesos dinamicos (leituras de trafego).
// O heap binario (std::priority_queue com lazy deletion) e identico em todas as variantes.
//
// Compilar:
//   g++ -O3 -std=c++17 -o dijkstra_td dijkstra_td.cpp
//   (com a arvore B+ de voces:  g++ -O3 -std=c++17 -DUSE_BPLUS -o dijkstra_td dijkstra_td.cpp)
//
// Uso:
//   ./dijkstra_td <grafo.gr> <variante> [opcoes] > resultados_<variante>.csv
//
// Variantes:
//   array  vetor indexado pelo horario (O(1)); so funciona com leituras em grade regular
//   hash   unordered_map (aresta, slot);       so funciona com leituras em grade regular
//   map    std::map (arvore rubro-negra), chave (aresta, horario); aceita horarios irregulares
//   vec    vetor ordenado por aresta + busca binaria;              aceita horarios irregulares
//   bplus  a nossa arvore B+ (precisa compilar com -DUSE_BPLUS)
//
// Opcoes (valores padrao entre parenteses):
//   --origens N      numero de origens sorteadas (20)
//   --semente S      semente das origens, igual a do baseline (42)
//   --frac F         fracao das arestas que tem historico de trafego (0.10)
//   --passo P        intervalo entre leituras em centesimos de segundo (30000 = 5 min)
//   --horizonte H    duracao total do historico em cs (8640000 = 24 h)
//   --irregular      horarios das leituras com variacao aleatoria (como GPS)
//   --ordem O        ordem de ingestao das leituras: "tempo" (padrao, como chegam do sensor),
//                    "chave" (ordenado por aresta, melhor caso para arvores) ou
//                    "aleatorio" (embaralhado; B+ sem pre-alocacao por aresta)
//   --saida T        horario de partida das consultas (horizonte/3)
//   --aquecimento K  execucoes descartadas antes de medir (1)
//   --resumo ARQ     acrescenta uma linha de resumo (construcao, memoria, mediana) neste CSV
//   --verificar      confere o Dijkstra contra um Bellman-Ford (use so em grafos pequenos)
//
// Modelo de tempo: cada aresta com historico tem leituras (t_k, w_k) e o peso em um horario t
// e a interpolacao LINEAR entre as duas leituras vizinhas (fora do intervalo, vale a leitura da
// ponta). Interpolacao linear com queda de no maximo 1 unidade de peso por unidade de tempo
// garante a propriedade FIFO (sair depois nunca faz chegar antes), exigida pelo Dijkstra.
// O gerador respeita essa condicao (ver gerar_trafego).

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <unistd.h>

using namespace std;
using Clock = chrono::steady_clock;

constexpr long long INF = numeric_limits<long long>::max();

// ---------------------------------------------------------------- grafo (igual ao baseline)
struct Graph {
    int n = 0;
    vector<int> first;  // arestas de u: [first[u], first[u+1])  (o indice e o id da aresta)
    vector<int> to;
    vector<int> w;      // peso base (estatico) de cada aresta
};

Graph read_dimacs(const string& path) {
    ifstream in(path);
    if (!in) {
        cerr << "Erro: nao consegui abrir " << path << "\n";
        exit(1);
    }
    struct Edge { int u, v, w; };
    vector<Edge> edges;
    int n = 0;
    string line;
    while (getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == 'p') {
            long long m = 0;
            sscanf(line.c_str(), "p sp %d %lld", &n, &m);
            edges.reserve(m);
        } else if (line[0] == 'a') {
            int u, v, w;
            if (sscanf(line.c_str(), "a %d %d %d", &u, &v, &w) == 3)
                edges.push_back({u - 1, v - 1, w});
        }
    }
    Graph g;
    g.n = n;
    g.first.assign(n + 1, 0);
    for (const auto& e : edges) g.first[e.u + 1]++;
    for (int i = 0; i < n; ++i) g.first[i + 1] += g.first[i];
    g.to.resize(edges.size());
    g.w.resize(edges.size());
    vector<int> pos(g.first.begin(), g.first.end() - 1);
    for (const auto& e : edges) {
        int idx = pos[e.u]++;
        g.to[idx] = e.v;
        g.w[idx] = e.w;
    }
    return g;
}

// ---------------------------------------------------------------- trafego
// Reading, Traffic e interp estao em traffic_types.hpp (compartilhado com src/test_bplus.cpp).
#include "traffic_types.hpp"
// flat[id*K + k]: k = 0..K-1, ordenadas por t

// Gera as series. FIFO: a leitura k nunca cai mais que (t_k - t_{k-1}) em relacao a k-1.
Traffic gerar_trafego(const Graph& g, double frac, int step, int horizon, bool irregular,
                      unsigned seed) {
    Traffic tr;
    tr.step = step;
    tr.horizon = (horizon / step) * step;
    tr.K = tr.horizon / step + 1;
    tr.regular = !irregular;
    mt19937 rng(seed);
    uniform_real_distribution<double> U(0.0, 1.0);
    int m = (int)g.to.size();
    tr.td_id.assign(m, -1);
    for (int e = 0; e < m; ++e)
        if (U(rng) < frac) tr.td_id[e] = tr.n_td++;
    tr.flat.resize((size_t)tr.n_td * tr.K);

    // base[id] = peso base da aresta com aquele id
    vector<int> times(tr.K);
    auto sq = [](double x) { return x * x; };
    for (int e = 0; e < m; ++e) {
        int id = tr.td_id[e];
        if (id < 0) continue;
        for (int k = 0; k < tr.K; ++k) {
            times[k] = k * step;
            if (irregular && k > 0 && k < tr.K - 1) {
                int j = (int)lround((U(rng) - 0.5) * 0.5 * step);  // +-step/4
                times[k] += j;
            }
        }
        double A = 0.5 + 2.5 * U(rng);  // intensidade do congestionamento desta via
        int prevw = 0;
        for (int k = 0; k < tr.K; ++k) {
            double x = (double)times[k] / tr.horizon;  // fracao do dia
            // dois picos: ~8h e ~18h
            double f = 1.0 + A * (exp(-sq((x - 8.0 / 24) / 0.05)) + exp(-sq((x - 18.0 / 24) / 0.05)));
            double ruido = 1.0 + 0.10 * (U(rng) - 0.5);
            int w = max(1, (int)lround(g.w[e] * f * ruido));
            if (k > 0) w = max(w, prevw - (times[k] - times[k - 1]));  // garante FIFO
            tr.flat[(size_t)id * tr.K + k] = {times[k], w};
            prevw = w;
        }
    }
    return tr;
}

// ---------------------------------------------------------------- estruturas de armazenamento
// Interface exigida de qualquer estrutura :
//   explicit Store(const Traffic& tr);                    // aloca, sem inserir
//   void insert(int id, int k, const Reading& r);         // ingere a leitura k da aresta id
//   int  query(int id, int t) const;                      // peso da aresta id no horario t
// O mesmo (id, t) tem que devolver o MESMO valor em todas as estruturas (o checksum confere).

// 1) Vetor indexado pelo horario. O(1). Exige grade regular.
struct ArrayStore {
    int K, step, horizon;
    vector<int> w;
    explicit ArrayStore(const Traffic& tr)
        : K(tr.K), step(tr.step), horizon(tr.horizon), w((size_t)tr.n_td * tr.K) {}
    void insert(int id, int k, const Reading& r) { w[(size_t)id * K + k] = r.w; }
    int query(int id, int t) const {
        const int* p = &w[(size_t)id * K];
        if (t <= 0) return p[0];
        if (t >= horizon) return p[K - 1];
        int k = t / step;
        return interp({k * step, p[k]}, {(k + 1) * step, p[k + 1]}, t);
    }
};

// 2) Tabela hash com chave (aresta, slot). Exige grade regular.
struct HashStore {
    int K, step, horizon;
    unordered_map<long long, int> h;
    explicit HashStore(const Traffic& tr) : K(tr.K), step(tr.step), horizon(tr.horizon) {
        h.reserve((size_t)tr.n_td * tr.K);
    }
    void insert(int id, int k, const Reading& r) { h[(long long)id * K + k] = r.w; }
    int query(int id, int t) const {
        long long base = (long long)id * K;
        if (t <= 0) return h.find(base)->second;
        if (t >= horizon) return h.find(base + K - 1)->second;
        int k = t / step;
        return interp({k * step, h.find(base + k)->second},
                      {(k + 1) * step, h.find(base + k + 1)->second}, t);
    }
};

// 3) std::map (arvore rubro-negra) com chave (aresta, horario). Aceita horarios irregulares.
struct MapStore {
    map<pair<int, int>, int> m;
    explicit MapStore(const Traffic&) {}
    void insert(int id, int, const Reading& r) { m.emplace(make_pair(id, r.t), r.w); }
    int query(int id, int t) const {
        auto it = m.upper_bound({id, t});  // primeira leitura com horario > t
        if (it == m.begin() || prev(it)->first.first != id)  // antes da primeira leitura
            return m.lower_bound({id, INT_MIN})->second;
        auto cur = prev(it);
        if (it == m.end() || it->first.first != id) return cur->second;  // depois da ultima
        return interp({cur->first.second, cur->second}, {it->first.second, it->second}, t);
    }
};

// 4) Vetor ordenado (um bloco contiguo por aresta) + busca binaria. Aceita horarios irregulares.
struct VecStore {
    int K;
    vector<Reading> r;
    explicit VecStore(const Traffic& tr) : K(tr.K), r((size_t)tr.n_td * tr.K) {}
    void insert(int id, int k, const Reading& x) { r[(size_t)id * K + k] = x; }
    int query(int id, int t) const {
        const Reading* b = &r[(size_t)id * K];
        const Reading* e = b + K;
        if (t <= b->t) return b->w;
        if (t >= (e - 1)->t) return (e - 1)->w;
        const Reading* it = upper_bound(b, e, t, [](int tt, const Reading& x) { return tt < x.t; });
        return interp(*(it - 1), *it, t);
    }
};

#ifdef USE_BPLUS
// 5) Arvore B+ implementada a mao: folhas encadeadas, chave (id, horario).
//    Aceita insercao fora de ordem e horarios irregulares.
//    Ordem do no configuravel via BPLUS_ORDER (padrao 64).

#ifndef BPLUS_ORDER
#define BPLUS_ORDER 64
#endif

struct BKey {
    int id, t;
    bool operator<(const BKey& o) const { return id < o.id || (id == o.id && t < o.t); }
    bool operator==(const BKey& o) const { return id == o.id && t == o.t; }
};

struct BPlusStore {
    static constexpr int ORDER = BPLUS_ORDER;

    struct Leaf {
        int n = 0;
        BKey keys[ORDER];
        int  vals[ORDER];
        Leaf* prev = nullptr;
        Leaf* next = nullptr;
    };

    struct Inner {
        int n = 0;
        BKey  keys[ORDER];
        void* child[ORDER + 1] = {};
        bool  child_is_leaf[ORDER + 1] = {};
    };

    void* root = nullptr;
    bool  root_is_leaf = true;
    Leaf* head = nullptr;

    BPlusStore() {
        Leaf* l = new Leaf();
        root = l; root_is_leaf = true; head = l;
    }
    explicit BPlusStore(const Traffic&) : BPlusStore() {}
    ~BPlusStore() { destroy(root, root_is_leaf); }

    void destroy(void* node, bool is_leaf) {
        if (!node) return;
        if (is_leaf) { delete static_cast<Leaf*>(node); return; }
        Inner* in = static_cast<Inner*>(node);
        for (int i = 0; i <= in->n; ++i) destroy(in->child[i], in->child_is_leaf[i]);
        delete in;
    }

    struct SplitResult { BKey key; void* right; bool right_is_leaf; };

    void insert(int id, int /*k*/, const Reading& r) {
        BKey k{id, r.t};
        SplitResult sr{};
        if (!insert_node(root, root_is_leaf, k, r.w, sr)) return;
        Inner* nr = new Inner();
        nr->n = 1; nr->keys[0] = sr.key;
        nr->child[0] = root; nr->child_is_leaf[0] = root_is_leaf;
        nr->child[1] = sr.right; nr->child_is_leaf[1] = sr.right_is_leaf;
        root = nr; root_is_leaf = false;
    }

    bool insert_node(void* node, bool is_leaf, const BKey& k, int val, SplitResult& sr) {
        if (is_leaf) {
            Leaf* l = static_cast<Leaf*>(node);
            int pos = (int)(lower_bound(l->keys, l->keys + l->n, k) - l->keys);
            if (pos < l->n && l->keys[pos] == k) { l->vals[pos] = val; return false; }
            memmove(l->keys + pos + 1, l->keys + pos, (l->n - pos) * sizeof(BKey));
            memmove(l->vals + pos + 1, l->vals + pos, (l->n - pos) * sizeof(int));
            l->keys[pos] = k; l->vals[pos] = val; l->n++;
            if (l->n < ORDER) return false;
            Leaf* right = new Leaf();
            int mid = l->n / 2;
            right->n = l->n - mid;
            memcpy(right->keys, l->keys + mid, right->n * sizeof(BKey));
            memcpy(right->vals, l->vals + mid, right->n * sizeof(int));
            l->n = mid;
            right->next = l->next; right->prev = l;
            if (l->next) l->next->prev = right;
            l->next = right;
            sr = {right->keys[0], right, true};
            return true;
        }
        Inner* in = static_cast<Inner*>(node);
        int ci = (int)(lower_bound(in->keys, in->keys + in->n, k) - in->keys);
        SplitResult csr{};
        if (!insert_node(in->child[ci], in->child_is_leaf[ci], k, val, csr)) return false;
        memmove(in->keys + ci + 1, in->keys + ci, (in->n - ci) * sizeof(BKey));
        memmove(in->child + ci + 2, in->child + ci + 1, (in->n - ci) * sizeof(void*));
        memmove(in->child_is_leaf + ci + 2, in->child_is_leaf + ci + 1, (in->n - ci) * sizeof(bool));
        in->keys[ci] = csr.key;
        in->child[ci + 1] = csr.right;
        in->child_is_leaf[ci + 1] = csr.right_is_leaf;
        in->n++;
        if (in->n < ORDER) return false;
        Inner* right = new Inner();
        int mid = in->n / 2;
        BKey promoted = in->keys[mid];
        right->n = in->n - mid - 1;
        memcpy(right->keys, in->keys + mid + 1, right->n * sizeof(BKey));
        memcpy(right->child, in->child + mid + 1, (right->n + 1) * sizeof(void*));
        memcpy(right->child_is_leaf, in->child_is_leaf + mid + 1, (right->n + 1) * sizeof(bool));
        in->n = mid;
        sr = {promoted, right, false};
        return true;
    }

    int query(int id, int t) const {
        BKey lo{id, INT_MIN};
        const Leaf* l = find_leaf(lo);
        if (!l) return 0;
        Reading prev_buf{}, next_buf{};
        const Reading* prev_r = nullptr;
        const Reading* next_r = nullptr;
        for (const Leaf* cur = l; cur; cur = cur->next) {
            bool done = false;
            for (int i = 0; i < cur->n; ++i) {
                if (cur->keys[i].id != id) {
                    if (cur->keys[i].id > id) { done = true; break; }
                    continue;
                }
                if (cur->keys[i].t <= t) {
                    prev_buf = {cur->keys[i].t, cur->vals[i]}; prev_r = &prev_buf;
                } else {
                    next_buf = {cur->keys[i].t, cur->vals[i]}; next_r = &next_buf;
                    done = true; break;
                }
            }
            if (done) break;
        }
        if (!prev_r && !next_r) return 0;
        if (!prev_r) return next_r->w;
        if (!next_r) return prev_r->w;
        return interp(*prev_r, *next_r, t);
    }

    const Leaf* find_leaf(const BKey& k) const {
        if (root_is_leaf) return static_cast<const Leaf*>(root);
        const Inner* in = static_cast<const Inner*>(root);
        while (true) {
            int ci = (int)(lower_bound(in->keys, in->keys + in->n, k) - in->keys);
            if (in->child_is_leaf[ci]) return static_cast<const Leaf*>(in->child[ci]);
            in = static_cast<const Inner*>(in->child[ci]);
        }
    }
};
#endif

// ---------------------------------------------------------------- Dijkstra dependente do tempo
template <class Store>
static inline long long custo(const Graph& g, const Traffic& tr, const Store& st, int e,
                              long long d) {
    int id = tr.td_id[e];
    if (id < 0) return g.w[e];
    return st.query(id, d > INT_MAX ? INT_MAX : (int)d);
}

// arr[v] = horario de chegada mais cedo em v, saindo de s no horario t0.
template <class Store>
vector<long long> dijkstra_td(const Graph& g, const Traffic& tr, const Store& st, int s,
                              long long t0) {
    vector<long long> arr(g.n, INF);
    using P = pair<long long, int>;
    priority_queue<P, vector<P>, greater<P>> pq;  // heap binario FIXO em todas as variantes
    arr[s] = t0;
    pq.push({t0, s});
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > arr[u]) continue;  // entrada desatualizada (lazy deletion)
        for (int e = g.first[u]; e < g.first[u + 1]; ++e) {
            int v = g.to[e];
            long long nd = d + custo(g, tr, st, e, d);
            if (nd < arr[v]) {
                arr[v] = nd;
                pq.push({nd, v});
            }
        }
    }
    return arr;
}

// Referencia lenta (Bellman-Ford): so serve para conferir o resultado em grafos pequenos.
template <class Store>
vector<long long> referencia_bf(const Graph& g, const Traffic& tr, const Store& st, int s,
                                long long t0) {
    vector<long long> b(g.n, INF);
    b[s] = t0;
    bool mudou = true;
    while (mudou) {
        mudou = false;
        for (int u = 0; u < g.n; ++u) {
            if (b[u] == INF) continue;
            for (int e = g.first[u]; e < g.first[u + 1]; ++e) {
                long long nd = b[u] + custo(g, tr, st, e, b[u]);
                if (nd < b[g.to[e]]) { b[g.to[e]] = nd; mudou = true; }
            }
        }
    }
    return b;
}

// ---------------------------------------------------------------- utilidades
double rss_mb() {  // memoria residente atual (Linux)
    ifstream f("/proc/self/statm");
    long a = 0, b = 0;
    f >> a >> b;
    return b * (double)sysconf(_SC_PAGESIZE) / (1024.0 * 1024.0);
}

double ms_desde(Clock::time_point t) {
    return chrono::duration<double, milli>(Clock::now() - t).count();
}

// Unidade dos pesos no formato DIMACS road-t: centesimos de segundo (cs).
// 24 h = 86400 s = 8.640.000 cs.  Passo padrao: 30.000 cs = 5 minutos.
// Referencia: 9th DIMACS Challenge README, secao "edge weights".
struct Opcoes {
    string grafo, variante, resumo;
    int origens = 20, passo = 30000, horizonte = 8640000, aquecimento = 1;
    unsigned semente = 42;
    double frac = 0.10;
    bool irregular = false, verificar = false;
    string ordem = "tempo";  // "tempo" | "chave" | "aleatorio"
    long long saida = -1;
};

template <class Store>
int executar(const Opcoes& op, const Graph& g, Traffic& tr, double mem_grafo) {
    // --- ingestao: aloca a estrutura e insere todas as leituras (tempo medido)
    auto tb = Clock::now();
    Store st(tr);
    if (op.ordem == "tempo") {  // slot-major: todas as arestas para k=0, depois k=1...
        for (int k = 0; k < tr.K; ++k)
            for (int id = 0; id < tr.n_td; ++id) st.insert(id, k, tr.flat[(size_t)id * tr.K + k]);
    } else if (op.ordem == "chave") {  // edge-major: todas as leituras da aresta 0, depois 1...
        for (int id = 0; id < tr.n_td; ++id)
            for (int k = 0; k < tr.K; ++k) st.insert(id, k, tr.flat[(size_t)id * tr.K + k]);
    } else {  // aleatorio: ordem completamente aleatoria (ingestao incremental sem pre-alocacao)
        size_t total = tr.flat.size();
        vector<size_t> idx(total);
        iota(idx.begin(), idx.end(), 0);
        mt19937 rng_ord(op.semente + 999);
        shuffle(idx.begin(), idx.end(), rng_ord);
        for (size_t pos : idx) {
            int id = (int)(pos / tr.K), k = (int)(pos % tr.K);
            st.insert(id, k, tr.flat[pos]);
        }
    }
    double build_ms = ms_desde(tb);
    size_t leituras = tr.flat.size();
    vector<Reading>().swap(tr.flat);  // libera a copia canonica: sobra so a estrutura
    double mem_store = rss_mb() - mem_grafo;

    long long t0 = op.saida >= 0 ? op.saida : tr.horizon / 3;
    cerr << "variante=" << op.variante << " arestas_com_historico=" << tr.n_td
         << " leituras=" << leituras << " construcao=" << build_ms << " ms"
         << " memoria_aprox=" << mem_store << " MB (inclui td_id, comum a todas)\n";

    if (op.verificar) {
        int ok = 1;
        mt19937 r2(op.semente);
        uniform_int_distribution<int> pick(0, g.n - 1);
        for (int i = 0; i < min(op.origens, 3); ++i) {
            int s = pick(r2);
            if (dijkstra_td(g, tr, st, s, t0) != referencia_bf(g, tr, st, s, t0)) ok = 0;
        }
        cerr << (ok ? "VERIFICACAO OK: Dijkstra igual ao Bellman-Ford\n"
                    : "VERIFICACAO FALHOU: Dijkstra diferente do Bellman-Ford\n");
        if (!ok) return 2;
    }

    for (int i = 0; i < op.aquecimento; ++i) dijkstra_td(g, tr, st, 0, t0);  // descartado

    mt19937 rng(op.semente);  // mesma sequencia de origens do baseline
    uniform_int_distribution<int> pick(0, g.n - 1);
    printf("variante,origem,tempo_ms,alcancaveis,checksum\n");
    vector<double> tempos;
    for (int i = 0; i < op.origens; ++i) {
        int s = pick(rng);
        auto a = Clock::now();
        vector<long long> arr = dijkstra_td(g, tr, st, s, t0);
        double ms = ms_desde(a);
        long long soma = 0;
        int alc = 0;
        for (long long d : arr)
            if (d != INF) { soma += d - t0; alc++; }  // soma dos tempos de viagem
        printf("%s,%d,%.3f,%d,%lld\n", op.variante.c_str(), s + 1, ms, alc, soma);
        tempos.push_back(ms);
    }
    sort(tempos.begin(), tempos.end());
    size_t n = tempos.size();
    double mediana = n % 2 ? tempos[n / 2] : (tempos[n / 2 - 1] + tempos[n / 2]) / 2;
    double q25 = tempos[n / 4];
    double q75 = tempos[3 * n / 4 < n ? 3 * n / 4 : n - 1];
    cerr << "Mediana: " << mediana << " ms | Q25: " << q25 << " | Q75: " << q75
         << " | min: " << tempos.front() << " ms | max: " << tempos.back() << " ms\n";

    if (!op.resumo.empty()) {
        FILE* f = fopen(op.resumo.c_str(), "a");
        if (f) {
            if (ftell(f) == 0)
                fprintf(f, "variante,frac,passo,irregular,ordem,arestas_td,leituras,construcao_ms,"
                           "memoria_mb,mediana_ms,q25_ms,q75_ms,min_ms,max_ms,origens\n");
            fprintf(f, "%s,%.3f,%d,%d,%s,%d,%zu,%.1f,%.1f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n",
                    op.variante.c_str(), op.frac, op.passo, op.irregular ? 1 : 0,
                    op.ordem.c_str(), tr.n_td, leituras, build_ms, mem_store,
                    mediana, q25, q75, tempos.front(), tempos.back(), op.origens);
            fclose(f);
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " <grafo.gr> <array|hash|map|vec|bplus> [opcoes] (veja o topo do arquivo)\n";
        return 1;
    }
    Opcoes op;
    op.grafo = argv[1];
    op.variante = argv[2];
    for (int i = 3; i < argc; ++i) {
        string a = argv[i];
        auto prox = [&]() -> string {
            if (i + 1 >= argc) { cerr << "Falta valor para " << a << "\n"; exit(1); }
            return argv[++i];
        };
        if (a == "--origens") op.origens = stoi(prox());
        else if (a == "--semente") op.semente = (unsigned)stoul(prox());
        else if (a == "--frac") op.frac = stod(prox());
        else if (a == "--passo") op.passo = stoi(prox());
        else if (a == "--horizonte") op.horizonte = stoi(prox());
        else if (a == "--irregular") op.irregular = true;
        else if (a == "--ordem") { op.ordem = prox(); if (op.ordem != "tempo" && op.ordem != "chave" && op.ordem != "aleatorio") { cerr << "--ordem deve ser tempo, chave ou aleatorio\n"; return 1; } }
        else if (a == "--saida") op.saida = stoll(prox());
        else if (a == "--aquecimento") op.aquecimento = stoi(prox());
        else if (a == "--resumo") op.resumo = prox();
        else if (a == "--verificar") op.verificar = true;
        else { cerr << "Opcao desconhecida: " << a << "\n"; return 1; }
    }
    if ((op.variante == "array" || op.variante == "hash") && op.irregular) {
        cerr << "As variantes array e hash exigem leituras em grade regular (tire --irregular).\n";
        return 1;
    }

    Graph g = read_dimacs(op.grafo);
    cerr << "Grafo: " << g.n << " vertices, " << g.to.size() << " arestas\n";
    double mem_grafo = rss_mb();
    Traffic tr = gerar_trafego(g, op.frac, op.passo, op.horizonte, op.irregular, op.semente + 1);

    if (op.variante == "array") return executar<ArrayStore>(op, g, tr, mem_grafo);
    if (op.variante == "hash") return executar<HashStore>(op, g, tr, mem_grafo);
    if (op.variante == "map") return executar<MapStore>(op, g, tr, mem_grafo);
    if (op.variante == "vec") return executar<VecStore>(op, g, tr, mem_grafo);
#ifdef USE_BPLUS
    if (op.variante == "bplus") return executar<BPlusStore>(op, g, tr, mem_grafo);
#else
    if (op.variante == "bplus") {
        cerr << "Compile com -DUSE_BPLUS para usar a variante bplus.\n";
        return 1;
    }
#endif
    cerr << "Variante desconhecida: " << op.variante << "\n";
    return 1;
}
