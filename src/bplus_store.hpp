// bplus_store.hpp
// Arvore B+ para armazenar leituras de trafego (aresta, horario) -> peso.
// Folhas encadeadas em lista ligada, chave composta (id, t), sem std::map nem libs externas.
// Incluido por dijkstra_td.cpp e por test_bplus.cpp.

#pragma once
#include <algorithm>
#include <cstring>
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
        BKey  keys[ORDER];
        int   vals[ORDER];
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

    BPlusStore() {
        root = new Leaf();
        root_is_leaf = true;
    }
    explicit BPlusStore(const Traffic&) : BPlusStore() {}
    ~BPlusStore() { destroy(root, root_is_leaf); }

    BPlusStore(const BPlusStore&) = delete;
    BPlusStore& operator=(const BPlusStore&) = delete;

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
            right->next = l->next;
            l->next = right;
            sr = {right->keys[0], right, true};
            return true;
        }
        Inner* in = static_cast<Inner*>(node);
        // upper_bound: chave igual ao separador pertence ao filho da DIREITA (separador e
        // a primeira chave do filho direito apos o split), evitando o bug de chaves repetidas.
        int ci = (int)(std::upper_bound(in->keys, in->keys + in->n, k) - in->keys);
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
        // Desce ate a folha que contem (id, t) ou seu predecessor imediato.
        // Com upper_bound nos nos internos, o predecessor esta em keys[pos-1] da folha
        // de chegada; o sucessor em keys[pos] ou na primeira posicao da proxima folha.
        BKey qk{id, t};
        const Leaf* l = find_leaf(qk);
        if (!l) return 0;

        int pos = (int)(std::lower_bound(l->keys, l->keys + l->n, qk) - l->keys);

        // correspondencia exata
        if (pos < l->n && l->keys[pos] == qk)
            return l->vals[pos];

        // predecessor: ultima leitura da aresta com t' < t
        Reading prev_r{};
        bool has_prev = (pos > 0 && l->keys[pos - 1].id == id);
        if (has_prev)
            prev_r = {l->keys[pos - 1].t, l->vals[pos - 1]};

        // sucessor: primeira leitura da aresta com t' > t
        Reading next_r{};
        bool has_next = false;
        if (pos < l->n && l->keys[pos].id == id) {
            next_r  = {l->keys[pos].t, l->vals[pos]};
            has_next = true;
        } else if (l->next && l->next->n > 0 && l->next->keys[0].id == id) {
            next_r  = {l->next->keys[0].t, l->next->vals[0]};
            has_next = true;
        }

        if (!has_prev && !has_next) return 0;
        if (!has_prev) return next_r.w;
        if (!has_next) return prev_r.w;
        return interp(prev_r, next_r, t);
    }

    const Leaf* find_leaf(const BKey& k) const {
        if (root_is_leaf) return static_cast<const Leaf*>(root);
        const Inner* in = static_cast<const Inner*>(root);
        while (true) {
            // upper_bound: consistente com insert_node (chave igual ao separador -> filho direito)
            int ci = (int)(std::upper_bound(in->keys, in->keys + in->n, k) - in->keys);
            if (in->child_is_leaf[ci]) return static_cast<const Leaf*>(in->child[ci]);
            in = static_cast<const Inner*>(in->child[ci]);
        }
    }
};
