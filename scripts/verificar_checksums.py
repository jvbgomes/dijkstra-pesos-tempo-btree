"""
verificar_checksums.py
Verifica se todas as variantes produzem o mesmo checksum para cada origem.

Le os CSVs por origem em resultados/, agrupa por configuracao (frac, passo,
cenario, ordem) e origem, e compara checksums entre variantes presentes.
Salva relatorio em resultados/verificacao_checksums.txt e tambem o imprime.

Uso:
    python3 scripts/verificar_checksums.py
    python3 scripts/verificar_checksums.py <diretorio_resultados>
"""
import os
import re
import sys
from collections import defaultdict

resultados_dir = sys.argv[1] if len(sys.argv) > 1 else "resultados"
relatorio_path = os.path.join(resultados_dir, "verificacao_checksums.txt")

# Padrao: {variante}_f{frac}_p{passo}_{cenario}_{ordem}.csv
FNAME_RE = re.compile(
    r"^(?P<variante>[a-z]+)_f(?P<frac>[\d.]+)_p(?P<passo>\d+)"
    r"_(?P<cenario>[a-z]+)_(?P<ordem>[a-z]+)\.csv$"
)
# Linha de dados: variante,origem,tempo_ms,alcancaveis,checksum
DATA_RE = re.compile(r"^[a-z]+,\d+,[\d.]+,\d+,\d+$")

files = sorted(f for f in os.listdir(resultados_dir)
               if f.endswith(".csv") and f != "resumo.csv")

# (frac, passo, cenario, ordem) -> origem -> variante -> checksum
configs: dict = defaultdict(lambda: defaultdict(dict))
parseados = 0
sem_dados = []

for fname in files:
    m = FNAME_RE.match(fname)
    if not m:
        continue
    variante = m.group("variante")
    config_key = (m.group("frac"), m.group("passo"),
                  m.group("cenario"), m.group("ordem"))
    fpath = os.path.join(resultados_dir, fname)
    n_data = 0
    try:
        with open(fpath) as fh:
            for line in fh:
                line = line.strip()
                if not DATA_RE.match(line):
                    continue
                parts = line.split(",")
                if len(parts) != 5 or parts[0] == "variante":
                    continue
                origem = int(parts[1])
                checksum = int(parts[4])
                configs[config_key][origem][variante] = checksum
                n_data += 1
    except OSError as e:
        sem_dados.append(f"{fname}: {e}")
        continue
    if n_data > 0:
        parseados += 1
    else:
        sem_dados.append(f"{fname}: sem linhas de dados reconhecidas")

linhas = [
    "Relatorio de verificacao de checksums",
    f"Arquivos processados com dados: {parseados}",
    "",
]

divergencias_total = 0
configs_ok = 0
configs_uma_variante = 0

for config_key in sorted(configs.keys()):
    frac, passo, cenario, ordem = config_key
    origens_map = configs[config_key]
    variantes_config = sorted({v for d in origens_map.values() for v in d})

    if len(variantes_config) < 2:
        configs_uma_variante += 1
        continue

    erros_config = []
    for origem in sorted(origens_map.keys()):
        checksums = origens_map[origem]
        valores = set(checksums.values())
        if len(valores) > 1:
            erros_config.append(
                f"  origem {origem}: {checksums}"
            )

    if erros_config:
        divergencias_total += len(erros_config)
        linhas.append(
            f"[DIVERGENCIA] frac={frac} passo={passo} cenario={cenario} "
            f"ordem={ordem} variantes={variantes_config}"
        )
        linhas.extend(erros_config)
    else:
        configs_ok += 1

linhas += [
    "",
    f"Resultado: {configs_ok} configuracoes OK (>= 2 variantes, sem divergencias)",
    f"           {configs_uma_variante} configuracoes com apenas uma variante (nao verificadas)",
    f"           {divergencias_total} pares origem/configuracao com divergencia",
]

if sem_dados:
    linhas += ["", f"Arquivos sem dados ou com erro ({len(sem_dados)}):"]
    linhas += [f"  {s}" for s in sem_dados]

report = "\n".join(linhas)
print(report)
with open(relatorio_path, "w") as fh:
    fh.write(report + "\n")
print(f"\nRelatorio salvo em {relatorio_path}")
