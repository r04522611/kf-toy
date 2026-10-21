#include "kf/filter.hpp"
#include <cmath>
#include <stdexcept>
namespace kf {
double wrap(double a) { return std::atan2(std::sin(a), std::cos(a)); }
HeadingBiasKF::HeadingBiasKF(bool e) : estimate_(e), p11_(e ? 0.01 : 0) {}
void HeadingBiasKF::predict(double gyro, double speed, double dt) {
  if (!std::isfinite(dt) || dt < 0 || !std::isfinite(gyro) ||
      !std::isfinite(speed))
    throw std::runtime_error("invalid prediction");
  const double w = gyro - s_.bias, half = w * dt / 2;
  const double sinc =
      std::abs(half) < 1e-8 ? 1 - half * half / 6 : std::sin(half) / half;
  s_.x += speed * dt * sinc * std::cos(s_.yaw + half);
  s_.y += speed * dt * sinc * std::sin(s_.yaw + half);
  s_.yaw = wrap(s_.yaw + w * dt);
  // Continuous white gyro density and bias RW, integrated exactly for this
  // linear subsystem.
  const double qg = 1e-6, qb = estimate_ ? 1e-8 : 0;
  p00_ += -2 * dt * p01_ + dt * dt * p11_ + qg * dt + qb * dt * dt * dt / 3;
  p01_ += -dt * p11_ - qb * dt * dt / 2;
  p11_ += qb * dt;
}
void HeadingBiasKF::observeHeading(double z) {
  if (!std::isfinite(z))
    throw std::runtime_error("invalid heading");
  const double v = 0.0001, S = p00_ + v, k0 = p00_ / S, k1 = p01_ / S;
  const double residual = wrap(z - s_.yaw);
  s_.yaw = wrap(s_.yaw + k0 * residual);
  s_.bias += k1 * residual;
  // Joseph update, H=[1,0].
  const double a = 1 - k0, b = -k1;
  const double n00 = a * a * p00_ + k0 * k0 * v;
  const double n01 = a * (b * p00_ + p01_) + k0 * k1 * v;
  const double n11 = b * b * p00_ + 2 * b * p01_ + p11_ + k1 * k1 * v;
  p00_ = n00;
  p01_ = n01;
  p11_ = n11;
}
} // namespace kf
