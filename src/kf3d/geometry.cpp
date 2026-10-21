// Formula port from slam_in_autonomous_driving research/lie_tools.h and TG_EqF.
// See repository THIRD_PARTY_NOTICES.md for source attribution.
#include "internal.hpp"
#include <cmath>
#include <stdexcept>
namespace kf3d {
M3 hat(const V3 &x) {
  M3 A;
  A << 0, -x.z(), x.y(), x.z(), 0, -x.x(), -x.y(), x.x(), 0;
  return A;
}
M3 expSO3(const V3 &x) {
  double t = x.norm();
  M3 A = hat(x);
  double a, b;
  if (t < 1e-6) {
    double s = t * t;
    a = 1 - s / 6 + s * s / 120;
    b = .5 - s / 24 + s * s / 720;
  } else {
    a = sin(t) / t;
    b = (1 - cos(t)) / (t * t);
  }
  return M3::Identity() + a * A + b * A * A;
}
V3 logSO3(const M3 &R) {
  Eigen::Quaterniond q(R);
  q.normalize();
  if (q.w() < 0)
    q.coeffs() *= -1;
  double n = q.vec().norm();
  return n < 1e-12 ? 2 * q.vec() : 2 * atan2(n, q.w()) / n * q.vec();
}
double wrap(double a) { return atan2(sin(a), cos(a)); }
bool isTG(Recipe r) { return r == Recipe::Step5A || r == Recipe::Step5B; }
bool isRight(Recipe r) {
  return r == Recipe::Step3 || r == Recipe::Step4 || isTG(r) ||
         r == Recipe::C15A || r == Recipe::C15B;
}
int dimension(Recipe r) { return isTG(r) ? 18 : 15; }
Gamma gammas(const V3 &p, const V3 &a) {
  Gamma g;
  M3 A = hat(p), power = M3::Identity();
  std::array<M3, 3> d{M3::Zero(), M3::Zero(), M3::Zero()};
  double c = 1;
  for (int k = 0; k < 40; ++k) {
    g.one += c * power;
    g.two += c / (k + 2) * power;
    for (int j = 0; j < 3; ++j) {
      g.d1.col(j) += c * d[j] * a;
      g.d2.col(j) += c / (k + 2) * d[j] * a;
    }
    if (k > 8 && (c * power).norm() < 1e-18)
      break;
    for (int j = 0; j < 3; ++j)
      d[j] = hat(V3::Unit(j)) * power + A * d[j];
    power = (A * power).eval();
    c /= k + 2;
  }
  return g;
}
Matrix algebraJacobian(const Matrix &A) {
  Matrix J = Matrix::Identity(A.rows(), A.cols()), t = J;
  for (int k = 1; k < 70; ++k) {
    t = (t * A / (k + 1)).eval();
    J += t;
    if (k > 8 && t.norm() < 1e-16)
      break;
  }
  return J;
}
Matrix ad(const Vector &x) {
  Matrix A = Matrix::Zero(x.size(), x.size());
  for (int i = 0; i < x.size(); i += 3)
    A.block<3, 3>(i, i) = hat(x.head<3>());
  for (int i = 3; i < x.size(); i += 3)
    A.block<3, 3>(i, 0) = hat(x.segment<3>(i));
  return A;
}
Matrix adjoint(const State &s) {
  Matrix A = Matrix::Zero(9, 9);
  for (int i = 0; i < 9; i += 3)
    A.block<3, 3>(i, i) = s.R;
  A.block<3, 3>(3, 0) = hat(s.v) * s.R;
  A.block<3, 3>(6, 0) = hat(s.p) * s.R;
  return A;
}
State retract(Recipe r, const State &s, const Vector &d) {
  if (d.size() != dimension(r) || !d.allFinite())
    throw std::invalid_argument("tangent dimension mismatch");
  State x = s;
  if (isRight(r)) {
    auto g = gammas(d.head<3>());
    M3 E = expSO3(d.head<3>());
    x.R = E * s.R;
    x.v = E * s.v + g.one * d.segment<3>(3);
    x.p = E * s.p + g.one * d.segment<3>(6);
  } else {
    x.R = s.R * expSO3(d.head<3>());
    x.v += d.segment<3>(3);
    x.p += d.segment<3>(6);
  }
  Vector db = d.tail(d.size() - 9);
  if (r == Recipe::Step4 || isTG(r)) {
    int k = isTG(r) ? 9 : 6;
    db = adjoint(s).topLeftCorner(k, k).inverse() *
         algebraJacobian(-ad(d.head(k))) * db;
  }
  x.bg += db.head<3>();
  x.ba += db.segment<3>(3);
  if (isTG(r))
    x.bnu += db.tail<3>();
  return x;
}
Vector local(Recipe r, const State &s, const State &x) {
  Vector d = Vector::Zero(dimension(r));
  if (isRight(r)) {
    M3 E = x.R * s.R.transpose();
    d.head<3>() = logSO3(E);
    M3 J = gammas(d.head<3>()).one;
    d.segment<3>(3) = J.partialPivLu().solve(x.v - E * s.v);
    d.segment<3>(6) = J.partialPivLu().solve(x.p - E * s.p);
  } else {
    d.head<3>() = logSO3(s.R.transpose() * x.R);
    d.segment<3>(3) = x.v - s.v;
    d.segment<3>(6) = x.p - s.p;
  }
  Vector db(d.size() - 9);
  db.head<3>() = x.bg - s.bg;
  db.segment<3>(3) = x.ba - s.ba;
  if (isTG(r))
    db.tail<3>() = x.bnu - s.bnu;
  if (r == Recipe::Step4 || isTG(r)) {
    int k = db.size();
    db = algebraJacobian(-ad(d.head(k)))
             .partialPivLu()
             .solve(adjoint(s).topLeftCorner(k, k) * db);
  }
  d.tail(db.size()) = db;
  return d;
}
Matrix physicalJacobian(Recipe r, const State &s) {
  Matrix J = Matrix::Identity(dimension(r), dimension(r));
  if (isRight(r)) {
    J.block<3, 3>(0, 0) = s.R.transpose();
    J.block<3, 3>(3, 0) = -hat(s.v);
    J.block<3, 3>(6, 0) = -hat(s.p);
  }
  if (r == Recipe::Step4 || isTG(r)) {
    int k = isTG(r) ? 9 : 6;
    J.bottomRightCorner(k, k) = adjoint(s).topLeftCorner(k, k).inverse();
  }
  return J;
}
Matrix resetJacobian(Recipe r, const Vector &d) {
  int n = dimension(r);
  if (d.size() != n || !d.allFinite())
    throw std::invalid_argument("invalid reset tangent");
  Matrix J = Matrix::Identity(n, n);
  if (r == Recipe::Step4 || isTG(r)) {
    int k = n - 9;
    Matrix A = Matrix::Zero(n, n);
    A.topLeftCorner(9, 9) = ad(d.head(9));
    A.bottomRightCorner(k, k) = ad(d.head(k));
    A.block(9, 0, k, k) = ad(d.tail(k));
    J = algebraJacobian(A);
  } else if (isRight(r))
    J.topLeftCorner(9, 9) = algebraJacobian(ad(d.head(9)));
  else
    J.block<3, 3>(0, 0) = gammas(-d.head<3>()).one;
  return J;
}
Recipe parseRecipe(const std::string &s) {
  if (s == "step1")
    return Recipe::Step1;
  if (s == "step2")
    return Recipe::Step2;
  if (s == "step3")
    return Recipe::Step3;
  if (s == "step4")
    return Recipe::Step4;
  if (s == "step5a" || s == "step5A")
    return Recipe::Step5A;
  if (s == "step5b" || s == "step5B")
    return Recipe::Step5B;
  if (s == "c15a" || s == "c15_a" || s == "C15_A")
    return Recipe::C15A;
  if (s == "c15b" || s == "c15_b" || s == "C15_B")
    return Recipe::C15B;
  throw std::invalid_argument("unknown recipe: " + s);
}
Backend parseBackend(const std::string &s) {
  if (s == "kf" || s == "KF")
    return Backend::KF;
  if (s == "gn" || s == "GN")
    return Backend::GN;
  if (s == "lm" || s == "LM")
    return Backend::LM;
  throw std::invalid_argument("unknown backend: " + s);
}
} // namespace kf3d
