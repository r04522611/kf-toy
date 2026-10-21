// Standalone formula port of research_filter.cc and TG_EqF/tg_filter.cc.
#include "internal.hpp"
#include <cmath>
namespace kf3d {
static Prediction physicalFlow(const State &s, const V3 &gyro, const V3 &accel,
                               double h, bool euler) {
  Prediction o{s, Matrix::Identity(15, 15), Matrix::Zero(15, 15), Matrix{}};
  V3 w = gyro - s.bg, a = accel - s.ba;
  auto g = gammas(w * h, a);
  V3 dv = euler ? V3(h * a) : V3(h * g.one * a),
     dp = euler ? V3(.5 * h * h * a) : V3(h * h * g.two * a);
  o.state.R = s.R * expSO3(w * h);
  o.state.v += s.g * h + s.R * dv;
  o.state.p += s.v * h + .5 * s.g * h * h + s.R * dp;
  o.state.t += h;
  auto &F = o.F;
  F.block<3, 3>(0, 0) = expSO3(-w * h);
  F.block<3, 3>(0, 9) = -h * gammas(-w * h).one;
  F.block<3, 3>(3, 0) = -s.R * hat(dv);
  F.block<3, 3>(6, 0) = -s.R * hat(dp);
  F.block<3, 3>(6, 3) = h * M3::Identity();
  F.block<3, 3>(3, 12) =
      -s.R * (euler ? M3(h * M3::Identity()) : M3(h * g.one));
  F.block<3, 3>(6, 12) =
      -s.R * (euler ? M3(.5 * h * h * M3::Identity()) : M3(h * h * g.two));
  if (!euler) {
    F.block<3, 3>(3, 9) = -s.R * h * h * g.d1;
    F.block<3, 3>(6, 9) = -s.R * h * h * h * g.d2;
  }
  return o;
}
static Matrix noiseRoot(const State &s, const Config &c, bool tg) {
  int n = tg ? 18 : 15;
  Matrix G = Matrix::Zero(n, 12);
  if (tg) {
    G.block(0, 0, 9, 6) = -adjoint(s).leftCols(6);
    G.block(9, 6, 9, 6) = adjoint(s).leftCols(6);
  } else {
    G.block<3, 3>(0, 0) = -M3::Identity();
    G.block<3, 3>(3, 3) = -s.R;
    G.block<3, 3>(9, 6).setIdentity();
    G.block<3, 3>(12, 9).setIdentity();
  }
  return G * c.density_squared.cwiseSqrt().asDiagonal();
}
static State tgFlow(const State &s, const V3 &w, const V3 &a, double h) {
  auto o = physicalFlow(s, w, a, h, false).state;
  o.p -= s.R * (h * gammas((w - s.bg) * h).one * s.bnu);
  return o;
}
static Matrix dynamics(const State &s, const V3 &gyro, const V3 &accel) {
  Vector w(9);
  w << gyro - s.bg, accel - s.ba, -s.bnu;
  w = adjoint(s) * w;
  w.tail<3>() += s.v;
  w.segment<3>(3) += s.g;
  Matrix A = Matrix::Zero(18, 18);
  A.block<3, 3>(3, 0) = hat(s.g);
  A.block<3, 3>(6, 3).setIdentity();
  A.topRightCorner(9, 9) = -Matrix::Identity(9, 9);
  A.bottomRightCorner(9, 9) = ad(w);
  return A;
}
static Matrix transition(const Matrix &A, const Matrix &B, const Matrix &C,
                         double h) {
  Matrix I = Matrix::Identity(18, 18), k1 = A, k2 = B * (I + .5 * h * k1),
         k3 = B * (I + .5 * h * k2), k4 = C * (I + h * k3);
  return I + h / 6 * (k1 + 2 * k2 + 2 * k3 + k4);
}
Prediction predict(Recipe r, const State &s, const V3 &w, const V3 &a, double h,
                   const Config &c) {
  if (isTG(r)) {
    State mid = tgFlow(s, w, a, h * .5), end = tgFlow(s, w, a, h),
          q3 = tgFlow(s, w, a, h * .75);
    Matrix A = dynamics(s, w, a), B = dynamics(mid, w, a),
           C = dynamics(end, w, a), F = transition(A, B, C, h),
           Fm = transition(B, dynamics(q3, w, a), C, h * .5),
           L0 = F * noiseRoot(s, c, true), Lm = Fm * noiseRoot(mid, c, true),
           L1 = noiseRoot(end, c, true);
    Matrix N(18, 36);
    N << sqrt(h / 6) * L0, sqrt(2 * h / 3) * Lm, sqrt(h / 6) * L1;
    return {end, F,
            h / 6 *
                (L0 * L0.transpose() + 4 * Lm * Lm.transpose() +
                 L1 * L1.transpose()),
            N};
  }
  Prediction o = physicalFlow(s, w, a, h, r == Recipe::Step1);
  Matrix Q = Matrix::Zero(15, 15), N(15, 36);
  const double xs[] = {-.7745966692414834, 0, .7745966692414834},
               ws[] = {5.0 / 9, 8.0 / 9, 5.0 / 9};
  for (int k = 0; k < 3; ++k) {
    double t = h * (xs[k] + 1) * .5;
    State mid = physicalFlow(s, w, a, t, false).state;
    Matrix L =
        physicalFlow(mid, w, a, h - t, false).F * noiseRoot(mid, c, false);
    Q += h * ws[k] * .5 * L * L.transpose();
    N.middleCols(k * 12, 12) = sqrt(h * ws[k] * .5) * L;
  }
  Matrix J0 = physicalJacobian(r, s),
         Ji = physicalJacobian(r, o.state).inverse();
  o.F = (Ji * o.F * J0).eval();
  o.Q = Ji * Q * Ji.transpose();
  o.noise_columns = Ji * N;
  return o;
}
} // namespace kf3d
