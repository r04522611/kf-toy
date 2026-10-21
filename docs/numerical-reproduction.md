# Deterministic replay and archived numerical reproduction are separate checks

[Traditional Chinese version](https://github.com/r04522611/kf-toy-mandarin/blob/codex/kf-cpp-toy/docs/numerical-reproduction.md)

All eight CTest suites passed. Every full-matrix cell was repeated with the same binary/image/input/protocol and all result bytes compared. Direct GNSS antenna residual RMSE was also compared against 80 archived cells. The 16 new C15 GN/LM cells have no historical baseline.

The originally declared strict archive tolerance was 1 mm for horizontal/3D RMSE differences. Two recording-11 pure-DR cells exceed it:

|Cell|New horizontal RMSE (m)|Archived horizontal RMSE (m)|Difference (m)|
|---|---:|---:|---:|
|Step3 GN|188.1338323648597|188.1350582074412|-0.0012258425815|
|Step4 LM|111.12085871585587|111.11978187916851|+0.0010768366874|

**These two 1 mm checks failed.** Filter parameters and tolerances were not changed to make them pass. Byte-identical repeats of the same binary are a separate verified property.

## Read-only diagnosis

Two independent reviews compared all 18,469 solver events and 17,624 state snapshots:

- Noise, median dt, first/second fix times and timestamps/fusion/validity/initialization masks for all 18,415 GNSS rows match.
- Maximum physical-state difference at the first initialization is approximately 5.55e-17; bg/ba match.
- Step3 GN has no differences in iteration count, status or rejected trials. Position differences grow from approximately 2.34e-10 m at the second fix to at most approximately 2.91 mm. Solver stopping conditions therefore cannot explain the entire difference.
- Step4 LM first differs in trial rejection at update 729 (approximately 73 s), with both taking three iterations and final costs differing by approximately 7e-13. Status first differs at update 1556 (approximately 155 s), step versus stalled. Maximum state-position difference is approximately 2.97 mm.
- The second-fix north residual differs by 2.31684e-10 m, approximately half a UTM northing ULP. The original wrapper uses lat*(pi/180), while the new runner uses lat*pi/180. The real-valued formula is identical but floating-point evaluation order differs, providing a possible perturbation source.

Other mathematically equivalent but numerically distinct paths include Sophus quaternion exponential versus Rodrigues matrix exponential, 18D eigen whitening with zero gravity directions versus 15D whitening, and dense covariance versus QR roots. Evidence supports small perturbations amplified through long feedback, but each contribution has not been isolated. No unique root cause is claimed.

## Reading tool outcomes

`tools/run_matrix.py --reference-tolerance 0.001` reports failure for these cells even when execution and byte repeat succeed. The original strict results are retained locally in `results/research/matrix.strict-1mm.json`.

Without that option, the matrix still lists every reference difference. `tools/report_validation.py` defaults to the 1 mm strict pass/failure list, while its default exit status checks execution/byte determinism. Adding `--require-reference` makes the two differences cause a nonzero exit. This separates validation concepts rather than relaxing the numerical threshold.

The one-command reproduction generates the full report; readers may request the strict numerical gate. Original research tables in ignored `plan/` are preserved, and new experiment reports retain their own values and discrepancies.
