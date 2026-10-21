#pragma once
#include "kf3d/filter.hpp"
#include <array>
namespace kf3d {
struct Gamma {
  M3 one = M3::Zero(), two = M3::Zero(), d1 = M3::Zero(), d2 = M3::Zero();
};
Gamma gammas(const V3 &, const V3 &a = V3::Zero());
Matrix algebraJacobian(const Matrix &);
Matrix ad(const Vector &);
Matrix adjoint(const State &);
struct Prediction {
  State state;
  Matrix F, Q, noise_columns;
};
Prediction predict(Recipe, const State &, const V3 &, const V3 &, double,
                   const Config &);
bool isTG(Recipe);
bool isRight(Recipe);
int dimension(Recipe);
} // namespace kf3d
