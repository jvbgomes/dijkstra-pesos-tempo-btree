#!/bin/bash
# Uso: ./scripts/run_all.sh <grafo.gr> [origens=100]
# Gera CSVs individuais em resultados/ e acumula resultados/resumo.csv.
# Varre: fracao de arestas com historico x passo das leituras (volume),
#        regular vs irregular, ordem de ingestao (tempo/chave/aleatorio).
# Pesos em centesimos de segundo (cs); horizonte padrao = 8640000 cs = 24 h.

G=${1:?"informe o grafo .gr"}; N=${2:-100}
set -e
mkdir -p resultados

g++ -O3 -std=c++17 -o dijkstra_td src/dijkstra_td.cpp

rm -f resultados/resumo.csv

FRACS="0.05 0.20 0.50"
PASSOS="30000 6000"   # 5 min e 1 min em centesimos de segundo

for frac in $FRACS; do
  for passo in $PASSOS; do
    echo "--- frac=$frac passo=$passo regular ---"
    for v in array hash map vec bplus; do
      ./dijkstra_td "$G" $v --origens $N --frac $frac --passo $passo \
        --resumo resultados/resumo.csv \
        > resultados/${v}_f${frac}_p${passo}_reg_tempo.csv
    done

    echo "--- frac=$frac passo=$passo regular ordem=chave ---"
    for v in map vec bplus; do
      ./dijkstra_td "$G" $v --origens $N --frac $frac --passo $passo \
        --ordem chave --resumo resultados/resumo.csv \
        > resultados/${v}_f${frac}_p${passo}_reg_chave.csv
    done

    echo "--- frac=$frac passo=$passo irregular ---"
    for v in map vec bplus; do
      ./dijkstra_td "$G" $v --origens $N --frac $frac --passo $passo --irregular \
        --resumo resultados/resumo.csv \
        > resultados/${v}_f${frac}_p${passo}_irr_tempo.csv
    done

    echo "--- frac=$frac passo=$passo irregular ordem=chave ---"
    for v in map vec bplus; do
      ./dijkstra_td "$G" $v --origens $N --frac $frac --passo $passo --irregular \
        --ordem chave --resumo resultados/resumo.csv \
        > resultados/${v}_f${frac}_p${passo}_irr_chave.csv
    done

    echo "--- frac=$frac passo=$passo ingestao aleatorio (cenario B+) ---"
    for v in map vec bplus; do
      ./dijkstra_td "$G" $v --origens $N --frac $frac --passo $passo \
        --ordem aleatorio --resumo resultados/resumo.csv \
        > resultados/${v}_f${frac}_p${passo}_reg_aleatorio.csv
    done
  done
done

echo "Pronto. Resumo em resultados/resumo.csv"
