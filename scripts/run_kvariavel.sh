#!/bin/bash
# Uso: ./scripts/run_kvariavel.sh <grafo.gr> [origens=100]
# Roda apenas o cenario K variavel (map, dynvec, bplus) e ACRESCENTA ao resumo.csv existente.
# Execute apos run_all.sh terminar, sem apagar resultados/resumo.csv.

G=${1:?"informe o grafo .gr"}; N=${2:-100}
set -euo pipefail
mkdir -p resultados

g++ -O3 -std=c++17 -o dijkstra_td src/dijkstra_td.cpp

FRACS="0.05 0.20 0.50"
PASSOS="30000 6000"

run() {
  local out=$1; shift
  if ! ./dijkstra_td "$@" --resumo resultados/resumo.csv > "$out" 2>&1; then
    echo "  [AVISO] falhou (OOM ou erro): $out" >&2
    return 0
  fi
}

for frac in $FRACS; do
  for passo in $PASSOS; do
    echo "--- frac=$frac passo=$passo k-variavel tempo ---"
    for v in map dynvec bplus; do
      run resultados/${v}_f${frac}_p${passo}_kvariavel_tempo.csv \
        "$G" $v --origens $N --frac $frac --passo $passo --k-variavel
    done

    echo "--- frac=$frac passo=$passo k-variavel aleatorio ---"
    for v in dynvec bplus; do
      run resultados/${v}_f${frac}_p${passo}_kvariavel_aleatorio.csv \
        "$G" $v --origens $N --frac $frac --passo $passo --k-variavel --ordem aleatorio
    done
    if ! { [ "$frac" = "0.50" ] && [ "$passo" = "6000" ]; }; then
      run resultados/map_f${frac}_p${passo}_kvariavel_aleatorio.csv \
        "$G" map --origens $N --frac $frac --passo $passo --k-variavel --ordem aleatorio
    fi
  done
done

echo "Pronto. Resumo atualizado em resultados/resumo.csv"
