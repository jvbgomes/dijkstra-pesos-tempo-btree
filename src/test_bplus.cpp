// test_bplus.cpp
// Testa BPlusStore contra std::map com insercoes aleatorias.
// Compilar: g++ -O2 -std=c++17 -o test_bplus src/test_bplus.cpp
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
        if (it == m.begin() || std::prev(it)->first.first != id) {
            auto fwd = m.lower_bound({id, INT_MIN});
            if (fwd == m.end() || fwd->first.first != id) return 0;
            return fwd->second;
        }
        auto cur = std::prev(it);
        if (it == m.end() || it->first.first != id) return cur->second;
        return interp({cur->first.second, cur->second}, {it->first.second, it->second}, t);
    }
};

// Executa um caso de teste; retorna numero de divergencias.
template<int ORD>
int run_case(int n_edges, int n_readings, int horizon, int n_queries,
             bool allow_duplicates, unsigned seed) {
// Troca temporariamente BPLUS_ORDER nao e possivel em tempo de execucao,
// mas podemos instanciar BPlusStore com o template como parametro de ORDER.
// Como ORDER e constexpr da struct, usamos #define antes do include.
// Aqui instanciamos via especializacao: cada chamada usa o ORDER passado.
// Estrategia: reimplementar inline e pequena, ou usar o tipo com ORDER fixo.
// Por simplicidade, este teste usa apenas o ORDER compilado (BPLUS_ORDER).
// Para testar ordens menores, compilar com -DBPLUS_ORDER=<n>.
    (void)ORD; // ordem usada e a do tipo BPlusStore (BPLUS_ORDER)

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> edge_dist(0, n_edges - 1);
    std::uniform_int_distribution<int> time_dist(0, horizon);
    std::uniform_int_distribution<int> weight_dist(1, 500);

    Traffic tr;
    tr.n_td = n_edges;
    tr.K = n_readings;
    tr.horizon = horizon;

    BPlusStore bp(tr);
    RefStore   ref;

    std::vector<std::pair<int,Reading>> insercoes;
    for (int id = 0; id < n_edges; ++id) {
        std::vector<int> times;
        int attempts = 0;
        while ((int)times.size() < n_readings && attempts < n_readings * 10) {
            int t = time_dist(rng);
            if (allow_duplicates || std::find(times.begin(), times.end(), t) == times.end())
                times.push_back(t);
            ++attempts;
        }
        std::sort(times.begin(), times.end());
        for (int tk : times)
            insercoes.push_back({id, {tk, weight_dist(rng)}});
    }

    std::shuffle(insercoes.begin(), insercoes.end(), rng);

    for (auto& [id, r] : insercoes) {
        bp.insert(id, 0, r);
        ref.insert(id, 0, r);
    }

    int erros = 0;
    for (int i = 0; i < n_queries; ++i) {
        int id = edge_dist(rng);
        int t  = time_dist(rng);
        int vb = bp.query(id, t);
        int vr = ref.query(id, t);
        if (vb != vr) {
            if (erros < 5)
                fprintf(stderr, "  ERRO id=%d t=%d bplus=%d ref=%d\n", id, t, vb, vr);
            ++erros;
        }
    }
    return erros;
}

int main() {
    int total_erros = 0;
    int total_queries = 0;

    struct Caso {
        const char* desc;
        int n_edges, n_readings, horizon, n_queries;
        bool duplicates;
        unsigned seed;
    } casos[] = {
        {"chaves distintas, grande",  200,  25, 10000, 5000, false, 1234},
        {"chaves distintas, pequena", 50,   15,  5000, 3000, false, 5678},
        {"com chaves repetidas",      100,  30, 10000, 5000, true,  9999},
        {"stress: muitas arestas",    500, 100, 50000, 10000, false, 42},
    };

    for (auto& c : casos) {
        int e = run_case<BPLUS_ORDER>(
            c.n_edges, c.n_readings, c.horizon, c.n_queries, c.duplicates, c.seed);
        total_erros  += e;
        total_queries += c.n_queries;
        printf("  [%s] ORDER=%d: %d divergencias em %d consultas\n",
               c.desc, BPlusStore::ORDER, e, c.n_queries);
    }

    if (total_erros == 0)
        printf("OK: bplus == std::map em todos os %d casos (%d consultas)\n",
               (int)(sizeof(casos)/sizeof(casos[0])), total_queries);
    else
        printf("FALHOU: %d divergencias no total\n", total_erros);

    return total_erros ? 1 : 0;
}
