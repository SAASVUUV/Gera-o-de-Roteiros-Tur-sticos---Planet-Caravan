"""Ponta a ponta numa cidade fictícia: Yelp fictício -> CSV -> matriz (haversine)
-> prepare_data -> export_instance -> GRASP e Greedy em C++ -> check_results.

Também garante que o main.py continua lendo o RJ como antes."""

import json
import os
import shutil
import subprocess
import sys

import numpy as np
import pandas as pd
import pytest

import yelp_to_csv as y

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CITY = "TEST"
RING = [[[-90.10, 29.90], [-90.00, 29.90], [-90.00, 30.00], [-90.10, 30.00], [-90.10, 29.90]]]


def fake_businesses(rng):
    kinds = (["Hotels"] * 6 + ["Restaurants"] * 20 + ["Museums", "Parks", "Tours", "Bars"] * 5)
    rows = []
    for i, categories in enumerate(kinds):
        rows.append({
            "business_id": f"id{i:03d}", "name": f"Lugar {i}", "state": "LA",
            "latitude": 29.94 + rng.uniform(0, 0.03), "longitude": -90.07 + rng.uniform(0, 0.03),
            "stars": 5.0 if i < 3 else float(rng.choice([3.0, 3.5, 4.0, 4.5, 5.0])),
            "review_count": int(rng.integers(10, 500)), "is_open": 1,
            "categories": categories,
            # os 3 primeiros hotéis (nota 5) são baratos: o Greedy exige um que caiba
            "attributes": {"RestaurantsPriceRange2": "1" if i < 3 else str(rng.integers(1, 5))},
        })
    return rows


@pytest.fixture(scope="module")
def city_dir(tmp_path_factory):
    """Pasta de trabalho com Input/ e Parameters/ de uma cidade fictícia."""
    work = tmp_path_factory.mktemp("city")
    (work / "Input").mkdir()
    (work / "Parameters").mkdir()
    rng = np.random.default_rng(7)

    rj = pd.read_table(os.path.join(ROOT, "Input", "RJ.csv"), sep=";")
    df, summary = y.convert(fake_businesses(rng), RING, rj)
    assert summary["places"] == 46
    df.to_csv(work / "Input" / f"{CITY}.csv", sep=";", index=False)

    boundary = {"type": "FeatureCollection",
                "features": [{"type": "Feature", "geometry": {"type": "Polygon", "coordinates": RING}}]}
    (work / "Input" / f"{CITY}_boundary.geojson").write_text(json.dumps(boundary))

    params = {
        "user.json": {"budget": [500, 2500], "total_distance": [3000, 6000], "category": [3],
                      "amount_daily_places": [5], "start_time": ["8:00:00"],
                      "end_time": ["23:00:00"], "amount_days": [2],
                      "travel_end": ["03/01/2008 23:00"]},
        "algorithm.json": {"GRASP": {"max_iterations": [5]}, "Greedy": {}},
        "experiments.json": {"amount_executions": 3},
    }
    for name, value in params.items():
        (work / "Parameters" / name).write_text(json.dumps(value))

    run([sys.executable, os.path.join(ROOT, "scripts", "build_distance_matrix.py"),
         "--city", CITY, "--method", "haversine"], work)
    run([sys.executable, os.path.join(ROOT, "scripts", "export_instance.py"), "--city", CITY], work)
    return work


def run(command, cwd):
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
    assert result.returncode == 0, f"{command}\n{result.stdout}\n{result.stderr}"
    return result


def test_matrix_is_square_symmetric_zero_diagonal(city_dir):
    matrix = pd.read_table(city_dir / "Input" / f"{CITY}_distance_matrix.txt", index_col=0)
    assert matrix.shape == (46, 46)
    assert list(matrix.index) == list(matrix.columns)
    values = matrix.to_numpy()
    assert np.allclose(values, values.T, atol=0.1)
    assert (np.diag(values) == 0).all()
    assert (values[~np.eye(46, dtype=bool)] > 0).all()


def test_prepare_data_puts_hotels_first(city_dir, monkeypatch):
    import main

    monkeypatch.chdir(city_dir)
    df, distance_matrix = main.prepare_data(CITY)
    hotels = df[df["General Category"] == "hotel"]["Name"]
    others = df[df["General Category"] != "hotel"]["Name"]
    assert len(hotels) == 6 and hotels.max() < others.min()
    assert sorted(df["Name"]) == list(range(46))
    assert distance_matrix.shape == (46, 46)
    assert "Review Count" in df.columns


def test_exported_instance(city_dir):
    instance = city_dir / "Input" / f"{CITY}_instance"
    places = pd.read_table(instance / "places.tsv")
    assert len(places) == 46
    assert set(places["category"]) == {"hotel", "restaurant", "attraction"}
    matrix = np.fromfile(instance / "distance_matrix.bin", dtype="<f8")
    assert matrix.size == 46 * 46


@pytest.fixture(scope="module")
def cpp_build():
    if shutil.which("make") is None or shutil.which("g++") is None:
        pytest.skip("sem make/g++")
    run(["make", "-s", "-C", os.path.join(ROOT, "cpp")], ROOT)
    return os.path.join(ROOT, "cpp", "build")


@pytest.mark.slow
@pytest.mark.parametrize("program", ["grasp", "greedy"])
def test_cpp_runs_feasible_on_new_city(city_dir, cpp_build, program, tmp_path):
    instance = str(city_dir / "Input" / f"{CITY}_instance")
    output = str(tmp_path / f"results_{program}.txt")
    command = [os.path.join(cpp_build, program), "--instance", instance, "--output", output]
    if program == "grasp":
        command += ["--seed", "1"]
    run(command, city_dir)

    run([os.path.join(cpp_build, "check_results"), output, "--instance", instance], city_dir)
    checked = pd.read_table(output.replace(".txt", "_checked.tsv"))
    expected = 3 * 2 * 2  # execuções x budgets x distâncias
    assert len(checked) == expected
    assert checked["viavel"].all(), checked[checked["viavel"] == 0]
    assert (checked["fitness"] <= 45).all()


def test_main_accepts_city_argument():
    result = subprocess.run([sys.executable, os.path.join(ROOT, "src", "main.py"), "--help"],
                            capture_output=True, text=True, cwd=ROOT)
    assert result.returncode == 0 and "--city" in result.stdout


# --- o Rio não pode mudar ------------------------------------------------------


@pytest.mark.slow
def test_rio_instance_export_unchanged(tmp_path):
    reference = os.path.join(ROOT, "Input", "RJ_instance")
    if not os.path.isdir(reference):
        pytest.skip("Input/RJ_instance não exportada")
    out = tmp_path / "RJ_instance"
    run([sys.executable, os.path.join(ROOT, "scripts", "export_instance.py"),
         "--city", "RJ", "--out", str(out)], ROOT)
    for name in sorted(os.listdir(reference)):
        with open(os.path.join(reference, name), "rb") as a, open(out / name, "rb") as b:
            assert a.read() == b.read(), name


@pytest.mark.slow
def test_rio_grasp_v4_reproduces_first_rows(cpp_build, tmp_path):
    reference = os.path.join(ROOT, "Results Analysis", "My Results", "results_grasp_v4.txt")
    (tmp_path / "Parameters").mkdir()
    for name in ("user.json", "algorithm.json"):
        shutil.copy(os.path.join(ROOT, "Parameters", name), tmp_path / "Parameters" / name)
    (tmp_path / "Parameters" / "experiments.json").write_text('{"amount_executions": 3}')
    output = tmp_path / "out.txt"
    run([os.path.join(cpp_build, "grasp"), "--instance", os.path.join(ROOT, "Input", "RJ_instance"),
         "--output", str(output), "--seed", "1"], tmp_path)

    def rows(path, count):  # sem a coluna de tempo (4ª)
        with open(path) as file:
            lines = [line.rstrip("\n").split("\t") for line in file if not line.startswith("#")]
        return [line[:3] + line[4:] for line in lines[:count]]

    assert rows(output, 3) == rows(reference, 3)
