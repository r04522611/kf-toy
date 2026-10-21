// Handnote: from the ESKF to invariant and tangent-group filters (revision 2)
#let vsp = v
#set page(paper: "a4", margin: (x: 2.1cm, y: 2.2cm), numbering: "1", header: context {
  if counter(page).get().first() > 1 [
    #set text(size: 8.5pt, fill: luma(110))
    From the ESKF to invariant and tangent-group filters — review draft 3
    #h(1fr) kf-toy teaching note
  ]
})
#set text(font: "Libertinus Serif", size: 10.5pt)
#set par(justify: true, leading: 0.62em)
#set heading(numbering: "1.1")
#show heading.where(level: 1): it => { vsp(14pt); text(size: 17pt, it); vsp(6pt) }
#show heading.where(level: 2): it => { vsp(8pt); text(size: 12.5pt, it); vsp(3pt) }
#set math.mat(delim: "[")
#set math.vec(delim: "[")
#set math.equation(numbering: "(1)", supplement: [Eq.])
#show raw.where(block: false): it => box(fill: luma(240), inset: (x: 2.5pt, y: 0pt), outset: (y: 2.5pt), radius: 2pt, it)
#show raw.where(block: true): it => block(fill: luma(245), inset: 7pt, radius: 3pt, width: 100%, text(size: 8.6pt, it))
#show figure.caption: it => { set text(size: 9pt); it }
#show link: underline
#show ref: it => text(fill: rgb("#1f4e9c"), it)

#let note(title, body) = block(width: 100%, inset: 9pt, radius: 3pt, fill: rgb("#eef3fb"), stroke: (left: 2.5pt + rgb("#2455a4")))[
  #text(weight: "bold", title) \ #body ]
#let warn(title, body) = block(width: 100%, inset: 9pt, radius: 3pt, fill: rgb("#fdf3e7"), stroke: (left: 2.5pt + rgb("#df741a")))[
  #text(weight: "bold", title) \ #body ]
#let oneline(body) = block(width: 100%, inset: 9pt, radius: 3pt, fill: rgb("#eef7f1"), stroke: (left: 2.5pt + rgb("#278566")))[
  #text(weight: "bold")[In one line] \ #body ]
#let fig(path, cap, w: 100%) = figure(image("fig/" + path, width: w), caption: cap)
#let two(a, b) = grid(columns: (1fr, 1fr), gutter: 8pt, a, b)

// shorthands (never shadow Typst built-ins other than v, aliased above)
#let R = $bold(R)$
#let Rh = $hat(bold(R))$
#let v = $bold(v)$
#let p = $bold(p)$
#let g = $bold(g)$
#let I3 = $bold(I)_3$
#let I = $bold(I)$
#let w = $bold(omega)$
#let a = $bold(a)$
#let bg = $bold(b)_g$
#let ba = $bold(b)_a$
#let bnu = $bold(b)_nu$
#let dth = $delta bold(theta)$
#let hx(x) = $[#x]_times$

// ---------------------------------------------------------------- title
#vsp(0.6cm)
#align(center)[
  #text(size: 24pt, weight: "bold")[From the ESKF to invariant and \ tangent-group filters]
  #vsp(6pt)
  #text(size: 13pt)[A handnote in matrix form, illustrated with the `kf-toy` teaching examples]
  #vsp(10pt)
  #text(size: 10pt, fill: luma(90))[Review draft 3 — figures generated from `results/teaching` (4 seeds, 12 s, `kf_teaching-v2`)]
]
#vsp(0.4cm)

#heading(numbering: none)[What this note covers]

The note walks one estimation problem — IMU propagation with body-velocity and heading corrections — through five formulations. Each step changes a small number of matrices and leaves the rest of the filter untouched, and each chapter shows only the matrices that change, followed by the toy-example figures that make the change visible. Nothing in a chapter depends on a later one.

*Why a ladder and not a jump.* The end point, the equivariant filter on the tangent group, differs from a textbook ESKF in four independent ways at once: how the IMU is integrated, how the error is defined, how the biases are parametrized, and how large the state is. A single A/B comparison between the two cannot say which of the four produced a difference in the metrics. The recipes are therefore ordered so that each rung changes exactly one of them while the previous choices are kept, and the toy's equivalence tests confirm that a rung changes nothing else (for example Step3 and the reduced tangent-group filter C15A are identical to $10^(-8)$). The right-invariant chapter is both a filter in its own right — it is the invariant EKF with fixed gravity, usable as an "invariant ESKF" — and the rung without which the tangent-group chapter would change three things at once.

#text(size: 9pt)[#table(columns: (0.95fr, 0.7fr, 1.4fr, 1.25fr, 1.4fr, 0.45fr), inset: 4.5pt, stroke: 0.4pt + luma(170), align: (left, left, left, left, left, center),
  [*stage*], [*recipes*], [*what is new*], [*the one thing this rung isolates*], [*what the toy shows*], [*ch.*],
  [ESKF], [Step1], [nominal flow, error vector, $bold(F)$, $bold(Q)$, Joseph update, reset], [— (baseline)], [the baseline is consistent when the model matches], [#link(<ch:eskf>)[1]],
  [ESKF + Galilean flow], [Step2], [one matrix exponential replaces Euler; $Gamma$ blocks in $bold(F)$], [IMU integration formula; error definition unchanged], [exact for held input; input sampling is the remaining error], [#link(<ch:gal>)[2]],
  [Invariant error state], [Step3, Step4], [right-invariant error on the extended pose; physical and reset Jacobians], [error definition (Step3), then bias parametrization (Step4); flow unchanged], [propagation no longer depends on the attitude estimate], [#link(<ch:inv>)[3]],
  [EqF on the tangent group], [Step5A, C15A \ Step5B, C15B], [9-vector bias with a virtual part; constraint handling; midpoint output], [state extension by $bnu$ (5A vs Step4); output row only (B vs A)], [constant coupling; how $bnu$ is kept at zero; what one output row changes], [#link(<ch:tg>)[4], #link(<ch:cstar>)[5]],
  [Iterated solver], [GN, LM \ for every recipe], [fixed-prior MAP; the KF as the first Gauss–Newton step], [number of linearizations; prior, likelihood and chart unchanged], [large-error recovery; output variants coincide under iteration], [#link(<ch:gnlm>)[6]],
  [Closing], [—], [diagnostics under model mismatch], [the model, not the geometry], [NEES and coverage expose what cost and NIS do not], [#link(<ch:diag>)[7]])]

Notation, the operators $hx(dot)$, $Gamma_1, Gamma_2$, $op("Ad"), op("ad")$, the physical error chart used for every score, and the common experiment are collected in @app:common and linked at first use. Two conventions hold throughout: $t$ is time and $h$ is the length of one propagation interval; a hat marks an estimate, no hat the true value.

#pagebreak()
#outline(indent: auto, depth: 2)
#pagebreak()

// ---------------------------------------------------------------- 1
= The error-state Kalman filter (Step1) <ch:eskf>

The state is $(R, v, p, bg, ba)$ with the IMU model of @app:state; the skew matrix $hx(dot)$ and the series $Gamma_1, Gamma_2$ are defined in @app:ops. This chapter writes down the five matrices that every later chapter will modify.

== Nominal propagation with a held input

Over one interval of length $h$ with the held, bias-corrected input $w = tilde(w) - hat(bg)$, $a = tilde(a) - hat(ba)$, the Euler ESKF propagates
$ mat(R^+; v^+; p^+; bg^+; ba^+) = mat(
  R exp(h hx(w));
  v + h g + h R a;
  p + h v + 1/2 h^2 g + 1/2 h^2 R a;
  bg;
  ba). $ <eq:step1flow>

== Error definition: body-frame attitude, additive everything else

The true state is the estimate perturbed by the 15-vector $delta bold(x) = (dth, delta v, delta p, delta bg, delta ba)$:
$ mat(R; v; p; bg; ba) = mat(
  Rh exp(hx(dth));
  hat(v) + delta v;
  hat(p) + delta p;
  hat(bg) + delta bg;
  hat(ba) + delta ba). $ <eq:leftchart>
This is the *left* (body-frame) perturbation. It coincides with the physical error vector used for all scores (@app:yardstick), so for Step1 no coordinate change is needed before scoring.

== Discrete transition and process noise

Linearizing @eq:step1flow in the coordinates of @eq:leftchart:
$ bold(F)_"Step1" = mat(
  exp(-h hx(w)), 0, 0, -h Gamma_1(-h w), 0;
  -h Rh hx(a), I3, 0, 0, -h Rh;
  -1/2 h^2 Rh hx(a), h I3, I3, 0, -1/2 h^2 Rh;
  0, 0, 0, I3, 0;
  0, 0, 0, 0, I3). $ <eq:F1>
Continuous white noise $bold(n) = (bold(n)_g, bold(n)_a, bold(n)_(b g), bold(n)_(b a))$ enters through
$ bold(G) = mat(-I3, 0, 0, 0; 0, -Rh, 0, 0; 0, 0, 0, 0; 0, 0, I3, 0; 0, 0, 0, I3), wide
  bold(Q)_c = mat(sigma_g^2 I3, 0, 0, 0; 0, sigma_a^2 I3, 0, 0; 0, 0, sigma_(b g)^2 I3, 0; 0, 0, 0, sigma_(b a)^2 I3), $ <eq:G1>
and the discrete process covariance transports the noise injected at time $t in [0, h]$ to the end of the interval:
$ bold(Q) = integral_0^h bold(Phi)(h, t) bold(G)(t) bold(Q)_c bold(G)(t)^top bold(Phi)(h, t)^top dif t . $ <eq:Q>
The toy evaluates @eq:Q with three-point Gauss quadrature; the tests check it against integrated white acceleration,
$ mat(bold(Q)_(v v), bold(Q)_(v p); bold(Q)_(p v), bold(Q)_(p p)) = sigma_a^2 mat(h I3, 1/2 h^2 I3; 1/2 h^2 I3, 1/3 h^3 I3). $
The engine never forms $bold(F) bold(P) bold(F)^top + bold(Q)$ explicitly. It carries a square root $bold(L)$, $bold(P) = bold(L) bold(L)^top$, and compresses
$ mat(bold(F) bold(L), bold(N)) arrow.r.long^"QR" bold(L)^+, wide bold(N) bold(N)^top = bold(Q), $ <eq:sqrt>
so that positive semidefiniteness is structural.

== Measurement update and the reset

For a residual $bold(r) = bold(z) - bold(y)(hat(bold(x)))$ with predicted measurement $bold(y)$, Jacobian $bold(H)$ and noise $bold(V)$:
$ mat(bold(S); bold(K); delta bold(x); bold(P)') = mat(
  bold(H) bold(P) bold(H)^top + bold(V);
  bold(P) bold(H)^top bold(S)^(-1);
  bold(K) bold(r);
  (I - bold(K) bold(H)) bold(P) (I - bold(K) bold(H))^top + bold(K) bold(V) bold(K)^top). $ <eq:joseph>
The correction is retracted into the nominal state with @eq:leftchart. Because the attitude error was defined around the *old* estimate, the covariance is transported to the new one:
$ bold(P)^+ = bold(J) bold(P)' bold(J)^top, wide
  bold(J)_"exact" = mat(bold(J)_r (dth), 0; 0, bold(I)_12), quad
  bold(J)_"1st" = mat(I3 - 1/2 hx(dth), 0; 0, bold(I)_12), quad
  bold(J)_"textbook" = bold(I)_15, $ <eq:reset>
with $bold(J)_r (dth) = Gamma_1(-dth)$ the right Jacobian of SO(3). All recipes in the toy use the exact form; the size of the first-order error is plotted in @ch:gal.

== Measurement Jacobians in this chart

With $hat(v)_B = Rh^top hat(v)$ the predicted body velocity, lever arm $bold(l)$, and $bold(h)_theta$ the derivative of the yaw of the mounted x-axis with respect to $dth$:
$ mat(bold(H)_"vel"; bold(H)_"pos"; bold(H)_"yaw") = mat(
  hx(hat(v)_B), Rh^top, 0, 0, 0;
  -Rh hx(bold(l)), 0, I3, 0, 0;
  bold(h)_theta^top, 0, 0, 0, 0). $ <eq:H1>
In every scenario of this note $bold(H)_"vel"$ receives a genuine three-axis body-velocity measurement with noise on all three channels. A wheel-speed or CAN sensor supplies only the forward channel; what is usually done about the other two is the non-holonomic constraint, discussed on its own in @sec:nhc.

== Step1 in the toy

The baseline scenario is `matched_heading` (@app:experiment): fixed biases, matched noise, heading every 0.2 s.

#note("How to read NEES, NIS and coverage")[
  A filter reports an error covariance $bold(P)$ as well as an estimate; the question is whether $bold(P)$ is honest. The *normalized estimation error squared* compares the true error (available in the toy) with that covariance,
  $ op("NEES")_15 = bold(e)^top bold(P)_"phys"^(-1) bold(e), $
  with $bold(e)$ the 15-dimensional physical error of @app:yardstick. If the error is Gaussian with covariance $bold(P)$, $op("NEES")_15$ is $chi^2$ with 15 degrees of freedom and has mean 15; the toy reports its time average divided by 15, so a *consistent* filter sits near 1, a value above 1 means the filter is *overconfident* (its $bold(P)$ is too small for its actual error) and a value below 1 means it is *conservative*. NEES says nothing about how small the error is — a filter can have low RMSE and NEES of 70, or high RMSE and NEES of 1. The *normalized innovation squared* is the same construction applied to the measurement residual, $op("NIS") = bold(r)^top bold(S)^(-1) bold(r)$ with $bold(S)$ from @eq:joseph; it needs no truth and has mean 3 for a 3-D velocity, but it only sees what the measurement sees. *Coverage* is the fraction of time the true error of one component lies inside its $plus.minus 1.96 sigma$ band; it should be 0.95. Time samples are correlated, so these are diagnostics, not significance tests.
]
#two(fig("c1_matched_pos.svg", [Step1 KF position error, seed 0. Errors stay at the 0.1–0.3 m level set by the velocity noise and the prior.]),
     fig("c1_matched_nees.svg", [Step1 KF NEES15/15 over time, seed 0. It fluctuates around 1; the four-seed mean is 0.954.]))
Mean over 4 seeds, Step1 KF: position RMSE 0.179 m, attitude RMSE 0.0162 rad, NEES15/15 0.954, velocity NIS 2.90 (3 d.o.f.).

#oneline[
  The ESKF is five matrices: flow @eq:step1flow, chart @eq:leftchart, transition @eq:F1 with noise @eq:G1–@eq:Q, update @eq:joseph, reset @eq:reset. When noise and model match the data it is consistent (NEES ≈ 1, NIS ≈ 3).
]

// ---------------------------------------------------------------- 2
= ESKF with the Galilean flow (Step2) <ch:gal>

Step2 keeps the chart @eq:leftchart, the update @eq:joseph and the reset @eq:reset. It replaces the flow @eq:step1flow and therefore the transition @eq:F1. Writing the navigation state as one matrix shows why the new flow is exact.

== The navigation state as a matrix

$ bold(X) = mat(R, v, p; 0, 1, 0; 0, 0, 1) in "SE"_2(3) subset RR^(5 times 5), wide
  bold(X)_1 bold(X)_2 = mat(R_1 R_2, R_1 v_2 + v_1, R_1 p_2 + p_1; 0, 1, 0; 0, 0, 1). $ <eq:se23>
The product composes rotations and transports the two vectors. The Galilean group $"SGal"(3)$ adds a time entry in position $(2,3)$; that entry is what couples velocity into position inside a single exponential.

== Held IMU input is a single matrix exponential

Put the held body input into a matrix with a unit time rate:
$ bold(W) = mat(hx(w), a, 0; 0, 0, 1; 0, 0, 0), wide
  exp(h bold(W)) = mat(exp(h hx(w)), h Gamma_1(h w) a, h^2 Gamma_2(h w) a; 0, 1, h; 0, 0, 1). $ <eq:galexp>
The closed form follows from $bold(W)^k$ having $hx(w)^(k-1) a$ in the velocity column and $hx(w)^(k-2) a$ in the position column; summing with $1 \/ k!$ gives exactly the $Gamma$ series of @app:ops. With gravity as a left factor on a state whose time entry is zero at the start of the interval,
$ bold(X)^+ = underbrace(mat(I3, h g, -1/2 h^2 g; 0, 1, 0; 0, 0, 1), "gravity") space bold(X) space underbrace(exp(h bold(W)), "held body input"), $ <eq:galstep>
which multiplies out to the Step2 flow
$ mat(R^+; v^+; p^+; bg^+; ba^+) = mat(
  R exp(h hx(w));
  v + h g + h R Gamma_1(h w) a;
  p + h v + 1/2 h^2 g + h^2 R Gamma_2(h w) a;
  bg;
  ba). $ <eq:step2flow>
Against @eq:step1flow: Euler keeps the $k = 0$ term of each series, $Gamma_1 approx I3$, $Gamma_2 approx 1/2 I3$. For a constant body input over the interval @eq:step2flow is exact for any $h$; the math test checks it at $h = 0.5 "s"$, $omega_z = 2 "rad/s"$, $f_x = 3 "m/s"^2$.

== The transition matrix gains bias-sensitivity blocks

Differentiating @eq:step2flow in the same chart:
$ bold(F)_"Step2" = mat(
  exp(-h hx(w)), 0, 0, -h Gamma_1(-h w), 0;
  -Rh hx(h Gamma_1 a), I3, 0, -h^2 Rh bold(D)_1, -h Rh Gamma_1;
  -Rh hx(h^2 Gamma_2 a), h I3, I3, -h^3 Rh bold(D)_2, -h^2 Rh Gamma_2;
  0, 0, 0, I3, 0;
  0, 0, 0, 0, I3), wide
  bold(D)_i = (partial (Gamma_i (bold(phi)) a)) / (partial bold(phi)) |_(bold(phi) = h w). $ <eq:F2>
Two blocks changed relative to @eq:F1: the accelerometer-bias column is rotated by $Gamma_1, Gamma_2$ instead of $I3, 1/2 I3$, and the gyro-bias column now reaches velocity and position through $bold(D)_1, bold(D)_2$ (a gyro bias tilts the integrated acceleration). $bold(G)$ and @eq:Q are unchanged.

== What exactness does and does not buy

#two(fig("integration_held.svg", [Constant body force rotating at 2 rad/s, integrated over 1 s. Step2 is exact at every step size (the floor is round-off); Step1 converges at first order.]),
     fig("integration_varying.svg", [Body force $3 cos 1.7 t$. Both recipes now carry input-sampling error. Holding the *midpoint* sample lets Step2 converge at second order; the left sample does not.]))
Once the real signal varies between samples, the integration formula stops being the bottleneck: the description of the input over the interval is. Exact ZOH applied to the wrong sample is still first order.

#fig("so3.svg", [Additive rotation approximations vs. angle. $I + hx(bold(phi))$ loses orthogonality quadratically; the first-order reset $I - 1/2 hx(bold(phi))$ departs from $bold(J)_r$ cubically and is visible from 0.1 rad; $exp$ stays orthogonal to machine precision.], w: 70%)

On the toy scenarios at 40 Hz the two recipes are indistinguishable (KF, 4-seed mean): `matched_heading` 0.179 vs 0.181 m, `pure_dr` 0.962 vs 0.963 m, NEES15/15 0.954 vs 0.953. The step-size sweep is where the difference lives.

#oneline[
  Step2 = Step1 with the flow replaced by the single exponential @eq:galexp–@eq:galstep and the transition by @eq:F2. The error definition is untouched.
]

// ---------------------------------------------------------------- 3
= Invariant error state (Step3, Step4) <ch:inv>

== Why this step, and why now

@ch:gal changed how the IMU is integrated and left the error definition @eq:leftchart alone. This chapter does the opposite: it keeps the Step2 flow and changes only what the 15-vector $delta bold(x)$ *means*. Two reasons justify the rung on its own and one justifies its position.

As a filter, the result is the right-invariant EKF with fixed gravity — what is often called the invariant ESKF. Its selling point is visible directly in the matrices: the attitude and velocity rows of the error dynamics stop depending on the attitude estimate (@eq:Ainv below), so a wrong attitude does not corrupt its own propagation. That property is what the `pure_dr` scenario is designed to probe.

As a rung, Step3 is the tangent-group filter of @ch:tg with the virtual bias removed; the toy proves this by showing Step3 and C15A identical to $10^(-8)$. Had the note gone from Step2 straight to Step5A, the comparison would have changed the error definition, the bias parametrization and the state dimension together, and no metric could have been attributed to any one of them. Step3 isolates the error definition; Step4 then isolates the bias parametrization; @ch:tg isolates the state extension.

Changing what $delta bold(x)$ means forces three bookkeeping questions that the ESKF never had to ask, because in the left chart their answers are trivial (identity or nearly so). They are answered first, in @sec:retract–@sec:reset, so that the invariant error in @sec:rinv can be used immediately.

== Retract and local <sec:retract>

*Why it is needed.* The Kalman update @eq:joseph produces a *vector* $delta bold(x)$; the estimator state is a rotation matrix and four vectors. Something must turn the vector into a state (to apply the correction) and two states into a vector (to compute the error $bold(e)$ for scoring, to run GN/LM later, and to check Jacobians by finite differences). In the ESKF this was @eq:leftchart and it was never named. Once the vector is allowed to mean something else, the two maps must be written down explicitly, and every other matrix in the filter is a derivative of them.

A chart is a pair of inverse maps between a state and a vector, $bold(x) = op("retract")(hat(bold(x)), delta)$ and $delta = op("local")(hat(bold(x)), bold(x))$. @eq:leftchart is the left chart. The *right-invariant* chart multiplies the 5×5 matrix @eq:se23 from the left:
$ bold(X) = exp(bold(xi)^and) hat(bold(X)), wide
  bold(xi)^and = mat(hx(bold(xi)_theta), bold(xi)_v, bold(xi)_p; 0, 0, 0; 0, 0, 0), wide
  exp(bold(xi)^and) = mat(bold(E), Gamma_1(bold(xi)_theta) bold(xi)_v, Gamma_1(bold(xi)_theta) bold(xi)_p; 0, 1, 0; 0, 0, 1), $ <eq:rightchart>
with $bold(E) = exp(hx(bold(xi)_theta))$, so that
$ mat(R; v; p; bg; ba) = mat(
  bold(E) Rh;
  bold(E) hat(v) + Gamma_1(bold(xi)_theta) bold(xi)_v;
  bold(E) hat(p) + Gamma_1(bold(xi)_theta) bold(xi)_p;
  hat(bg) + delta bg;
  hat(ba) + delta ba). $ <eq:rightretract>
Biases stay additive in Step3; Step4 changes them too (@eq:step4bias).

== The physical Jacobian

*Why it is needed.* After the chart changes, the entry $P_(4 4)$ of Step3's covariance is no longer "variance of the x-velocity error" in the sense of @app:yardstick: the native coordinates $bold(xi)_v$ mix velocity with a world-frame rotation of the estimate. Comparing Step3 against Step1, computing NEES against the same truth, or plotting a $plus.minus 1.96 sigma$ band for the z position, all require moving the covariance into one agreed chart first. Without this matrix the recipes would be scored in different units and any ranking between them would be meaningless.

The native covariance $bold(P)$ now lives in $bold(xi)$ coordinates while every score lives in the physical chart (@app:yardstick). To first order $delta bold(x) = bold(J)_"phys" bold(xi)$ with
$ bold(J)_"phys" = mat(
  Rh^top, 0, 0, 0, 0;
  -hx(hat(v)), I3, 0, 0, 0;
  -hx(hat(p)), 0, I3, 0, 0;
  0, 0, 0, I3, 0;
  0, 0, 0, 0, I3), wide
  bold(P)_"phys" = bold(J)_"phys" bold(P) bold(J)_"phys"^top. $ <eq:Jphys>
The $Rh^top$ block says a world-frame rotation perturbation is a body-frame one rotated back; the $-hx(hat(v))$, $-hx(hat(p))$ blocks say that a world-frame rotation also moves velocity and position. A diagonal native $bold(P)$ in this chart is a *correlated* physical covariance. The math test checks @eq:Jphys against central finite differences for all recipes.

== The reset Jacobian <sec:reset>

*Why it is needed.* The covariance before an update describes a perturbation *around the old estimate*. The update moves the estimate by a finite $delta$; the tangent space moves with it, and a perturbation of the old estimate is not the same vector as the equally sized perturbation of the new one. For the left chart the mismatch is only in the attitude block and is the $bold(J)_r (dth)$ of @eq:reset, which textbooks often drop; the SO(3) figure in @ch:gal shows the dropped term growing cubically with the correction. In the right-invariant chart the mismatch couples attitude, velocity and position, so it can no longer be ignored, and the same matrix will be applied at every iterate of GN/LM in @ch:gnlm.

After a correction $delta$ the covariance is re-expressed around the new estimate. For the two charts:
$ bold(J)_"reset"^"left" = mat(Gamma_1(-dth), 0; 0, bold(I)_12), wide
  bold(J)_"reset"^"right" = mat(sum_(k >= 0) op("ad")(bold(xi)_"nav")^k / (k+1)!, 0; 0, bold(I)_6), $ <eq:resetR>
with $op("ad")$ from @app:ops. Both are finite maps, not first-order approximations.

== The discrete transition in the new chart

The toy does not derive a second transition matrix. It conjugates the physical-chart result:
$ bold(F)_"native" = bold(J)_"phys"(hat(bold(x))^+)^(-1) space bold(F)_"phys" space bold(J)_"phys"(hat(bold(x))), wide
  bold(Q)_"native" = bold(J)_"phys"(hat(bold(x))^+)^(-1) space bold(Q)_"phys" space bold(J)_"phys"(hat(bold(x))^+)^(-top). $ <eq:conj>
Step3 therefore propagates the same physical covariance as Step2 up to linearization; a difference in results is a difference in error definition, not in integration.

== The right-invariant error and its dynamics <sec:rinv>

Define the error as the group difference between true and estimated extended poses, true on the left:
$ bold(eta) = bold(X) hat(bold(X))^(-1) = mat(R Rh^top, v - R Rh^top hat(v), p - R Rh^top hat(p); 0, 1, 0; 0, 0, 1), wide bold(xi) = op("Log")(bold(eta)). $ <eq:rinv>
To first order $bold(xi)_theta$ is a *world-frame* attitude error and $bold(xi)_v approx v - bold(E) hat(v)$, $bold(xi)_p approx p - bold(E) hat(p)$ — the chart @eq:rightchart. Differentiating along the IMU flow with bias errors $delta bg = bg - hat(bg)$, $delta ba = ba - hat(ba)$ gives $dot(bold(xi)) = bold(A) bold(xi) + bold(G) bold(n)$ with, side by side,

$ bold(A)_"left (Step1/2)" = mat(
  -hx(w), 0, 0, -I3, 0;
  -Rh hx(a), 0, 0, 0, -Rh;
  0, I3, 0, 0, 0;
  0, 0, 0, 0, 0;
  0, 0, 0, 0, 0), $
$ bold(A)_"right-invariant (Step3)" = mat(
  0, 0, 0, -Rh, 0;
  hx(g), 0, 0, -hx(hat(v)) Rh, -Rh;
  0, I3, 0, -hx(hat(p)) Rh, 0;
  0, 0, 0, 0, 0;
  0, 0, 0, 0, 0). $ <eq:Ainv>

Read the two 9×9 navigation blocks (upper-left). In the left chart they contain the inputs $w$, $a$ and the estimate $Rh$. In the right-invariant chart they contain only $hx(g)$ and $I3$. The estimate has moved into the bias columns and into the noise matrix,
$ bold(G)_"Step3" = mat(-Rh, 0, 0, 0; -hx(hat(v)) Rh, -Rh, 0, 0; -hx(hat(p)) Rh, 0, 0, 0; 0, 0, I3, 0; 0, 0, 0, I3)
  = mat(-op("Ad")_(hat(bold(X))) mat(bold(I)_6; 0), 0; 0, bold(I)_6), $ <eq:Ginv>
with $op("Ad")$ from @app:ops. Two consequences follow and are what the figures below test: the attitude row no longer depends on the current attitude estimate, so a large attitude error does not corrupt its own propagation; and world-frame measurements get simple Jacobians (used in @ch:cstar).

== Step4: bias coordinates that move with the state

Step3 leaves the biases additive, so their coupling columns in @eq:Ainv still carry $Rh$, $hat(v)$, $hat(p)$. Step4 changes the *bias* chart:
$ mat(delta bg; delta ba) = op("Ad")_(Rh, hat(v))^(-1) space bold(J)_(-op("ad"))(bold(xi)_(theta v)) space mat(bold(xi)_(b g); bold(xi)_(b a)), wide
  op("Ad")_(Rh, hat(v)) = mat(Rh, 0; hx(hat(v)) Rh, Rh), wide
  bold(J)_(-op("ad"))(bold(xi)) = sum_(k >= 0) (-op("ad")(bold(xi)))^k / (k+1)!, $ <eq:step4bias>
with the 6×6 restriction of $op("Ad")$, $op("ad")$. Its physical Jacobian carries $op("Ad")_(Rh, hat(v))^(-1)$ in the bias block and its reset Jacobian is the 15×15 series of $mat(op("ad")(bold(xi)_"nav"), 0; op("ad")(bold(xi)_b), op("ad")(bold(xi)_(theta v)))$. The bias uncertainty is expressed in the same moving frame as the navigation error.

== Step1–Step4 in the toy

The scenario that separates the charts is `pure_dr` (IMU and body velocity only; global yaw unobservable), with `heading_dropout` as the control.
#two(fig("c3_pure_dr_pos.svg", [`pure_dr`, KF, seed 0. Position error grows for all four recipes; Step3 and Step4 sit below Step1/2 on this seed.]),
     fig("c3_pure_dr_att.svg", [`pure_dr`, KF attitude error, seed 0. The unobservable yaw drifts; the invariant charts drift less on this seed.]))
#two(fig("c3_pure_dr_bars.svg", [`pure_dr`, KF, 4-seed means. Position RMSE 0.96 / 0.96 / 0.92 / 0.87 m; NEES15/15 1.28 / 1.28 / 1.20 / 1.13 for Step1–4. All are overconfident; the invariant charts less so.]),
     fig("c3_dropout_att.svg", [`heading_dropout`, KF attitude error. Inside the shaded window all four drift alike and all recover at once when heading returns: no chart supplies missing information.]))

#warn("Reading these figures")[
  The `pure_dr` differences are real but small and four seeds do not establish a ranking. What the figures support: the right-invariant error improves *consistency* when attitude is poorly observed, because its propagation @eq:Ainv does not depend on the attitude estimate. It does not improve *observability*.
]

#oneline[
  Invariant error = move the estimate out of the navigation block of $bold(A)$ and into the bias and noise columns (@eq:Ainv–@eq:Ginv). Reading and transporting the covariance needs @eq:Jphys and @eq:resetR. Step4 additionally re-expresses the biases (@eq:step4bias).
]

// ---------------------------------------------------------------- 4
= EqF on the tangent group (Step5A, C15A) <ch:tg>

== Why extend the state

In @eq:Ainv the bias columns $-Rh$, $-hx(hat(v)) Rh$, $-hx(hat(p)) Rh$ are the first two column blocks of $-op("Ad")_(hat(bold(X)))$. If the bias were a full 9-vector $bold(b) = (bg, ba, bnu)$ paired with all three rows of the navigation algebra, the coupling would be $-op("Ad")$ in full, and expressing the bias error in the frame moved by $op("Ad")$ would make it a constant $-bold(I)_9$. That is the tangent group $"TG" = "SE"_2(3) times.l frak(s e)_2(3)$ with state $(bold(X), bold(b)) in RR^18$.

The third block $bnu$ is *virtual*: the position row of the algebra has no sensor. It enters the flow as a velocity-like term:
$ mat(R^+; v^+; p^+; bg^+; ba^+; bnu^+) = mat(
  R exp(h hx(w));
  v + h g + h R Gamma_1(h w) a;
  p + h v + 1/2 h^2 g + h^2 R Gamma_2(h w) a - h R Gamma_1(h w) bnu;
  bg;
  ba;
  bnu), $ <eq:tgflow>
i.e. the TG filter models $dot(p) = v - R bnu$. Physically $bnu equiv 0$; the rest of the chapter is about how a filter should know that.

== TG error dynamics: the constant coupling

Transport the body input by the adjoint,
$ bold(u) = op("Ad")_(hat(bold(X))) mat(w; a; -hat(bnu)) + mat(0; g; hat(v)). $ <eq:u>
The 18×18 continuous error dynamics in the TG chart are
$ bold(A)_"TG" = mat(bold(A)_"nav", -bold(I)_9; 0, op("ad")(bold(u))), wide
  bold(A)_"nav" = mat(0, 0, 0; hx(g), 0, 0; 0, I3, 0), wide
  bold(G)_"TG" = mat(-op("Ad")_(hat(bold(X))) mat(bold(I)_6; 0), 0; 0, op("Ad")_(hat(bold(X))) mat(bold(I)_6; 0)). $ <eq:Atg>
Against @eq:Ainv the navigation-to-bias coupling is now $-bold(I)_9$: no $Rh$, $hat(v)$, $hat(p)$. The state dependence has moved into the bias block $op("ad")(bold(u))$ and into $bold(G)$. The toy integrates @eq:Atg with RK4 for $bold(F)$ and Simpson's rule for the noise columns.

== TG retraction and physical Jacobian

The bias part of the TG retraction is the 9×9 version of @eq:step4bias:
$ mat(delta bg; delta ba; delta bnu) = op("Ad")_(hat(bold(X)))^(-1) space bold(J)_(-op("ad"))(bold(xi)_"nav") space bold(xi)_b, wide
  bold(J)_"phys"^"TG" = mat(bold(J)_"phys"^"nav", 0; 0, op("Ad")_(hat(bold(X)))^(-1)) in RR^(18 times 18), $ <eq:tgphys>
with $bold(J)_"phys"^"nav"$ the 9×9 block of @eq:Jphys. The physical chart now has 18 entries, the last three being $delta bnu$ in the body frame.

== The constraint

Take the simplest case where the problem shows: identity prior navigation, corrections $delta p = (delta p_x, 0, 0)$ and $delta bg = (0, delta b_y, 0)$, with the native TG correction built so that its *first-order* virtual perturbation is zero. The finite retraction @eq:tgphys then gives
$ bnu^"new" = -1/2 hx(delta p) delta bg, $ <eq:bnu>
a non-zero virtual bias of second order. The surface $bnu = 0$ is not invariant under the TG retraction: every finite correction that touches both position and gyro bias leaves it.

#fig("constraints.svg", [`constraints.csv`. The TG finite retraction follows @eq:bnu exactly (dashed prediction); the reduced C15 retraction stays at zero. This is the derived subspace only, not a global statement.], w: 70%)

Four operations are often conflated; they are different matrices:
#table(columns: (auto, 1fr, 1fr), inset: 5pt, stroke: 0.4pt + luma(170),
  [*operation*], [*what it does*], [*is it a Bayesian update?*],
  [Reduced representation (C15)], [state has no $bnu$; the retraction never leaves the surface], [yes — a different model],
  [Finite-noise anchor], [measurement $bold(z) = 0$, $bold(H) = mat(0, 0, 0, 0, 0, I3)$, noise $bold(V)_a$], [yes — adds evidence each time it is applied],
  [Zero first-order tangent variance], [prior $bold(P)_(nu nu) = 0$; linear support restricted], [only to first order; @eq:bnu reintroduces $bnu$],
  [Hard mean clamp], [set $hat(bnu) = 0$ after the update, leave $bold(P)$], [no — mean and covariance disagree])

== C15: the reduced embedding

C15A is the 15-dimensional filter obtained by deleting $bnu$ and its row and column from the TG. Its navigation chart is @eq:rightchart, its biases are additive and its flow is @eq:step2flow — which is Step3. The `constrainedEmbedding` test lifts a C15 correction into the TG with the compatible $bold(xi)_b = bold(J)_(-op("ad"))^(-1) op("Ad") (delta bg, delta ba, 0)$ and verifies agreement to $10^(-9)$ with $bnu = 0$: C15 is the TG restricted to the constraint surface, not an approximation of it. In `metrics.csv` Step3 and C15A agree to $10^(-8)$.

== Step3, Step4, Step5A, C15A in the toy

#fig("c4_pure_dr_bars.svg", [`pure_dr`, KF, 4-seed means. Step4 and Step5A are numerically close (0.865 vs 0.866 m; NEES 1.13 vs 1.12): the 18D TG with $bnu$ free adds little over Step4 here. Step3 = C15A.], w: 70%)

The anchor study isolates the virtual-bias treatment with a deliberately wide prior on $bnu$ ($0.01 I3$) so that it matters:
#two(fig("anchor_rmse.svg", [Position RMSE: TG without anchor, TG with a $0.001 (m\/s)^2$ anchor every 0.2 s, and C15. `matched_heading`: identical. `pure_dr`: 1.10 / 0.90 / 1.23 m.]),
     fig("anchor_nees.svg", [NEES15/15 in the same study: 0.61–0.82, all conservative because of the inflated prior.]))
#fig("anchor_bnu.svg", [Final $|bnu|$ per seed in `pure_dr`. Without anchor the virtual bias wanders to $3$–$10 times 10^(-3)$ m/s; the anchor holds it near $10^(-3)$; C15 is structurally zero.], w: 60%)

#warn("What the anchor study does not isolate")[
  C15 changes the dynamics (no $bnu$ term in @eq:tgflow), the bias retraction and the Gaussian family at once. The `pure_dr` ordering therefore cannot be attributed to curvature of the constraint surface alone, and the archived road-data parameters are not reproduced here.
]

#oneline[
  The tangent group buys a constant navigation-to-bias coupling (@eq:Atg) at the price of a latent $bnu$ with no sensor. A finite TG correction generically leaves $bnu = 0$ (@eq:bnu). The two consistent responses are to add evidence (anchor) or to remove the coordinate (C15).
]

// ---------------------------------------------------------------- 5
= Output approximation: the midpoint (C-star) output (Step5B, C15B) <ch:cstar>

== The body-velocity measurement in the invariant chart

The velocity sensor gives $bold(z) = R^top v + bold(n)$. Rotate the residual into the world frame with the current estimate and expand it in the right-invariant perturbation @eq:rightchart:
$ bold(r) = Rh bold(z) - hat(v) = Rh R^top v - hat(v) = bold(E)^top (bold(E) hat(v) + Gamma_1(bold(xi)_theta) bold(xi)_v) - hat(v) = Gamma_1(-bold(xi)_theta) bold(xi)_v. $ <eq:rexact>
To first order $bold(r) = bold(xi)_v$: the Jacobian is constant. One order further,
$ Gamma_1(-bold(xi)_theta) bold(xi)_v = bold(xi)_v + 1/2 hx(bold(xi)_v) bold(xi)_theta + dots, $
and replacing $bold(xi)_v$ by the observed $bold(r)$ in the second-order term gives the two output models
$ mat(bold(H)_A; bold(H)_B) = mat(0, I3, 0, 0, 0; 1/2 hx(bold(r)), I3, 0, 0, 0), wide
  bold(V)_"world" = Rh bold(V)_"body" Rh^top. $ <eq:HAB>
$bold(H)_B$ is the C-star (midpoint) output. It is the only difference between Step5A and Step5B, and between C15A and C15B, and it is applied on the single-pass KF gain row only.

== Remainder orders

For a noiseless perturbed state the two residual models leave different remainders:
$ mat(bold(e)_A; bold(e)_B) = mat(bold(r) - bold(xi)_v; bold(r) - bold(xi)_v - 1/2 hx(bold(r)) bold(xi)_theta) = mat(O(|bold(xi)|^2); O(|bold(xi)|^3)). $ <eq:remainder>
#fig("remainder.svg", [`remainder.csv`, direction 1. Halving the scale divides the ordinary remainder by 4 and the midpoint remainder by 8; the test checks seven directions and five scales for ratios within $4 plus.minus 0.3$ and $8 plus.minus 0.4$.], w: 66%)

#warn("A cubic remainder is a local, noiseless statement")[
  With noise, $bold(H)_B$ depends on the measurement through $bold(r)$, so the gain becomes data dependent. Nothing above implies posterior calibration or lower RMSE over a run.
]

== A versus B in the toy (KF)

#two(fig("c5_recovery_kf.svg", [One body-velocity update from a 1.2 rad yaw error with a narrow velocity prior and wide yaw prior (truth 0). Output A lands at $+0.27$ rad, output B at $-0.17$ rad: a different single step, neither converged.]),
     fig("c5_pure_dr_bars.svg", [`pure_dr`, KF, 4-seed means. B lowers position RMSE from 0.866 to 0.813 m (Step5) and 0.919 to 0.861 m (C15) and brings NEES15/15 from 1.12/1.20 to 1.07/1.13 — within what four seeds can resolve.]))

#oneline[
  C-star changes one Jacobian row (@eq:HAB) and nothing else. Its effect is bounded by what a single linearization can do.
]

// ---------------------------------------------------------------- 6
= Iterated solver: GN and LM as fixed-prior MAP <ch:gnlm>

Every recipe above was a single-pass filter. The same prior, likelihood and chart can be solved iteratively; this chapter writes the KF update as the first iterate of that solver.

== The KF update is one Gauss–Newton step

Parametrize the posterior in the native tangent of the prior, whitened by the prior root $bold(L)$ ($bold(P)^- = bold(L) bold(L)^top$):
$ bold(x)(bold(z)) = op("retract")(hat(bold(x))^-, bold(L) bold(z)), wide
  C(bold(z)) = 1/2 bold(z)^top bold(z) + 1/2 bold(r)(bold(x)(bold(z)))^top bold(V)^(-1) bold(r)(bold(x)(bold(z))). $ <eq:cost>
The prior enters once, as $1/2 bold(z)^top bold(z)$, and is held fixed. Linearizing the residual at the current iterate,
$ mat(bold(A); bold(B); bold(g)) = mat(
  bold(H)_"phys"(bold(x)) space bold(J)_"phys"(bold(x)) space bold(J)_"reset"(bold(L) bold(z)) space bold(L);
  I + bold(A)^top bold(V)^(-1) bold(A);
  bold(A)^top bold(V)^(-1) bold(r) - bold(z)), $ <eq:gn>
the Gauss–Newton and Levenberg–Marquardt steps are
$ Delta bold(z)_"GN" = bold(B)^(-1) bold(g), wide
  Delta bold(z)_"LM" = (bold(B) + lambda op("diag")(bold(B)))^(-1) bold(g), $ <eq:steps>
with $lambda$ multiplied by 10 after a rejected trial and divided by 3 after an accepted one. At $bold(z) = 0$ one GN step equals the Kalman correction of @eq:joseph (matrix-inversion lemma on $bold(B)^(-1)$): the KF is the first iterate.

== Posterior covariance

After the last accepted iterate the covariance is formed from the undamped information and mapped once through the final reset:
$ bold(P)^+ = bold(J)_"reset"(bold(L) bold(z)) space bold(L) space bold(B)^(-1) space bold(L)^top space bold(J)_"reset"(bold(L) bold(z))^top . $ <eq:gnpost>
LM damping stabilizes the search but never shrinks the posterior. Stopping statuses — `gradient`, `step`, `cost`, `stalled`, `capped`, `time_budget` — are recorded per update; only the first three mean convergence.

== Why the prior must stay fixed

Replacing the prior by the previous iterate's posterior would add $1/2 bold(z)^top bold(z)$ again with a smaller $bold(L)$: the same measurement would be counted at every iteration and the covariance would collapse. The `gaussian` test enforces the linear case: KF, GN and LM must all return $bold(P)^+ = (bold(P)^(-1) + bold(H)^top bold(V)^(-1) bold(H))^(-1)$ regardless of iteration count.

== GN/LM across all recipes in the toy

#fig("recovery.svg", [`recovery.csv`: the recovery problem of @ch:cstar for all recipes and backends. GN and LM take 4–5 iterations from cost 1020 to 0.36 and reach $4 times 10^(-4)$ rad for every recipe; the chart and the output variant stop mattering.], w: 78%)
#two(fig("bars_pure_dr_rmse.svg", [`pure_dr`, 4-seed means, all recipes and backends. GN/LM lower RMSE by 3–6% and make A and B identical (rows agree to $10^(-8)$).]),
     fig("bars_pure_dr_nees.svg", [`pure_dr` NEES15/15. Iteration moves every recipe 0.05–0.15 toward 1; none reach it. In `matched_heading` GN/LM change nothing measurable.]))

#oneline[
  GN/LM = repeat the Kalman step with the same prior and an updated linearization (@eq:gn–@eq:steps), then report @eq:gnpost. They solve the stated problem better; they do not change what it assumes.
]

// ---------------------------------------------------------------- 7
= Reading the diagnostics: when the model is wrong <ch:diag>

== Cost is not truth

One heading update from prior yaw 1.2 rad, truth 0, measurement noise 0.05 rad:
#two(fig("assumptions.svg", [Posterior yaw error. Correct likelihood: 0.0015 rad. Heading biased by 0.8 rad: the filter lands at 0.80, where the data says. Overconfident prior ($10^(-5) "rad"^2$): the correct measurement is ignored, 1.195 rad remains.]),
     fig("assumptions_cost.svg", [GN cost before and after for the same cases. The wrong-heading case has the *lowest* final cost (0.04): the optimizer solved its problem perfectly. The overconfident case barely moves (288 → 287).]))
Identical numbers appear for every recipe and backend: no chart or solver can hide a wrong assumption.

== An unmodelled bias step

#two(fig("trace_bias_mismatch_nees.svg", [NEES15/15 after a +0.12 rad/s gyro-z step at 6 s with zero bias random walk in the filter. All recipes rise to ≈300 by 12 s: the covariance has frozen while the error grows.]),
     fig("trace_bias_mismatch_bg.svg", [Gyro-bias error norm: the filter follows the step slowly (0.05 rad/s remaining after 6 s) because its process model says the bias cannot move.]))
`metrics.csv` reports NEES15/15 ≈ 72 for every recipe and backend. Invariant charts, tangent groups and iterated solvers all give the same answer because none of them touch the process-noise model.

== Full 3-D velocity versus forward + NHC <sec:nhc>

#note("What the non-holonomic constraint is, and why it is assumed")[
  A ground vehicle's speed sensor (wheel odometry, CAN) measures one number: the forward speed along the body x-axis. The velocity row of @eq:H1 wants three. The *non-holonomic constraint* (NHC) fills the missing two with a kinematic assumption about wheeled motion: a vehicle that does not skid sideways and stays on the road surface has zero lateral and zero vertical velocity *at the wheel contact point, in the body frame*. The constraint is written as a pseudo-measurement that reuses @eq:H1 unchanged,
  $ bold(z)_"NHC" = mat(tilde(v)_x; 0; 0), wide bold(y) = Rh^top hat(v), wide bold(V)_"NHC" = mat(sigma_x^2, 0, 0; 0, sigma_"lat"^2, 0; 0, 0, sigma_"vert"^2), $ <eq:nhc>
  so the filter sees a full 3-D velocity although only one channel was sensed. The assumption is made because it is cheap and powerful: the two zero channels are what ties the IMU to the vehicle. Through the $hx(hat(v)_B)$ block of @eq:H1 the lateral channel observes yaw-rate-related attitude and gyro-bias error, and the vertical channel observes pitch and the vertical velocity — exactly the directions that dead reckoning on forward speed alone leaves unobserved. The price is that $sigma_"lat"$, $sigma_"vert"$ are not sensor specifications but a statement about the vehicle: they must cover sideslip in fast cornering or low friction, vertical motion on ramps and bumps, and the lever arm between the IMU and the axle (the IMU's own lateral velocity in a turn is $omega_z l$, not zero). In this note the toy sets all three variances to $0.0025 (m\/s)^2$ and asks what happens when the vehicle does and does not obey the assumption.
]

Step2, truth with lateral body velocity 0 or 0.6 m/s, heading always available, each velocity channel variance $0.0025 (m\/s)^2$. The NHC variant @eq:nhc uses the same noisy forward sample and sets the lateral and vertical channels to zero.
#two(fig("slip_rmse.svg", [Log scale. Without sideslip NHC is a valid constraint and slightly helps (0.086 vs 0.099 m). With 0.6 m/s sideslip the NHC likelihood is wrong: 2.98 m.]),
     fig("slip_cov.svg", [Lateral-velocity 95% coverage. Measured 3-D: 0.97. NHC without slip: 0.99. NHC with slip: 0.00 — confidently wrong in the lateral direction.]))
#fig("slip_nis.svg", [Mean velocity NIS (3 d.o.f.). NHC *lowers* NIS (1.2–1.7 vs 2.75) even when wrong, because two of its three channels are noiseless by construction; NIS alone would not flag the failure. Coverage against truth does.], w: 60%)

#oneline[
  NEES and coverage against truth expose model mismatch; cost and NIS do not. The remedies — bias random walk, sideslip in the measurement model — are in the model, not in the geometry.
]

// ---------------------------------------------------------------- Appendices
#pagebreak()
#counter(heading).update(0)
#set heading(supplement: [], numbering: (..nums) => {
  let n = nums.pos()
  if n.len() == 1 { "Appendix " + numbering("A", n.at(0)) + ":" } else { numbering("A.1", ..n) }
})

= Common ground: state, operators and the yardstick <app:common>

== State, frames and IMU model <app:state>

A world frame $W$ and a body (IMU) frame $B$; $R$ maps body to world. Gravity is a fixed parameter.
$ bold(x) = (R, v, p, bg, ba), wide R in "SO"(3), quad v, p in RR^3 "(world)", quad bg, ba in RR^3 "(body)", $
$ g = mat(0; 0; -9.81) "(fixed parameter)". $
IMU samples are gyro $tilde(w)$ and specific force $tilde(a)$ in the body frame, with white noise $bold(n)$ and random-walk biases:
$ mat(tilde(w); tilde(a)) = mat(w; a) + mat(bg; ba) + mat(bold(n)_g; bold(n)_a), wide
  mat(dot(bg); dot(ba)) = mat(bold(n)_(b g); bold(n)_(b a)). $ <eq:imu>
Time is $t$; one propagation interval has length $h$. A hat marks an estimate; the same symbol without a hat is the true value; a superscript $+$ is the value after a propagation or update.

== Operators <app:ops>

*Skew matrix.* For $bold(x) in RR^3$,
$ hx(bold(x)) = mat(0, -x_3, x_2; x_3, 0, -x_1; -x_2, x_1, 0), wide hx(bold(x)) bold(y) = bold(x) times bold(y). $ <eq:skew>

*Rotation exponential and the $Gamma$ matrices.* With $bold(phi) in RR^3$ and $bold(Phi) = hx(bold(phi))$,
$ exp(bold(Phi)) = sum_(k >= 0) bold(Phi)^k / k!, wide
  Gamma_1(bold(phi)) = sum_(k >= 0) bold(Phi)^k / (k+1)!, wide
  Gamma_2(bold(phi)) = sum_(k >= 0) bold(Phi)^k / (k+2)!. $ <eq:gamma>
$exp(bold(Phi))$ is a rotation matrix; $Gamma_1$ is the left Jacobian of SO(3), so $Gamma_1(-bold(phi))$ is the right Jacobian $bold(J)_r (bold(phi))$; $Gamma_2$ is its second integral. The toy evaluates all three by the series (`gammas`) and the tests compare them with closed forms.

*Adjoint and little adjoint of the extended pose.* For a navigation triple $(R, v, p)$ and a tangent vector $bold(xi) = (bold(xi)_theta, bold(xi)_v, bold(xi)_p)$,
$ op("Ad")_(R, v, p) = mat(R, 0, 0; hx(v) R, R, 0; hx(p) R, 0, R), wide
  op("ad")(bold(xi)) = mat(hx(bold(xi)_theta), 0, 0; hx(bold(xi)_v), hx(bold(xi)_theta), 0; hx(bold(xi)_p), 0, hx(bold(xi)_theta)). $ <eq:adjoint>
The 6×6 restriction to $(R, v)$ drops the last block row and column.

== The physical chart and the scores <app:yardstick>

Each recipe stores its covariance in its own coordinates. To compare them, every covariance is mapped once into a physical error vector of fixed meaning,
$ bold(e) = mat(op("Log")(Rh^top R); v - hat(v); p - hat(p); bg - hat(bg); ba - hat(ba)) in RR^15, wide
  "ordered as" (dth_"body", delta v_"world", delta p_"world", delta bg, delta ba), $ <eq:physical>
and all scores are computed there:
$ mat(op("NEES")_15; "coverage"_z; op("NIS")) = mat(
  bold(e)^top bold(P)_"phys"^(-1) bold(e);
  Pr(|e_z| <= 1.96 sqrt(P_(z z)));
  bold(r)^top (bold(H) bold(P) bold(H)^top + bold(V))^(-1) bold(r)). $ <eq:scores>
`normalized_nees15` divides the time average of $op("NEES")_15$ by 15; a consistent filter sits near 1. NIS is read after the velocity update (3 d.o.f., expect ≈ 3). Pose RMSE is direct world-frame error without alignment.

== The common experiment <app:experiment>

All scenarios use one closed-form, variable-speed circle; truth never calls the estimator.
$ mat(R(t); v(t)) = mat(R_z (0.3 t); R(t) mat(2 + 0.3 sin 0.7 t; 0; 0)), $
with IMU at 40 Hz (interval midpoint sampled and held) and body velocity and heading every 0.2 s. Noise is seeded `mt19937` with an explicit Box–Muller transform; all recipes receive the same samples and the same prior.

#table(columns: (auto, 1fr, 1fr), inset: 5pt, stroke: 0.4pt + luma(170),
  [*scenario*], [*assumption*], [*what to look at*],
  [`matched_heading`], [fixed biases; matched IMU / velocity / heading noise], [baseline errors, NEES, coverage],
  [`heading_dropout`], [heading absent from 35% to 75% of the run], [attitude/bias uncertainty growth and recovery],
  [`pure_dr`], [IMU and full 3-D body velocity only], [global yaw is unobservable],
  [`bias_mismatch`], [+0.12 rad/s gyro-z step at mid-run, zero bias random walk in the filter], [a converged filter can be confidently wrong])

Prior errors are drawn in the physical chart with per-axis standard deviations
$ mat(sigma_theta; sigma_v; sigma_p; sigma_(b g); sigma_(b a)) = mat(0.05 "rad"; 0.1 "m/s"; 0.1 "m"; 0.02 "rad/s"; 0.05 "m/s"^2). $
Four seeds are illustrative, not statistics; time samples within a seed are correlated.

= Symbol to code
#set text(size: 9pt)
#table(columns: (1.15fr, 1.15fr, 0.8fr), inset: 4.5pt, stroke: 0.4pt + luma(170),
  [*symbol / equation*], [*function*], [*file*],
  [$hx(dot)$, $exp$, $op("Log")$, $Gamma_1$, $Gamma_2$ (@eq:skew, @eq:gamma)], [`hat`, `expSO3`, `logSO3`, `gammas`], [`src/kf3d/geometry.cpp`],
  [$op("Ad")$, $op("ad")$, $sum op("ad")^k \/ (k+1)!$ (@eq:adjoint)], [`adjoint`, `ad`, `algebraJacobian`], [`src/kf3d/geometry.cpp`],
  [charts @eq:leftchart, @eq:rightretract, @eq:step4bias, @eq:tgphys], [`retract`, `local`], [`src/kf3d/geometry.cpp`],
  [$bold(J)_"phys"$ @eq:Jphys, $bold(J)_"reset"$ @eq:resetR], [`physicalJacobian`, `resetJacobian`], [`src/kf3d/geometry.cpp`],
  [flows @eq:step1flow, @eq:step2flow; $bold(F)$ @eq:F1, @eq:F2], [`physicalFlow`], [`src/kf3d/propagation.cpp`],
  [$bold(G)$ @eq:G1, $bold(Q)$ @eq:Q, conjugation @eq:conj], [`noiseRoot`, `predict`], [`src/kf3d/propagation.cpp`],
  [TG flow @eq:tgflow, $bold(A)_"TG"$ @eq:Atg, RK4], [`tgFlow`, `dynamics`, `transition`], [`src/kf3d/propagation.cpp`],
  [Joseph @eq:joseph, reset @eq:reset, QR root @eq:sqrt], [`update`, `compress`], [`src/kf3d/filter.cpp`],
  [$bold(H)_A$, $bold(H)_B$ @eq:HAB], [`observeVelocity`], [`src/kf3d/filter.cpp`],
  [cost @eq:cost, GN/LM @eq:gn–@eq:steps, posterior @eq:gnpost], [`update` (non-KF branch), `GnSolver`, `LmSolver`], [`src/kf3d/filter.cpp`],
  [anchor $bold(H) = mat(0, dots, I3)$], [`observeVirtualBias`], [`src/kf3d/filter.cpp`],
  [scenario and study generators], [`events`, `*Csv`], [`examples/teaching.cpp`],
  [analytic / finite-difference oracles], [`geometry`, `zoh`, `gaussian`, `prediction`, `equivalence`, `remainder`], [`tests/math_contracts.cpp`])
#set text(size: 10.5pt)

= Regenerating the figures
```bash
./build/kf_math_tests                       # analytic + finite-difference contracts
./build/kf_teaching results/teaching 4 12   # all CSVs used in this note
python3 docs/handnote/make_figs.py            # fig/*.svg, metrics_mean.csv
typst compile docs/handnote/handnote.typ      # this PDF (Typst 0.15)
```
The figures in this note were drawn from the CSVs with a standard-library SVG script (`make_figs.py`, no third-party packages); no estimator code was changed. The same binary, platform and arguments reproduce the CSV bytes; the four-seed means quoted in captions are rounded from those files.

= Mean metrics, 4 seeds, 12 s
#let rows = csv("metrics_mean.csv")
#table(columns: 7, inset: 3.5pt, stroke: 0.3pt + luma(190), align: (left, left, left, right, right, right, right),
  ..rows.flatten().map(c => text(size: 8pt, c)))
