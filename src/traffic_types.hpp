// traffic_types.hpp
// Tipos e funcoes compartilhados entre dijkstra_td.cpp e bplus_store.hpp.

#pragma once
#include <cstddef>
#include <vector>

struct Reading { int t; int w; };

struct Traffic {
    int step = 0, horizon = 0, K = 0;  // K = max K por aresta (ou K uniforme se edge_k vazio)
    int n_td = 0;
    std::vector<int>     td_id;    // td_id[aresta] = id em [0..n_td), ou -1
    std::vector<Reading> flat;     // leituras; layout depende de edge_k (ver flat_idx)
    std::vector<int>     edge_k;   // K por aresta (tamanho n_td); vazio se K e uniforme
    std::vector<size_t>  edge_off; // soma-prefixo de edge_k (tamanho n_td+1); vazio se K uniforme

    // K da aresta id (K uniforme ou variavel)
    int edge_K(int id) const {
        return edge_k.empty() ? K : edge_k[id];
    }
    // Indice em flat para a k-esima leitura da aresta id
    size_t flat_idx(int id, int k) const {
        return edge_k.empty() ? (size_t)id * K + k : edge_off[id] + k;
    }
};

static inline long long td_floordiv(long long a, long long b) {
    long long q = a / b;
    if ((a % b != 0) && (a < 0)) --q;
    return q;
}

static inline int interp(const Reading& a, const Reading& b, int t) {
    long long num = (long long)(b.w - a.w) * (t - a.t);
    return a.w + (int)td_floordiv(num, b.t - a.t);
}
