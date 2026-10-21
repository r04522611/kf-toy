#include "kf3d/typed.hpp"
#include <iostream>
int main() {
  using namespace kf3d;
  Filter<Recipe::Step2> filter(Backend::GN);
  PhysicalState initial;
  Filter<Recipe::Step2>::Covariance covariance =
      Filter<Recipe::Step2>::Covariance::Identity() * .01;
  filter.initialize(initial, covariance);
  filter.advance(.1, V3::Zero(), V3(0, 0, 9.81));
  filter.observePosition(V3(1, 0, 0));
  std::cout << filter.state().p.transpose() << '\n';
}
