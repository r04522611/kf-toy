# Data contract for reproducing recordings 10.txt and 11.txt

[Traditional Chinese version](https://github.com/r04522611/kf-toy-mandarin/blob/codex/kf-cpp-toy/docs/data-contract.md)

## One long-format events.csv and manifest.json per recording

Keep states and clocks for recordings 10 and 11 separate. They share a schema but initialize independently. Each CSV row is one event in source order:

`recording_id,sequence,timestamp_s,type,gx_rad_s,gy_rad_s,gz_rad_s,ax_m_s2,ay_m_s2,az_m_s2,left_pulse,right_pulse,latitude_deg,longitude_deg,altitude_m,heading_deg,heading_valid`

Leave inapplicable fields empty. A double timestamp requires 17 significant digits for round trips; preserve timestamp_ns if the source has an exact integer clock, and do not invent precision from coarse text. Sequence resolves equal-timestamp ordering. Do not sort away source order: validate the original clock and report anomalies under a fixed repair policy.

Original SLAM TxtIO records are:

- `IMU time gx gy gz ax ay az`: gyro values are used as rad/s without degree conversion.
- `ODOM time left_pulse right_pulse`: preserve original pulses as well as any derived speed.
- `GNSS time lat lon alt heading heading_valid`: preserve heading convention and validity.

The existing wheel-speed runner uses radius 0.155 m, 1024 pulses and a 0.1 s pulse window. These are runner settings, not universal hardware facts. Heading uses wrap((90-heading_deg)*pi/180). Antenna position requires the same projection, origin, lever arm and mounting yaw.

## Required manifest information

1. Source/CSV SHA256, converter version, total rows and per-event counts.
2. Timestamp units, event order, previous-sample ZOH, maximum substep, pulse aperture and offset.
3. Frame/axis/gravity conventions, GNSS projection implementation/version, UTM zone and origin.
4. Static IMU initialization thresholds/window, initial bg/ba/tilt/g and physical covariance. Retain the initial stationary IMU segment, not just two GNSS fixes.
5. Selection of the first two valid position/heading fixes and the complete second-fix update.
6. Continuous noise intensities, measurement variances, bias random walk, gravity mode, extrinsics and wheel/NHC assumptions.
7. Information mode: pure DR fuses no GNSS after initialization; heading-only fuses valid heading only. Position can be scored without entering estimation.
8. Recipe, KF/GN/LM settings, gates, anchor R/schedule, code revision, image ID, compiler/flags/architecture.
9. GNSS evaluation-time mask and direct antenna residual versus initialization-residual subtraction; keep the definitions separate.

A minimum reproducible package also requires configuration, initialization protocol, scorer and reference outputs/hashes. Road data lacks independent full pose truth: call its metric a GNSS residual proxy, and do not describe fused-heading residual as independent yaw accuracy.

## Initial output design and verification stages

The initial design specified states.csv with timestamp, position, explicit wxyz quaternion, velocity, bg, ba and extra states; innovations.csv with source, fusion/evaluation flags, residual, NIS and dimension; covariance with explicit physical chart/layout; metrics.json with common-mask horizontal/vertical/3D residual RMSE and clock/update checks; and run metadata with input/config/source/image hashes. Actual implemented filenames and contents are listed below. Keep elapsed wall time separate from deterministic hashes.

Verify TXT-to-CSV field/count/order round trips, then compare the original filter on TXT versus CSV, and finally compare with the new library. Cross-algorithm comparisons may use explicit tolerances; identical-binary repeats should be byte-identical. Do not tune parameters to recover historical scores.

## The original planar toy uses another schema

The synchronous analytic fixture uses `t_s,gyro_z_rad_s,speed_m_s,heading_rad,truth_x_m,truth_y_m,truth_yaw_rad`. It checks a minimal end-to-end pipeline rather than the full 3D event schema. Truth is used for scoring; replay validates those fields without fusing them.

## Implemented standalone 3D pipeline

`tools/convert_events.py SOURCE OUTPUT --recording 10` writes the 17-column CSV and adjacent `.manifest.json`. Numeric decimal tokens are preserved without floating-point reformatting or reordering. Unknown types, wrong field counts, NaN/Inf, decreasing timestamps and invalid validity flags are rejected. Sequence is a zero-based consecutive event index. Comments and whitespace are not events and are omitted.

`replay3d --input data/events/10.csv --output results/my-run --recipe step2 --backend kf --mode pure` uses a fixed version-controlled protocol. Recipes are `step1 step2 step3 step4 step5a step5b c15a c15b`, backends `kf gn lm`, and modes `pure heading`. Output must not already exist. Failed directories remain for diagnosis; use a new directory or explicitly move the failed one before retrying.

`configs/research-twofix.json` describes fixed replay parameters. It is a readable specification, not a runtime override. Replay does not load arbitrary JSON configurations, preventing edits to an unused file from appearing to change experiments.

### Initialization and clocks

1. Stationarity requires both ODOM pulse counts `<5`. Collect more than 10 seconds of stationary IMU, with queue capacity 2000. Sum means in source order and use unbiased sample variance with denominator `n-1`.
2. Set gravity from the mean acceleration direction with magnitude 9.81. Gyro bias is gyro mean; accelerometer bias is acceleration-plus-gravity mean. Variance norms must be at most 0.5 and 0.05 respectively.
3. The next IMU after initializer success makes the filter ready. Noise density is the median positive pre-initialization IMU interval multiplied by sample variance; bias random walk uses fixed engineering settings.
4. The first valid-heading GNSS fix after readiness defines the origin. Combine IMU tilt with antenna heading for yaw; position is `-R*lever`. The 15D prior standard deviations are `(2deg,2deg,1deg; .1m/s x3; .1m x3; .001rad/s x3; .1m/s² x3)`, including lever-induced attitude/position correlation.
5. The second valid-heading fix performs a full joint antenna position/heading update with the selected KF/GN/LM backend. Residuals must be below 1 m and 5 deg before evaluation begins.
6. Advance to each event timestamp first, holding the previous IMU until the next IMU event arrives. There is no timestamp-only advance and no silent removal of long gaps. Maximum substep is .01 s.
7. After evaluation begins, GNSS positions are scoring-only. Pure mode fuses no heading; heading mode fuses only source validity=1 headings. Wheel `(speed,0,0)` includes two NHC assumptions, not three measured channels.

### Outputs and comparisons

- `states.txt`: whitespace-separated `timestamp p_xyz q_wxyz v_xyz bg_xyz ba_xyz`, using the research archive's at-least-.1-second snapshot cadence.
- `covariance.csv`: at-least-1-second snapshots, row-major 15×15 physical covariance ordered rotation,velocity,position,gyro-bias,accel-bias. Virtual bias is not labeled gravity.
- `virtual.csv`: physical virtual-bias mean at covariance snapshot times; reduced 15D recipes produce zero.
- `gnss.csv`: archived column layout; two initialization=1 rows are excluded from scoring and every other row is scored. `pre_*` precedes heading update; `rx/ry/rz` follows it. Invalid headings remain empty.
- `innovations.csv`: per-update NIS, iterations, cost, cumulative rejected trials and status. These are not ground truth, and C-star NIS alone does not establish exact-likelihood consistency.
- `config.json`, `initialization.json`, `runtime.json`: actual inferred noise, fix timestamps, row counts and update counts. Wall-clock timing is excluded for byte repeats.
- `metrics.json`: `tools/score_replay.py` computes direct horizontal/vertical/3D antenna residual RMSE on initialization=0 rows and separately reports a second-fix-subtracted horizontal increment metric. The report's primary score is direct RMSE.

`tools/score_replay.py RUN --reference OLD/gnss.csv` checks event timestamps/masks and reports maximum absolute residual difference. It does not align trajectories or tune scores. Altitude is the original GNSS altitude; the archived UTM implementation is used without extra geoid changes.

`tools/run_matrix.py --binary build/replay3d --jobs 2 --repeat` runs 8×3×2×2=96 cells. Each `complete.json` seals binary/input/output hashes; resume requires completion and matching hashes. Changed binaries or outputs are rejected. `--repeat` reruns the same binary and compares SHA256 for every deterministic output, deleting the repeat directory only on success. Matrix failures remain explicit and cause failure status.

Byte determinism applies only to the same image/compiler/binary/architecture/input. Across compilers, Eigen, CPUs or implementations, use named tolerances. Identical-binary repetition does not establish every research conclusion; archived reference evidence is compared separately.

### Archive and uniform numerical protocols

Replay/matrix accept `--protocol archive|uniform`, defaulting to archive. Step1–4 archive runs use historical absolute-timestamp substeps and per-update eigen prior whitening. TG/C15 retain duration substeps and carried roots. Every run records these choices in config.json.

Uniform uses duration substeps and carried roots throughout, suitable for Step3/C15_A equivalence comparisons with numerical choices controlled. Clocks align at event times, but epoch floating-point substeps can differ slightly, and LM diagonal damping depends on whitening basis. Outputs across protocols need not be byte-identical. `--reference-tolerance METRES` makes excessive archived horizontal/3D RMSE differences fail. Without it, differences remain visible while GNSS row counts and heading-fusion counts are still enforced.
