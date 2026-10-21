#pragma once
#include <Eigen/Dense>
#include <memory>
#include <string>
namespace kf3d {
using V3 = Eigen::Vector3d;
using M3 = Eigen::Matrix3d;
using Matrix = Eigen::MatrixXd;
using Vector = Eigen::VectorXd;
struct State {
  M3 R = M3::Identity();
  V3 v = V3::Zero(), p = V3::Zero(), bg = V3::Zero(), ba = V3::Zero(),
     g{0, 0, -9.81}, bnu = V3::Zero();
  double t = 0;
};
enum class Recipe { Step1, Step2, Step3, Step4, Step5A, Step5B, C15A, C15B };
enum class Backend { KF, GN, LM };
struct SolverOptions {
  int max_iterations = 15, max_trials = 12;
  double initial_lambda = 1e-3, gradient_tolerance = 1e-8,
         step_tolerance = 1e-8, cost_tolerance = 1e-10, time_budget_seconds = 0;
};
enum class SolverWhitening { CarriedRoot, LegacyEigen };
enum class ClockStepping { Duration, LegacyAbsolute };
struct Config {
  SolverWhitening solver_whitening = SolverWhitening::CarriedRoot;
  ClockStepping clock_stepping = ClockStepping::Duration;
  Eigen::Matrix<double, 12, 1> density_squared =
      Eigen::Matrix<double, 12, 1>::Constant(1e-6);
  V3 position_std = V3::Ones(), wheel_std = V3::Constant(.1),
     lever = V3::Zero();
  double heading_std = .05, antenna_yaw = 0, max_substep = .02,
         virtual_bias_initial_variance = 1e-12;
  SolverOptions solver;
};
struct Diagnostics {
  unsigned long propagation_steps = 0, updates = 0, solver_calls = 0,
                rejected_trials = 0;
  int iterations = 0;
  double nis = 0, initial_cost = 0, final_cost = 0;
  std::string status = "initialized";
};
struct PredictionResult {
  State state;
  Matrix transition, process_covariance, noise_columns;
};
class Formulation {
public:
  virtual ~Formulation() = default;
  virtual int dimension() const = 0;
  virtual State retract(const State &, const Vector &) const = 0;
  virtual Vector local(const State &, const State &) const = 0;
  virtual Matrix physical(const State &) const = 0;
  virtual Matrix differential(const Vector &) const = 0;
  virtual PredictionResult predict(const State &, const V3 &, const V3 &,
                                   double, const Config &) const = 0;
};
std::unique_ptr<Formulation> makeFormulation(Recipe);
// Exact likelihood linearization: Hphysical is the prediction derivative, so
// residual(x + dx) = residual(x) - Hphysical * dx to first order.
struct MeasurementLinearization {
  Vector residual;
  Matrix Hphysical;
};
class MeasurementModel {
public:
  virtual ~MeasurementModel() = default;
  virtual MeasurementLinearization evaluate(const State &) const = 0;
};
class NonlinearSolver {
public:
  virtual ~NonlinearSolver() = default;
  virtual Vector trialStep(const Matrix &information,
                           const Vector &negative_gradient, double alpha,
                           double lambda) const = 0;
};
std::unique_ptr<NonlinearSolver> makeNonlinearSolver(Backend);
class Estimator {
public:
  virtual ~Estimator() = default;
  virtual void initialize(const State &, const Matrix &physical_covariance) = 0;
  virtual void advance(double, const V3 &gyro, const V3 &accel) = 0;
  virtual void observeMeasurement(const MeasurementModel &,
                                  const Matrix &noise_covariance) = 0;
  virtual void observeVirtualBias(const V3 &target,
                                  const M3 &noise_covariance) = 0;
  virtual void observeVelocity(const V3 &) = 0;
  virtual void observePosition(const V3 &) = 0;
  virtual void observeHeading(double) = 0;
  virtual void observeGnss(const V3 &, bool, double) = 0;
  virtual const State &state() const = 0;
  virtual Matrix physicalCovariance() const = 0;
  virtual const Diagnostics &diagnostics() const = 0;
};
std::unique_ptr<Estimator> makeEstimator(Recipe, Backend,
                                         const Config & = Config{});
// Inject derived components while retaining a built-in measurement/output
// compatibility profile. Formulation must preserve its state layout/dimension.
// nullptr selects built-in components; KF rejects a nonlinear solver.
std::unique_ptr<Estimator> makeEstimator(
    Recipe compatibility_recipe, Backend, const Config &,
    std::unique_ptr<Formulation>,
    std::unique_ptr<NonlinearSolver> = nullptr);
Recipe parseRecipe(const std::string &);
Backend parseBackend(const std::string &);
// Public manifold functions support teaching and numerical derivative tests.
M3 hat(const V3 &);
M3 expSO3(const V3 &);
V3 logSO3(const M3 &);
double wrap(double);
State retract(Recipe, const State &, const Vector &);
Vector local(Recipe, const State &, const State &);
Matrix physicalJacobian(Recipe, const State &);
Matrix resetJacobian(Recipe, const Vector &);
} // namespace kf3d
