#!/usr/bin/env python3
"""Gera um grafo sintetico no formato DIMACS (.gr), parecido com uma malha viaria:
uma grade W x H com arestas nos dois sentidos e pesos inteiros aleatorios.
Serve para testar o codigo quando o grafo de Nova York nao esta a mao.

Uso: python3 gerar_grafo_teste.py LARGURA ALTURA saida.gr [semente]
"""
import random
import sys

w, h, saida = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
random.seed(int(sys.argv[4]) if len(sys.argv) > 4 else 1)

arestas = []
def vid(x, y):
    return y * w + x + 1  # DIMACS numera a partir de 1

for y in range(h):
    for x in range(w):
        for dx, dy in ((1, 0), (0, 1)):
            nx, ny = x + dx, y + dy
            if nx < w and ny < h:
                p = random.randint(50, 2000)
                arestas.append((vid(x, y), vid(nx, ny), p))
                arestas.append((vid(nx, ny), vid(x, y), random.randint(50, 2000)))

with open(saida, "w") as f:
    f.write("c grafo sintetico de teste\n")
    f.write(f"p sp {w * h} {len(arestas)}\n")
    for u, v, p in arestas:
        f.write(f"a {u} {v} {p}\n")
print(f"{w * h} vertices, {len(arestas)} arestas -> {saida}")
