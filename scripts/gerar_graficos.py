"""
gerar_graficos.py
Lê resultados/resumo.csv e gera os gráficos do artigo com seaborn.

Uso:
    python3 scripts/gerar_graficos.py
    python3 scripts/gerar_graficos.py outro_resumo.csv

Dependências:
    pip install pandas matplotlib seaborn
"""

import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# ---- entrada ----
csv_path = sys.argv[1] if len(sys.argv) > 1 else "resultados/resumo.csv"
if not os.path.exists(csv_path):
    print(f"Arquivo não encontrado: {csv_path}")
    print("Execute primeiro: ./scripts/run_all.sh data/USA-road-t.NY.gr 100")
    sys.exit(1)

df = pd.read_csv(csv_path)

# Compatibilidade: se coluna k_variavel nao existe, assume 0 para todos
if "k_variavel" not in df.columns:
    df["k_variavel"] = 0

os.makedirs("resultados/graficos", exist_ok=True)

# ---- estilo global ----
sns.set_theme(style="whitegrid", context="paper", font_scale=1.2)
PALETA = {
    "array":  "#2196F3",
    "hash":   "#4CAF50",
    "vec":    "#9C27B0",
    "bplus":  "#F44336",
    "map":    "#FF9800",
    "dynvec": "#795548",
}
ORDEM_VAR   = ["array", "hash", "vec", "dynvec", "bplus", "map"]
ORDEM_KV    = ["dynvec", "bplus", "map"]   # variantes do cenario k-variavel

# cenarios regulares e k-variavel separados
reg = df[df["k_variavel"] == 0]
kv  = df[df["k_variavel"] == 1]

def salvar(fig, nome):
    for ext in ("pdf", "png"):
        fig.savefig(f"resultados/graficos/{nome}.{ext}", bbox_inches="tight")
    print(f"  → resultados/graficos/{nome}.png")
    plt.close(fig)

# ============================================================
# Figura 1 — mediana de tempo de consulta por variante
# cenário base: regular, ordem=tempo, passo=30000, frac=0.20
# ============================================================
d1 = reg[
    (reg["irregular"] == 0) &
    (reg["ordem"] == "tempo") &
    (reg["passo"] == 30000) &
    (reg["frac"] == 0.20)
]
if not d1.empty:
    fig, ax = plt.subplots(figsize=(7, 4))
    variantes = [v for v in ORDEM_VAR if v in d1["variante"].unique()]
    sns.barplot(data=d1, x="variante", y="mediana_ms",
                order=variantes, hue="variante", legend=False,
                palette={v: PALETA[v] for v in variantes},
                errorbar=("pi", 50), capsize=0.1, ax=ax)
    ax.set_xlabel("Estrutura de armazenamento")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title("Tempo do Dijkstra TD por variante\nNY · frac=0,20 · passo=5 min · grade regular")
    salvar(fig, "fig1_mediana_por_variante")

# ============================================================
# Figura 2 — mediana × fração de arestas com histórico
# regular, ordem=tempo, passo=30000
# ============================================================
d2 = reg[(reg["irregular"] == 0) & (reg["ordem"] == "tempo") & (reg["passo"] == 30000)]
if not d2.empty:
    fig, ax = plt.subplots(figsize=(7, 4))
    variantes = [v for v in ORDEM_VAR if v in d2["variante"].unique()]
    sns.lineplot(data=d2, x="frac", y="mediana_ms",
                 hue="variante", hue_order=variantes,
                 palette={v: PALETA[v] for v in variantes},
                 marker="o", linewidth=2, ax=ax)
    ax.set_xlabel("Fração de arestas com histórico de tráfego")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title("Escalabilidade com volume de histórico\nNY · passo=5 min · grade regular")
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig2_tempo_x_frac")

# ============================================================
# Figura 3 — regular vs irregular (map, vec, bplus)
# frac=0.20, passo=30000, ordem=tempo
# ============================================================
d3 = reg[
    (reg["frac"] == 0.20) & (reg["passo"] == 30000) & (reg["ordem"] == "tempo") &
    (reg["variante"].isin(["map", "vec", "bplus"]))
].copy()
d3["horários"] = d3["irregular"].map({0: "Regular", 1: "Irregular"})
if not d3.empty:
    fig, ax = plt.subplots(figsize=(6, 4))
    variantes = [v for v in ["vec", "bplus", "map"] if v in d3["variante"].unique()]
    sns.barplot(data=d3, x="variante", y="mediana_ms", hue="horários",
                order=variantes, palette="Set2",
                capsize=0.1, errorbar=("pi", 50), ax=ax)
    ax.set_xlabel("Estrutura"); ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title("Grade regular vs horários irregulares\nNY · frac=0,20 · passo=5 min")
    ax.legend(title="Horários")
    salvar(fig, "fig3_regular_vs_irregular")

# ============================================================
# Figura 4 — custo de ingestão por ordem de chegada
# frac=0.20, passo=30000, regular; map, vec, bplus
# ============================================================
d4 = reg[
    (reg["frac"] == 0.20) & (reg["passo"] == 30000) & (reg["irregular"] == 0) &
    (reg["variante"].isin(["map", "vec", "bplus"]))
]
if not d4.empty and d4["ordem"].nunique() > 1:
    fig, ax = plt.subplots(figsize=(6, 4))
    variantes = [v for v in ["vec", "bplus", "map"] if v in d4["variante"].unique()]
    sns.barplot(data=d4, x="variante", y="construcao_ms", hue="ordem",
                order=variantes, palette="Set1",
                capsize=0.1, errorbar=("pi", 50), ax=ax)
    ax.set_xlabel("Estrutura"); ax.set_ylabel("Tempo de construção (ms)")
    ax.set_title("Custo de ingestão por ordem de chegada\nNY · frac=0,20 · passo=5 min")
    ax.legend(title="Ordem de ingestão")
    salvar(fig, "fig4_custo_ingestao")

# ============================================================
# Figura 5 — mediana × passo das leituras
# regular, ordem=tempo, frac=0.20
# ============================================================
d5 = reg[(reg["irregular"] == 0) & (reg["ordem"] == "tempo") & (reg["frac"] == 0.20)].copy()
if not d5.empty and d5["passo"].nunique() > 1:
    fig, ax = plt.subplots(figsize=(7, 4))
    variantes = [v for v in ORDEM_VAR if v in d5["variante"].unique()]
    d5["leituras_por_aresta"] = 8640000 // d5["passo"] + 1
    sns.lineplot(data=d5, x="leituras_por_aresta", y="mediana_ms",
                 hue="variante", hue_order=variantes,
                 palette={v: PALETA[v] for v in variantes},
                 marker="o", linewidth=2, ax=ax)
    ax.set_xlabel("Leituras por aresta (maior = histórico mais denso)")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title("Impacto da densidade de leituras por aresta\nNY · frac=0,20 · grade regular")
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig5_tempo_x_leituras")

# ============================================================
# Figura 6 — cenário mais favorável à B+:
# irregular, frac=0.50, ordem=tempo
# ============================================================
d6 = reg[
    (reg["irregular"] == 1) & (reg["frac"] == 0.50) & (reg["ordem"] == "tempo") &
    (reg["variante"].isin(["map", "vec", "bplus"]))
]
if not d6.empty:
    fig, ax = plt.subplots(figsize=(6, 4))
    variantes = [v for v in ["vec", "bplus", "map"] if v in d6["variante"].unique()]
    sns.barplot(data=d6, x="variante", y="mediana_ms",
                order=variantes, hue="variante", legend=False,
                palette={v: PALETA[v] for v in variantes},
                errorbar=("pi", 50), capsize=0.12, ax=ax)
    ax.set_xlabel("Estrutura"); ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title("Cenário favorável à B+: irregular + alto volume\nNY · frac=0,50 · barra=mediana · traço=IQR")
    salvar(fig, "fig6_cenario_bplus")

# ============================================================
# Figura 7 — K variável: consulta map vs dynvec vs bplus
# ordem=tempo, passo=30000
# ============================================================
d7 = kv[(kv["ordem"] == "tempo") & (kv["passo"] == 30000)]
if not d7.empty:
    fig, ax = plt.subplots(figsize=(7, 4))
    variantes = [v for v in ORDEM_KV if v in d7["variante"].unique()]
    sns.lineplot(data=d7, x="frac", y="mediana_ms",
                 hue="variante", hue_order=variantes,
                 palette={v: PALETA[v] for v in variantes},
                 marker="o", linewidth=2, ax=ax)
    ax.set_xlabel("Fração de arestas com histórico")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title("K variável por aresta: consulta\nNY · passo=5 min · ordem=tempo")
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig7_kvariavel_consulta")

# ============================================================
# Figura 8 — K variável: custo de ingestão (tempo vs aleatorio)
# frac=0.20, passo=30000
# ============================================================
d8 = kv[(kv["frac"] == 0.20) & (kv["passo"] == 30000) &
        (kv["variante"].isin(["map", "dynvec", "bplus"]))]
if not d8.empty and d8["ordem"].nunique() > 1:
    fig, ax = plt.subplots(figsize=(6, 4))
    variantes = [v for v in ORDEM_KV if v in d8["variante"].unique()]
    sns.barplot(data=d8, x="variante", y="construcao_ms", hue="ordem",
                order=variantes, palette="Set1",
                capsize=0.1, errorbar=("pi", 50), ax=ax)
    ax.set_xlabel("Estrutura"); ax.set_ylabel("Tempo de construção (ms)")
    ax.set_title("K variável: custo de ingestão por ordem\nNY · frac=0,20 · passo=5 min")
    ax.legend(title="Ordem de ingestão")
    salvar(fig, "fig8_kvariavel_ingestao")

# ============================================================
# Figura 9 — K variável: memória (H1: dynvec usa menos que map)
# ordem=tempo, passo=30000
# ============================================================
d9 = kv[(kv["ordem"] == "tempo") & (kv["passo"] == 30000)]
if not d9.empty:
    fig, ax = plt.subplots(figsize=(7, 4))
    variantes = [v for v in ORDEM_KV if v in d9["variante"].unique()]
    sns.lineplot(data=d9, x="frac", y="memoria_mb",
                 hue="variante", hue_order=variantes,
                 palette={v: PALETA[v] for v in variantes},
                 marker="o", linewidth=2, ax=ax)
    ax.set_xlabel("Fração de arestas com histórico")
    ax.set_ylabel("Memória RSS (MB)")
    ax.set_title("K variável: uso de memória\nNY · passo=5 min · ordem=tempo")
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig9_kvariavel_memoria")

print(f"\nTodos os gráficos salvos em resultados/graficos/")
