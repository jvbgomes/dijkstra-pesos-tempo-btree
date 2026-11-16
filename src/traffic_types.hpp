// traffic_types.hpp
// Tipos e funcoes compartilhados entre dijkstra_td.cpp e bplus_store.hpp.

#pragma once
#include <vector>

struct Reading { int t; int w; };

struct Traffic {
    int step = 0, horizon = 0, K = 0;
    int n_td = 0;
    std::vector<int> td_id;
    std::vector<Reading> flat;
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
