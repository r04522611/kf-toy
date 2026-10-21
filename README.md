# Kalman Filter teaching library: C++17 / Ubuntu Docker

English edition and primary implementation repository. [Traditional Chinese edition](https://github.com/r04522611/kf-toy-mandarin). Both editions share the same algorithms, tests and numerical evidence.

Standalone project: `/home/pinghua/kf-toy`, branch `codex/kf-cpp-toy`. The original SLAM repository supplies read-only equations, data and archived references. This library requires no ROS, Sophus, PCL, OpenCV, or headers/binaries from that repository.

## Documentation

- [Usage guide](README.md)
- [Twelve teaching lessons](docs/teaching.md)
- [Full validation report](docs/validation.md)
- [Data contract](docs/data-contract.md)
- [Implementation contracts](docs/implementation.md)
- [Numerical reproduction](docs/numerical-reproduction.md)
- [Edition maintenance](docs/editions.md)
- [Traditional Chinese documentation](https://github.com/r04522611/kf-toy-mandarin)

## Implemented capabilities

- `kf3d` static library: Step1, Step2, Step3, Step4, Step5A, Step5B, C15_A and C15_B, with fixed gravity.
- Three backends: single-pass KF, fixed-prior GN and LM; 24 named configurations.
- Square-root propagation, Joseph update, finite manifold reset and PSD prior support.
- Public virtual `Formulation`, `MeasurementModel`, `NonlinearSolver` and `Estimator`; a typed state/covariance facade, `Filter<Recipe>`.
- Body velocity with explicit NHC conventions, GNSS antenna position, mounted heading and an explicit virtual-bias constraint for TG only.
- CSV replay with two-fix initialization, followed by pure DR or heading-only operation: 96 cells across recordings 10 and 11.
- Source/input/binary hashes, byte-identical repeats using the same binary, a common scorer and 80 archived reference metrics.
- Analytic/finite-difference tests and teaching scenarios with known truth; see the [teaching guide](docs/teaching.md).

C15_A shares its formulation with fixed-gravity Step3. C15_B uses the same 15D formulation with navigation midpoint output. A/B use the same exact likelihood for GN/LM and should agree. C-star is not used as a cost derivative.

This is a tested standalone research and teaching library. The core uses small dynamic Eigen matrices; the typed facade does not guarantee allocation-free or hard real-time execution. ROS adapters, deployment performance acceptance, robust losses and fixed-lag research are outside this release. Historical estimated-gravity variants are separate from the fixed-gravity results.

## Minimal environment and tests

Run in Ubuntu:

```bash
cd /home/pinghua/kf-toy
docker build -t kf-toy:dev .
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work kf-toy:dev bash -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2 && cmake -E chdir build ctest --output-on-failure'
```

Dependencies are GCC, CMake, Make, Eigen and the Python standard library. The Ubuntu base digest is pinned, but apt packages are still obtained at image build time. Archive the image ID, package versions and source hash; a future apt build is not guaranteed to produce the same binary.

`typed_client` is a minimal library client. `include/kf3d/typed.hpp` distinguishes 15D `PhysicalState` from 18D `TangentGroupState`.

```cpp
#include <kf3d/typed.hpp>
using namespace kf3d;
Filter<Recipe::Step2> filter(Backend::GN);
PhysicalState initial;
Filter<Recipe::Step2>::Covariance P = Filter<Recipe::Step2>::Covariance::Identity() * .01;
filter.initialize(initial, P);
filter.advance(.1, V3::Zero(), V3(0,0,9.81));
filter.observePosition(V3(1,0,0));
auto pose = filter.state();
```

Install headers and the library with `cmake --install build --prefix /chosen/prefix`. Downstream clients use `find_package(kf3d CONFIG REQUIRED)` and link `kf3d::kf3d`.

## Convert recordings 10/11 and replay one configuration

```bash
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" -v /home/pinghua/slam_in_autonomous_driving/data/ch3:/source:ro -w /work kf-toy:dev python3 tools/convert_events.py /source/10.txt data/events/10.csv --recording 10
```

The converter refuses to overwrite an existing CSV. It preserves numeric tokens and original event order, without sensor interpolation. See the [data contract](docs/data-contract.md).

```bash
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work kf-toy:dev ./build/replay3d --input data/events/10.csv --output results/one --recipe step5b --backend kf --mode pure --protocol archive
```

The output directory must not exist. Choices: `--recipe` step1/step2/step3/step4/step5a/step5b/c15a/c15b; `--backend` kf/gn/lm; `--mode` pure/heading.

Replay uses the fixed archived protocol described in `configs/research-twofix.json`. This JSON is a machine-readable protocol description; the runner currently does not load arbitrary JSON overrides. General library clients configure the typed `Config`. Unspecified tuning is excluded from reference reproduction.

## Run the complete reproduction

```bash
./tools/reproduce.sh /home/pinghua/slam_in_autonomous_driving/data/ch3 research
```

This builds, runs CTest, converts missing CSVs, executes 96 cells with a second byte comparison for each, and generates four-seed teaching scenarios and SVG figures. The full matrix takes longer than one toy case. `run_matrix.py` supports subsets through `--recipes`, `--backends`, `--datasets`, `--modes` and `--jobs`. Use separate output directories for subsets.

```bash
python3 tools/run_matrix.py --binary build/replay3d --data data/events --output results/research --jobs 4 --repeat --protocol archive
```

A completed, sealed run can resume only with matching binary, input, script and protocol signatures. Changed signatures or outputs require a new output directory. Failed/interrupted directories are not considered complete. `--reference-tolerance METRES` enforces horizontal and 3D archived RMSE tolerances; the default reports differences explicitly.

### Archive and uniform protocols

- `archive`: Step1–4 use legacy absolute-timestamp substeps; GN/LM recompute the native covariance eigen-root. TG uses carried roots and duration substeps; C15 retains its original duration policy. This preserves historical numerical choices.
- `uniform`: all recipes use carried-root whitening and duration substeps, suitable for comparing formulations. Byte identity with older finite-iteration LM results is not promised.

Sensors, noise and initialization are identical between protocols. Whitening basis affects LM diagonal damping and stopping criteria, so a numerical-policy difference should not be attributed to geometry. Each run records its actual profile. C15 GN/LM capabilities are new and have no invented historical baseline.

## Outputs and interpretation

- `states.txt`: timestamp, position, quaternion (wxyz), velocity, gyro bias and accelerometer bias, approximately 10 Hz.
- `covariance.csv`: physical 15D covariance at 1 Hz, ordered theta_body,v_world,p_world,bg_body,ba_body; this is not native P.
- `virtual.csv`: TG physical bnu; C15 zeros are structural values.
- `gnss.csv`: antenna residuals at common evaluation times, fusion flags and initialization markers.
- `innovations.csv`: NIS, solver iterations, cost and status, including rejection/budget outcomes.
- `config.json`, `initialization.json`, `runtime.json`: noise, extrinsics, solver settings, two fixes and clock checks.
- Matrix `comparison.csv` / `matrix.json`: cell scores, reference differences and determinism results.
- Teaching `metrics.csv`, `trace.csv`, `recovery.csv`, `remainder.csv`, `constraints.csv`, `anchor.csv` and SVGs.

Road-data metrics are GNSS antenna residual proxies, without independent full SE3/bias truth. A heading residual after heading fusion is not independent yaw accuracy. Byte-identical repeats and agreement with archived algorithm metrics are separate checks. Cross-CPU/compiler/libm bitwise identity is not guaranteed.

## Validation results and known discrepancies

All 96 final road-data cells completed and repeated byte-identically. All eight CTest suites passed. Of 80 historical reference cells, 78 satisfy the originally declared 1 mm RMSE tolerance. Recording 11 pure DR Step3 GN and Step4 LM differ by approximately 1.226 mm and 1.077 mm respectively, so those two strict checks failed. Parameters and thresholds were not changed to hide these differences.

See the [English validation report](docs/validation.md) and [numerical investigation](docs/numerical-reproduction.md). The report separates execution/determinism from archive tolerance. `tools/report_validation.py --require-reference` enforces the latter and currently should exit nonzero. The local full report is `results/research/validation.md`; the compact tracked summary is `references/validation-summary.json`. Report generation uses the local edition language, with identical numeric rows in both repositories. The Chinese edition supplies a text-only locale file; the report verification and scoring code are shared.

## Run the teaching examples

```bash
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work kf-toy:dev bash -c './build/kf_teaching results/teaching 4 12 && python3 tools/render_results.py results/teaching'
```

Closed-form truth is independent of filter propagation. Midpoint-held IMU sampling is approximately matched; coverage is not an exact Bayesian proof. Seeds are independent comparison units, while time samples are correlated. The [teaching guide](docs/teaching.md) covers scalar KF, SO(3)/ZOH, geometry, bias mismatch, heading/dropout, GN/LM recovery, C-star remainder and TG structural/soft constraints.

The original planar `kf`, `replay` and `tools/verify.py` remain as an introductory example; they do not output full pose covariance.

Design plans and research reports reside in ignored `plan/`. External sources, UTM notices and file hashes are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and `references/`.
