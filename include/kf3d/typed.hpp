#pragma once
#include "kf3d/filter.hpp"
#include <stdexcept>
#include <type_traits>

namespace kf3d {
// Public schemas do not overload the meaning of the last three coordinates.
struct PhysicalState {
  M3 R = M3::Identity();
  V3 v = V3::Zero(), p = V3::Zero(), bg = V3::Zero(), ba = V3::Zero();
  double t = 0;
};
struct TangentGroupState {
  PhysicalState physical;
  V3 virtual_bias = V3::Zero();
};

template <Recipe R> struct RecipeTraits {
  static constexpr bool augmented = R == Recipe::Step5A || R == Recipe::Step5B;
  static constexpr int dimension = augmented ? 18 : 15;
  using StateType =
      std::conditional_t<augmented, TangentGroupState, PhysicalState>;
  using Covariance = Eigen::Matrix<double, dimension, dimension>;
};

// Typed facade: compile-time state/covariance contracts; runtime virtual core.
// The core currently uses bounded-small dynamic Eigen buffers, not a zero-heap
// implementation. This facade does not make a realtime allocation guarantee.
template <Recipe R> class Filter {
public:
  using Traits = RecipeTraits<R>;
  using StateType = typename Traits::StateType;
  using Covariance = typename Traits::Covariance;
  explicit Filter(Backend backend = Backend::KF,
                  const Config &config = Config{},
                  V3 fixed_gravity = V3(0, 0, -9.81))
      : engine_(makeEstimator(R, backend, config)), gravity_(fixed_gravity) {
    if (!gravity_.allFinite())
      throw std::invalid_argument("nonfinite fixed gravity");
  }
  void initialize(const StateType &x, const Covariance &covariance) {
    State internal;
    const PhysicalState &physical = physicalPart(x);
    internal.R = physical.R;
    internal.v = physical.v;
    internal.p = physical.p;
    internal.bg = physical.bg;
    internal.ba = physical.ba;
    internal.t = physical.t;
    internal.g = gravity_;
    if constexpr (Traits::augmented)
      internal.bnu = x.virtual_bias;
    engine_->initialize(internal, covariance);
  }
  void advance(double t, const V3 &gyro, const V3 &accel) {
    engine_->advance(t, gyro, accel);
  }
  void observeVelocity(const V3 &z) { engine_->observeVelocity(z); }
  void observePosition(const V3 &z) { engine_->observePosition(z); }
  void observeHeading(double z) { engine_->observeHeading(z); }
  void observeGnss(const V3 &z, bool valid, double heading) {
    engine_->observeGnss(z, valid, heading);
  }
  void observeMeasurement(const MeasurementModel &model, const Matrix &noise) {
    engine_->observeMeasurement(model, noise);
  }
  template <bool Enabled = Traits::augmented,
            std::enable_if_t<Enabled, int> = 0>
  void observeVirtualBias(const V3 &target, const M3 &noise) {
    engine_->observeVirtualBias(target, noise);
  }
  StateType state() const {
    const State &s = engine_->state();
    PhysicalState p{s.R, s.v, s.p, s.bg, s.ba, s.t};
    if constexpr (Traits::augmented)
      return TangentGroupState{p, s.bnu};
    else
      return p;
  }
  Covariance covariance() const { return engine_->physicalCovariance(); }
  const Diagnostics &diagnostics() const { return engine_->diagnostics(); }

private:
  static const PhysicalState &physicalPart(const PhysicalState &x) { return x; }
  static const PhysicalState &physicalPart(const TangentGroupState &x) {
    return x.physical;
  }
  std::unique_ptr<Estimator> engine_;
  V3 gravity_;
};
} // namespace kf3d
