"""Testes do conversor Yelp -> CSV, com dados fictícios (sem rede, sem Yelp real)."""

import json

import numpy as np
import pandas as pd
import pytest

import yelp_to_csv as y

# Quadrado de ~2,2 km em torno de (0, 0), como anel [lon, lat].
SQUARE = [[[-0.01, -0.01], [0.01, -0.01], [0.01, 0.01], [-0.01, 0.01], [-0.01, -0.01]]]


def business(business_id, categories, stars=4.0, reviews=10, price="2",
             lat=0.0, lon=0.0, is_open=1, name=None, attributes="default"):
    if attributes == "default":
        attributes = {"RestaurantsPriceRange2": price} if price is not None else {}
    return {
        "business_id": business_id, "name": name or f"Place {business_id}",
        "state": "LA", "city": "New Orleans", "latitude": lat, "longitude": lon,
        "stars": stars, "review_count": reviews, "is_open": is_open,
        "categories": categories, "attributes": attributes,
    }


@pytest.fixture
def rj():
    """RJ.csv mínimo: preços crescentes por categoria, para quantis previsíveis."""
    rows = []
    for category, base in (("hotels", 100), ("restaurant", 10), ("Museus", 0)):
        for i in range(9):  # 9 valores: quantis caem em pontos interpolados exatos
            rows.append({"category": category, "min price": base * i,
                         "max price": base * i * 2 + (50 if base == 0 else 0)})
    return pd.DataFrame(rows)


# --- classificação ----------------------------------------------------------


@pytest.mark.parametrize("categories, expected", [
    ("Hotels, Hotels & Travel", ("hotel", "hotels")),
    ("Bed & Breakfast, Event Planning & Services", ("hotel", "hotels")),
    ("Restaurants, Cajun/Creole", ("restaurant", "restaurant")),
    ("Cafes, Coffee & Tea", ("restaurant", "restaurant")),
    ("Museums, Arts & Entertainment", ("attraction", "Museums")),
    ("Bars, Nightlife, Jazz & Blues", ("attraction", "Jazz & Blues")),  # ordem da lista
    ("Hotels, Restaurants, Bars", ("hotel", "hotels")),  # hotel tem prioridade
    ("Restaurants, Bars", ("restaurant", "restaurant")),
])
def test_classify(categories, expected):
    assert y.classify(business("x", categories)) == expected


@pytest.mark.parametrize("categories", [
    None, "", "Hotels & Travel, Car Rental",  # "Hotels & Travel" não é "Hotels"
    "Shopping, Fashion", "Food, Grocery",
])
def test_classify_ignores_other_places(categories):
    assert y.classify(business("x", categories)) is None


# --- preço ------------------------------------------------------------------


@pytest.mark.parametrize("attributes, expected", [
    ({"RestaurantsPriceRange2": "1"}, 1), ({"RestaurantsPriceRange2": "4"}, 4),
    ({"RestaurantsPriceRange2": "'3'"}, 3),  # algumas linhas vêm com aspas
    ({"RestaurantsPriceRange2": "None"}, None), ({"RestaurantsPriceRange2": "5"}, None),
    ({}, None), (None, None),
])
def test_price_level(attributes, expected):
    assert y.price_level(business("x", "Restaurants", attributes=attributes)) == expected


def test_price_table_uses_quartile_midpoints(rj):
    table = y.price_table(rj)
    hotel_min = rj[rj.category == "hotels"]["min price"]
    for level, q in ((1, 1 / 8), (2, 3 / 8), (3, 5 / 8), (4, 7 / 8), (None, 1 / 2)):
        assert table["hotel"][level][0] == pytest.approx(hotel_min.quantile(q))
    # cresce com a faixa e min <= max
    for kind in table:
        levels = [table[kind][k] for k in (1, 2, 3, 4)]
        assert levels == sorted(levels)
        assert all(low <= high for low, high in levels)
    # atrações usam as categorias "outras" do Rio
    assert table["attraction"][None] == (0.0, 50.0)


def test_price_table_requires_each_kind(rj):
    with pytest.raises(SystemExit):
        y.price_table(rj[rj.category != "restaurant"])


# --- url e limite de avaliações ----------------------------------------------


def test_synthetic_url_sorts_hotels_first_in_main():
    urls = [y.synthetic_url(k, "abc") for k in ("attraction", "restaurant", "hotel")]
    # main.py troca "/Hotel" por "/0AHotel_" e ordena: hotéis primeiro
    renamed = [u.replace("/Hotel", "/0AHotel_") for u in urls]
    assert sorted(renamed)[0].startswith("/0AHotel_")


def test_synthetic_url_rejects_html_in_id():
    with pytest.raises(ValueError):
        y.synthetic_url("hotel", "abchtmlxyz")


@pytest.mark.parametrize("counts, target, expected_kept", [
    ([50, 40, 30, 20, 10], 3, 3),
    ([50, 40, 30, 30, 10], 3, 2),  # empate no corte: os dois 30 saem juntos
    ([5, 5, 5], 10, 3),  # menos locais que o alvo: fica tudo
    ([9, 8, 7], 3, 3),
])
def test_choose_min_reviews(counts, target, expected_kept):
    threshold = y.choose_min_reviews(counts, target)
    kept = sum(c >= threshold for c in counts)
    assert kept == expected_kept
    assert kept <= target


# --- conversão completa -------------------------------------------------------


def test_convert_filters_and_formats(rj):
    businesses = [
        business("h5", "Hotels", stars=5.0, reviews=100, price="3"),
        business("h4", "Hotels", stars=4.0, reviews=90, price=None),
        business("r1", "Restaurants", reviews=80, price="1", name="Café; du  Monde"),
        business("a1", "Museums", reviews=70, price=None, stars=4.5),
        business("closed", "Restaurants", reviews=500, is_open=0),
        business("far", "Restaurants", reviews=500, lat=0.5, lon=0.5),  # fora do limite
        business("shop", "Shopping", reviews=500),
        business("few", "Restaurants", reviews=1),  # cortado pelo mínimo
    ]
    df, summary = y.convert(businesses, SQUARE, rj, target_size=4)

    assert list(df.columns) == y.CSV_COLUMNS
    assert set(df["name"]) == {"Place h5", "Place h4", "Café; du Monde", "Place a1"}
    assert summary["candidates"] == 6  # sem fechado e sem shopping
    assert summary["inside_boundary"] == 5
    assert summary["places"] == 4 and summary["hotels"] == 2
    assert summary["five_star_hotels"] == 1 and summary["without_price"] == 2

    table = y.price_table(rj)
    row = df.set_index("name")
    assert (row.loc["Place h5", "min price"], row.loc["Place h5", "max price"]) == table["hotel"][3]
    assert (row.loc["Place h4", "min price"], row.loc["Place h4", "max price"]) == table["hotel"][None]
    assert row.loc["Place a1", "category"] == "Museums"
    assert row.loc["Place a1", "rating"] == 4.5
    assert row.loc["Café; du Monde", "review count"] == 80
    assert df["local url"].is_unique


def test_convert_keeps_place_near_boundary(rj):
    # 0,0105° ≈ 55 m além da borda: fica com tolerância de 100 m, sai com 10 m
    near = [business("n", "Restaurants", lat=0.0, lon=0.0105)]
    assert len(y.convert(near, SQUARE, rj, tolerance_m=100)[0]) == 1
    assert len(y.convert(near, SQUARE, rj, tolerance_m=10)[0]) == 0


def test_convert_without_candidates_fails_clearly(rj):
    with pytest.raises(SystemExit):
        y.convert([business("o", "Shopping")], SQUARE, rj)


def test_csv_round_trip_with_semicolons(rj, tmp_path):
    df, _ = y.convert([business("r", "Restaurants", name='A; "B"')], SQUARE, rj)
    path = tmp_path / "X.csv"
    df.to_csv(path, sep=";", index=False)
    back = pd.read_table(path, sep=";")
    assert back.loc[0, "name"] == 'A; "B"'
    assert np.isclose(back.loc[0, "max price"], df.loc[0, "max price"])


def test_read_businesses_filters_state(tmp_path):
    path = tmp_path / "business.json"
    rows = [business("a", "Hotels"), dict(business("b", "Hotels"), state="PA")]
    path.write_text("\n".join(json.dumps(r) for r in rows) + "\n")
    assert [b["business_id"] for b in y.read_businesses(path, "LA")] == ["a"]


# --- estimativa da faixa pela categoria ----------------------------------------


def priced(n, categories, level):
    return [business(f"{categories}{level}{i}", categories, price=str(level)) for i in range(n)]


@pytest.fixture
def stats():
    rows = (priced(10, "Restaurants, Fast Food", 1) + priced(10, "Restaurants, Steakhouses", 4)
            + priced(5, "Restaurants, Steakhouses", 2)  # média de Steakhouses: 50/15 = 3.33
            + priced(9, "Restaurants, Rare", 4)  # abaixo do suporte mínimo
            + priced(10, "Shopping, Fast Food", 4)  # não é restaurante: não conta
            + priced(10, "Hotels, Resorts", 4) + priced(10, "Hotels", 2))
    return y.category_price_stats(rows)


def test_category_price_stats(stats):
    assert stats["restaurant"]["Fast Food"] == (10, 10)
    assert stats["restaurant"]["Steakhouses"] == (50, 15)
    assert "Restaurants" not in stats["restaurant"]  # genérica
    assert stats["hotel"]["Hotels"] == (60, 20)


@pytest.mark.parametrize("categories, kind, expected", [
    ("Restaurants, Fast Food", "restaurant", 1),
    ("Restaurants, Steakhouses", "restaurant", 3),  # 3.33 -> 3
    ("Restaurants, Fast Food, Steakhouses", "restaurant", 2),  # (1 + 3.33)/2 = 2.17
    ("Restaurants, Rare", "restaurant", None),  # suporte insuficiente
    ("Restaurants", "restaurant", None),  # só genérica
    ("Hotels, Resorts", "hotel", 4),  # (3 + 4)/2 = 3.5 -> 4 (.5 sobe)
])
def test_estimate_level(stats, categories, kind, expected):
    place = business("x", categories, price=None)
    assert y.estimate_level(place, kind, stats) == expected


def test_convert_estimates_missing_levels(rj, stats):
    places = [business("f", "Restaurants, Fast Food", price=None, reviews=50),
              business("u", "Restaurants", price=None, reviews=50),
              business("k", "Restaurants, Fast Food", price="4", reviews=50)]
    df, summary = y.convert(places, SQUARE, rj, level_stats=stats)
    table = y.price_table(rj)["restaurant"]
    row = df.set_index("name")
    assert (row.loc["Place f", "min price"], row.loc["Place f", "max price"]) == table[1]
    assert (row.loc["Place u", "min price"], row.loc["Place u", "max price"]) == table[None]
    assert (row.loc["Place k", "min price"], row.loc["Place k", "max price"]) == table[4]  # faixa real vence
    assert summary["without_price"] == 2 and summary["estimated"] == 1


# --- cidades cadastradas ----------------------------------------------------------


def test_every_yelp_city_has_osm_boundary_query():
    assert set(y.CITY_STATE) <= set(y.bdm.CITY_OSM_QUERY)
    for city, state in y.CITY_STATE.items():
        assert len(state) == 2 and state.isupper(), city


@pytest.mark.parametrize("state", ["TN", "CA"])
def test_read_businesses_new_states(tmp_path, state):
    path = tmp_path / "business.json"
    rows = [dict(business("in", "Hotels"), state=state), business("out", "Hotels")]  # LA
    path.write_text("\n".join(json.dumps(r) for r in rows) + "\n")
    assert [b["business_id"] for b in y.read_businesses(path, state)] == ["in"]
