// test_bplus.cpp
// Testa BPlusStore contra std::map com insercoes aleatorias.
// Compilar: g++ -O2 -std=c++17 -o test_bplus test_bplus.cpp
// Rodar:    ./test_bplus

#include <cstdio>
#include <map>
#include <random>
#include <vector>
#include <algorithm>
#include <climits>

#include "bplus_store.hpp"  // inclui traffic_types.hpp internamente

// Referencia: mesmo semantico de MapStore do dijkstra_td.cpp
struct RefStore {
    std::map<std::pair<int,int>, int> m;
    void insert(int id, int, const Reading& r) { m[{id, r.t}] = r.w; }
    int query(int id, int t) const {
        auto it = m.upper_bound({id, t});
        if (it == m.begin() || std::prev(it)->first.first != id)
            return m.lower_bound({id, INT_MIN})->second;
        auto cur = std::prev(it);
        if (it == m.end() || it->first.first != id) return cur->second;
        return interp({cur->first.second, cur->second}, {it->first.second, it->second}, t);
    }
};

int main() {
    // Parametros do teste
    const int N_EDGES = 200;    // ids de aresta
    const int N_READINGS = 25;  // leituras por aresta
    const int HORIZON = 10000;
    const int N_QUERIES = 5000;

    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> edge_dist(0, N_EDGES - 1);
    std::uniform_int_distribution<int> time_dist(0, HORIZON);
    std::uniform_int_distribution<int> weight_dist(1, 500);

    Traffic tr;
    tr.n_td = N_EDGES;
    tr.K = N_READINGS;
    tr.horizon = HORIZON;

    BPlusStore bp(tr);
    RefStore   ref;

    // Gera leituras aleatorias por aresta (sem sobreposicao de horarios)
    std::vector<std::pair<int,Reading>> insercoes;
    for (int id = 0; id < N_EDGES; ++id) {
        // sorteia N_READINGS horarios distintos para esta aresta
        std::vector<int> times;
        while ((int)times.size() < N_READINGS) {
            int t = time_dist(rng);
            if (std::find(times.begin(), times.end(), t) == times.end())
                times.push_back(t);
        }
        std::sort(times.begin(), times.end());
        for (int k = 0; k < N_READINGS; ++k) {
            Reading r{times[k], weight_dist(rng)};
            insercoes.push_back({id, r});
        }
    }

    // Embaralha a ordem de insercao (pior caso: fora de ordem)
    std::shuffle(insercoes.begin(), insercoes.end(), rng);

    for (auto& [id, r] : insercoes) {
        bp.insert(id, 0, r);
        ref.insert(id, 0, r);
    }

    // Consultas aleatorias
    int erros = 0;
    for (int i = 0; i < N_QUERIES; ++i) {
        int id = edge_dist(rng);
        int t  = time_dist(rng);
        int vb = bp.query(id, t);
        int vr = ref.query(id, t);
        if (vb != vr) {
            fprintf(stderr, "ERRO id=%d t=%d bplus=%d ref=%d\n", id, t, vb, vr);
            ++erros;
            if (erros > 10) break;
        }
    }

    if (erros == 0)
        printf("OK: %d insercoes aleatorias, %d consultas, bplus == std::map em todos os casos\n",
               (int)insercoes.size(), N_QUERIES);
    else
        printf("FALHOU: %d divergencias\n", erros);

    return erros ? 1 : 0;
}
