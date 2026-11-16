# Dijkstra com pesos dependentes do tempo: onde guardar as leituras de tráfego

Projeto final da Unidade III de EDB2 (IMD0039, UFRN). Mantemos a fila de prioridade fixa
(`std::priority_queue`, heap binário com lazy deletion) e comparamos estruturas de dados para
armazenar os pesos dependentes do tempo das arestas: vetor indexado (`array`), tabela hash
(`hash`), `std::map` (rubro-negra), vetor ordenado com busca binária (`vec`), vetor dinâmico
por aresta (`dynvec`) e uma árvore B+ implementada à mão (`bplus`).

**Pergunta central:** sob quais condições a B+ compete com as alternativas? O experimento testa
três hipóteses: (H1) com K variável por aresta, `dynvec` supera `vec` em memória porque
não pré-aloca o máximo; (H2) em ingestão incremental e fora de ordem, `bplus` supera `dynvec`
em construção; (H3) em consulta, `vec`/`dynvec` superam `bplus` quando K é pequeno e fixo
(localidade de cache).

## Dados

O grafo de Nova York do 9th DIMACS Implementation Challenge (tempo de viagem, 264.346 vértices e
733.846 arcos) já está incluído em `data/USA-road-t.NY.gr`. Os dados são de domínio público.
Fonte original: https://www.diag.uniroma1.it/challenge9/download.shtml

**Unidade dos pesos:** unidades de tempo do grafo (a página do DIMACS não especifica a unidade
absoluta). O experimento adota a convenção de que o horizonte equivale a 24 h, o que define a
escala. Os defaults de `--passo` (30.000) e `--horizonte` (8.640.000) seguem essa convenção.

## Como compilar

    make dijkstra_td          # compila com suporte a todas as 6 variantes (padrão para experimentos)
    make dijkstra_base        # baseline estático
    make test_bplus           # teste isolado da B+ contra std::map

## Como rodar

    # Grafo de teste gerado artificialmente (rápido)
    python3 scripts/gerar_grafo_teste.py 50 50 data/teste.gr
    ./dijkstra_td data/teste.gr bplus --origens 20 --frac 0.5 --passo 100 --horizonte 2400

    # Baseline estático
    ./dijkstra_base data/USA-road-t.NY.gr 20 42 > resultados/base.csv

    # Variante individual
    ./dijkstra_td data/USA-road-t.NY.gr vec --origens 100 > resultados/vec.csv

    # Varredura completa (gera resultados/resumo.csv com mediana e IQR)
    ./scripts/run_all.sh data/USA-road-t.NY.gr 100

## Opções do `dijkstra_td`

As opções completas estão no topo de `src/dijkstra_td.cpp`. As principais:

| Opção | Padrão | Descrição |
|---|---|---|
| `--frac F` | 0.10 | Fração das arestas com histórico de tráfego |
| `--passo P` | 30000 | Intervalo entre leituras (convenção: 30000 ≈ 5 min na escala de 24 h) |
| `--horizonte H` | 8640000 | Duração total do histórico (convenção: 8640000 = 24 h) |
| `--irregular` | off | Horários com variação aleatória (como GPS real) |
| `--ordem O` | tempo | Ordem de ingestão: `tempo` (slot-major), `chave` (edge-major), `aleatorio` |
| `--origens N` | 20 | Número de origens sorteadas |
| `--verificar` | off | Confere Dijkstra contra Bellman-Ford (apenas grafos pequenos) |
| `--resumo ARQ` | — | Acrescenta linha de resumo com mediana e IQR (Q25/Q75) em CSV |

O modo `--ordem aleatorio` simula ingestão incremental sem pré-alocação por aresta.
O modo `--k-variavel` sorteia um K diferente por aresta (entre K/4 e K), habilitando o
cenário justo para `dynvec` e `bplus` (que crescem sob demanda) vs `map`.

**Correção:** todos os resultados usam checksums idênticos entre variantes e confirmados contra
Bellman-Ford em grafos pequenos (`--verificar`). Os números reportados são mediana + IQR de
100 origens com 1 warmup descartado.

A query da B+ é uma **busca pelo predecessor** (última leitura com t' ≤ t), não um range scan
genérico — as folhas encadeadas servem apenas para buscar o sucessor quando o predecessor está
na borda direita da folha de chegada.

## Verificação

`--verificar` compara o Dijkstra com Bellman-Ford em grafos pequenos. Todas as variantes
devem produzir o mesmo checksum para as mesmas origens, e `--frac 0` deve reproduzir o checksum
do `dijkstra_base`.

    ./dijkstra_td data/teste.gr bplus --origens 10 --frac 0.5 --passo 100 --horizonte 2400 --verificar
    ./test_bplus   # teste unitário da B+ contra std::map com inserções aleatórias

## Estrutura dos arquivos

```
src/
  dijkstra_base.cpp   baseline estático (Dijkstra com pesos fixos)
  dijkstra_td.cpp     Dijkstra TD + variantes (array, hash, map, vec, dynvec, bplus)
  traffic_types.hpp   tipos compartilhados: Reading, Traffic (K variável), interp
  bplus_store.hpp     árvore B+ implementada à mão (folhas encadeadas, ORDER configurável)
  test_bplus.cpp      teste isolado da B+ (chaves repetidas, ORDER 4/8/64)
scripts/
  run_all.sh          varredura completa; gera resultados/resumo.csv
  gerar_grafo_teste.py  grafo sintético para testes rápidos
```

## Autores

João Victor, Eduardo, Radla e Gabriel. O uso de IA no desenvolvimento está declarado no
Anexo A do artigo.
