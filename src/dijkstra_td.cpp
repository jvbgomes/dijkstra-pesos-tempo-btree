// dijkstra_td.cpp
// Experimento: Dijkstra dependente do tempo (earliest arrival) sobre um grafo DIMACS,
// variando SOMENTE a estrutura que guarda os pesos dinamicos (leituras de trafego).
// O heap binario (std::priority_queue com lazy deletion) e identico em todas as variantes.
//
// Compilar:
//   g++ -O3 -std=c++17 -o dijkstra_td dijkstra_td.cpp
//
// Uso:
//   ./dijkstra_td <grafo.gr> <variante> [opcoes] > resultados_<variante>.csv
//
// Variantes:
//   array   vetor indexado pelo horario (O(1)); so funciona com grade regular e K uniforme
//   hash    unordered_map (aresta, slot);       so funciona com grade regular e K uniforme
//   map     std::map (arvore rubro-negra), chave (aresta, horario); aceita irregular e K variavel
//   vec     vetor ordenado por aresta + busca binaria;              aceita irregular, K uniforme
//   dynvec  vector<Reading> por aresta, insercao ordenada;          aceita irregular e K variavel
//   bplus   arvore B+ (ver bplus_store.hpp);                        aceita irregular e K variavel
//
// Opcoes (valores padrao entre parenteses):
//   --origens N      numero de origens sorteadas (20)
//   --semente S      semente das origens, igual a do baseline (42)
//   --frac F         fracao das arestas que tem historico de trafego (0.10)
//   --passo P        intervalo entre leituras (30000; horizonte padrao = 8640000 ~ 24 h)
//   --horizonte H    duracao total do historico (8640000)
//   --irregular      horarios das leituras com variacao aleatoria (como GPS)
//   --k-variavel     K diferente por aresta (sorteado em [K/4, K]); habilita cenario dynvec
//   --ordem O        ordem de ingestao das leituras: "tempo" (padrao, como chegam do sensor),
//                    "chave" (ordenado por aresta, melhor caso para arvores) ou
//                    "aleatorio" (embaralhado; B+ e dynvec sem pre-alocacao por aresta)
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
// Com k_variavel=true cada aresta recebe K_e in [K/4, K] sorteado, simulando sensores com
// historicos de tamanhos diferentes (cenario justo para DynVecStore vs map/bplus).
Traffic gerar_trafego(const Graph& g, double frac, int step, int horizon, bool irregular,
                      bool k_variavel, unsigned seed) {
    Traffic tr;
    tr.step = step;
    tr.horizon = (horizon / step) * step;
    tr.K = tr.horizon / step + 1;  // K maximo (ou K uniforme se !k_variavel)
    mt19937 rng(seed);
    uniform_real_distribution<double> U(0.0, 1.0);
    int m = (int)g.to.size();
    tr.td_id.assign(m, -1);
    for (int e = 0; e < m; ++e)
        if (U(rng) < frac) tr.td_id[e] = tr.n_td++;

    if (k_variavel) {
        // sorteia K por aresta, constroi soma-prefixo e redimensiona flat
        tr.edge_k.resize(tr.n_td);
        uniform_int_distribution<int> kd(max(2, tr.K / 4), tr.K);
        for (int id = 0; id < tr.n_td; ++id) tr.edge_k[id] = kd(rng);
        tr.edge_off.resize(tr.n_td + 1);
        tr.edge_off[0] = 0;
        for (int id = 0; id < tr.n_td; ++id)
            tr.edge_off[id + 1] = tr.edge_off[id] + tr.edge_k[id];
        tr.flat.resize(tr.edge_off[tr.n_td]);
    } else {
        tr.flat.resize((size_t)tr.n_td * tr.K);
    }

    auto sq = [](double x) { return x * x; };
    for (int e = 0; e < m; ++e) {
        int id = tr.td_id[e];
        if (id < 0) continue;
        int K_e = tr.edge_K(id);
        vector<int> times(K_e);
        for (int k = 0; k < K_e; ++k) {
            times[k] = k * step;
            if (irregular && k > 0 && k < K_e - 1) {
                int j = (int)lround((U(rng) - 0.5) * 0.5 * step);
                times[k] += j;
            }
        }
        double A = 0.5 + 2.5 * U(rng);
        int prevw = 0;
        for (int k = 0; k < K_e; ++k) {
            double x = (double)times[k] / tr.horizon;
            double f = 1.0 + A * (exp(-sq((x - 8.0 / 24) / 0.05)) + exp(-sq((x - 18.0 / 24) / 0.05)));
            double ruido = 1.0 + 0.10 * (U(rng) - 0.5);
            int w = max(1, (int)lround(g.w[e] * f * ruido));
            if (k > 0) w = max(w, prevw - (times[k] - times[k - 1]));  // garante FIFO
            tr.flat[tr.flat_idx(id, k)] = {times[k], w};
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
//    Requer K uniforme: pre-aloca n_td * K slots; nao suporta --k-variavel.
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

// 5) Vetor dinamico por aresta com insercao ordenada. Aceita K variavel e ingestao incremental.
//    Concorrente justo do vec no cenario --k-variavel: usa so a memoria necessaria por aresta,
//    mas tem insercao O(K_e) vs O(1) do vec (que exige k conhecido na hora da insercao).
struct DynVecStore {
    vector<vector<Reading>> buckets;
    explicit DynVecStore(const Traffic& tr) : buckets(tr.n_td) {}
    void insert(int id, int /*k*/, const Reading& r) {
        auto& b = buckets[id];
        auto it = lower_bound(b.begin(), b.end(), r.t,
                              [](const Reading& x, int t) { return x.t < t; });
        if (it != b.end() && it->t == r.t)
            it->w = r.w;  // sobreescreve leitura com mesmo horario
        else
            b.insert(it, r);
    }
    int query(int id, int t) const {
        const auto& b = buckets[id];
        if (b.empty()) return 0;
        if (t <= b.front().t) return b.front().w;
        if (t >= b.back().t)  return b.back().w;
        auto it = upper_bound(b.begin(), b.end(), t,
                              [](int tt, const Reading& x) { return tt < x.t; });
        return interp(*(it - 1), *it, t);
    }
};

#include "bplus_store.hpp"

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

// Unidade dos pesos no formato DIMACS road-t: unidades de tempo do grafo (pagina do DIMACS
// nao especifica unidade absoluta). O experimento adota a convencao de que horizonte=8640000
// equivale a 24 h, o que fixa a escala. O trafego e sintetico de qualquer forma.
struct Opcoes {
    string grafo, variante, resumo;
    int origens = 20, passo = 30000, horizonte = 8640000, aquecimento = 1;
    unsigned semente = 42;
    double frac = 0.10;
    bool irregular = false, verificar = false, k_variavel = false;
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
            for (int id = 0; id < tr.n_td; ++id)
                if (k < tr.edge_K(id))
                    st.insert(id, k, tr.flat[tr.flat_idx(id, k)]);
    } else if (op.ordem == "chave") {  // edge-major: todas as leituras da aresta 0, depois 1...
        for (int id = 0; id < tr.n_td; ++id)
            for (int k = 0; k < tr.edge_K(id); ++k)
                st.insert(id, k, tr.flat[tr.flat_idx(id, k)]);
    } else {  // aleatorio: ordem completamente aleatoria (ingestao incremental sem pre-alocacao)
        // Enumeramos todos os pares (id, k) e embaralhamos — funciona para K uniforme e variavel.
        vector<pair<int,int>> pairs;
        pairs.reserve(tr.flat.size());
        for (int id = 0; id < tr.n_td; ++id)
            for (int k = 0; k < tr.edge_K(id); ++k)
                pairs.push_back({id, k});
        mt19937 rng_ord(op.semente + 999);
        shuffle(pairs.begin(), pairs.end(), rng_ord);
        for (auto [id, k] : pairs)
            st.insert(id, k, tr.flat[tr.flat_idx(id, k)]);
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
        cerr << "Uso: " << argv[0] << " <grafo.gr> <array|hash|map|vec|dynvec|bplus> [opcoes] (veja o topo do arquivo)\n";
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
        else if (a == "--k-variavel") op.k_variavel = true;
        else if (a == "--ordem") { op.ordem = prox(); if (op.ordem != "tempo" && op.ordem != "chave" && op.ordem != "aleatorio") { cerr << "--ordem deve ser tempo, chave ou aleatorio\n"; return 1; } }
        else if (a == "--saida") op.saida = stoll(prox());
        else if (a == "--aquecimento") op.aquecimento = stoi(prox());
        else if (a == "--resumo") op.resumo = prox();
        else if (a == "--verificar") op.verificar = true;
        else { cerr << "Opcao desconhecida: " << a << "\n"; return 1; }
    }
    if ((op.variante == "array" || op.variante == "hash") && op.irregular) {
        cerr << "As variantes array e hash exigem grade regular (tire --irregular).\n";
        return 1;
    }
    if ((op.variante == "array" || op.variante == "hash" || op.variante == "vec") && op.k_variavel) {
        cerr << "As variantes array, hash e vec nao suportam --k-variavel (K deve ser uniforme).\n";
        return 1;
    }

    Graph g = read_dimacs(op.grafo);
    cerr << "Grafo: " << g.n << " vertices, " << g.to.size() << " arestas\n";
    double mem_grafo = rss_mb();
    Traffic tr = gerar_trafego(g, op.frac, op.passo, op.horizonte, op.irregular,
                               op.k_variavel, op.semente + 1);

    if (op.variante == "array")  return executar<ArrayStore> (op, g, tr, mem_grafo);
    if (op.variante == "hash")   return executar<HashStore>  (op, g, tr, mem_grafo);
    if (op.variante == "map")    return executar<MapStore>   (op, g, tr, mem_grafo);
    if (op.variante == "vec")    return executar<VecStore>   (op, g, tr, mem_grafo);
    if (op.variante == "dynvec") return executar<DynVecStore>(op, g, tr, mem_grafo);
    if (op.variante == "bplus")  return executar<BPlusStore> (op, g, tr, mem_grafo);
    cerr << "Variante desconhecida: " << op.variante << "\n";
    return 1;
}
