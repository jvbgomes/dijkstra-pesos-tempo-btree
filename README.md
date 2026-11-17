# Dijkstra com pesos dependentes do tempo: onde guardar as leituras de tráfego

Projeto final da Unidade III de EDB2 (IMD0039, UFRN). Mantemos a fila de prioridade fixa
(`std::priority_queue`, heap binário com lazy deletion) e comparamos estruturas de dados para
armazenar os pesos dependentes do tempo das arestas: vetor indexado (`array`), tabela hash
(`hash`), `std::map` (rubro-negra), vetor ordenado com busca binária (`vec`), vetor dinâmico
por aresta (`dynvec`) e uma árvore B+ implementada à mão (`bplus`).

**Pergunta central:** sob quais condições a B+ compete com as alternativas? O experimento
levantou cinco hipóteses: (H1) com K fixo, `bplus` usa cerca de 40% da memória do `map` e
tem consulta mais rápida (observada); (H2) `dynvec` vence `bplus` em consulta para K até
1.441 leituras por aresta (observada); (H3) `bplus` se aproxima de `dynvec` na ingestão
conforme K cresce, a testar com maior faixa de K; (H4) a ordem de ingestão degrada a consulta
do `map` mais do que a da `bplus`, provavelmente por localidade de cache, mecanismo não medido
(observada); (H5) horário irregular não altera o tempo de consulta em nenhuma variante
(observada).

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
genérico. As folhas encadeadas servem apenas para buscar o sucessor quando o predecessor está
na borda direita da folha de chegada.

## Modelo de tráfego sintético

O tráfego é puramente sintético, gerado pela função `gerar_trafego` em `src/dijkstra_td.cpp`.
Não há calibração com dados reais.

**Picos de congestionamento:** dois picos gaussianos centrados em 8/24 e 18/24 do horizonte
(8 h e 18 h por convenção). Fator de congestionamento por aresta:

    f = 1 + A * (exp(-((x - 8/24) / 0,05)^2) + exp(-((x - 18/24) / 0,05)^2))

onde x = t / horizonte e A é sorteado em [0,5; 3,0] por aresta (uniform).

**Ruído multiplicativo:** fator ruido = 1 + 0,10 * (U - 0,5), ou seja, perturbação de ±5%
em torno do valor determinístico.

**Peso resultante:** `w = max(1, round(w_base * f * ruido))`.

**Garantia FIFO:** `w_k = max(w_k_raw, w_{k-1} - (t_k - t_{k-1}))`. Esse clamp garante que
a leitura k nunca cai mais do que o intervalo de tempo em relação à anterior, preservando a
propriedade FIFO exigida pelo Dijkstra TD.

**Modo irregular (`--irregular`):** jitter de ±25% do passo aplicado aos instantes internos,
ou seja, `j = round((U - 0,5) * 0,5 * passo)`. Os extremos (k=0 e k=K-1) são mantidos fixos.

**Modo K variável (`--k-variavel`):** K por aresta sorteado em `[max(2, K/4), K]`.

**Interpolação:** `interp(a, b, t) = a.w + floordiv((b.w - a.w) * (t - a.t), b.t - a.t)`,
com divisão inteira de piso (floor division, não truncamento), o que preserva a propriedade
FIFO na resposta da consulta.

## Limitações

O histórico de tráfego é sintético; os valores de peso não vêm de sensores reais. A escala
de tempo é uma convenção do experimento (8.640.000 unidades = 24 h); o DIMACS não especifica
unidade absoluta. O experimento usa um único grafo (Nova York) e foi executado numa única
máquina. Latências absolutas não são comparáveis entre máquinas distintas.

## Verificação

`--verificar` compara o Dijkstra com Bellman-Ford em grafos pequenos. Todas as variantes
devem produzir o mesmo checksum para as mesmas origens, e `--frac 0` deve reproduzir o checksum
do `dijkstra_base`.

    ./dijkstra_td data/teste.gr bplus --origens 10 --frac 0.5 --passo 100 --horizonte 2400 --verificar
    ./test_bplus   # teste unitário da B+ contra std::map com inserções aleatórias

## Resultados e gráficos

Os resultados da varredura ficam em `resultados/resumo.csv`. Colunas:

| Coluna | Descrição |
|--------|-----------|
| `variante` | estrutura de armazenamento testada |
| `frac` | fração das arestas com histórico |
| `passo` | intervalo entre leituras (unidades do grafo) |
| `irregular` | 0 = grade regular, 1 = horários com jitter |
| `ordem` | ordem de ingestão: `tempo`, `chave` ou `aleatorio` |
| `arestas_td` | total de arestas com histórico de tráfego |
| `leituras` | total de leituras armazenadas (arestas_td * K médio) |
| `construcao_ms` | tempo de ingestão completa (medição única, em ms) |
| `memoria_mb` | RSS após ingestão (MB, inclui grafo e td_id) |
| `mediana_ms` | mediana do tempo de Dijkstra TD sobre `origens` consultas |
| `q25_ms` | 1º quartil do tempo de consulta (ms) |
| `q75_ms` | 3º quartil do tempo de consulta (ms) |
| `min_ms` | tempo mínimo de consulta (ms) |
| `max_ms` | tempo máximo de consulta (ms) |
| `origens` | número de origens usadas (100 no experimento principal) |
| `k_variavel` | 0 = K uniforme por aresta, 1 = K variável por aresta |

Combinações não executadas (OOM ou skip preventivo) estão em `resultados/falhas.log`.

**Regenerar os gráficos:**

    python3 scripts/gerar_graficos.py          # usa resultados/resumo.csv
    python3 scripts/gerar_graficos.py outro.csv

**Figuras principais** (usadas no artigo), em `resultados/graficos/`:

| Figura | Conteúdo |
|--------|----------|
| `figA_consulta_kfixo` | consulta por volume de histórico, K fixo, vec/bplus/map |
| `figB_kvariavel_composto` | 3 painéis K variável: consulta, ingestão e memória |
| `figC_memoria_por_leitura` | bytes por leitura armazenada, K fixo |

**Figuras complementares** (fig1 a fig9): mostram todas as variantes e
configurações; ficam no repositório como suporte.

**Verificar checksums entre variantes:**

    python3 scripts/verificar_checksums.py     # salva resultados/verificacao_checksums.txt

## Estrutura dos arquivos

```
src/
  dijkstra_base.cpp   baseline estático (Dijkstra com pesos fixos)
  dijkstra_td.cpp     Dijkstra TD + variantes (array, hash, map, vec, dynvec, bplus)
  traffic_types.hpp   tipos compartilhados: Reading, Traffic (K variável), interp
  bplus_store.hpp     árvore B+ implementada à mão (folhas encadeadas, ORDER configurável)
  test_bplus.cpp      teste isolado da B+ (chaves repetidas, ORDER 4/8/64)
scripts/
  run_all.sh              varredura completa; gera resultados/resumo.csv
  run_kvariavel.sh        somente cenário K variável; acrescenta ao resumo.csv existente
  gerar_graficos.py       gera os gráficos (fig1-fig9 e figA-figC) a partir do resumo.csv
  verificar_checksums.py  confere checksums entre variantes nos CSVs por origem
  gerar_grafo_teste.py    grafo sintético para testes rápidos
resultados/
  resumo.csv              tabela de resultados (versionada)
  falhas.log              combinações não executadas e correções aplicadas
  verificacao_checksums.txt  relatório de verificação de checksums (versionado)
  graficos/               figuras em PDF e PNG
```

## Autores

João Victor, Eduardo, Radla e Gabriel. O uso de IA no desenvolvimento está declarado no
Anexo A do artigo.
