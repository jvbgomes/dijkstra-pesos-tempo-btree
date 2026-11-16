# Dijkstra com pesos dependentes do tempo: onde guardar as leituras de tráfego

Projeto final da Unidade III de EDB2 (IMD0039, UFRN). Mantemos a fila de prioridade fixa
(`std::priority_queue`, heap binário com lazy deletion) e comparamos estruturas de dados para
armazenar os pesos dependentes do tempo das arestas: vetor indexado, tabela hash, `std::map`
(rubro-negra), vetor ordenado com busca binária e uma árvore B+ (a ser implementada).

## Dados

Grafo de Nova York do 9th DIMACS Implementation Challenge (264.346 vértices, 733.846 arcos).
Baixe `USA-road-t.NY.gr.gz` (tempo de viagem) em http://www.dis.uniroma1.it/~challenge9 ,
descompacte e coloque em `data/`. Os grafos não são versionados.

## Como compilar e rodar

    make
    ./dijkstra_base data/USA-road-t.NY.gr 20 42 > resultados/base.csv
    ./dijkstra_td data/USA-road-t.NY.gr vec --origens 100 > resultados/vec.csv
    ./scripts/run_all.sh data/USA-road-t.NY.gr 100

As opções do `dijkstra_td` estão descritas no topo de `src/dijkstra_td.cpp`. Para testar sem o
grafo real: `python3 scripts/gerar_grafo_teste.py 300 300 data/teste.gr`.

## Verificação

`--verificar` compara o Dijkstra com um Bellman-Ford em grafos pequenos. Todas as variantes
devem produzir o mesmo checksum para as mesmas origens, e `--frac 0` deve reproduzir o checksum
do `dijkstra_base`.

## Autores

João Victor, Eduardo, Radla e Gabriel. O uso de IA no desenvolvimento está declarado no
Anexo A do artigo.
