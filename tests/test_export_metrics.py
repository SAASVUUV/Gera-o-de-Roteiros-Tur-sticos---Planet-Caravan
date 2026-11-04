"""Testes da exportação de métricas (o que pode ir ao repositório público)."""

import pandas as pd

import export_metrics as em


def test_export_keeps_only_metric_columns(tmp_path):
    folder = tmp_path / "rio"
    folder.mkdir()
    checked = pd.DataFrame({
        "secao": ["GRASP_v4 max_iterations=10"], "algorithm": ["GRASP_v4"],
        "budget": [500], "total_distance": [10000], "time": [0.1], "fitness": [44.0],
        "fitness_recalculado": [44.0], "viavel": [1], "custo": [480],
        "roteiro": ["Hotel X | Dia 1: Museu Y"], "route": ["[1,2,3]"], "origem": ["grasp"],
    })
    checked.to_csv(folder / "results_v4_checked.tsv", sep="\t", index=False)

    written = em.export(str(tmp_path))

    assert written == [str(folder / "results_v4_metrics.tsv")]
    out = pd.read_table(written[0])
    assert list(out.columns) == em.METRIC_COLUMNS
    assert out.loc[0, "fitness"] == 44.0 and out.loc[0, "viavel"] == 1
    text = (folder / "results_v4_metrics.tsv").read_text()
    assert "Hotel X" not in text and "[1,2,3]" not in text


def test_export_without_origin_column(tmp_path):
    checked = pd.DataFrame({"secao": [""], "algorithm": ["GRASP"], "budget": [500],
                            "total_distance": [1], "time": [0.1], "fitness": [40.0],
                            "viavel": [0], "route": ["[1]"]})
    checked.to_csv(tmp_path / "results_python_checked.tsv", sep="\t", index=False)
    out = pd.read_table(em.export(str(tmp_path))[0])
    assert list(out.columns) == [c for c in em.METRIC_COLUMNS if c != "origem"]
