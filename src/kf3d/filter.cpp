// Standalone port of fixed_reference / fixed_iterated / TG / constrained
// formulas.
#include "internal.hpp"
#include <Eigen/Eigenvalues>
#include <chrono>
#include <cmath>
#include <functional>
#include <stdexcept>
namespace kf3d {
namespace {
class Chart final : public Formulation {
  Recipe r;
public:
  explicit Chart(Recipe x) : r(x) {}
  int dimension() const override { return kf3d::dimension(r); }
  State retract(const State &s, const Vector &d) const override {
    return kf3d::retract(r, s, d);
  }
  Vector local(const State &s, const State &x) const override {
    return kf3d::local(r, s, x);
  }
  Matrix physical(const State &s) const override {
    return physicalJacobian(r, s);
  }
  Matrix differential(const Vector &d) const override {
    return resetJacobian(r, d);
  }
  PredictionResult predict(const State &s, const V3 &w, const V3 &a, double h,
                           const Config &c) const override {
    auto o = kf3d::predict(r, s, w, a, h, c);
    return {o.state, o.F, o.Q, o.noise_columns};
  }
};
using Observation = MeasurementLinearization;
class GnSolver final : public NonlinearSolver {
public:
  Vector trialStep(const Matrix &B, const Vector &g, double alpha,
                   double) const override {
    return alpha * B.ldlt().solve(g);
  }
};
class LmSolver final : public NonlinearSolver {
public:
  Vector trialStep(const Matrix &B, const Vector &g, double,
                   double lambda) const override {
    Matrix M = B;
    M.diagonal().array() += lambda * B.diagonal().array().max(1e-12);
    return M.ldlt().solve(g);
  }
};
using Likelihood = std::function<Observation(const State &)>;
Matrix wheelH(const State &s, int n) {
  Matrix H = Matrix::Zero(3, n);
  H.block<3, 3>(0, 0) = hat(s.R.transpose() * s.v);
  H.block<3, 3>(0, 3) = s.R.transpose();
  return H;
}
double yaw(const State &s, double angle) {
  M3 R = s.R * expSO3(V3(0, 0, angle));
  return atan2(R(1, 0), R(0, 0));
}
Matrix headingH(const State &s, double angle, int n) {
  M3 mount = expSO3(V3(0, 0, angle)), R = s.R * mount;
  double d = R(0, 0) * R(0, 0) + R(1, 0) * R(1, 0);
  if (d < 1e-8)
    throw std::runtime_error("heading singular");
  Matrix H = Matrix::Zero(1, n);
  H.leftCols(3) = Eigen::RowVector3d(-R(1, 0) / d, R(0, 0) / d, 0) *
                  (-R * hat(V3::UnitX())) * mount.transpose();
  return H;
}
Matrix antennaH(const State &s, const V3 &lever, int n) {
  Matrix H = Matrix::Zero(3, n);
  H.block<3, 3>(0, 0) = -s.R * hat(lever);
  H.block<3, 3>(0, 6).setIdentity();
  return H;
}
bool finiteState(const State &s) {
  return s.R.allFinite() && s.v.allFinite() && s.p.allFinite() &&
         s.bg.allFinite() && s.ba.allFinite() && s.bnu.allFinite() &&
         s.g.allFinite() && std::isfinite(s.t);
}
Matrix compress(const Matrix &C) {
  int n = C.rows();
  Eigen::HouseholderQR<Matrix> q(C.transpose());
  Matrix U = q.matrixQR().topRows(n).triangularView<Eigen::Upper>();
  return U.transpose();
}
Matrix root(const Matrix &P) {
  Eigen::LLT<Matrix> f(P);
  if (f.info() != Eigen::Success)
    throw std::runtime_error("covariance not positive definite");
  return f.matrixL();
}
Matrix priorRoot(const Matrix &P) {
  Eigen::LLT<Matrix> f(P);
  if (f.info() == Eigen::Success)
    return f.matrixL();
  Eigen::SelfAdjointEigenSolver<Matrix> e(P);
  double tol = 1e-12 * std::max(1.0, P.norm());
  if (e.info() != Eigen::Success || e.eigenvalues().minCoeff() < -tol)
    throw std::invalid_argument("initial covariance not PSD");
  return e.eigenvectors() *
         e.eigenvalues().cwiseMax(0).cwiseSqrt().asDiagonal();
}
class GaussianEstimator final : public Estimator {
  Recipe recipe;
  Backend backend;
  Config c;
  std::unique_ptr<Formulation> formulation;
  std::unique_ptr<NonlinearSolver> solver;
  State s;
  Matrix P, Lroot;
  Diagnostics diag;
  bool ready = false;
  void require() const {
    if (!ready)
      throw std::logic_error("estimator not initialized");
  }
  void update(const Vector &r, const Matrix &H, const Matrix &V,
              const Likelihood &evaluate) {
    require();
    if (!r.allFinite() || !H.allFinite() || !V.allFinite())
      throw std::invalid_argument("nonfinite measurement");
    int n = P.rows();
    if (H.rows() != r.size() || H.cols() != n || V.rows() != r.size() ||
        V.cols() != r.size() || (V - V.transpose()).norm() > 1e-10)
      throw std::invalid_argument("measurement dimensions/symmetry");
    Matrix U = H * Lroot;
    Matrix S = U * U.transpose() + V;
    Eigen::LDLT<Matrix> sf(S), vf(V);
    if (sf.info() != Eigen::Success || !sf.isPositive() ||
        vf.info() != Eigen::Success || !vf.isPositive())
      throw std::runtime_error("innovation/noise not SPD");
    State next = s;
    Matrix posterior, posteriorRoot;
    Diagnostics stats = diag;
    stats.nis = r.dot(sf.solve(r));
    stats.iterations = 0;
    stats.status = "kf";
    if (backend == Backend::KF) {
      Matrix K = Lroot * U.transpose() *
                 sf.solve(Matrix::Identity(S.rows(), S.cols()));
      Vector d = K * r;
      Matrix A = Matrix::Identity(n, n) - K * H,
             J = formulation->differential(d);
      Matrix columns(n, n + V.rows());
      columns.leftCols(n) = A * Lroot;
      columns.rightCols(V.rows()) = K * root(V);
      posteriorRoot = compress(J * columns);
      posterior = posteriorRoot * posteriorRoot.transpose();
      next = formulation->retract(s, d);
      stats.initial_cost = stats.final_cost = 0;
    } else {
      const State prior = s;
      Matrix L = Lroot;
      if (c.solver_whitening == SolverWhitening::LegacyEigen) {
        Eigen::SelfAdjointEigenSolver<Matrix> eig(P);
        if (eig.info() != Eigen::Success ||
            eig.eigenvalues().minCoeff() < -1e-8)
          throw std::runtime_error("legacy native prior not PSD");
        L = eig.eigenvectors() *
            eig.eigenvalues().cwiseMax(0).cwiseSqrt().asDiagonal();
      }
      const Matrix W = vf.solve(Matrix::Identity(V.rows(), V.cols()));
      Vector z = Vector::Zero(n);
      Observation obs = evaluate(prior);
      auto validate = [&](const Observation &o) {
        if (o.residual.size() != r.size() || o.Hphysical.rows() != r.size() ||
            o.Hphysical.cols() != n || !o.residual.allFinite() ||
            !o.Hphysical.allFinite())
          throw std::runtime_error("invalid likelihood linearization");
      };
      validate(obs);
      auto cost = [&](const Vector &u, const Observation &o) {
        return .5 * (u.squaredNorm() + o.residual.dot(W * o.residual));
      };
      double current = cost(z, obs), lambda = c.solver.initial_lambda;
      stats.initial_cost = current;
      stats.status = "capped";
      Matrix B = Matrix::Identity(n, n);
      const auto started = std::chrono::steady_clock::now();
      for (int it = 0; it < c.solver.max_iterations; ++it) {
        if (c.solver.time_budget_seconds > 0 &&
            std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                          started)
                    .count() > c.solver.time_budget_seconds) {
          stats.status = "time_budget";
          break;
        }
        Matrix J = formulation->differential(L * z),
               A = obs.Hphysical * formulation->physical(next) * J * L;
        B = Matrix::Identity(n, n) + A.transpose() * W * A;
        Vector g = A.transpose() * W * obs.residual - z;
        stats.iterations = it + 1;
        if (g.lpNorm<Eigen::Infinity>() <
            c.solver.gradient_tolerance * (1 + sqrt(2 * current))) {
          stats.status = "gradient";
          break;
        }
        Vector accepted_step;
        bool accepted = false;
        double alpha = 1, improvement = 0;
        for (int tr = 0; tr < c.solver.max_trials; ++tr) {
          Vector step = solver->trialStep(B, g, alpha, lambda);
          if (step.size() != n)
            throw std::runtime_error("custom solver step dimension mismatch");
          Vector nz = z + step, nd = L * nz;
          if (step.allFinite() &&
              nd.head<3>().norm() < 3.141592653589793 - 1e-6) {
            State candidate = formulation->retract(prior, nd);
            try {
              Observation no = evaluate(candidate);
              validate(no);
              double nc = cost(nz, no);
              if (std::isfinite(nc) && nc < current) {
                accepted = true;
                accepted_step = step;
                improvement = current - nc;
                current = nc;
                z = nz;
                next = candidate;
                obs = no;
                lambda = std::max(1e-12, lambda / 3);
                break;
              }
            } catch (const std::runtime_error &) {
            }
          }
          ++stats.rejected_trials;
          alpha *= .5;
          lambda = std::min(1e12, lambda * 10);
        }
        if (!accepted) {
          stats.status = "stalled";
          break;
        }
        if (accepted_step.norm() < c.solver.step_tolerance * (1 + z.norm())) {
          stats.status = "step";
          break;
        }
        if (improvement < c.solver.cost_tolerance * (1 + current)) {
          stats.status = "cost";
          break;
        }
      }
      Matrix J = formulation->differential(L * z),
             A = obs.Hphysical * formulation->physical(next) * J * L;
      if (c.solver.max_iterations != 1)
        B = Matrix::Identity(n, n) + A.transpose() * W * A;
      Matrix Broot = root(B),
             inverse = Broot.triangularView<Eigen::Lower>().solve(
                 Matrix::Identity(n, n));
      posteriorRoot = J * L * inverse.transpose();
      posterior = posteriorRoot * posteriorRoot.transpose();
      stats.final_cost = current;
      ++stats.solver_calls;
      if (current > stats.initial_cost + 1e-9)
        throw std::runtime_error("nonlinear cost increase");
    }
    posterior = (.5 * (posterior + posterior.transpose())).eval();
    if (!finiteState(next) || !posterior.allFinite())
      throw std::runtime_error("nonfinite posterior");
    s = next;
    P = posterior;
    Lroot = posteriorRoot;
    ++stats.updates;
    diag = stats;
  }

public:
  GaussianEstimator(Recipe r, Backend b, Config config,
                    std::unique_ptr<Formulation> custom_formulation = nullptr,
                    std::unique_ptr<NonlinearSolver> custom_solver = nullptr)
      : recipe(r), backend(b), c(config),
        formulation(custom_formulation ? std::move(custom_formulation)
                                       : makeFormulation(r)),
        solver(custom_solver ? std::move(custom_solver)
                             : (b == Backend::KF ? nullptr : makeNonlinearSolver(b))) {
    if (formulation->dimension() != kf3d::dimension(r))
      throw std::invalid_argument("formulation incompatible with recipe dimension");
    if (b == Backend::KF && solver)
      throw std::invalid_argument("single-pass KF does not consume nonlinear solver");
    if (!c.density_squared.allFinite() ||
        (c.density_squared.array() < 0).any() || !c.position_std.allFinite() ||
        (c.position_std.array() <= 0).any() || !c.wheel_std.allFinite() ||
        (c.wheel_std.array() <= 0).any() || !(c.heading_std > 0) ||
        !std::isfinite(c.heading_std) || !(c.max_substep > 0) ||
        !std::isfinite(c.max_substep) || c.solver.max_iterations < 1 ||
        c.solver.max_trials < 1 || !c.lever.allFinite() ||
        !std::isfinite(c.antenna_yaw) ||
        !(c.virtual_bias_initial_variance >= 0) ||
        !std::isfinite(c.virtual_bias_initial_variance) ||
        !(c.solver.initial_lambda > 0) ||
        !std::isfinite(c.solver.initial_lambda))
      throw std::invalid_argument("invalid filter configuration");
    if (static_cast<int>(r) < 0 || static_cast<int>(r) > 7 ||
        static_cast<int>(b) < 0 || static_cast<int>(b) > 2)
      throw std::invalid_argument("invalid recipe/backend");
    for (double value : {c.solver.gradient_tolerance, c.solver.step_tolerance,
                         c.solver.cost_tolerance, c.solver.time_budget_seconds})
      if (!std::isfinite(value) || value < 0)
        throw std::invalid_argument("invalid solver tolerance/budget");
  }
  void initialize(const State &x, const Matrix &C) override {
    int n = formulation->dimension();
    if (!finiteState(x) ||
        (x.R.transpose() * x.R - M3::Identity()).norm() > 1e-6 ||
        x.R.determinant() < 0)
      throw std::invalid_argument("invalid initial state");
    if (C.rows() != C.cols() || (C.rows() != 15 && C.rows() != n) ||
        !C.allFinite() || (C - C.transpose()).norm() > 1e-9)
      throw std::invalid_argument("invalid covariance shape/symmetry");
    if (!isTG(recipe) && x.bnu.norm() != 0)
      throw std::invalid_argument("non-TG state cannot contain virtual bias");
    Matrix cp = Matrix::Zero(n, n);
    cp.topLeftCorner(15, 15) = C.topLeftCorner(15, 15);
    if (n == 18) {
      if (C.rows() == 18)
        cp = C;
      else
        cp.bottomRightCorner(3, 3) =
            c.virtual_bias_initial_variance * M3::Identity();
    }
    Matrix cpRoot = priorRoot(cp);
    Matrix J = formulation->physical(x).inverse(), np = J * cp * J.transpose();
    s = x;
    if (!isTG(recipe))
      s.bnu.setZero();
    P = np;
    Lroot = J * cpRoot;
    ready = true;
    diag = Diagnostics{};
  }
  void advance(double t, const V3 &w, const V3 &a) override {
    require();
    if (!std::isfinite(t) || t < s.t || !w.allFinite() || !a.allFinite())
      throw std::invalid_argument("out of order/nonfinite IMU");
    State next = s;
    Matrix covariance = P, nextRoot = Lroot;
    unsigned long count = 0;
    double duration = t - s.t, elapsed = 0;
    while (c.clock_stepping == ClockStepping::LegacyAbsolute
               ? t - next.t > 1e-9
               : duration - elapsed > 1e-12) {
      double next_time = std::min(t, next.t + c.max_substep);
      double h = c.clock_stepping == ClockStepping::LegacyAbsolute
                     ? next_time - next.t
                     : std::min(c.max_substep, duration - elapsed);
      auto o = formulation->predict(next, w, a, h, c);
      const int n = nextRoot.rows();
      if (o.transition.rows() != n || o.transition.cols() != n ||
          o.noise_columns.rows() != n || !o.transition.allFinite() ||
          !o.noise_columns.allFinite() || !finiteState(o.state))
        throw std::runtime_error("invalid formulation prediction shape/value");
      Matrix columns(nextRoot.rows(), nextRoot.cols() + o.noise_columns.cols());
      columns.leftCols(nextRoot.cols()) = o.transition * nextRoot;
      columns.rightCols(o.noise_columns.cols()) = o.noise_columns;
      nextRoot = compress(columns);
      covariance = nextRoot * nextRoot.transpose();
      covariance = (.5 * (covariance + covariance.transpose())).eval();
      next = o.state;
      if (c.clock_stepping == ClockStepping::LegacyAbsolute)
        next.t = next_time;
      elapsed += h;
      ++count;
    }
    next.t = t;
    if (!finiteState(next) || !covariance.allFinite())
      throw std::runtime_error("nonfinite propagation");
    s = next;
    P = covariance;
    Lroot = nextRoot;
    diag.propagation_steps += count;
  }
  void observeMeasurement(const MeasurementModel &model,
                          const Matrix &V) override {
    require();
    Likelihood f = [&](const State &x) { return model.evaluate(x); };
    auto o = f(s);
    if (o.Hphysical.rows() != o.residual.size() ||
        o.Hphysical.cols() != P.rows() || V.rows() != o.residual.size() ||
        V.cols() != V.rows())
      throw std::invalid_argument("measurement dimensions");
    update(o.residual, o.Hphysical * formulation->physical(s), V, f);
  }
  void observeVirtualBias(const V3 &z, const M3 &V) override {
    require();
    if (!isTG(recipe))
      throw std::invalid_argument("virtual bias constraint requires TG");
    Likelihood f = [=](const State &x) {
      Matrix H = Matrix::Zero(3, 18);
      H.rightCols(3).setIdentity();
      return Observation{V3(z - x.bnu), H};
    };
    auto o = f(s);
    update(o.residual, o.Hphysical * formulation->physical(s), V, f);
  }
  void observeVelocity(const V3 &z) override {
    require();
    int n = P.rows();
    M3 R = s.R;
    bool world =
        isTG(recipe) || recipe == Recipe::C15A || recipe == Recipe::C15B;
    Matrix V = c.wheel_std.array().square().matrix().asDiagonal();
    Likelihood eval = [=](const State &x) {
      return Observation{V3(z - x.R.transpose() * x.v), wheelH(x, n)};
    };
    Vector r = z - s.R.transpose() * s.v;
    Matrix H = wheelH(s, n) * formulation->physical(s);
    if (world) {
      r = R * z - s.v;
      V = (R * V * R.transpose()).eval();
      H = Matrix::Zero(3, n);
      H.block<3, 3>(0, 3).setIdentity();
      if ((recipe == Recipe::Step5B || recipe == Recipe::C15B) &&
          backend == Backend::KF)
        H.block<3, 3>(0, 0) = .5 * hat(r);
      eval = [=](const State &x) {
        return Observation{V3(R * (z - x.R.transpose() * x.v)),
                           Matrix(R * wheelH(x, n))};
      };
    }
    update(r, H, V, eval);
  }
  void observePosition(const V3 &z) override { observeGnss(z, false, 0); }
  void observeHeading(double z) override {
    require();
    int n = P.rows();
    Likelihood f = [=](const State &x) {
      Vector r(1);
      r[0] = wrap(z - yaw(x, c.antenna_yaw));
      return Observation{r, headingH(x, c.antenna_yaw, n)};
    };
    auto o = f(s);
    Matrix V(1, 1);
    V(0, 0) = c.heading_std * c.heading_std;
    update(o.residual, o.Hphysical * formulation->physical(s), V, f);
  }
  void observeGnss(const V3 &z, bool valid, double heading) override {
    require();
    int n = P.rows(), m = valid ? 4 : 3;
    Likelihood f = [=](const State &x) {
      Observation o;
      o.residual = Vector::Zero(m);
      o.Hphysical = Matrix::Zero(m, n);
      o.residual.head<3>() = z - x.p - x.R * c.lever;
      o.Hphysical.topRows(3) = antennaH(x, c.lever, n);
      if (valid) {
        o.residual[3] = wrap(heading - yaw(x, c.antenna_yaw));
        o.Hphysical.row(3) = headingH(x, c.antenna_yaw, n);
      }
      return o;
    };
    auto o = f(s);
    Matrix V = Matrix::Zero(m, m);
    V.topLeftCorner(3, 3) =
        c.position_std.array().square().matrix().asDiagonal();
    if (valid)
      V(3, 3) = c.heading_std * c.heading_std;
    update(o.residual, o.Hphysical * formulation->physical(s), V, f);
  }
  const State &state() const override {
    require();
    return s;
  }
  Matrix physicalCovariance() const override {
    require();
    Matrix J = formulation->physical(s);
    Matrix L = J * Lroot;
    return L * L.transpose();
  }
  const Diagnostics &diagnostics() const override { return diag; }
};
} // namespace
std::unique_ptr<NonlinearSolver> makeNonlinearSolver(Backend b) {
  if (b == Backend::GN)
    return std::make_unique<GnSolver>();
  if (b == Backend::LM)
    return std::make_unique<LmSolver>();
  throw std::invalid_argument("KF is not a nonlinear solver");
}
std::unique_ptr<Formulation> makeFormulation(Recipe r) {
  return std::make_unique<Chart>(r);
}
std::unique_ptr<Estimator> makeEstimator(Recipe r, Backend b, const Config &c) {
  return std::make_unique<GaussianEstimator>(r, b, c);
}
std::unique_ptr<Estimator> makeEstimator(
    Recipe r, Backend b, const Config &c,
    std::unique_ptr<Formulation> formulation,
    std::unique_ptr<NonlinearSolver> solver) {
  return std::make_unique<GaussianEstimator>(r, b, c, std::move(formulation),
                                             std::move(solver));
}
} // namespace kf3d
