// bplus_store.hpp
// Usado apenas por src/test_bplus.cpp.
// A implementacao principal de BPlusStore esta em dijkstra_td.cpp (bloco #ifdef USE_BPLUS).
// Este header replica o mesmo codigo para permitir teste isolado sem compilar dijkstra_td.cpp.

#pragma once
#include <algorithm>
#include <cstring>
#include <climits>
#include "traffic_types.hpp"

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
            int pos = (int)(std::lower_bound(l->keys, l->keys + l->n, k) - l->keys);
            if (pos < l->n && l->keys[pos] == k) { l->vals[pos] = val; return false; }
            std::memmove(l->keys + pos + 1, l->keys + pos, (l->n - pos) * sizeof(BKey));
            std::memmove(l->vals + pos + 1, l->vals + pos, (l->n - pos) * sizeof(int));
            l->keys[pos] = k; l->vals[pos] = val; l->n++;
            if (l->n < ORDER) return false;
            Leaf* right = new Leaf();
            int mid = l->n / 2;
            right->n = l->n - mid;
            std::memcpy(right->keys, l->keys + mid, right->n * sizeof(BKey));
            std::memcpy(right->vals, l->vals + mid, right->n * sizeof(int));
            l->n = mid;
            right->next = l->next; right->prev = l;
            if (l->next) l->next->prev = right;
            l->next = right;
            sr = {right->keys[0], right, true};
            return true;
        }
        Inner* in = static_cast<Inner*>(node);
        int ci = (int)(std::lower_bound(in->keys, in->keys + in->n, k) - in->keys);
        SplitResult csr{};
        if (!insert_node(in->child[ci], in->child_is_leaf[ci], k, val, csr)) return false;
        std::memmove(in->keys + ci + 1, in->keys + ci, (in->n - ci) * sizeof(BKey));
        std::memmove(in->child + ci + 2, in->child + ci + 1, (in->n - ci) * sizeof(void*));
        std::memmove(in->child_is_leaf + ci + 2, in->child_is_leaf + ci + 1, (in->n - ci) * sizeof(bool));
        in->keys[ci] = csr.key;
        in->child[ci + 1] = csr.right;
        in->child_is_leaf[ci + 1] = csr.right_is_leaf;
        in->n++;
        if (in->n < ORDER) return false;
        Inner* right = new Inner();
        int mid = in->n / 2;
        BKey promoted = in->keys[mid];
        right->n = in->n - mid - 1;
        std::memcpy(right->keys, in->keys + mid + 1, right->n * sizeof(BKey));
        std::memcpy(right->child, in->child + mid + 1, (right->n + 1) * sizeof(void*));
        std::memcpy(right->child_is_leaf, in->child_is_leaf + mid + 1, (right->n + 1) * sizeof(bool));
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
            int ci = (int)(std::lower_bound(in->keys, in->keys + in->n, k) - in->keys);
            if (in->child_is_leaf[ci]) return static_cast<const Leaf*>(in->child[ci]);
            in = static_cast<const Inner*>(in->child[ci]);
        }
    }
};
