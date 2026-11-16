#!/bin/bash
# Uso: ./scripts/run_all.sh <grafo.gr> [origens=100]
# Gera CSVs individuais em resultados/ e acumula resultados/resumo.csv.
# Varre: fracao de arestas com historico x passo das leituras (volume),
#        regular vs irregular, ordem de ingestao (tempo/chave/aleatorio).
# Escala de tempo: unidades do grafo; horizonte = 8640000 (24 h por convencao).

G=${1:?"informe o grafo .gr"}; N=${2:-100}
set -euo pipefail
mkdir -p resultados

g++ -O3 -std=c++17 -o dijkstra_td src/dijkstra_td.cpp

rm -f resultados/resumo.csv

FRACS="0.05 0.20 0.50"
PASSOS="30000 6000"

# Wrapper que nao aborta o script em caso de OOM/segfault:
# registra a falha no stderr e continua.
run() {
  local out=$1; shift
  if ! ./dijkstra_td "$@" --resumo resultados/resumo.csv > "$out" 2>&1; then
    echo "  [AVISO] falhou (OOM ou erro): $out" >&2
    return 0
  fi
}

# passo=6000, frac=0.50 -> ~36 GB para o map; pulo para evitar OOM
# passo=30000, frac=0.50 irregular -> ~7 GB (OK), mas irregular pula por precaucao
usar_map() {
  local frac=$1 passo=$2
  [ "$frac" = "0.50" ] && [ "$passo" = "6000" ] && return 1   # skip
  [ "$frac" = "0.50" ]                            && return 1   # irregular f=0.50 ja bloqueado abaixo
  return 0
}

for frac in $FRACS; do
  for passo in $PASSOS; do
    echo "--- frac=$frac passo=$passo regular ---"
    for v in array hash vec bplus; do
      run resultados/${v}_f${frac}_p${passo}_reg_tempo.csv \
        "$G" $v --origens $N --frac $frac --passo $passo
    done
    # map: pulo apenas frac=0.50+passo=6000 (~36 GB); demais combinacoes cabem em 32 GB
    if ! { [ "$frac" = "0.50" ] && [ "$passo" = "6000" ]; }; then
      run resultados/map_f${frac}_p${passo}_reg_tempo.csv \
        "$G" map --origens $N --frac $frac --passo $passo
    else
      echo "  [SKIP] map frac=0.50 passo=6000 (~36 GB)" >&2
    fi

    echo "--- frac=$frac passo=$passo regular ordem=chave ---"
    for v in vec bplus; do
      run resultados/${v}_f${frac}_p${passo}_reg_chave.csv \
        "$G" $v --origens $N --frac $frac --passo $passo --ordem chave
    done
    if ! { [ "$frac" = "0.50" ] && [ "$passo" = "6000" ]; }; then
      run resultados/map_f${frac}_p${passo}_reg_chave.csv \
        "$G" map --origens $N --frac $frac --passo $passo --ordem chave
    fi

    echo "--- frac=$frac passo=$passo irregular ---"
    # map usa >6 GB com frac=0.50 (qualquer passo); pulamos
    for v in vec bplus; do
      run resultados/${v}_f${frac}_p${passo}_irr_tempo.csv \
        "$G" $v --origens $N --frac $frac --passo $passo --irregular
    done
    if [ "$frac" != "0.50" ]; then
      run resultados/map_f${frac}_p${passo}_irr_tempo.csv \
        "$G" map --origens $N --frac $frac --passo $passo --irregular
    fi

    echo "--- frac=$frac passo=$passo irregular ordem=chave ---"
    for v in vec bplus; do
      run resultados/${v}_f${frac}_p${passo}_irr_chave.csv \
        "$G" $v --origens $N --frac $frac --passo $passo --irregular --ordem chave
    done
    if [ "$frac" != "0.50" ]; then
      run resultados/map_f${frac}_p${passo}_irr_chave.csv \
        "$G" map --origens $N --frac $frac --passo $passo --irregular --ordem chave
    fi

    echo "--- frac=$frac passo=$passo ingestao aleatorio ---"
    for v in vec bplus; do
      run resultados/${v}_f${frac}_p${passo}_reg_aleatorio.csv \
        "$G" $v --origens $N --frac $frac --passo $passo --ordem aleatorio
    done
    if ! { [ "$frac" = "0.50" ] && [ "$passo" = "6000" ]; }; then
      run resultados/map_f${frac}_p${passo}_reg_aleatorio.csv \
        "$G" map --origens $N --frac $frac --passo $passo --ordem aleatorio
    fi
  done
done

echo "Pronto. Resumo em resultados/resumo.csv"
