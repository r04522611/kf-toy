# Implementation contracts and design mapping

[Traditional Chinese version](https://github.com/r04522611/kf-toy-mandarin/blob/codex/kf-cpp-toy/docs/implementation.md)

The implementation follows `plan/KF.md`: fixed-gravity Step1–5B/C15, KF/GN/LM, CSV replay and teaching examples. ROS, hard real-time acceptance and rerunning every historical H1–H7/graph experiment are outside this release.

## Formula to source

|Contract|Implementation|
|---|---|
|SO(3)/SE₂(3), semidirect/TG finite maps|`src/kf3d/geometry.cpp`|
|ZOH Gamma, bias sensitivity, three-point Q quadrature|`src/kf3d/propagation.cpp`|
|TG analytic A, RK4 transition, Simpson noise columns|`src/kf3d/propagation.cpp`|
|Virtual Formulation, exact MeasurementModel, solver strategy|`include/kf3d/filter.hpp`|
|QR Gaussian engine, Joseph update, fixed-prior iteration, posterior builder|`src/kf3d/filter.cpp`|
|Compile-time recipe/state/covariance facade|`include/kf3d/typed.hpp`|
|Static initialization, UTM, two fixes, event ordering|`src/replay3d.cpp`|
|CSV round trip, source hashes, scoring/matrix/repeats|`tools/`|
|Independent analytic/finite-difference tests|`tests/math_contracts.cpp`|
|Input and information-leak integration tests|`tests/test_replay_protocol.py`|

## Dimensions and state

The public typed interface separates `PhysicalState` and `TangentGroupState`, with covariance dimensions fixed at 15 and 18. The virtual core uses shared internal State storage for runtime recipes, including fixed-g and bnu slots. Non-TG initialization rejects nonzero bnu instead of ignoring it. Physical covariance order is theta_body,v_world,p_world,bg_body,ba_body[,bnu_body]. Gravity is a parameter without gravity covariance.

This is not an entirely static Eigen implementation. Templates fix public dimensions, while internal dynamic matrices and QR workspaces allocate memory. The numerical core is tested; allocation-free and hard real-time operation require separate profiling and buffer design before deployment.

## Atomic updates

Measurements are evaluated on local candidates. State and root are committed together only after finite checks; failure preserves the prior. Out-of-order/nonfinite IMU data, incorrect covariance/custom measurement shapes and invalid configurations are rejected. Calling `state()` before initialization is rejected.

GN/LM use the same prior and noise at every iteration, with candidate `Retract(prior,L*z)`. Final covariance uses undamped information and maps the root once into the final tangent. PSD priors permit zero directions without artificial jitter. TG preserves the tiny virtual conditional root rather than refactoring the full native covariance.

## Solver behavior

- GN: fixed-prior Gauss–Newton step with backtracking.
- LM: diagonally scaled damping; lambda increases by 10 after rejection and decreases by 3 after acceptance, matching the archived reference.
- Iteration/trial limits and step/gradient/cost tolerances are configurable.
- An optional time budget defaults to 0 (disabled) for deterministic replay.
- Status values distinguish kf, gradient, step, cost, capped, stalled and time_budget.
- A capped or budget-limited solve returns the last valid candidate and its local posterior; that status does not establish convergence.
- Default diagnostics record per-update iterations, initial/final cost and rejected trials. Trial matrices are omitted to avoid hot-path I/O. An observer can provide detailed tracing without changing inference equations.

## Numerical compatibility

The archive profile records legacy absolute-time substeps and eigen-root whitening. The uniform profile uses duration substeps and carried roots. These are explicit numerical policies, not data tuning. Library defaults use uniform behavior; replay defaults use archive behavior to reproduce historical protocols.

Refactoring full-P propagation into QR roots introduces roundoff differences. Cross-implementation archived comparisons use the declared 1 mm horizontal/3D RMSE tolerance; repeat executions of the same binary use byte comparisons. Read actual run summaries and keep these two checks separate.

## Measurement assumptions

`observeVelocity(z)` means 3D body velocity with noise from `Config.wheel_std`. Replay supplies `(speed,0,0)`; lateral/vertical entries are NHC pseudo constraints, not two measured zero channels. A true 1D wheel observation can be implemented through the public exact MeasurementModel with corresponding H and 1×1 noise. C-star is a single-pass output for supported pairings; exact custom models bypass C-star.

`observeVirtualBias` is TG-only and requires explicit target and finite-noise covariance. The core does not schedule it. Replay defaults to no anchor; the separate teaching anchor case records its R and frequency. C15 has no virtual latent state and rejects anchors.

## Reproducibility boundaries

Without a time budget, the same image/binary/input/protocol can reproduce CSV bytes. Compiler evaluation order, Eigen, libm and architecture can alter floating point and noise sample ordering; cross-platform bit identity is not promised. Teaching truth does not share the filter's flow helper, but held-input discretization of varying IMU signals remains approximate, so coverage is diagnostic.

The matrix uses original recordings 10 and 11. Multiple methods on one recording are not independent road evidence, and additional teaching seeds do not provide vehicle ground truth. Global mode switching, robust loss, fixed-lag estimation, unknown clocks and estimated gravity are not implemented.

## Custom formulations and solvers

`makeEstimator(compatibility_recipe, backend, config, custom_formulation, custom_solver)` accepts user-derived objects. Null selects built-ins; KF rejects a nonlinear solver. The compatibility recipe still defines built-in measurement/output state layout and symmetry assumptions. Custom formulations must preserve that contract; matching dimensions alone does not justify applying C-star.

`tests/custom_components.cpp` decorates the built-in formulation, injects a GN solver and supplies a custom 1D position measurement, verifying derived-object calls. New hypotheses can replace compatible dynamics/noise/chart/reset components without copying the Gaussian engine. A new state schema or output symmetry requires an explicit profile and mathematical tests.
