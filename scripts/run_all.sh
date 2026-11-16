#!/bin/bash
# Uso: ./run_all.sh USA-road-t.NY.gr [origens]   (gera CSVs na pasta resultados/)
# Varre: fracao de arestas com historico x passo das leituras (volume), regular e irregular.
G=${1:?"informe o grafo .gr"}; N=${2:-100}
set -e
g++ -O3 -std=c++17 -o dijkstra_td src/dijkstra_td.cpp
mkdir -p resultados
for frac in 0.05 0.20 0.50; do
  for passo in 3000 600; do            # na unidade do arquivo; menor passo = mais leituras
    for v in array hash map vec; do
      ./dijkstra_td "$G" $v --origens $N --frac $frac --passo $passo --resumo resultados/resumo.csv \
        > resultados/${v}_f${frac}_p${passo}_reg.csv
    done
    for v in map vec; do               # grade irregular: array/hash nao se aplicam
      ./dijkstra_td "$G" $v --origens $N --frac $frac --passo $passo --irregular --resumo resultados/resumo.csv \
        > resultados/${v}_f${frac}_p${passo}_irr.csv
    done
  done
done
echo "Pronto. Resumo em resultados/resumo.csv"
