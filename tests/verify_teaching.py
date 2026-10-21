#!/usr/bin/env python3
"""Smoke-run all recipes/backends and check byte determinism and invariants."""
import csv
import hashlib
import math
from pathlib import Path
import subprocess
import sys
import tempfile


def read(path):
    with path.open() as stream:
        return list(csv.DictReader(stream))


def main():
    binary = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="kf-teaching-test-") as scratch:
        a, b = Path(scratch)/"a", Path(scratch)/"b"
        for directory in (a, b):
            subprocess.run([binary, str(directory), "1", "0.4"], check=True)
        names = sorted(path.name for path in a.iterdir())
        assert names == sorted(path.name for path in b.iterdir())
        for name in names:
            assert (a/name).read_bytes() == (b/name).read_bytes(), name
        rows = read(a/"metrics.csv")
        assert len(rows) == 4*8*3
        metrics = ["position_rmse", "attitude_rmse", "normalized_nees15",
                   "z_95_coverage", "mean_velocity_nis"]
        indexed = {(row["scenario"], row["recipe"], row["backend"]): row for row in rows}
        for row in rows:
            for field in metrics:
                assert math.isfinite(float(row[field])), (field, row)
            assert 0 <= float(row["z_95_coverage"]) <= 1
            assert float(row["position_rmse"]) >= 0
        for scenario in {row["scenario"] for row in rows}:
            for backend in ("KF", "GN", "LM"):
                for field in metrics:
                    assert abs(float(indexed[scenario,"Step3",backend][field])-
                               float(indexed[scenario,"C15A",backend][field])) < 1e-8
            for backend in ("GN", "LM"):
                for left, right in (("Step5A","Step5B"),("C15A","C15B")):
                    for field in metrics:
                        assert abs(float(indexed[scenario,left,backend][field])-
                                   float(indexed[scenario,right,backend][field])) < 1e-8
        for row in read(a/"constraints.csv"):
            assert abs(float(row["tg_finite_bnu_z"])-float(row["subspace_prediction"])) < 1e-12
            assert abs(float(row["reduced_bnu_z"])) < 1e-15
        for row in read(a/"anchor.csv"):
            if row["method"] == "C15_structural":
                assert abs(float(row["final_bnu_norm"])) < 1e-15
        integration=read(a/"integration.csv")
        assert len(integration)==48
        for row in integration:
            for field in ("velocity_error","position_error","rotation_error"):
                assert math.isfinite(float(row[field]))
            if row["input_case"]=="held_force" and row["recipe"]=="Step2":
                assert float(row["velocity_error"])<1e-10
                assert float(row["position_error"])<1e-10
        for row in read(a/"so3.csv"):
            assert float(row["exp_orthogonality"])<1e-12
            assert float(row["exp_log_rotation_error"])<1e-6
        assumption_rows=read(a/"assumptions.csv")
        assert len(assumption_rows)==72
        indexed_assumptions={(row["case"],row["recipe"],row["backend"]):row for row in assumption_rows}
        for row in assumption_rows:
            assert math.isfinite(float(row["absolute_truth_error"]))
            if row["backend"]!="KF":
                assert float(row["final_cost"])<=float(row["initial_cost"])+1e-8
        for recipe in {row["recipe"] for row in assumption_rows}:
            for backend in ("KF","GN","LM"):
                good=float(indexed_assumptions["correct_likelihood",recipe,backend]["absolute_truth_error"])
                wrong=float(indexed_assumptions["wrong_heading",recipe,backend]["absolute_truth_error"])
                confident=float(indexed_assumptions["overconfident_wrong_prior",recipe,backend]["absolute_truth_error"])
                assert good<.01 and wrong>.7 and confident>1.
        slip_rows=read(a/"slip.csv")
        assert len(slip_rows)==12
        for row in slip_rows:
            for field in ("position_rmse","attitude_rmse","mean_velocity_nis","final_accel_bias_norm","lateral_95_coverage"):
                assert math.isfinite(float(row[field]))
            assert 0<=float(row["lateral_95_coverage"])<=1
        digest = hashlib.sha256((a/"metrics.csv").read_bytes()).hexdigest()
        print("All 96 scenario/recipe/backend combinations finite; all outputs byte deterministic.")
        print("metrics.csv sha256="+digest)


if __name__ == "__main__":
    main()
