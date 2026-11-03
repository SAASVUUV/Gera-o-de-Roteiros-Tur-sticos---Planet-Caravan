"""
Compara as versões a partir das saídas do check_results (<arquivo>_checked.tsv).

Uso, a partir da raiz do repositório:

    python scripts/compare_versions.py --out comparison.md \\
        --section "NOLA, perfil Rio" "Results Analysis/My Results/NOLA/rio" \\
        --section "Rio" "Results Analysis/My Results/NOLA/rio_reference"

Cada pasta de seção tem um results_<versão>_checked.tsv por versão (python,
literal, v1, v2, v3, v4, greedy; as que faltarem são omitidas). Para cada seção
o relatório traz, por grupo (iterações × budget × distância), o fitness médio
das soluções viáveis, a % de viáveis, o tempo médio e o do_grasp. No fim vem
uma tabela que resume cada versão em todas as seções, para comparar instâncias.
"""

import argparse
import glob
import json
import os
import re

import pandas as pd

VERSION_ORDER = ["python", "literal", "v1", "v2", "v3", "v4", "greedy"]
VERSION_LABEL = {
    "python": "Python (autores)", "literal": "Literal C++", "v1": "GRASP v1",
    "v2": "GRASP v2", "v3": "GRASP v3", "v4": "GRASP v4", "greedy": "Greedy",
}


def infer_iterations(df, order):
    """Preenche max_iterations nas linhas GRASP sem seção (Python e tradução
    literal não escrevem as linhas "# ... max_iterations=N").

    Os experimentos rodam os valores de max_iterations na ordem de
    algorithm.json, cada um com o mesmo número de linhas, então a posição da
    linha diz o grupo."""
    missing = (df["algorithm"] == "GRASP") & df["secao"].map(iterations_of).isna()
    if not missing.any():
        return df
    if not order:
        raise SystemExit("Não dá para inferir max_iterations: informe a ordem "
                         "(Parameters/algorithm.json na pasta da seção)")
    rows = df.index[missing]
    if len(rows) % len(order):
        raise SystemExit(f"{len(rows)} linhas GRASP não se dividem em {len(order)} grupos")
    block = len(rows) // len(order)
    df = df.copy()
    for i, row in enumerate(rows):
        df.loc[row, "secao"] = f"GRASP max_iterations={order[i // block]}"
    return df


def iteration_order(folder, others):
    """Ordem dos max_iterations: algorithm.json da seção ou, sem ele, os valores
    que todas as versões com seção têm em comum, em ordem crescente (como no
    algorithm.json padrão). Uma versão com grupos extras (ex.: 200 e 500) não
    entra na conta."""
    path = os.path.join(folder, "Parameters", "algorithm.json")
    if os.path.exists(path):
        with open(path) as file:
            return json.load(file)["GRASP"]["max_iterations"]
    sets = []
    for df in others:
        values = {iterations_of(s) for s in df["secao"].unique()} - {None}
        if values:
            sets.append(values)
    return sorted(set.intersection(*sets)) if sets else []


def load_section(folder, iterations=None):
    """{versão: DataFrame do _checked.tsv}, na ordem de VERSION_ORDER.

    iterations: se informado, mantém só esses grupos de max_iterations."""
    found = {}
    for path in glob.glob(os.path.join(folder, "results_*_checked.tsv")):
        version = re.fullmatch(r"results_(.+)_checked\.tsv", os.path.basename(path)).group(1)
        if version in VERSION_ORDER:
            df = pd.read_table(path)
            df["secao"] = df["secao"].astype(object).where(df["secao"].notna(), "")
            if version == "python":  # o main.py roda GRASP e Greedy; o Greedy tem arquivo próprio
                df = df[df["algorithm"] == "GRASP"].reset_index(drop=True)
            found[version] = df
    if not found:
        raise SystemExit(f"Nenhum results_<versão>_checked.tsv em {folder}")
    order = iteration_order(folder, found.values())
    section = {}
    for version in VERSION_ORDER:
        if version not in found:
            continue
        df = infer_iterations(found[version], order)
        if iterations is not None:
            it = df["secao"].map(iterations_of)
            df = df[it.isna() | it.isin(iterations)]
        section[version] = df
    return section


def iterations_of(section):
    match = re.search(r"max_iterations=(\d+)", str(section))
    return int(match.group(1)) if match else None


def group_stats(df):
    """Uma linha por (iterações, budget, distância) com as métricas do grupo."""
    df = df.assign(iterations=df["secao"].map(iterations_of),
                   from_grasp=df.get("origem", pd.Series("grasp", index=df.index)) != "greedy")
    feasible = df[df["viavel"] == 1]
    keys = ["iterations", "budget", "total_distance"]
    stats = df.groupby(keys, dropna=False).agg(
        n=("fitness", "size"),
        feasible=("viavel", "mean"),
        fitness_all=("fitness", "mean"),
        time=("time", "mean"),
        do_grasp=("from_grasp", "mean"),
    )
    stats["fitness_feasible"] = feasible.groupby(keys, dropna=False)["fitness"].mean()
    return stats.reset_index()


def merge_versions(section):
    """Tabela larga: uma linha por grupo, colunas (métrica, versão).

    O Greedy não tem iterações: seu valor se repete em todas as linhas de
    iterações, para comparar lado a lado."""
    stats = {v: group_stats(df) for v, df in section.items()}
    iterations = sorted({i for s in stats.values() for i in s["iterations"].dropna().unique()})
    rows = []
    for version, s in stats.items():
        if s["iterations"].isna().all() and iterations:
            s = pd.concat([s.assign(iterations=i) for i in iterations])
        rows.append(s.assign(version=version))
    long = pd.concat(rows)
    wide = long.pivot_table(index=["iterations", "budget", "total_distance"],
                            columns="version", aggfunc="first")
    return wide


def markdown_table(wide, metric, fmt):
    versions = [v for v in VERSION_ORDER if (metric, v) in wide.columns]
    header = "| iterações | budget | distância | " + " | ".join(VERSION_LABEL[v] for v in versions) + " |"
    lines = [header, "|" + "---|" * (3 + len(versions))]
    for (iterations, budget, distance), row in wide.iterrows():
        cells = [fmt(row[(metric, v)]) if pd.notna(row[(metric, v)]) else "–" for v in versions]
        it = "–" if pd.isna(iterations) else f"{int(iterations)}"
        lines.append(f"| {it} | {budget:g} | {distance:g} | " + " | ".join(cells) + " |")
    return "\n".join(lines)


def overview(sections):
    """Resumo por versão e seção: médias sobre todas as soluções."""
    lines = ["| seção | versão | soluções | % viáveis | fitness médio (viáveis) | tempo médio (s) |",
             "|---|---|---|---|---|---|"]
    for title, section in sections:
        for version, df in section.items():
            feasible = df[df["viavel"] == 1]
            fitness = feasible["fitness"].mean() if len(feasible) else float("nan")
            lines.append(f"| {title} | {VERSION_LABEL[version]} | {len(df)} | "
                         f"{100 * df['viavel'].mean():.1f}% | {fitness:.2f} | {df['time'].mean():.4f} |")
    return "\n".join(lines)


def report(sections):
    parts = ["# Comparação das versões", "",
             "Gerado por `scripts/compare_versions.py` a partir das saídas do `check_results`. "
             "Fitness máximo possível: 45 (4 locais/dia, regra dos autores).", ""]
    for title, section in sections:
        wide = merge_versions(section)
        parts += [f"## {title}", "",
                  "### Fitness médio das soluções viáveis", "",
                  markdown_table(wide, "fitness_feasible", lambda x: f"{x:.2f}"), "",
                  "### % de soluções viáveis", "",
                  markdown_table(wide, "feasible", lambda x: f"{100 * x:.0f}%"), "",
                  "### Tempo médio por execução (s)", "",
                  markdown_table(wide, "time", lambda x: f"{x:.4f}"), ""]
        if any(("do_grasp", v) in wide.columns for v in ("v3", "v4")):
            only = wide.loc[:, [c for c in wide.columns if c[0] == "do_grasp" and c[1] in ("v3", "v4")]]
            parts += ["### do_grasp (v3/v4: % das soluções que vieram do GRASP, não do Greedy semente)", "",
                      markdown_table(only, "do_grasp", lambda x: f"{100 * x:.0f}%"), ""]
    parts += ["## Resumo por seção", "", overview(sections), ""]
    return "\n".join(parts)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    parser.add_argument("--section", nargs=2, action="append", required=True,
                        metavar=("TÍTULO", "PASTA"))
    parser.add_argument("--iterations", type=int, nargs="+",
                        help="só estes max_iterations (ex.: 10 50 100)")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    sections = [(title, load_section(folder, args.iterations)) for title, folder in args.section]
    text = report(sections)
    with open(args.out, "w", encoding="utf-8") as file:
        file.write(text)
    print(f"Relatório salvo em {args.out}")


if __name__ == "__main__":
    main()
