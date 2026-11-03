"""Testes do relatório de comparação, com saídas fictícias do check_results."""

import pandas as pd
import pytest

import compare_versions as cv


def checked(rows):
    """rows: (secao, budget, distância, tempo, fitness, viável, origem)."""
    return pd.DataFrame(rows, columns=["secao", "budget", "total_distance", "time",
                                       "fitness", "viavel", "origem"])


@pytest.fixture
def section(tmp_path):
    v4 = checked([
        ("GRASP_v4 max_iterations=10", 500, 10000, 0.1, 44.0, 1, "grasp"),
        ("GRASP_v4 max_iterations=10", 500, 10000, 0.3, 42.0, 1, "greedy"),
        ("GRASP_v4 max_iterations=50", 500, 10000, 0.5, 45.0, 1, "grasp"),
    ])
    python = checked([
        ("GRASP max_iterations=10", 500, 10000, 1.0, 43.0, 1, "grasp"),
        ("GRASP max_iterations=10", 500, 10000, 1.0, 45.0, 0, "grasp"),  # inviável
        ("GRASP max_iterations=50", 500, 10000, 2.0, 44.0, 1, "grasp"),
    ])
    greedy = checked([("Greedy", 500, 10000, 0.01, 41.0, 1, "greedy")])
    for name, df in (("v4", v4), ("python", python), ("greedy", greedy),
                     ("unknown", greedy)):
        df.to_csv(tmp_path / f"results_{name}_checked.tsv", sep="\t", index=False)
    return tmp_path


def test_load_section_orders_versions_and_ignores_unknown(section):
    assert list(cv.load_section(section)) == ["python", "v4", "greedy"]


def test_load_section_empty_folder(tmp_path):
    with pytest.raises(SystemExit):
        cv.load_section(tmp_path)


def test_group_stats(section):
    stats = cv.group_stats(cv.load_section(section)["python"]).set_index("iterations")
    assert stats.loc[10, "n"] == 2
    assert stats.loc[10, "feasible"] == 0.5
    assert stats.loc[10, "fitness_all"] == 44.0
    assert stats.loc[10, "fitness_feasible"] == 43.0  # só a viável


def test_do_grasp_counts_seed_wins(section):
    stats = cv.group_stats(cv.load_section(section)["v4"]).set_index("iterations")
    assert stats.loc[10, "do_grasp"] == 0.5
    assert stats.loc[50, "do_grasp"] == 1.0


def test_greedy_repeats_across_iterations(section):
    wide = cv.merge_versions(cv.load_section(section))
    assert list(wide.index.get_level_values("iterations")) == [10, 50]
    assert (wide[("fitness_feasible", "greedy")] == 41.0).all()


def test_report_contains_all_sections(section):
    text = cv.report([("NOLA", cv.load_section(section)), ("Rio", cv.load_section(section))])
    assert "## NOLA" in text and "## Rio" in text
    assert "Python (autores)" in text and "GRASP v4" in text and "Greedy" in text
    assert "do_grasp" in text
    # tabela do fitness: grupo de 10 iterações, Python 43.00, v4 43.00, Greedy 41.00
    assert "| 10 | 500 | 10000 | 43.00 | 43.00 | 41.00 |" in text
    assert "| NOLA | Python (autores) | 3 | 66.7% | 43.50 |" in text
