// dijkstra_base.cpp
// Baseline do experimento: Dijkstra com std::priority_queue (heap binario)
// sobre um grafo no formato DIMACS (.gr), como o USA-road-d.NY.gr.
//
// Compilar:  g++ -O3 -std=c++17 -o dijkstra_base dijkstra_base.cpp
// Rodar:     ./dijkstra_base USA-road-d.NY.gr 20 42 > resultados_heap.csv
//            (arquivo, numero de origens, semente aleatoria)
//
// A saida em CSV vai para o stdout (fica facil de ler em Python para os graficos)
// e um resumo vai para o stderr.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <vector>

using namespace std;
using Clock = chrono::steady_clock;

constexpr long long INF = numeric_limits<long long>::max();

// Grafo em CSR (Compressed Sparse Row): compacto e bom para cache.
// As arestas que saem do vertice u ficam em [first[u], first[u+1]).
struct Graph {
    int n = 0;
    vector<int> first;
    vector<int> to;
    vector<int> w;
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
            // p sp <n> <m>
            long long m = 0;
            sscanf(line.c_str(), "p sp %d %lld", &n, &m);
            edges.reserve(m);
        } else if (line[0] == 'a') {
            // a <u> <v> <peso>   (vertices numerados a partir de 1)
            int u, v, w;
            if (sscanf(line.c_str(), "a %d %d %d", &u, &v, &w) == 3)
                edges.push_back({u - 1, v - 1, w});
        }
        // linhas 'c' (comentario) sao ignoradas
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

// Dijkstra com "lazy deletion": em vez de diminuir a chave de um vertice,
// inserimos um par novo e ignoramos pares velhos ao retirar do heap.
vector<long long> dijkstra(const Graph& g, int s) {
    vector<long long> dist(g.n, INF);
    using P = pair<long long, int>;  // (distancia, vertice)
    priority_queue<P, vector<P>, greater<P>> pq;

    dist[s] = 0;
    pq.push({0, s});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u]) continue;  // par desatualizado

        for (int e = g.first[u]; e < g.first[u + 1]; ++e) {
            int v = g.to[e];
            long long nd = d + g.w[e];
            if (nd < dist[v]) {
                dist[v] = nd;
                pq.push({nd, v});
            }
        }
    }
    return dist;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " <arquivo.gr> [num_origens=10] [semente=42]\n";
        return 1;
    }
    string path = argv[1];
    int num_sources = argc > 2 ? atoi(argv[2]) : 10;
    unsigned seed = argc > 3 ? (unsigned)atoi(argv[3]) : 42;

    auto t0 = Clock::now();
    Graph g = read_dimacs(path);
    double load_ms = chrono::duration<double, milli>(Clock::now() - t0).count();
    cerr << "Grafo: " << g.n << " vertices, " << g.to.size() << " arestas (leitura: "
         << load_ms << " ms)\n";

    // Mesmas origens para qualquer variante, desde que a semente seja a mesma.
    mt19937 rng(seed);
    uniform_int_distribution<int> pick(0, g.n - 1);

    // O "checksum" (soma das distancias finitas) serve para conferir que
    // duas variantes do algoritmo chegaram exatamente ao mesmo resultado.
    printf("variante,origem,tempo_ms,alcancaveis,checksum\n");
    vector<double> times;
    for (int i = 0; i < num_sources; ++i) {
        int s = pick(rng);
        auto a = Clock::now();
        vector<long long> dist = dijkstra(g, s);
        double ms = chrono::duration<double, milli>(Clock::now() - a).count();

        long long sum = 0;
        int reach = 0;
        for (long long d : dist)
            if (d != INF) { sum += d; reach++; }

        printf("heap,%d,%.3f,%d,%lld\n", s + 1, ms, reach, sum);  // origem em base 1 (como no DIMACS)
        times.push_back(ms);
    }

    sort(times.begin(), times.end());
    cerr << "Mediana: " << times[times.size() / 2] << " ms | min: " << times.front()
         << " ms | max: " << times.back() << " ms\n";
    return 0;
}
