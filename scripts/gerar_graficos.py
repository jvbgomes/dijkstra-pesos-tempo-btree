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
os.makedirs("resultados/graficos", exist_ok=True)

# ---- estilo global ----
sns.set_theme(style="whitegrid", context="paper", font_scale=1.2)
PALETA = {
    "array": "#2196F3",
    "hash":  "#4CAF50",
    "vec":   "#9C27B0",
    "bplus": "#F44336",
    "map":   "#FF9800",
}
ORDEM = ["array", "hash", "vec", "bplus", "map"]

def salvar(fig, nome):
    for ext in ("pdf", "png"):
        fig.savefig(f"resultados/graficos/{nome}.{ext}", bbox_inches="tight")
    print(f"  → resultados/graficos/{nome}.png")
    plt.close(fig)

# ============================================================
# Figura 1 — mediana de tempo de consulta por variante
# cenário base: regular, ordem=tempo, passo=30000, frac=0.20
# ============================================================
d1 = df[
    (df["irregular"] == 0) &
    (df["ordem"] == "tempo") &
    (df["passo"] == 30000) &
    (df["frac"] == 0.20)
]

if not d1.empty:
    fig, ax = plt.subplots(figsize=(7, 4))
    variantes = [v for v in ORDEM if v in d1["variante"].unique()]
    paleta = {v: PALETA[v] for v in variantes}

    sns.barplot(
        data=d1, x="variante", y="mediana_ms",
        order=variantes, palette=paleta,
        errorbar=("pi", 50),   # intervalo entre Q25 e Q75
        capsize=0.1, ax=ax
    )
    ax.set_xlabel("Estrutura de armazenamento")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title(
        "Tempo do Dijkstra TD por variante\n"
        "NY · frac=0,20 · passo=5 min · grade regular"
    )
    salvar(fig, "fig1_mediana_por_variante")

# ============================================================
# Figura 2 — mediana × fração de arestas com histórico
# regular, ordem=tempo, passo=30000
# ============================================================
d2 = df[
    (df["irregular"] == 0) &
    (df["ordem"] == "tempo") &
    (df["passo"] == 30000)
]

if not d2.empty:
    fig, ax = plt.subplots(figsize=(7, 4))
    variantes = [v for v in ORDEM if v in d2["variante"].unique()]
    paleta = {v: PALETA[v] for v in variantes}

    sns.lineplot(
        data=d2, x="frac", y="mediana_ms",
        hue="variante", hue_order=variantes, palette=paleta,
        marker="o", linewidth=2, ax=ax
    )
    ax.set_xlabel("Fração de arestas com histórico de tráfego")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title(
        "Escalabilidade com volume de histórico\n"
        "NY · passo=5 min · grade regular"
    )
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig2_tempo_x_frac")

# ============================================================
# Figura 3 — regular vs irregular (map, vec, bplus)
# frac=0.20, passo=30000, ordem=tempo
# ============================================================
d3 = df[
    (df["frac"] == 0.20) &
    (df["passo"] == 30000) &
    (df["ordem"] == "tempo") &
    (df["variante"].isin(["map", "vec", "bplus"]))
].copy()
d3["horários"] = d3["irregular"].map({0: "Regular", 1: "Irregular"})

if not d3.empty:
    fig, ax = plt.subplots(figsize=(6, 4))
    variantes = [v for v in ["vec", "bplus", "map"] if v in d3["variante"].unique()]
    paleta = {v: PALETA[v] for v in variantes}

    sns.barplot(
        data=d3, x="variante", y="mediana_ms",
        hue="horários", order=variantes,
        palette="Set2", capsize=0.1,
        errorbar=("pi", 50), ax=ax
    )
    ax.set_xlabel("Estrutura de armazenamento")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title(
        "Grade regular vs horários irregulares\n"
        "NY · frac=0,20 · passo=5 min"
    )
    ax.legend(title="Horários")
    salvar(fig, "fig3_regular_vs_irregular")

# ============================================================
# Figura 4 — custo de ingestão por ordem de chegada
# frac=0.20, passo=30000, regular; map, vec, bplus
# ============================================================
d4 = df[
    (df["frac"] == 0.20) &
    (df["passo"] == 30000) &
    (df["irregular"] == 0) &
    (df["variante"].isin(["map", "vec", "bplus"]))
]

if not d4.empty and d4["ordem"].nunique() > 1:
    fig, ax = plt.subplots(figsize=(6, 4))
    variantes = [v for v in ["vec", "bplus", "map"] if v in d4["variante"].unique()]

    sns.barplot(
        data=d4, x="variante", y="construcao_ms",
        hue="ordem", order=variantes,
        palette="Set1", capsize=0.1,
        errorbar=("pi", 50), ax=ax
    )
    ax.set_xlabel("Estrutura de armazenamento")
    ax.set_ylabel("Tempo de construção (ms)")
    ax.set_title(
        "Custo de ingestão por ordem de chegada\n"
        "NY · frac=0,20 · passo=5 min"
    )
    ax.legend(title="Ordem de ingestão")
    salvar(fig, "fig4_custo_ingestao")

# ============================================================
# Figura 5 — mediana × passo das leituras
# regular, ordem=tempo, frac=0.20
# ============================================================
d5 = df[
    (df["irregular"] == 0) &
    (df["ordem"] == "tempo") &
    (df["frac"] == 0.20)
]

if not d5.empty and d5["passo"].nunique() > 1:
    fig, ax = plt.subplots(figsize=(7, 4))
    variantes = [v for v in ORDEM if v in d5["variante"].unique()]
    paleta = {v: PALETA[v] for v in variantes}

    # converte passo para número de leituras por aresta (mais intuitivo)
    d5 = d5.copy()
    d5["leituras_por_aresta"] = 8640000 // d5["passo"] + 1

    sns.lineplot(
        data=d5, x="leituras_por_aresta", y="mediana_ms",
        hue="variante", hue_order=variantes, palette=paleta,
        marker="o", linewidth=2, ax=ax
    )
    ax.set_xlabel("Leituras por aresta (maior = histórico mais denso)")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title(
        "Impacto da densidade de leituras por aresta\n"
        "NY · frac=0,20 · grade regular"
    )
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig5_tempo_x_leituras")

# ============================================================
# Figura 6 — cenário mais favorável à B+:
# irregular, alto volume (frac=0.50), ordem=tempo
# ============================================================
d6 = df[
    (df["irregular"] == 1) &
    (df["frac"] == 0.50) &
    (df["ordem"] == "tempo") &
    (df["variante"].isin(["map", "vec", "bplus"]))
]

if not d6.empty:
    fig, ax = plt.subplots(figsize=(6, 4))
    variantes = [v for v in ["vec", "bplus", "map"] if v in d6["variante"].unique()]
    paleta = {v: PALETA[v] for v in variantes}

    sns.barplot(
        data=d6, x="variante", y="mediana_ms",
        order=variantes, palette=paleta,
        errorbar=("pi", 50), capsize=0.12, ax=ax
    )
    ax.set_xlabel("Estrutura de armazenamento")
    ax.set_ylabel("Tempo de consulta — mediana (ms)")
    ax.set_title(
        "Cenário favorável à B+: irregular + alto volume\n"
        "NY · frac=0,50 · barra=mediana · traço=IQR"
    )
    salvar(fig, "fig6_cenario_bplus")

print("\nTodos os gráficos salvos em resultados/graficos/")
