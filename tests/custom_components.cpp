#include "kf3d/filter.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace kf3d;
namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
// A user-defined formulation can decorate a built-in one, or replace its
// compatible equations, without implementing another Gaussian engine.
class CountingFormulation final : public Formulation {
  std::unique_ptr<Formulation> inner = makeFormulation(Recipe::Step2);
  int& count;
public:
  explicit CountingFormulation(int& value) : count(value) {}
  int dimension() const override { return 15; }
  State retract(const State& s,const Vector& d) const override { return inner->retract(s,d); }
  Vector local(const State& s,const State& x) const override { return inner->local(s,x); }
  Matrix physical(const State& s) const override { return inner->physical(s); }
  Matrix differential(const Vector& d) const override { return inner->differential(d); }
  PredictionResult predict(const State& s,const V3& w,const V3& a,double h,const Config& c) const override {
    ++count;
    return inner->predict(s,w,a,h,c);
  }
};
class CountingGn final : public NonlinearSolver {
  int& count;
public:
  explicit CountingGn(int& value):count(value){}
  Vector trialStep(const Matrix& B,const Vector& g,double alpha,double) const override {
    ++count;
    return alpha*B.ldlt().solve(g);
  }
};
class PositionX final : public MeasurementModel {
public:
  MeasurementLinearization evaluate(const State& s) const override {
    Vector residual(1); residual[0]=1-s.p.x();
    Matrix H=Matrix::Zero(1,15); H(0,6)=1;
    return {residual,H};
  }
};
}
int main() {
  try {
    Config c; int predictions=0,trials=0;
    auto estimator=makeEstimator(Recipe::Step2,Backend::GN,c,
        std::make_unique<CountingFormulation>(predictions),
        std::make_unique<CountingGn>(trials));
    estimator->initialize(State{},Matrix::Identity(15,15));
    estimator->advance(.01,V3::Zero(),V3(0,0,9.81));
    PositionX measurement; Matrix noise=Matrix::Identity(1,1);
    estimator->observeMeasurement(measurement,noise);
    require(predictions>0,"injected formulation was not consumed");
    require(trials>0,"injected nonlinear strategy was not consumed");
    require(estimator->state().p.x()>0 && estimator->state().p.x()<1,
            "custom measurement did not update posterior");
    bool rejected=false;
    try { auto bad=makeEstimator(Recipe::Step2,Backend::KF,c,
                                makeFormulation(Recipe::Step5A)); }
    catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,"incompatible formulation dimension accepted");
    rejected=false;
    try { auto bad=makeEstimator(Recipe::Step2,Backend::KF,c,nullptr,
                                makeNonlinearSolver(Backend::GN)); }
    catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,"unused KF solver accepted");
    // Explicit null dependencies preserve the normal factory behavior.
    auto builtin=makeEstimator(Recipe::Step2,Backend::GN,c);
    auto defaults=makeEstimator(Recipe::Step2,Backend::GN,c,nullptr);
    for(auto* e:{builtin.get(),defaults.get()}) {
      e->initialize(State{},Matrix::Identity(15,15));
      e->observePosition(V3(1,2,3));
    }
    require((builtin->state().p-defaults->state().p).norm()==0,
            "default dependency injection changed existing results");
    require((builtin->physicalCovariance()-defaults->physicalCovariance()).norm()==0,
            "default dependency injection changed covariance");
    std::cout << "Custom components and default compatibility passed\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}
