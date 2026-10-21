# KF teaching examples: from analytic answers to research limits

[繁體中文](teaching.md) | [English](teaching.en.md)

Use this guide with `kf_teaching` and `kf_math_tests`. With common data, priors, noise and scoring, readers can distinguish changes to the formulation, output approximation and nonlinear solver. These examples do not rank algorithms or treat road-data GNSS proxies as full pose truth.

## Execution and artifacts

After building in the Docker environment described in the README:

```bash
./build/kf_math_tests
./build/kf_teaching results/teaching 4 12
python3 tools/render_results.py results/teaching
# optional: python3 tools/plot_examples.py results/teaching
```

The three arguments after the executable are output directory, seed count and duration in seconds. Defaults are `results/teaching 4 12`. `render_results.py` produces SVGs using the Python standard library; only optional PNG plotting with `plot_examples.py` requires matplotlib. Plotting does not affect estimation, scoring or deterministic comparisons. The minimal C++ runtime requires neither Python nor ROS. Plotting can run on the Ubuntu host.

|File|Contents|
|---|---|
|`metrics.csv`|Position/attitude RMSE, normalized NEES15, z coverage and velocity NIS for every scenario × seed × recipe × backend|
|`trace.csv`|Seed-0 time series: truth, estimate, bias error, NEES and heading availability|
|`remainder.csv`|Ordinary/midpoint noiseless remainders across seven directions and multiple scales|
|`constraints.csv`|Difference between linear tangent support and nonlinear constrained state|
|`recovery.csv`|One measurement update from a common large attitude error, using KF/GN/LM|
|`anchor.csv`|Separate comparison of TG B without anchor, with finite-noise anchor, and C15_B structural constraint|
|`integration.csv`|Step1/Step2 step-size sweep with held/varying force and left/midpoint samples|
|`so3.csv`|Small/large-angle SO(3) exponential, additive approximation and finite reset|
|`assumptions.csv`|Correct heading, incorrect heading likelihood and overconfident incorrect prior|
|`slip.csv`|Direct 3D velocity versus forward + NHC, with and without sideslip|
|`scalar.csv`|A hand-checkable one-dimensional random-walk KF|
|`manifest.txt`|Definitions of seeds, sampling, truth, dropout and model mismatch|

The same binary, platform and arguments should produce identical CSV bytes. Changes to compiler, Eigen, libm or architecture require fresh mathematical tests and numerical regression; cross-version byte identity is not promised. Execution timing is excluded from deterministic CSVs.

## Lesson 1: how a scalar KF allocates trust

`scalar.csv` uses a one-dimensional random walk with known input 0.1 at every step, process variance 0.01 and measurement variance 0.04:

\[
P^- = P^+ + Q,\quad K=P^-/(P^-+R),\quad
\hat x^+=\hat x^-+K(z-\hat x^-).
\]

The Joseph form is:

\[
P^+=(1-K)^2P^-+K^2R.
\]

Calculate the first update by hand, then inspect the steady gain. This scalar example implements the formulas directly as preparation for the 3D library. Separately, `math_contracts` verifies the analytic posterior through the library's linear position observation, so verification extends beyond the teaching generator itself.

## Lesson 2: the assumptions behind exact discretization

The mathematical test holds angular velocity at \(\omega_z=2\), body specific force at \(f_x=3\), and interval length at \(h=0.5\). The exact answer is:

\[
v_x=3\sin(2h)/2,\quad v_y=3(1-\cos(2h))/2,
\]

\[
p_x=3(1-\cos(2h))/4,\quad
p_y=3(h/2-\sin(2h)/4).
\]

Step2's ZOH flow should match; Step1 has visible error. Exactness here concerns the mean flow with constant body input within the interval. It does not establish constant real signals between samples or exact stochastic covariance in every situation.

The full teaching trajectory has time-varying speed. IMU input is sampled at the interval midpoint and held. This deliberately retains small discretization error and avoids circular verification through a truth generator calling the estimator's Gamma helper.

## Lesson 3: charts, physical covariance and finite corrections

At nonzero attitude, velocity, position and bias, `math_contracts` checks:

1. `local(prior,retract(prior,delta)) ≈ delta`.
2. The Jacobian from native correction to physical error, against central finite differences.
3. The post-correction tangent reset Jacobian, against central finite differences.
4. SO(3) exp/log round trips near zero and pi.

Step1 and Step2 both use the exact SO(3) right-Jacobian reset, matching this fixed-gravity reference. Earlier baseline/mean-only first-order resets are a different approximation.

The common physical error order is `[theta_body, v_world, p_world, bg_body, ba_body]`. Map uncertainty into this physical chart before comparing formulations. Identical native covariance indices need not represent identical physical directions.

## Lesson 4: Step3 and C15_A teach equivalence

With fixed gravity and identical priors, events, noise and output, Step3 and C15_A share right-invariant navigation with physical additive bias. Tests compare multistep state and physical covariance for KF, GN and LM.

Both names preserve research history; they are not independent performance evidence. Overlapping curves are expected. Production implementation should share the underlying equations rather than duplicate C15_A and let two copies diverge.

## Lesson 5: the output order of C15_B

Let navigation perturbation be \(\eta=(\theta,\rho_v,\rho_p)\), generate \(z\) from a noiseless perturbed state, and define \(r=\hat Rz-\hat v\). Compare:

\[
e_A=r-\rho_v,\qquad
e_B=r-\rho_v-\tfrac12[r]_\times\theta.
\]

Scaling all perturbation components together gives a second-order ordinary remainder and third-order midpoint remainder. Halving scale should yield norm ratios near 4 and 8. Mathematical tests check seven directions and five scales; the CSV includes finer scales for plotting.

This is a local noiseless property. With noise, the update matrix depends on the measurement. A cubic remainder alone does not establish posterior calibration or lower long-term RMSE. Consult the research reports in `plan/` for tradeoffs across recordings; this toy does not replace those results.

## Lesson 6: how GN/LM reuse one piece of information

`recovery.csv` starts with prior yaw 1.2 rad, trusted world velocity near `(2,0,0)` and body velocity measurement `(2,0,0)`. A wide attitude prior and narrow velocity prior allow the body-velocity residual to inform attitude recovery.

Every backend uses the same prior and likelihood. GN/LM hold that prior fixed throughout iteration. Replacing it with the previous iteration's posterior would count the same measurement repeatedly and artificially shrink covariance.

Read the results in this order:

1. Inspect the single KF correction and remaining nonlinear residual.
2. Inspect GN/LM `initial_cost`, `final_cost`, iterations and status.
3. Compare the linear-position test: KF/GN/LM must recover the analytic posterior, irrespective of iteration count.
4. Inspect the GN/LM equivalence tests for C15_A/C15_B and Step5A/Step5B.

Ordinary/C-star changes only the single-pass output approximation. GN/LM use the same exact likelihood and derivative, so iterative A/B results should coincide. LM damping stabilizes the solver; it is not added directly to final posterior information.

Solving the chosen problem more thoroughly does not establish correct assumptions. Continue with the bias-mismatch case.

## Lesson 7: matched inputs, heading dropout, pure DR and model mismatch

All scenarios use one closed-form circle with varying speed:

\[
R=R_z(0.3t),\quad v=R[2+0.3\sin(0.7t),0,0]^\top.
\]

Position comes from analytic integration and IMU means from trajectory derivatives, without estimator propagation calls. Noise uses fixed-seed mt19937 with an explicit Box–Muller transform. All recipes receive the same samples and physical prior.

|Scenario|Assumption|What to inspect|
|---|---|---|
|`matched_heading`|Fixed physical biases; matched IMU/velocity/heading noise|Baseline errors, NEES and coverage; a small sample need not yield exactly 95%|
|`heading_dropout`|Heading absent from 35% to 75% of the duration|Attitude/bias uncertainty and recovery after dropout|
|`pure_dr`|Only IMU and full body velocity throughout|Global yaw gauge freedom; changing charts cannot supply missing external information|
|`bias_mismatch`|Additional +0.12 rad/s gyro-z step halfway through; zero filter bias random walk|Lower optimizer cost may coexist with overconfidence under an incorrect process model|

Velocity here is a genuine three-dimensional body-velocity observation with explicit noise on all three axes. It is not forward wheel speed with two measured zero channels. Vehicle NHC lateral/vertical zeros are pseudo constraints requiring their own noise definitions and failure conditions.

Prior errors are specified in the physical chart with a common physical covariance. Initial attitude is truth right-multiplied by a physical rotation perturbation; velocity, position and physical biases receive additive perturbations.

The four-seed default is illustrative, not evidence of statistical significance or calibrated coverage. Increase seed count for stable statistics, retain paired comparisons and report between-seed variation. Time samples are correlated; individual timestamps are not independent Monte Carlo trials for confidence intervals.

### Score definitions

\[
e=[\operatorname{Log}(\hat R^TR),\ v-\hat v,\ p-\hat p,\ b_g-\hat b_g,\ b_a-\hat b_a],
\]

\[
\operatorname{NEES}_{15}=e^TP_{physical,15}^{-1}e.
\]

`normalized_nees15` averages NEES at velocity events and divides by 15. `z_95_coverage` is the fraction satisfying \(|e_z|\le1.95996398454\sqrt{P_{zz}}\). `mean_velocity_nis` is read after the velocity update and before the heading update; heading residuals are excluded. Report NIS separately for different measurement types.

Pose RMSE is direct world-frame error without post-hoc SE(3) alignment. This toy has truth. Recording 10/11 antenna residuals are only proxies and are not interchangeable with toy pose-truth RMSE.

## Lesson 8: structural constraints and anchors

`constraints.csv` uses a derived subspace: identity prior navigation, with only position-x and gyro-bias-y perturbations. The inverse physical Jacobian first constructs a TG native correction with **zero linear virtual perturbation**, followed by finite joint retraction:

\[
b_{\nu,new}=-\tfrac12\delta p\times\delta b_g.
\]

The full TG finite correction can produce nonzero virtual bias. C15's reduced retraction naturally retains \(b_\nu=0\). The plotted prediction applies only to this subspace, not globally to arbitrary states.

Distinguish four operations:

- Reduced representation: every candidate lies on the constraint surface.
- Finite-noise anchor likelihood: retain the latent state and add evidence.
- Zero first-order tangent variance: restrict linear Gaussian support.
- Hard mean clamp: modify the mean afterward; without compatible uncertainty treatment this is not a complete Bayesian update.

Anchors are **off** in the main `metrics.csv` comparison, keeping output approximation separate from constraint information. A separate `anchor.csv` compares TG_B without anchor, TG_B with finite-noise anchor and C15_B structural constraint using common matched-heading/pure-DR samples. TG virtual prior variance is 0.01, anchor variance 0.001 and update interval 0.2 s, with an explicit common physical prior. These are teaching settings, not replacements for archived parameters. C15 has no virtual state and receives no anchor. The toy does not isolate curvature as the unique cause of anchor failure: C15 also changes dynamics, bias retraction and the Gaussian family.

## Lesson 9: separate integration formulas from sampling assumptions

`integration.csv` fixes duration at 1 s and angular velocity at 2 rad/s, sweeping step sizes 1, 0.5, 0.2, 0.1, 0.05 and 0.025 s. Each step size is evaluated with left-endpoint and midpoint samples.

- `held_force`: constant body-x force 3. Step2's mean should match the analytic result regardless of step size; Step1 improves with smaller steps.
- `varying_force`: body-x force 3 cos(1.7t). The real signal varies between samples, so exact integration of held samples still has input approximation error.

The second case's true velocity/position comes independently from analytic integrals of cos(1.7t)cos(2t) and cos(1.7t)sin(2t). It does not call the same Gamma helper at finer dt. Comparing left and midpoint samples distinguishes improved interval-input description from improved held-input flow.

Do not hard-code a universal winner across step sizes. Only matched held-force Step2 receives an analytic exactness oracle.

## Lesson 10: failure of additive SO(3) approximations

`so3.csv` fixes a general 3D rotation axis and sweeps angles from near zero to near pi, reporting:

- Orthogonality error of exp(phi).
- Orthogonality error of I + hat(phi).
- Rotation-matrix round-trip error of exp(log(exp(phi))).
- Difference between the exact right reset Jacobian and I − 0.5hat(phi).

This teaches local approximation; elementwise rotation-matrix addition is not a valid estimator recipe. Near pi, the logarithm's vector representation has a branch. Tests compare reconstructed rotation matrices rather than requiring componentwise equality of rotation vectors across branches.

## Lesson 11: a converged optimizer can trust an incorrect world

`assumptions.csv` fixes true yaw at zero and prior yaw at 1.2 rad:

|Case|Prior yaw variance|Heading observation|Interpretation|
|---|---|---|---|
|`correct_likelihood`|2 rad²|0 rad|A trustworthy observation corrects a large initial error|
|`wrong_heading`|2 rad²|0.8 rad|Optimization correctly solves a likelihood displaced from reality|
|`overconfident_wrong_prior`|1e-5 rad²|0 rad|An incorrect narrow prior can dominate a correct observation|

Measurement standard deviation is 0.05 rad in all three cases. Each runs all eight recipes and three backends. Read `absolute_truth_error` alongside `final_cost`: cost describes the specified problem and cannot validate its assumptions. KF diagnostic cost is not a full optimizer history; interpret cost decrease as solver progress only for GN/LM.

The pure-yaw setup exposes information weighting across formulations. For nonlinear improvement, use the separate large-attitude body-velocity recovery in `recovery.csv`.

## Lesson 12: full body velocity and forward + NHC

`slip.csv` fixes Step2 and runs KF/GN/LM on independent analytic circle truth. Body velocity is `(2, lateral, 0)` with lateral 0 or 0.6 m/s. Horizontal specific force comes directly from omega cross body_velocity, including actual sideslip kinematics.

The two updates are:

- `measured_3D_velocity`: actual body velocity plus measurement noise on all three axes.
- `forward_plus_NHC`: the same noisy forward sample, with lateral/vertical zero pseudo constraints.

Each variance is explicitly 0.0025 (m/s)². NHC zeros are not presented as independent measured noisy samples. Without sideslip, NHC can provide useful valid constraints. With sideslip, its incorrect likelihood can produce velocity error, incorrect bias feedback or low coverage. Heading is always available to separate NHC failure from missing global heading information.

The table reports pose RMSE, velocity NIS, final accelerometer-bias norm and body-lateral velocity 95% interval coverage. Lateral error/variance project physical velocity error/covariance onto the true orientation's lateral axis. This is a scoring axis available in the toy, not truth supplied to estimator updates.

Seed counts and durations remain small teaching examples. These coverage samples do not establish statistical precision, and GN/LM cannot be claimed to repair incorrect NHC assumptions.

## Acceptance boundaries

`kf_math_tests` verifies mathematical contracts. Teaching CSVs are readable, repeatable experiments without hard-coded winners in every scenario. Tests cover analytic position posterior, ZOH mean, geometry derivatives, C15 constraints, chart/GNLM equivalence, nonlinear recovery, PSD and boundary rejection.

These tests do not replace full recording-10/11 protocol regression or establish ROS integration, hard real-time behavior, allocation budgets, real sensor calibration or safety certification. See the [data contract (Chinese)](data-contract.md) for conversion, initialization and GNSS information modes.

Research equations and naming originate in `plan/KF.md` and the read-only SLAM research POC. Finite-difference/remainder test structures were informed by the original `constrained_tg/test_constrained.cc`, re-expressed through the new library's public API. See [source provenance (Chinese)](source-provenance.md) for the complete record.
