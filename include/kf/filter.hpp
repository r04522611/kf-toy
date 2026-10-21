#pragma once
namespace kf {
struct State {
  double x = 0, y = 0, yaw = 0, bias = 0;
};
class PoseEstimator {
public:
  virtual ~PoseEstimator() = default;
  virtual void predict(double gyro, double speed, double dt) = 0;
  virtual void observeHeading(double radians) = 0;
  virtual const State &state() const = 0;
};
// Planar teaching model: yaw/bias covariance; x/y are dead-reckoned outputs.
// This does not report a full pose covariance.
class HeadingBiasKF final : public PoseEstimator {
public:
  explicit HeadingBiasKF(bool estimate_bias);
  void predict(double gyro, double speed, double dt) override;
  void observeHeading(double radians) override;
  const State &state() const override { return s_; }

private:
  State s_;
  bool estimate_;
  double p00_ = 0.01, p01_ = 0, p11_ = 0;
};
double wrap(double angle);
} // namespace kf
