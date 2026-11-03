"""
Converte o Yelp Open Dataset em Input/<CITY>.csv, no mesmo formato do RJ.csv.

O Yelp só permite uso acadêmico e proíbe compartilhar os dados, então nem o
JSON nem o CSV gerado entram no git (ver .gitignore).

Uso, a partir da raiz do repositório:

    python scripts/yelp_to_csv.py [--city NOLA] [--target-size 2100]

Regras da conversão:
  - Locais: só os abertos (is_open = 1) dentro do limite do município
    (Input/<CITY>_boundary.geojson, baixado do OSM se não existir). O campo
    "city" do Yelp não serve: mistura cidades vizinhas.
  - Categoria, por ordem de prioridade: hotel, restaurante, atração (listas
    abaixo). Locais sem nenhuma dessas categorias ficam de fora.
  - Tamanho: fica o menor número mínimo de avaliações que deixa no máximo
    --target-size locais (ou --min-reviews, se informado).
  - local url: sintética, no padrão do TripAdvisor, porque o main.py usa o
    prefixo "/Hotel" para pôr os hotéis primeiro.
  - rating: as estrelas do Yelp (já vão de 0,5 em 0,5, como no TripAdvisor).
  - min/max price: a faixa $–$$$$ do Yelp vira o preço de um local "típico"
    daquele quartil de preço no RJ.csv, por categoria. Assim os budgets do Rio
    continuam fazendo sentido. Sem faixa: a mediana da categoria no Rio.
  - review count: número de avaliações (coluna extra, ainda não usada).
"""

import argparse
import json
import os
import sys

import numpy as np
import pandas as pd

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import build_distance_matrix as bdm  # noqa: E402

CITY_STATE = {"NOLA": "LA"}

HOTEL_CATEGORIES = {"Hotels", "Bed & Breakfast", "Hostels", "Resorts", "Guest Houses"}
RESTAURANT_CATEGORIES = {"Restaurants", "Cafes"}
# Na ordem de preferência para o tipo exibido (coluna "category" do CSV).
ATTRACTION_CATEGORIES = [
    "Museums", "Landmarks & Historical Buildings", "Parks", "Botanical Gardens",
    "Zoos", "Aquariums", "Art Galleries", "Performing Arts", "Music Venues",
    "Jazz & Blues", "Churches", "Funeral Services & Cemeteries",
    "Walking Tours", "Historical Tours", "Tours", "Cultural Center",
    "Public Markets", "Amusement Parks", "Festivals", "Bars", "Beaches",
]

# Categorias do RJ.csv que o main.py trata como hotel e restaurante; o resto é atração.
RJ_HOTEL, RJ_RESTAURANT = "hotels", "restaurant"
CSV_COLUMNS = ["local url", "name", "min price", "max price", "rating",
               "latitude", "longitude", "category", "time duration", "review count"]


def categories_of(business):
    return {c.strip() for c in (business.get("categories") or "").split(",") if c.strip()}


def classify(business):
    """(tipo geral, tipo exibido) ou None se o local não interessa."""
    categories = categories_of(business)
    if categories & HOTEL_CATEGORIES:
        return "hotel", RJ_HOTEL
    if categories & RESTAURANT_CATEGORIES:
        return "restaurant", RJ_RESTAURANT
    for category in ATTRACTION_CATEGORIES:
        if category in categories:
            return "attraction", category
    return None


def price_level(business):
    """1–4 a partir de RestaurantsPriceRange2, ou None se ausente/inválido."""
    value = (business.get("attributes") or {}).get("RestaurantsPriceRange2")
    try:
        level = int(str(value).strip("'\" "))
    except ValueError:
        return None
    return level if 1 <= level <= 4 else None


def price_table(rj):
    """{tipo geral: {nível ou None: (min, max)}} calibrada nos quartis do Rio.

    O nível k (1–4) recebe o preço no meio do k-ésimo quartil, (2k − 1)/8.
    Mínimo e máximo usam o mesmo quantil, então min <= max sempre que isso vale
    no Rio."""
    general = np.where(rj["category"] == RJ_HOTEL, "hotel",
                       np.where(rj["category"] == RJ_RESTAURANT, "restaurant", "attraction"))
    table = {}
    for kind in ("hotel", "restaurant", "attraction"):
        rows = rj[general == kind]
        if rows.empty:
            raise SystemExit(f"RJ.csv não tem locais do tipo {kind} para calibrar os preços")
        table[kind] = {
            level: (float(rows["min price"].quantile(q)), float(rows["max price"].quantile(q)))
            for level, q in ((1, 1 / 8), (2, 3 / 8), (3, 5 / 8), (4, 7 / 8), (None, 1 / 2))
        }
    return table


def synthetic_url(kind, business_id):
    prefix = {"hotel": "Hotel", "restaurant": "Restaurant", "attraction": "Attraction"}[kind]
    if "html" in business_id:
        # o main.py apaga "html" dos nomes, e dois locais poderiam colidir
        raise ValueError(f"business_id contém 'html': {business_id}")
    return f"/{prefix}_Review-{business_id}-Reviews.html"


def read_businesses(path, state):
    """Locais do estado, lidos linha a linha (o arquivo tem ~150 mil)."""
    with open(path, encoding="utf-8") as file:
        for line in file:
            business = json.loads(line)
            if business.get("state") == state:
                yield business


def choose_min_reviews(review_counts, target_size):
    """Menor mínimo de avaliações que deixa no máximo target_size locais."""
    counts = np.sort(np.asarray(review_counts))[::-1]
    if len(counts) <= target_size:
        return int(counts.min()) if len(counts) else 0
    # Só sobra quem tem >= t; empates no corte saem juntos, então sobe até caber.
    threshold = int(counts[target_size]) + 1
    return threshold


def convert(businesses, rings, rj, target_size=None, min_reviews=None, tolerance_m=100):
    """DataFrame no formato do RJ.csv, mais um resumo da conversão (dict)."""
    candidates = []
    for business in businesses:
        if business.get("is_open") != 1:
            continue
        kind = classify(business)
        if kind is None:
            continue
        candidates.append((business, kind))

    summary = {"candidates": len(candidates)}
    if not candidates:
        raise SystemExit("Nenhum local aberto com categoria de interesse")

    lat = np.array([float(b["latitude"]) for b, _ in candidates])
    lon = np.array([float(b["longitude"]) for b, _ in candidates])
    inside, _ = bdm.inside_boundary(lat, lon, rings, tolerance_m)
    candidates = [c for c, keep in zip(candidates, inside) if keep]
    summary["inside_boundary"] = len(candidates)

    reviews = [int(b.get("review_count") or 0) for b, _ in candidates]
    if min_reviews is None:
        min_reviews = choose_min_reviews(reviews, target_size) if target_size else 0
    candidates = [c for c, r in zip(candidates, reviews) if r >= min_reviews]
    summary["min_reviews"] = min_reviews

    prices = price_table(rj)
    rows, without_price = [], 0
    for business, (kind, shown) in candidates:
        level = price_level(business)
        without_price += level is None
        low, high = prices[kind][level]
        rows.append({
            "local url": synthetic_url(kind, business["business_id"]),
            "name": " ".join(str(business["name"]).split()),
            "min price": low,
            "max price": high,
            "rating": float(business["stars"]),
            "latitude": float(business["latitude"]),
            "longitude": float(business["longitude"]),
            "category": shown,
            "time duration": 1.0,
            "review count": int(business.get("review_count") or 0),
        })

    df = pd.DataFrame(rows, columns=CSV_COLUMNS)
    summary.update(
        places=len(df),
        hotels=int((df["category"] == RJ_HOTEL).sum()),
        restaurants=int((df["category"] == RJ_RESTAURANT).sum()),
        attractions=int((~df["category"].isin([RJ_HOTEL, RJ_RESTAURANT])).sum()),
        five_star_hotels=int(((df["category"] == RJ_HOTEL) & (df["rating"] == 5)).sum()),
        without_price=without_price,
        price_table=prices,
    )
    return df, summary


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    parser.add_argument("--city", default="NOLA")
    parser.add_argument("--yelp-json",
                        default="Input/yelp/yelp_academic_dataset_business.json")
    parser.add_argument("--target-size", type=int, default=2100,
                        help="máximo de locais (escolhe o mínimo de avaliações)")
    parser.add_argument("--min-reviews", type=int,
                        help="mínimo de avaliações fixo (ignora --target-size)")
    parser.add_argument("--boundary-tolerance-m", type=float, default=100)
    parser.add_argument("--out", help="padrão: Input/<city>.csv")
    args = parser.parse_args()

    if args.city not in CITY_STATE:
        raise SystemExit(f"Cidade desconhecida: {args.city}. Adicione-a em CITY_STATE.")

    rj = pd.read_table("Input/RJ.csv", sep=";")
    rings = bdm.load_boundary(args.city)
    df, summary = convert(
        read_businesses(args.yelp_json, CITY_STATE[args.city]), rings, rj,
        target_size=args.target_size, min_reviews=args.min_reviews,
        tolerance_m=args.boundary_tolerance_m,
    )

    out = args.out or f"Input/{args.city}.csv"
    df.to_csv(out, sep=";", index=False)

    print(f"{summary['candidates']} locais abertos com categoria de interesse no estado, "
          f"{summary['inside_boundary']} dentro do município")
    print(f"Mínimo de avaliações: {summary['min_reviews']}")
    print(f"Salvos {summary['places']} locais em {out}: {summary['hotels']} hotéis "
          f"({summary['five_star_hotels']} com nota 5), {summary['restaurants']} "
          f"restaurantes, {summary['attractions']} atrações; "
          f"{summary['without_price']} sem faixa de preço (mediana do Rio)")
    print("Tabela de preços (min, max) por faixa, calibrada no RJ.csv:")
    for kind, levels in summary["price_table"].items():
        text = ", ".join(f"{'$' * level if level else 'sem faixa'}: {low:g}–{high:g}"
                         for level, (low, high) in levels.items())
        print(f"  {kind}: {text}")


if __name__ == "__main__":
    main()
