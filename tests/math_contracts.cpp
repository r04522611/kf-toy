// Mathematical contracts inspired by the original research audit; independently
// expressed against the public kf3d API, with analytic/finite-difference
// oracles.
#include "kf3d/filter.hpp"
#include <Eigen/Eigenvalues>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace kf3d;
namespace {
const std::array<Recipe, 8> recipes{
    Recipe::Step1,  Recipe::Step2,  Recipe::Step3, Recipe::Step4,
    Recipe::Step5A, Recipe::Step5B, Recipe::C15A,  Recipe::C15B};
int dim(Recipe r) {
  return r == Recipe::Step5A || r == Recipe::Step5B ? 18 : 15;
}
void require(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
}
void near(double value, double bound, const char *s) {
  if (!(value <= bound)) {
    std::cerr << s << ": " << value << " > " << bound << '\n';
    throw std::runtime_error(s);
  }
}
State general() {
  State s;
  s.R = expSO3(V3(.2, -.3, .4));
  s.v = V3(2, -1, .4);
  s.p = V3(4, 3, -2);
  s.bg = V3(.01, -.02, .03);
  s.ba = V3(.1, .2, -.1);
  return s;
}
Vector perturbation(int n, double scale) {
  Vector q(n);
  for (int j = 0; j < n; ++j)
    q[j] = scale * std::sin(.7 * (j + 1));
  return q;
}
void geometry() {
  const double eps = 1e-6;
  double local_error = 0, physical_error = 0, reset_error = 0;
  for (auto r : recipes) {
    int n = dim(r);
    State s = general();
    Vector q = perturbation(n, r == Recipe::Step1 ? 1e-5 : .04);
    State x = retract(r, s, q);
    local_error = std::max(local_error, (local(r, s, x) - q).norm());
    Matrix G = physicalJacobian(r, s), J = resetJacobian(r, q),
           Gfd = Matrix::Zero(n, n), Jfd(n, n);
    for (int j = 0; j < n; ++j) {
      Vector e = Vector::Zero(n);
      e[j] = eps;
      State plus = retract(r, s, e), minus = retract(r, s, -e);
      Vector gp = Vector::Zero(n), gm = Vector::Zero(n);
      gp.head(15) = local(Recipe::Step2, s, plus);
      gm.head(15) = local(Recipe::Step2, s, minus);
      if (n == 18) {
        gp.tail(3) = plus.bnu - s.bnu;
        gm.tail(3) = minus.bnu - s.bnu;
      }
      Gfd.col(j) = (gp - gm) / (2 * eps);
      Jfd.col(j) = (local(r, x, retract(r, s, q + e)) -
                    local(r, x, retract(r, s, q - e))) /
                   (2 * eps);
    }
    physical_error = std::max(physical_error, (G - Gfd).cwiseAbs().maxCoeff());
    reset_error = std::max(reset_error, (J - Jfd).cwiseAbs().maxCoeff());
    if (r == Recipe::C15A || r == Recipe::C15B)
      near(x.bnu.norm(), 1e-15, "C15 structural bnu constraint");
  }
  near(local_error, 1e-9, "local/retract inverse");
  near(physical_error, 2e-6, "physical tangent finite difference");
  near(reset_error, 2e-6, "reset finite difference");
  std::cout << "geometry inverse=" << local_error
            << " physical_fd=" << physical_error << " reset_fd=" << reset_error
            << '\n';
}
void constrainedEmbedding() {
  double worst = 0;
  for (int k = 1; k <= 12; ++k) {
    State s = general();
    s.R = expSO3(V3(.04 * k, -.03, .2));
    s.p *= k;
    Vector q = perturbation(15, .03 * k / 12);
    Matrix A = Matrix::Zero(9, 9), Ad = Matrix::Zero(9, 9);
    for (int j = 0; j < 3; ++j) {
      A.block(3 * j, 3 * j, 3, 3) = hat(q.head(3));
      Ad.block(3 * j, 3 * j, 3, 3) = s.R;
    }
    A.block(3, 0, 3, 3) = hat(q.segment(3, 3));
    A.block(6, 0, 3, 3) = hat(q.segment(6, 3));
    Ad.block(3, 0, 3, 3) = hat(s.v) * s.R;
    Ad.block(6, 0, 3, 3) = hat(s.p) * s.R;
    Matrix J = Matrix::Identity(9, 9), term = J;
    for (int j = 1; j <= 35; ++j) {
      term = term * (-A) / (j + 1.);
      J += term;
    }
    Vector db = Vector::Zero(9);
    db.head(6) = q.tail(6);
    Vector native = Vector::Zero(18);
    native.head(9) = q.head(9);
    native.tail(9) = J.partialPivLu().solve(Ad * db);
    State lifted = retract(Recipe::Step5A, s, native),
          reduced = retract(Recipe::C15A, s, q);
    double error =
        local(Recipe::Step2, reduced, lifted).norm() + lifted.bnu.norm();
    worst = std::max(worst, error);
  }
  near(worst, 1e-9,
       "nonlinear constrained TG embedding equals reduced finite map");
  std::cout << "constrained_embedding=" << worst << '\n';
}
void edgeCases() {
  for (double angle : {1e-10, .5, 3.141592653589793 - 1e-7}) {
    V3 q = V3(1, 2, -3).normalized() * angle;
    near((expSO3(logSO3(expSO3(q))) - expSO3(q)).norm(), 1e-7,
         "SO3 log near pi round trip");
  }
  near(std::abs(wrap(3.141592653589793 + .03) + 3.141592653589793 - .03), 1e-12,
       "heading wrap boundary");
  auto e = makeEstimator(Recipe::Step2, Backend::LM);
  State s;
  Matrix P = Matrix::Identity(15, 15);
  e->initialize(s, P);
  e->advance(.01, V3::Zero(), V3(0, 0, 9.81));
  State before = e->state();
  Matrix beforeP = e->physicalCovariance();
  bool threw = false;
  try {
    e->advance(.005, V3::Zero(), V3::Zero());
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "out of order input must reject");
  near(local(Recipe::Step2, before, e->state()).norm() +
           (beforeP - e->physicalCovariance()).norm(),
       1e-15, "out of order input atomicity");
  Matrix invalid = P;
  invalid(0, 0) = -1;
  threw = false;
  try {
    e->initialize(s, invalid);
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "negative covariance must reject");
  near(local(Recipe::Step2, before, e->state()).norm() +
           (beforeP - e->physicalCovariance()).norm(),
       1e-15, "invalid initialization atomicity");
}
void zoh() {
  Config c;
  c.max_substep = 1.;
  c.density_squared.setZero();
  auto a = makeEstimator(Recipe::Step1, Backend::KF, c),
       b = makeEstimator(Recipe::Step2, Backend::KF, c);
  State s;
  Matrix P = Matrix::Identity(15, 15) * 1e-4;
  a->initialize(s, P);
  b->initialize(s, P);
  // Analytic rotating body force: integral Rz(wt) e_x dt, including position.
  const double h = .5, w = 2., f = 3.;
  V3 gyro(0, 0, w), acc(f, 0, 9.81);
  a->advance(h, gyro, acc);
  b->advance(h, gyro, acc);
  V3 v(f * std::sin(w * h) / w, f * (1 - std::cos(w * h)) / w, 0),
      p(f * (1 - std::cos(w * h)) / (w * w),
        f * (h / w - std::sin(w * h) / (w * w)), 0);
  near((b->state().v - v).norm(), 1e-9,
       "ZOH analytic rotating acceleration velocity");
  near((b->state().p - p).norm(), 1e-9,
       "ZOH analytic rotating acceleration position");
  require((a->state().v - v).norm() > .1,
          "Euler/ZOH example must distinguish approximations");
}
void gaussian() {
  for (auto r : recipes)
    for (auto backend : {Backend::KF, Backend::GN, Backend::LM}) {
      Config c;
      c.position_std = V3::Constant(.5);
      auto e = makeEstimator(r, backend, c);
      State s;
      Matrix P = Matrix::Zero(15, 15);
      P.block(6, 6, 3, 3) = .4 * M3::Identity();
      e->initialize(s, P);
      e->observePosition(V3(2, -1, .5));
      const double gain = .4 / (.4 + .25);
      near((e->state().p - gain * V3(2, -1, .5)).norm(), 2e-5,
           "linear Bayesian posterior mean");
      near((e->physicalCovariance().block(6, 6, 3, 3) -
            Matrix::Identity(3, 3) * (.4 * .25 / .65))
               .norm(),
           2e-5,
           "linear Bayesian posterior covariance; no repeated likelihood");
    }
}
void prediction() {
  Config c;
  const double h = .003, eps = 1e-6;
  State s = general();
  V3 w(.1, -.2, .3), a(.4, .2, 9.7);
  double worst = 0;
  for (auto r : recipes) {
    auto f = makeFormulation(r);
    auto p = f->predict(s, w, a, h, c);
    Matrix fd(dim(r), dim(r));
    for (int j = 0; j < dim(r); ++j) {
      Vector e = Vector::Zero(dim(r));
      e[j] = eps;
      auto plus = f->predict(f->retract(s, e), w, a, h, c),
           minus = f->predict(f->retract(s, -e), w, a, h, c);
      fd.col(j) =
          (f->local(p.state, plus.state) - f->local(p.state, minus.state)) /
          (2 * eps);
    }
    double e = (fd - p.transition).cwiseAbs().maxCoeff();
    worst = std::max(worst, e);
    near(e, 2e-6, "prediction transition finite difference");
    near((p.noise_columns * p.noise_columns.transpose() - p.process_covariance)
             .norm(),
         1e-12, "process root quadrature consistency");
    Eigen::SelfAdjointEigenSolver<Matrix> eig(p.process_covariance);
    require(eig.eigenvalues().minCoeff() > -1e-12, "process covariance PSD");
  }
  // Analytic integrated white acceleration covariance; independent of library's
  // quadrature implementation, including the crucial p-v cross covariance.
  c.density_squared.setZero();
  c.density_squared.segment(3, 3).setConstant(.003);
  State zero;
  auto p = makeFormulation(Recipe::Step2)
               ->predict(zero, V3::Zero(), V3(0, 0, 9.81), .2, c);
  near((p.process_covariance.block(3, 3, 3, 3) - .003 * .2 * M3::Identity())
           .norm(),
       1e-13, "integrated white acceleration vv");
  near((p.process_covariance.block(6, 3, 3, 3) -
        .003 * .2 * .2 / 2 * M3::Identity())
           .norm(),
       1e-13, "integrated white acceleration pv");
  near((p.process_covariance.block(6, 6, 3, 3) -
        .003 * .2 * .2 * .2 / 3 * M3::Identity())
           .norm(),
       1e-13, "integrated white acceleration pp");
  std::cout << "prediction_fd=" << worst << '\n';
}
void anchor() {
  Config c;
  State s;
  s.bnu = V3(.2, -.1, .3);
  Matrix P = Matrix::Identity(18, 18) * .1;
  auto e = makeEstimator(Recipe::Step5A, Backend::KF, c);
  e->initialize(s, P);
  e->observeVirtualBias(V3::Zero(), M3::Identity() * .2);
  near((e->state().bnu - (2. / 3) * s.bnu).norm(), 1e-12,
       "finite noise anchor mean");
  require(e->state().bnu.norm() > 0, "soft anchor not hard clamp");
  auto reduced = makeEstimator(Recipe::C15A, Backend::KF, c);
  reduced->initialize(State{}, Matrix::Identity(15, 15));
  bool threw = false;
  try {
    reduced->observeVirtualBias(V3::Zero(), M3::Identity());
  } catch (const std::exception &) {
    threw = true;
  }
  require(threw, "reduced state must reject latent anchor");
}
void equivalence() {
  Config c;
  c.max_substep = .01;
  Matrix P = Matrix::Identity(15, 15) * .01;
  State s = general();
  auto run = [&](Recipe ra, Recipe rb, Backend backend) {
    auto a = makeEstimator(ra, backend, c), b = makeEstimator(rb, backend, c);
    a->initialize(s, P);
    b->initialize(s, P);
    for (int i = 1; i <= 30; ++i) {
      V3 w(.1, .02, .3), f(.1, .2, 9.8);
      a->advance(.01 * i, w, f);
      b->advance(.01 * i, w, f);
      if (i % 5 == 0) {
        V3 z(1.8, .1, 0);
        a->observeVelocity(z);
        b->observeVelocity(z);
        a->observeHeading(.3);
        b->observeHeading(.3);
      }
    }
    near(local(Recipe::Step2, a->state(), b->state()).norm() +
             (a->state().bnu - b->state().bnu).norm(),
         1e-8, "recipe state equivalence");
    near((a->physicalCovariance() - b->physicalCovariance()).norm(), 1e-8,
         "recipe covariance equivalence");
  };
  for (auto backend : {Backend::KF, Backend::GN, Backend::LM})
    run(Recipe::Step3, Recipe::C15A, backend);
  for (auto backend : {Backend::GN, Backend::LM}) {
    run(Recipe::C15A, Recipe::C15B, backend);
    run(Recipe::Step5A, Recipe::Step5B, backend);
  }
}
void nonlinear() {
  for (auto backend : {Backend::GN, Backend::LM}) {
    Config c;
    c.wheel_std = V3::Constant(.05);
    State s;
    s.R = expSO3(V3(0, 0, 1.2));
    s.v = V3(2, 0, 0);
    Matrix P = Matrix::Identity(15, 15) * 1e-4;
    P(2, 2) = 2;
    auto e = makeEstimator(Recipe::C15A, backend, c);
    e->initialize(s, P);
    e->observeVelocity(V3(2, 0, 0));
    auto d = e->diagnostics();
    require(d.final_cost <= d.initial_cost + 1e-8,
            "GN/LM accepted cost must decrease");
    require(std::abs(logSO3(e->state().R).z()) < .1,
            "nonlinear heading recovery from body velocity");
    Eigen::SelfAdjointEigenSolver<Matrix> eig(e->physicalCovariance());
    require(eig.info() == Eigen::Success &&
                eig.eigenvalues().minCoeff() > -1e-10,
            "posterior PSD");
  }
}
void remainder() {
  double max_ordinary = 0, max_midpoint = 0;
  for (int direction = 1; direction <= 7; ++direction) {
    State s = general();
    Vector q = Vector::Zero(15);
    q.head(6) = perturbation(6, .18 / direction);
    double prev_o = 0, prev_m = 0;
    for (int k = 0; k < 5; ++k) {
      double a = std::pow(.5, k);
      State x = retract(Recipe::C15A, s, a * q);
      V3 z = x.R.transpose() * x.v, r = s.R * z - s.v;
      double o = (r - a * q.segment(3, 3)).norm(),
             m = (r - a * q.segment(3, 3) - .5 * hat(r) * (a * q.head(3)))
                     .norm();
      if (k) {
        double ro = prev_o / o, rm = prev_m / m;
        require(ro > 3.7 && ro < 4.3, "ordinary remainder quadratic");
        require(rm > 7.6 && rm < 8.4, "midpoint remainder cubic");
        max_ordinary = std::max(max_ordinary, std::abs(ro - 4));
        max_midpoint = std::max(max_midpoint, std::abs(rm - 8));
      }
      prev_o = o;
      prev_m = m;
    }
  }
  std::cout << "remainder halving ratio deviations ordinary=" << max_ordinary
            << " midpoint=" << max_midpoint << '\n';
}
} // namespace
int main() {
  try {
    geometry();
    constrainedEmbedding();
    edgeCases();
    zoh();
    gaussian();
    prediction();
    anchor();
    equivalence();
    nonlinear();
    remainder();
    std::cout << "All mathematical contracts passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
