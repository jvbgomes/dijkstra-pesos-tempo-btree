#!/bin/bash
# Uso: ./scripts/run_all.sh <grafo.gr> [origens=100]
# Gera CSVs individuais em resultados/ e acumula resultados/resumo.csv.
# Varre: fracao de arestas com historico x passo das leituras (volume),
#        regular vs irregular, ordem de ingestao (tempo/chave/aleatorio).
# Pesos em centesimos de segundo (cs); horizonte padrao = 8640000 cs = 24 h.

G=${1:?"informe o grafo .gr"}; N=${2:-100}
set -euo pipefail
mkdir -p resultados

g++ -O3 -std=c++17 -o dijkstra_td src/dijkstra_td.cpp

rm -f resultados/resumo.csv

FRACS="0.05 0.20 0.50"
PASSOS="30000 6000"   # 5 min e 1 min em centesimos de segundo

# Wrapper que nao aborta o script em caso de OOM/segfault:
# registra a falha no stderr e continua.
run() {
  local out=$1; shift
  if ! ./dijkstra_td "$@" --resumo resultados/resumo.csv > "$out" 2>&1; then
    echo "  [AVISO] falhou (OOM ou erro): $out" >&2
    return 0
  fi
}

for frac in $FRACS; do
  for passo in $PASSOS; do
    echo "--- frac=$frac passo=$passo regular ---"
    for v in array hash map vec bplus; do
      run resultados/${v}_f${frac}_p${passo}_reg_tempo.csv \
        "$G" $v --origens $N --frac $frac --passo $passo
    done

    echo "--- frac=$frac passo=$passo regular ordem=chave ---"
    for v in map vec bplus; do
      run resultados/${v}_f${frac}_p${passo}_reg_chave.csv \
        "$G" $v --origens $N --frac $frac --passo $passo --ordem chave
    done

    echo "--- frac=$frac passo=$passo irregular ---"
    # map usa >6 GB com frac=0.50; pulamos para nao crashear a maquina
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
    for v in map vec bplus; do
      run resultados/${v}_f${frac}_p${passo}_reg_aleatorio.csv \
        "$G" $v --origens $N --frac $frac --passo $passo --ordem aleatorio
    done
  done
done

echo "Pronto. Resumo em resultados/resumo.csv"
