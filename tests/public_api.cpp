#include "kf3d/typed.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace kf3d;
void check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
struct ScalarPosition final : MeasurementModel {
    MeasurementLinearization evaluate(const State& x) const override {
        MeasurementLinearization o{Vector::Constant(1,2-x.p.x()),Matrix::Zero(1,15)};
        o.Hphysical(0,6)=1;return o;
    }
};
struct WrongShape final : MeasurementModel {
    MeasurementLinearization evaluate(const State&) const override {
        return {Vector::Zero(2),Matrix::Zero(3,15)};
    }
};
int main(){try {
    for(auto backend:{Backend::KF,Backend::GN,Backend::LM}) {
        auto engine=makeEstimator(Recipe::Step2,backend);
        State s; Matrix P=Matrix::Zero(15,15);P(6,6)=.4;engine->initialize(s,P);
        ScalarPosition model;Matrix noise=Matrix::Constant(1,1,.25);engine->observeMeasurement(model,noise);
        check(std::abs(engine->state().p.x()-2*.4/.65)<1e-5,"custom scalar posterior mean");
        check(std::abs(engine->physicalCovariance()(6,6)-.4*.25/.65)<1e-5,"custom scalar posterior covariance");
        State before=engine->state();Matrix covariance=engine->physicalCovariance();bool rejected=false;
        try { WrongShape invalid;engine->observeMeasurement(invalid,noise); } catch(const std::exception&){rejected=true;}
        check(rejected,"bad custom model shape rejected");
        check((engine->state().p-before.p).norm()==0 && (engine->physicalCovariance()-covariance).norm()==0,"bad custom model atomicity");
    }
    Config bad;bad.solver.initial_lambda=0;bool rejected=false;
    try {auto x=makeEstimator(Recipe::Step2,Backend::LM,bad);}catch(const std::exception&){rejected=true;}
    check(rejected,"zero LM damping rejected");
    bad=Config{};bad.lever.x()=std::numeric_limits<double>::quiet_NaN();rejected=false;
    try{auto x=makeEstimator(Recipe::Step2,Backend::KF,bad);}catch(const std::exception&){rejected=true;}
    check(rejected,"nonfinite extrinsic rejected");
    Filter<Recipe::Step5A> tg;
    TangentGroupState initial;
    Filter<Recipe::Step5A>::Covariance P=Filter<Recipe::Step5A>::Covariance::Identity()*.01;
    tg.initialize(initial,P);tg.observeVirtualBias(V3::Zero(),M3::Identity()*.001);
    check(tg.state().virtual_bias.allFinite(),"typed TG schema");
    static_assert(RecipeTraits<Recipe::C15A>::dimension==15,"reduced dimension");
    static_assert(RecipeTraits<Recipe::Step5A>::dimension==18,"TG dimension");
    std::cout<<"Public typed/custom measurement API contracts passed\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
