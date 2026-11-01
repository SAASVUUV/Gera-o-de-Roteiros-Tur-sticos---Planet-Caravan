"""
Exporta os dados já preparados pelo código Python (src/main.py e src/opt_call.py)
para a versão em C++ (cpp/) ler.

Uso, a partir da raiz do repositório:

    python scripts/export_instance.py [--city RJ] [--out Input/RJ_instance]

Arquivos gerados na pasta de saída:

    places.tsv            id, price, rating, category (um local por linha, na ordem do ID)
    hotels.txt            IDs de places_parameters["hotels"], na mesma ordem
    places_available.txt  IDs de places_parameters["places_available"], na mesma ordem
    greedy_order.txt      ordem em que o Greedy percorre os locais (por nota, decrescente)
    names.tsv             id, nome e tipo original do local (ex.: "Praias"), para mostrar os roteiros
    distance_matrix.bin   matriz n x n em float64 little-endian, linha a linha

Os floats de places.tsv são escritos com repr(), que o C++ lê de volta sem perda.
"""

import argparse
import os
import sys

import numpy as np
import pandas as pd

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src"))

import main  # noqa: E402
import opt_call  # noqa: E402


def export(places_parameters, out_dir, city):

    os.makedirs(out_dir, exist_ok=True)

    price = np.asarray(places_parameters["price"])
    rating = np.asarray(places_parameters["rating"])
    category = np.asarray(places_parameters["category"])
    hotels = np.asarray(places_parameters["hotels"])
    places_available = np.asarray(places_parameters["places_available"])
    distance_matrix = np.asarray(places_parameters["distance_matrix"])

    n = len(price)

    # os arrays são indexados pelo ID do local, então os IDs têm que ser 0..n-1
    ids = np.sort(places_parameters["df"]["Name"].unique())
    assert np.array_equal(ids, np.arange(n)), "IDs não são contíguos 0..n-1"
    assert len(rating) == n and len(category) == n
    assert distance_matrix.shape == (n, n)
    assert price.dtype == np.float64, price.dtype
    assert rating.dtype == np.float64, rating.dtype
    assert distance_matrix.dtype == np.float64, distance_matrix.dtype
    assert not np.isnan(distance_matrix).any(), "matriz com NaN"
    assert set(category) <= {"hotel", "restaurant", "attraction"}, set(category)

    with open(os.path.join(out_dir, "places.tsv"), "w") as file:
        file.write("id\tprice\trating\tcategory\n")
        for place in range(n):
            file.write(
                f"{place}\t{float(price[place])!r}\t{float(rating[place])!r}\t{category[place]}\n"
            )

    # Ordem em que o Greedy (src/heuristics/simple_greedy.py) percorre os locais:
    # mesma expressão do construct_greedy_route. O argsort do numpy não define a
    # ordem dos empates de nota, então ela é gravada aqui em vez de ser refeita
    # no C++.
    greedy_order = places_available[np.flip(np.argsort(rating[places_available]))]

    for name, values in (
        ("hotels", hotels),
        ("places_available", places_available),
        ("greedy_order", greedy_order),
    ):
        with open(os.path.join(out_dir, name + ".txt"), "w") as file:
            file.write("".join(f"{int(value)}\n" for value in values))

    # Nome e tipo original de cada local, só para exibição (não entram nas
    # heurísticas). O main.py reduz a categoria do CSV a hotel/restaurant/
    # attraction; o tipo original (ex.: "Museus de arte") vem direto do CSV.
    df_wr = places_parameters["df"].drop_duplicates(subset=["Name"]).set_index("Name").sort_index()
    csv = pd.read_table(os.path.join("Input", city + ".csv"), sep=";")
    csv.columns = csv.columns.str.title()
    original_type = csv.drop_duplicates(subset=["Local Url"]).set_index("Local Url")["Category"]

    def clean(text):
        return " ".join(str(text).split())  # sem tab nem quebra de linha

    with open(os.path.join(out_dir, "names.tsv"), "w", encoding="utf-8") as file:
        file.write("id\tname\ttype\n")
        for place in range(n):
            row = df_wr.loc[place]
            kind = original_type.get(row["Local Url"])
            if pd.isna(kind) or row["General Category"] != "attraction":
                kind = ""  # só atrações têm um tipo útil para mostrar
            file.write(f"{place}\t{clean(row['Original Name'])}\t{clean(kind)}\n")

    np.ascontiguousarray(distance_matrix, dtype="<f8").tofile(
        os.path.join(out_dir, "distance_matrix.bin")
    )

    print(
        f"{out_dir}: {n} locais, {len(hotels)} hotéis, "
        f"{len(places_available)} locais visitáveis"
    )


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    parser.add_argument("--city", default="RJ")
    parser.add_argument("--out", default=None)
    args = parser.parse_args()

    df, distance_matrix = main.prepare_data(args.city)

    places_parameters = opt_call.build_places_parameters(df, distance_matrix)

    export(places_parameters, args.out or os.path.join("Input", args.city + "_instance"), args.city)
