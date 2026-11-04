"""
Exporta só o desempenho dos algoritmos, sem nada que identifique os locais.

Uso, a partir da raiz do repositório:

    python scripts/export_metrics.py "Results Analysis/My Results/NOLA" "Results Analysis/Authors/NOLA"

Para cada results_<versão>_checked.tsv encontrado (recursivamente), grava ao
lado um results_<versão>_metrics.tsv com uma linha por execução e só as colunas
de METRIC_COLUMNS. É o que pode ir para o repositório público nas cidades do
Yelp: as rotas, os nomes, preços e notas dos locais ficam de fora (termos do
Yelp, ver CLAUDE.md).
"""

import argparse
import glob
import os

import pandas as pd

METRIC_COLUMNS = ["secao", "algorithm", "budget", "total_distance", "time",
                  "fitness", "viavel", "origem"]


def metrics(checked):
    """Só as colunas de desempenho, na ordem de METRIC_COLUMNS (as que existirem)."""
    return checked[[c for c in METRIC_COLUMNS if c in checked.columns]]


def export(folder):
    """Grava um _metrics.tsv para cada _checked.tsv da pasta; devolve os caminhos."""
    written = []
    pattern = os.path.join(folder, "**", "results_*_checked.tsv")
    for path in sorted(glob.glob(pattern, recursive=True)):
        out = path[: -len("_checked.tsv")] + "_metrics.tsv"
        metrics(pd.read_table(path)).to_csv(out, sep="\t", index=False)
        written.append(out)
    return written


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    parser.add_argument("folders", nargs="+")
    for folder in parser.parse_args().folders:
        for path in export(folder):
            print(path)


if __name__ == "__main__":
    main()
