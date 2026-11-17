"""
gerar_graficos.py
Le resultados/resumo.csv e gera os graficos do artigo com matplotlib.

Figuras PRINCIPAIS (artigo):  figA, figB, figC
  Mostram apenas tres estruturas com rotulos padronizados:
    "vetor ordenado", "árvore B+", "árvore rubro-negra (std::map)"
  Mesmas cores em todas as figuras principais.

Figuras COMPLEMENTARES (repositorio): fig1 a fig9
  Mostram todas as variantes; servem de suporte ao repositorio.

Uso:
    python3 scripts/gerar_graficos.py
    python3 scripts/gerar_graficos.py outro_resumo.csv
"""
import sys
import os
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt

matplotlib.rcParams.update({
    "font.size": 11, "axes.titlesize": 10, "axes.labelsize": 10,
    "legend.fontsize": 9, "xtick.labelsize": 9, "ytick.labelsize": 9,
    "axes.grid": True, "grid.alpha": 0.35, "figure.dpi": 120,
})

csv_path = sys.argv[1] if len(sys.argv) > 1 else "resultados/resumo.csv"
if not os.path.exists(csv_path):
    print(f"Arquivo nao encontrado: {csv_path}")
    print("Execute primeiro: ./scripts/run_all.sh data/USA-road-t.NY.gr 100")
    sys.exit(1)

df = pd.read_csv(csv_path)
if "k_variavel" not in df.columns:
    df["k_variavel"] = 0

os.makedirs("resultados/graficos", exist_ok=True)

# ================================================================
# Paletas e constantes
# ================================================================

PALETA = {
    "array":  "#2196F3",
    "hash":   "#4CAF50",
    "vec":    "#9C27B0",
    "dynvec": "#795548",
    "bplus":  "#F44336",
    "map":    "#FF9800",
}
ORDEM_VAR = ["array", "hash", "vec", "dynvec", "bplus", "map"]
ORDEM_KV  = ["dynvec", "bplus", "map"]

# Figuras principais: rotulos padronizados para tres estruturas
ROTULO_ART = {
    "vec":    "vetor ordenado",
    "dynvec": "vetor ordenado",
    "bplus":  "árvore B+",
    "map":    "árvore rubro-negra (std::map)",
}
PALETA_ART = {
    "vetor ordenado":                "#9C27B0",
    "árvore B+":                     "#F44336",
    "árvore rubro-negra (std::map)": "#FF9800",
}
ORDEM_ART = ["vetor ordenado", "árvore B+", "árvore rubro-negra (std::map)"]

PAL_ORDEM   = {"tempo": "#1976D2", "chave": "#388E3C", "aleatorio": "#D32F2F"}
PAL_HORARIO = {"Regular": "#42A5F5", "Irregular": "#EF5350"}

CONV_NOTA = "convenção: 30 000 unidades = 5 min em escala de 24 h"

reg = df[df["k_variavel"] == 0]
kv  = df[df["k_variavel"] == 1]

# ================================================================
# Helpers
# ================================================================

def _check_single(data, groupby, context=""):
    """Avisa se algum grupo de `data` tem mais de uma linha do CSV.
    Uma barra ou ponto num grafico com mais de uma linha pode misturar
    configuracoes diferentes (ex: dois passos numa mesma barra)."""
    if data.empty:
        return
    counts = data.groupby(groupby).size()
    over = counts[counts > 1]
    if not over.empty:
        gstr = "+".join(groupby)
        print(f"  [AVISO] {context}: grupos com mais de uma linha do CSV em ({gstr}):")
        for idx, cnt in over.items():
            kv_dict = dict(zip(groupby,
                               idx if isinstance(idx, tuple) else (idx,)))
            print(f"    {kv_dict}: {cnt} linhas -- pode misturar configuracoes")


def _bar_iqr(ax, data, x_col, x_order, palette):
    """Barra simples (sem hue); altura=mediana_ms, tracoes=IQR (q25/q75)."""
    for xi, x in enumerate(x_order):
        rows = data[data[x_col] == x]
        if rows.empty:
            continue
        r = rows.iloc[0]
        ax.bar(xi, r["mediana_ms"], 0.55, color=palette.get(x, "#aaa"), label=x)
        ax.errorbar(xi, r["mediana_ms"],
                    yerr=[[r["mediana_ms"] - r["q25_ms"]],
                          [r["q75_ms"] - r["mediana_ms"]]],
                    fmt="none", color="black", capsize=5, linewidth=1.3)
    ax.set_xticks(range(len(x_order)))
    ax.set_xticklabels(x_order)


def _bar_hue_iqr(ax, data, x_col, x_order, hue_col, hue_order, palette):
    """Barras agrupadas; altura=mediana_ms, tracoes=IQR (q25/q75)."""
    n = len(hue_order)
    bw = 0.75 / n
    offs = np.linspace(-(0.75 - bw) / 2, (0.75 - bw) / 2, n)
    handles = {}
    for hi, hue in enumerate(hue_order):
        for xi, x in enumerate(x_order):
            rows = data[(data[x_col] == x) & (data[hue_col] == hue)]
            if rows.empty:
                continue
            r = rows.iloc[0]
            pos = xi + offs[hi]
            bar = ax.bar(pos, r["mediana_ms"], bw * 0.85,
                         color=palette.get(hue, "#aaa"))
            ax.errorbar(pos, r["mediana_ms"],
                        yerr=[[r["mediana_ms"] - r["q25_ms"]],
                              [r["q75_ms"] - r["mediana_ms"]]],
                        fmt="none", color="black", capsize=3, linewidth=0.9)
            if hue not in handles:
                handles[hue] = bar[0]
    ax.set_xticks(range(len(x_order)))
    ax.set_xticklabels(x_order)
    return handles


def _bar_hue_plain(ax, data, x_col, x_order, hue_col, hue_order, y_col, palette):
    """Barras agrupadas sem IQR (para construcao_ms, memoria_mb)."""
    n = len(hue_order)
    bw = 0.75 / n
    offs = np.linspace(-(0.75 - bw) / 2, (0.75 - bw) / 2, n)
    handles = {}
    for hi, hue in enumerate(hue_order):
        for xi, x in enumerate(x_order):
            rows = data[(data[x_col] == x) & (data[hue_col] == hue)]
            if rows.empty:
                continue
            r = rows.iloc[0]
            pos = xi + offs[hi]
            bar = ax.bar(pos, r[y_col], bw * 0.85,
                         color=palette.get(hue, "#aaa"))
            if hue not in handles:
                handles[hue] = bar[0]
    ax.set_xticks(range(len(x_order)))
    ax.set_xticklabels(x_order)
    return handles


def _line_iqr(ax, data, x_col, hue_col, hue_order, palette, labels=None):
    """Lineplot com faixa de IQR (q25_ms, q75_ms)."""
    if labels is None:
        labels = {h: h for h in hue_order}
    for hue in hue_order:
        d = data[data[hue_col] == hue].sort_values(x_col)
        if d.empty:
            continue
        c = palette.get(hue, "#aaa")
        ax.plot(d[x_col], d["mediana_ms"], marker="o", linewidth=2,
                color=c, label=labels.get(hue, hue))
        ax.fill_between(d[x_col], d["q25_ms"], d["q75_ms"],
                        alpha=0.15, color=c)


def _line_plain(ax, data, x_col, y_col, hue_col, hue_order, palette, labels=None):
    """Lineplot simples sem faixa de IQR (para construcao_ms, memoria_mb)."""
    if labels is None:
        labels = {h: h for h in hue_order}
    for hue in hue_order:
        d = data[data[hue_col] == hue].sort_values(x_col)
        if d.empty:
            continue
        c = palette.get(hue, "#aaa")
        ax.plot(d[x_col], d[y_col], marker="o", linewidth=2,
                color=c, label=labels.get(hue, hue))


def _rotulado(data):
    """Aplica ROTULO_ART ao campo variante para figuras principais."""
    d = data.copy()
    d["variante"] = d["variante"].map(ROTULO_ART)
    return d


def salvar(fig, nome, tipo="complementar"):
    for ext in ("pdf", "png"):
        fig.savefig(f"resultados/graficos/{nome}.{ext}", bbox_inches="tight")
    print(f"  -> resultados/graficos/{nome} ({tipo})")
    plt.close(fig)


# ================================================================
# FIGURAS COMPLEMENTARES (fig1 a fig9)
# ================================================================

print("Gerando figuras complementares...")

# ----------------------------------------------------------------
# Fig 1 -- mediana de tempo de consulta por variante
# cenario base: regular, ordem=tempo, passo=30000, frac=0.20
# ----------------------------------------------------------------
d1 = reg[
    (reg["irregular"] == 0) &
    (reg["ordem"] == "tempo") &
    (reg["passo"] == 30000) &
    (reg["frac"] == 0.20)
]
_check_single(d1, ["variante"], "fig1")
if not d1.empty:
    variantes = [v for v in ORDEM_VAR if v in d1["variante"].unique()]
    fig, ax = plt.subplots(figsize=(7, 4))
    _bar_iqr(ax, d1, "variante", variantes, PALETA)
    ax.set_xlabel("Estrutura de armazenamento")
    ax.set_ylabel("Tempo do Dijkstra (ms), mediana")
    ax.set_title(
        f"Tempo do Dijkstra TD por variante\n"
        f"NY, frac=0,20, passo=5 min ({CONV_NOTA}), grade regular\n"
        f"barra=mediana, traço=IQR (Q25/Q75) sobre 100 origens"
    )
    salvar(fig, "fig1_mediana_por_variante")

# ----------------------------------------------------------------
# Fig 2 -- mediana x fracao de arestas com histórico
# regular, ordem=tempo, passo=30000
# ----------------------------------------------------------------
d2 = reg[(reg["irregular"] == 0) & (reg["ordem"] == "tempo") & (reg["passo"] == 30000)]
_check_single(d2, ["variante", "frac"], "fig2")
if not d2.empty:
    variantes = [v for v in ORDEM_VAR if v in d2["variante"].unique()]
    fig, ax = plt.subplots(figsize=(7, 4))
    _line_iqr(ax, d2, "frac", "variante", variantes, PALETA)
    ax.set_xlabel("Fração de arestas com histórico de tráfego")
    ax.set_ylabel("Tempo do Dijkstra (ms), mediana")
    ax.set_title(
        f"Escalabilidade com volume de histórico\n"
        f"NY, passo=5 min ({CONV_NOTA}), grade regular\n"
        f"linha=mediana, faixa=IQR (Q25/Q75) sobre 100 origens"
    )
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig2_tempo_x_frac")

# ----------------------------------------------------------------
# Fig 3 -- regular vs irregular (map, vec, bplus)
# frac=0.20, passo=30000, ordem=tempo
# ----------------------------------------------------------------
d3 = reg[
    (reg["frac"] == 0.20) & (reg["passo"] == 30000) & (reg["ordem"] == "tempo") &
    (reg["variante"].isin(["map", "vec", "bplus"]))
].copy()
d3["horario"] = d3["irregular"].map({0: "Regular", 1: "Irregular"})
_check_single(d3, ["variante", "horario"], "fig3")
if not d3.empty:
    variantes = [v for v in ["vec", "bplus", "map"] if v in d3["variante"].unique()]
    hue_order = [h for h in ["Regular", "Irregular"] if h in d3["horario"].unique()]
    fig, ax = plt.subplots(figsize=(6, 4))
    h = _bar_hue_iqr(ax, d3, "variante", variantes, "horario", hue_order, PAL_HORARIO)
    ax.legend(handles=list(h.values()), labels=list(h.keys()), title="Horário")
    ax.set_xlabel("Estrutura")
    ax.set_ylabel("Tempo do Dijkstra (ms), mediana")
    ax.set_title(
        f"Grade regular vs horários irregulares\n"
        f"NY, frac=0,20, passo=5 min ({CONV_NOTA})\n"
        f"barra=mediana, traço=IQR sobre 100 origens"
    )
    salvar(fig, "fig3_regular_vs_irregular")

# ----------------------------------------------------------------
# Fig 4 -- custo de ingestao por ordem de chegada
# frac=0.20, passo=30000, regular; map, vec, bplus
# Escala: segundos, eixo logaritmico (vec e bplus ficam visiveis)
# ----------------------------------------------------------------
d4 = reg[
    (reg["frac"] == 0.20) & (reg["passo"] == 30000) & (reg["irregular"] == 0) &
    (reg["variante"].isin(["map", "vec", "bplus"]))
].copy()
d4["construcao_s"] = d4["construcao_ms"] / 1000.0
_check_single(d4, ["variante", "ordem"], "fig4")
if not d4.empty and d4["ordem"].nunique() > 1:
    variantes = [v for v in ["vec", "bplus", "map"] if v in d4["variante"].unique()]
    ordens = [o for o in ["tempo", "chave", "aleatorio"] if o in d4["ordem"].unique()]
    fig, ax = plt.subplots(figsize=(6, 4))
    h = _bar_hue_plain(ax, d4, "variante", variantes, "ordem", ordens,
                       "construcao_s", PAL_ORDEM)
    ax.set_yscale("log")
    ax.set_xlabel("Estrutura")
    ax.set_ylabel("Tempo de construção (s), escala log")
    ax.set_title(
        f"Custo de ingestão por ordem de chegada\n"
        f"NY, frac=0,20, passo=5 min ({CONV_NOTA})"
    )
    ax.legend(handles=list(h.values()), labels=list(h.keys()),
              title="Ordem de ingestão")
    salvar(fig, "fig4_custo_ingestao")

# ----------------------------------------------------------------
# Fig 5 -- mediana x passo das leituras
# regular, ordem=tempo, frac=0.20
# Apenas 2 pontos por linha (passo=30000 e passo=6000)
# ----------------------------------------------------------------
d5 = reg[(reg["irregular"] == 0) & (reg["ordem"] == "tempo") & (reg["frac"] == 0.20)].copy()
_check_single(d5, ["variante", "passo"], "fig5")
if not d5.empty and d5["passo"].nunique() > 1:
    d5["leituras_por_aresta"] = 8640000 // d5["passo"] + 1
    variantes = [v for v in ORDEM_VAR if v in d5["variante"].unique()]
    fig, ax = plt.subplots(figsize=(7, 4))
    _line_iqr(ax, d5, "leituras_por_aresta", "variante", variantes, PALETA)
    passos_disp = sorted(d5["passo"].unique(), reverse=True)
    pts_str = " e ".join(str(8640000 // p + 1) for p in passos_disp)
    ax.set_xlabel("Leituras por aresta (maior = histórico mais denso)")
    ax.set_ylabel("Tempo do Dijkstra (ms), mediana")
    ax.set_title(
        f"Impacto da densidade de leituras por aresta\n"
        f"NY, frac=0,20, grade regular  "
        f"[apenas {len(passos_disp)} pontos por linha: {pts_str} leituras/aresta]"
    )
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig5_tempo_x_leituras")

# ----------------------------------------------------------------
# Fig 6 -- irregular, 50% das arestas (dois paineis: um por passo)
# CORRECAO: versao anterior misturava passo=30000 e passo=6000 numa
#           unica barra (seaborn agregava a media das duas medianas).
#           Agora cada painel usa um unico passo.
# Map nao disponivel para frac=0.50 irregular (pulado no run_all.sh).
# ----------------------------------------------------------------
variantes_6 = [v for v in ["vec", "bplus"]
               if v in reg["variante"].unique()]
fig6_datasets = {
    30000: reg[(reg["irregular"] == 1) & (reg["frac"] == 0.50) &
               (reg["passo"] == 30000) & (reg["ordem"] == "tempo") &
               (reg["variante"].isin(variantes_6))],
    6000:  reg[(reg["irregular"] == 1) & (reg["frac"] == 0.50) &
               (reg["passo"] == 6000) & (reg["ordem"] == "tempo") &
               (reg["variante"].isin(variantes_6))],
}
if any(not d.empty for d in fig6_datasets.values()):
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), sharey=False)
    for ax, (passo, d6) in zip(axes, fig6_datasets.items()):
        _check_single(d6, ["variante"], f"fig6 passo={passo}")
        if d6.empty:
            ax.set_visible(False)
            continue
        vv = [v for v in variantes_6 if v in d6["variante"].unique()]
        _bar_iqr(ax, d6, "variante", vv, PALETA)
        leituras = 8640000 // passo + 1
        ax.set_title(f"passo={passo} ({leituras} leit./aresta)")
        ax.set_xlabel("Estrutura")
        ax.set_ylabel("Tempo do Dijkstra (ms), mediana")
    fig.suptitle(
        "Irregular, 50% das arestas com histórico\n"
        "NY, barra=mediana, traço=IQR  "
        "[map não disponível para frac=0,50 irregular; ver falhas.log]",
        fontsize=9
    )
    fig.tight_layout()
    salvar(fig, "fig6_irregular_frac50")

# ----------------------------------------------------------------
# Fig 7 -- K variável: consulta map vs dynvec vs bplus
# ordem=tempo, passo=30000
# ----------------------------------------------------------------
d7 = kv[(kv["ordem"] == "tempo") & (kv["passo"] == 30000)]
_check_single(d7, ["variante", "frac"], "fig7")
if not d7.empty:
    variantes = [v for v in ORDEM_KV if v in d7["variante"].unique()]
    fig, ax = plt.subplots(figsize=(7, 4))
    _line_iqr(ax, d7, "frac", "variante", variantes, PALETA)
    ax.set_xlabel("Fração de arestas com histórico")
    ax.set_ylabel("Tempo do Dijkstra (ms), mediana")
    ax.set_title(
        f"K variável por aresta: consulta\n"
        f"NY, passo=5 min ({CONV_NOTA}), ordem=tempo\n"
        f"linha=mediana, faixa=IQR sobre 100 origens"
    )
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig7_kvariavel_consulta")

# ----------------------------------------------------------------
# Fig 8 -- K variável: custo de ingestao por ordem
# frac=0.20, passo=30000
# Escala: segundos, eixo logaritmico (dynvec fica visivel)
# ----------------------------------------------------------------
d8 = kv[(kv["frac"] == 0.20) & (kv["passo"] == 30000) &
        (kv["variante"].isin(["map", "dynvec", "bplus"]))].copy()
d8["construcao_s"] = d8["construcao_ms"] / 1000.0
_check_single(d8, ["variante", "ordem"], "fig8")
if not d8.empty and d8["ordem"].nunique() > 1:
    variantes = [v for v in ORDEM_KV if v in d8["variante"].unique()]
    ordens = [o for o in ["tempo", "aleatorio"] if o in d8["ordem"].unique()]
    fig, ax = plt.subplots(figsize=(6, 4))
    h = _bar_hue_plain(ax, d8, "variante", variantes, "ordem", ordens,
                       "construcao_s", PAL_ORDEM)
    ax.set_yscale("log")
    ax.set_xlabel("Estrutura")
    ax.set_ylabel("Tempo de construção (s), escala log")
    ax.set_title(
        f"K variável: custo de ingestão por ordem\n"
        f"NY, frac=0,20, passo=5 min ({CONV_NOTA})"
    )
    ax.legend(handles=list(h.values()), labels=list(h.keys()),
              title="Ordem de ingestão")
    salvar(fig, "fig8_kvariavel_ingestao")

# ----------------------------------------------------------------
# Fig 9 -- K variável: memoria RSS
# ordem=tempo, passo=30000
# ----------------------------------------------------------------
d9 = kv[(kv["ordem"] == "tempo") & (kv["passo"] == 30000)]
_check_single(d9, ["variante", "frac"], "fig9")
if not d9.empty:
    variantes = [v for v in ORDEM_KV if v in d9["variante"].unique()]
    fig, ax = plt.subplots(figsize=(7, 4))
    _line_plain(ax, d9, "frac", "memoria_mb", "variante", variantes, PALETA)
    ax.set_xlabel("Fração de arestas com histórico")
    ax.set_ylabel("Memória RSS (MB)")
    ax.set_title(
        f"K variável: uso de memória\n"
        f"NY, passo=5 min ({CONV_NOTA}), ordem=tempo"
    )
    ax.legend(title="Estrutura", bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "fig9_kvariavel_memoria")


# ================================================================
# FIGURAS PRINCIPAIS DO ARTIGO (figA, figB, figC)
# ================================================================

print("Gerando figuras principais...")

# ----------------------------------------------------------------
# Fig A -- consulta em funcao do volume de histórico (K fixo)
# vec, bplus, map; regular, ordem=tempo, passo=30000
# ----------------------------------------------------------------
dA_raw = reg[
    (reg["irregular"] == 0) & (reg["ordem"] == "tempo") & (reg["passo"] == 30000) &
    (reg["variante"].isin(["vec", "bplus", "map"]))
]
_check_single(dA_raw, ["variante", "frac"], "figA")
if not dA_raw.empty:
    dA = _rotulado(dA_raw)
    fig, ax = plt.subplots(figsize=(7, 4))
    _line_iqr(ax, dA, "frac", "variante", ORDEM_ART, PALETA_ART)
    ax.set_xlabel("Fração de arestas com histórico de tráfego")
    ax.set_ylabel("Tempo do Dijkstra (ms), mediana")
    ax.set_title(
        f"Consulta em função do volume de histórico (K fixo)\n"
        f"NY, passo=5 min ({CONV_NOTA}), grade regular, ordem=tempo\n"
        f"linha=mediana, faixa=IQR (Q25/Q75) sobre 100 origens"
    )
    ax.legend(bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "figA_consulta_kfixo", tipo="principal")

# ----------------------------------------------------------------
# Fig B -- cenario K variável: tres paineis
#   (a) consulta  -- ordem=tempo,     passo=30000
#   (b) ingestao  -- ordem=aleatorio, passo=30000, escala log
#   (c) memoria   -- ordem=tempo,     passo=30000
# dynvec, bplus, map; rotulados como "vetor ordenado", ...
# ----------------------------------------------------------------
dB_q = kv[(kv["passo"] == 30000) & (kv["ordem"] == "tempo") &
          (kv["variante"].isin(["dynvec", "bplus", "map"]))]
dB_i = kv[(kv["passo"] == 30000) & (kv["ordem"] == "aleatorio") &
          (kv["variante"].isin(["dynvec", "bplus", "map"]))].copy()
dB_m = kv[(kv["passo"] == 30000) & (kv["ordem"] == "tempo") &
          (kv["variante"].isin(["dynvec", "bplus", "map"]))]
_check_single(dB_q, ["variante", "frac"], "figB painel consulta")
_check_single(dB_i, ["variante", "frac"], "figB painel ingestão")
_check_single(dB_m, ["variante", "frac"], "figB painel memória")

if not dB_q.empty:
    dB_q_r = _rotulado(dB_q)
    dB_i["construcao_s"] = dB_i["construcao_ms"] / 1000.0
    dB_i_r = _rotulado(dB_i)
    dB_m_r = _rotulado(dB_m)

    fig, axes = plt.subplots(1, 3, figsize=(15, 4))

    # painel (a): consulta com IQR
    _line_iqr(axes[0], dB_q_r, "frac", "variante", ORDEM_ART, PALETA_ART)
    axes[0].set_xlabel("Fração de arestas com histórico")
    axes[0].set_ylabel("Tempo do Dijkstra (ms), mediana")
    axes[0].set_title("(a) Tempo do Dijkstra (ordem=tempo)\nlinha=mediana, faixa=IQR")

    # painel (b): ingestao em segundos, escala log
    for hue in ORDEM_ART:
        d = dB_i_r[dB_i_r["variante"] == hue].sort_values("frac")
        if d.empty:
            continue
        axes[1].plot(d["frac"], d["construcao_s"], marker="o", linewidth=2,
                     color=PALETA_ART.get(hue, "#aaa"), label=hue)
    axes[1].set_yscale("log")
    axes[1].set_xlabel("Fração de arestas com histórico")
    axes[1].set_ylabel("Tempo de construção (s), log")
    axes[1].set_title("(b) Ingestão (ordem=aleatório)")

    # painel (c): memoria
    for hue in ORDEM_ART:
        d = dB_m_r[dB_m_r["variante"] == hue].sort_values("frac")
        if d.empty:
            continue
        axes[2].plot(d["frac"], d["memoria_mb"], marker="o", linewidth=2,
                     color=PALETA_ART.get(hue, "#aaa"), label=hue)
    axes[2].set_xlabel("Fração de arestas com histórico")
    axes[2].set_ylabel("Memória RSS (MB)")
    axes[2].set_title("(c) Memória (ordem=tempo)")
    axes[2].legend(bbox_to_anchor=(1.01, 1), loc="upper left", fontsize=8)

    fig.suptitle(
        f"Cenário K variável por aresta  |  "
        f"NY, passo=5 min ({CONV_NOTA})",
        fontsize=9
    )
    fig.tight_layout()
    salvar(fig, "figB_kvariavel_composto", tipo="principal")

# ----------------------------------------------------------------
# Fig C -- memoria por leitura armazenada (K fixo)
# vec, bplus, map; regular, ordem=tempo, passo=30000
# Bytes por leitura = memoria_mb * 1e6 / leituras
# ----------------------------------------------------------------
dC_raw = reg[
    (reg["irregular"] == 0) & (reg["ordem"] == "tempo") & (reg["passo"] == 30000) &
    (reg["variante"].isin(["vec", "bplus", "map"]))
].copy()
dC_raw["bytes_por_leitura"] = dC_raw["memoria_mb"] * 1e6 / dC_raw["leituras"]
_check_single(dC_raw, ["variante", "frac"], "figC")
if not dC_raw.empty:
    dC = _rotulado(dC_raw)
    fig, ax = plt.subplots(figsize=(7, 4))
    for hue in ORDEM_ART:
        d = dC[dC["variante"] == hue].sort_values("frac")
        if d.empty:
            continue
        ax.plot(d["frac"], d["bytes_por_leitura"], marker="o", linewidth=2,
                color=PALETA_ART.get(hue, "#aaa"), label=hue)
    ax.set_xlabel("Fração de arestas com histórico de tráfego")
    ax.set_ylabel("Bytes por leitura (estimativa RSS)")
    ax.set_title(
        f"Consumo de memória por leitura armazenada (K fixo)\n"
        f"NY, passo=5 min ({CONV_NOTA}), grade regular, ordem=tempo"
    )
    ax.legend(bbox_to_anchor=(1.01, 1), loc="upper left")
    fig.tight_layout()
    salvar(fig, "figC_memoria_por_leitura", tipo="principal")


print(f"\nFiguras salvas em resultados/graficos/")
print(f"  Principais (artigo): figA, figB, figC")
print(f"  Complementares (repo): fig1 a fig9")
