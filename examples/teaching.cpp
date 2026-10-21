// Deterministic, standalone teaching experiments. Truth is a closed-form
// trajectory; it does not call estimator propagation or manifold helpers.
#include "kf3d/filter.hpp"
#include <Eigen/Cholesky>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace kf3d;
namespace {
constexpr double pi = 3.14159265358979323846;
struct Noise {
  std::mt19937 rng;
  explicit Noise(unsigned seed) : rng(seed) {}
  double uniform() { return (static_cast<double>(rng()) + .5) / 4294967296.; }
  double normal() {
    return std::sqrt(-2 * std::log(uniform())) * std::cos(2 * pi * uniform());
  }
  V3 vector() { return V3(normal(), normal(), normal()); }
};
struct NamedRecipe {
  const char *name;
  Recipe value;
};
const std::vector<NamedRecipe> recipes{
    {"Step1", Recipe::Step1},   {"Step2", Recipe::Step2},
    {"Step3", Recipe::Step3},   {"Step4", Recipe::Step4},
    {"Step5A", Recipe::Step5A}, {"Step5B", Recipe::Step5B},
    {"C15A", Recipe::C15A},     {"C15B", Recipe::C15B}};
struct NamedBackend {
  const char *name;
  Backend value;
};
const std::vector<NamedBackend> backends{
    {"KF", Backend::KF}, {"GN", Backend::GN}, {"LM", Backend::LM}};
State truth(double t) {
  const double w = .3, k = .7, speed = 2 + .3 * std::sin(k * t), a = w * t;
  State s;
  s.t = t;
  s.R << std::cos(a), -std::sin(a), 0, std::sin(a), std::cos(a), 0, 0, 0, 1;
  s.v = V3(speed * std::cos(a), speed * std::sin(a), 0);
  s.p = V3(
      2 * std::sin(w * t) / w + .15 * ((1 - std::cos((k + w) * t)) / (k + w) +
                                       (1 - std::cos((k - w) * t)) / (k - w)),
      2 * (1 - std::cos(w * t)) / w + .15 * (std::sin((k - w) * t) / (k - w) -
                                             std::sin((k + w) * t) / (k + w)),
      0);
  s.bg = V3(.004, -.003, .015);
  s.ba = V3(.025, -.02, .03);
  return s;
}
Vector error(const State &estimate, const State &target) {
  Vector e(15);
  e.head(3) = logSO3(estimate.R.transpose() * target.R);
  e.segment(3, 3) = target.v - estimate.v;
  e.segment(6, 3) = target.p - estimate.p;
  e.segment(9, 3) = target.bg - estimate.bg;
  e.segment(12, 3) = target.ba - estimate.ba;
  return e;
}
struct Event {
  double t;
  V3 gyro, accel, velocity;
  double heading;
  bool update;
};
std::vector<Event> events(unsigned seed, double duration,
                          const std::string &scenario, const Config &c) {
  Noise noise(seed);
  std::vector<Event> out;
  const double dt = .025;
  int count = static_cast<int>(std::round(duration / dt));
  for (int i = 1; i <= count; ++i) {
    double t = i * dt, mid = t - .5 * dt;
    State s = truth(t);
    V3 gyro = V3(0, 0, .3) + s.bg +
              std::sqrt(c.density_squared[0] / dt) * noise.vector();
    V3 accel =
        V3(.21 * std::cos(.7 * mid), .3 * (2 + .3 * std::sin(.7 * mid)), 9.81) +
        s.ba + std::sqrt(c.density_squared[3] / dt) * noise.vector();
    if (scenario == "bias_mismatch" && t > duration / 2)
      gyro.z() += .12;
    V3 velocity =
        s.R.transpose() * s.v + c.wheel_std.cwiseProduct(noise.vector());
    double heading = .3 * t + c.heading_std * noise.normal();
    out.push_back({t, gyro, accel, velocity, heading, i % 8 == 0});
  }
  return out;
}
void remainderCsv(const std::filesystem::path &path) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "direction,scale,ordinary_remainder,midpoint_remainder\n";
  State s = truth(1);
  for (int d = 1; d <= 7; ++d)
    for (int k = 0; k <= 8; ++k) {
      double a = std::pow(.5, k);
      Vector q = Vector::Zero(15);
      q.head(3) = V3(.1, .05, -.03) / d;
      q.segment(3, 3) = V3(.3, -.2, .1);
      State x = retract(Recipe::C15A, s, a * q);
      V3 z = x.R.transpose() * x.v, r = s.R * z - s.v;
      out << d << ',' << a << ',' << (r - a * q.segment(3, 3)).norm() << ','
          << (r - a * q.segment(3, 3) - .5 * hat(r) * (a * q.head(3))).norm()
          << '\n';
    }
}
void constraintsCsv(const std::filesystem::path &path) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "scale,delta_p_x,delta_bg_y,tg_finite_bnu_z,reduced_bnu_z,subspace_"
         "prediction\n";
  // In this proved subspace eta has only p_x and db only gyro_y. The
  // physicalJacobian inverse removes first-order virtual perturbation. Finite
  // joint retraction still need not remain on the physical constraint surface.
  for (int k = -8; k <= 8; ++k) {
    double a = .05 * k;
    State s;
    Vector physical = Vector::Zero(18);
    physical[6] = a;
    physical[10] = .2;
    Vector native =
        physicalJacobian(Recipe::Step5A, s).fullPivLu().solve(physical);
    State tg = retract(Recipe::Step5A, s, native);
    Vector reduced = Vector::Zero(15);
    reduced[6] = a;
    reduced[10] = .2;
    State c = retract(Recipe::C15A, s, reduced);
    out << a << ',' << a << ",0.2," << tg.bnu.z() << ',' << c.bnu.z() << ','
        << -.5 * a * .2 << '\n';
  }
}
void recoveryCsv(const std::filesystem::path &path) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "recipe,backend,prior_yaw,posterior_yaw,initial_cost,final_cost,"
         "iterations,status\n";
  for (auto r : recipes)
    for (auto b : backends) {
      Config c;
      c.wheel_std = V3::Constant(.05);
      State s;
      s.v = V3(2, 0, 0);
      s.R = expSO3(V3(0, 0, 1.2));
      Matrix P = Matrix::Identity(15, 15) * 1e-4;
      P(2, 2) = 2;
      auto e = makeEstimator(r.value, b.value, c);
      e->initialize(s, P);
      e->observeVelocity(V3(2, 0, 0));
      auto d = e->diagnostics();
      out << r.name << ',' << b.name << ",1.2," << logSO3(e->state().R).z()
          << ',' << d.initial_cost << ',' << d.final_cost << ',' << d.iterations
          << ',' << d.status << '\n';
    }
}
void anchorCsv(const std::filesystem::path &path, unsigned seeds,
               double duration) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "scenario,seed,method,position_rmse,normalized_nees15,z_95_coverage,"
         "final_bnu_norm,anchor_variance,anchor_period\n";
  Config c;
  c.max_substep = .025;
  c.density_squared.setZero();
  c.density_squared.head(3).setConstant(1e-5);
  c.density_squared.segment(3, 3).setConstant(1e-3);
  c.wheel_std = V3::Constant(.08);
  c.heading_std = .03;
  c.virtual_bias_initial_variance = .01;
  Matrix P = Matrix::Identity(15, 15) * .01;
  for (const std::string scenario : {"matched_heading", "pure_dr"})
    for (unsigned seed = 0; seed < seeds; ++seed) {
      auto samples = events(20261020 + seed, duration, scenario, c);
      for (int method = 0; method < 3; ++method) {
        Recipe r = method == 2 ? Recipe::C15B : Recipe::Step5B;
        auto est = makeEstimator(r, Backend::KF, c);
        State prior = truth(0);
        prior.R = expSO3(V3(.02, -.03, .05));
        prior.bg.setZero();
        prior.ba.setZero();
        est->initialize(prior, P);
        double sum = 0, nees = 0;
        unsigned n = 0, coverage = 0;
        for (const auto &sample : samples) {
          est->advance(sample.t, sample.gyro, sample.accel);
          if (!sample.update)
            continue;
          est->observeVelocity(sample.velocity);
          if (scenario == "matched_heading")
            est->observeHeading(sample.heading);
          if (method == 1)
            est->observeVirtualBias(V3::Zero(), .001 * M3::Identity());
          Vector e = error(est->state(), truth(sample.t));
          Matrix C = est->physicalCovariance().topLeftCorner(15, 15);
          sum += e.segment(6, 3).squaredNorm();
          nees += e.dot(C.ldlt().solve(e));
          coverage += std::abs(e[8]) <=
                      1.959963984540054 * std::sqrt(std::max(0., C(8, 8)));
          ++n;
        }
        out << scenario << ',' << seed << ','
            << (method == 0 ? "TG_no_anchor"
                            : method == 1 ? "TG_soft_anchor" : "C15_structural")
            << ',' << std::sqrt(sum / n) << ',' << nees / (15 * n) << ','
            << static_cast<double>(coverage) / n << ','
            << est->state().bnu.norm() << ',' << (method == 1 ? .001 : 0) << ','
            << (method == 1 ? .2 : 0) << '\n';
      }
    }
}
void integrationCsv(const std::filesystem::path &path) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "input_case,sampling,recipe,dt,velocity_error,position_error,rotation_error\n";
  const double T = 1., w = 2., k = 1.7;
  Config c;
  c.density_squared.setZero();
  for (bool variable : {false, true})
    for (bool midpoint : {false, true})
      for (int steps : {1, 2, 5, 10, 20, 40})
        for (Recipe r : {Recipe::Step1, Recipe::Step2}) {
          State s;
          const double dt = T / steps;
          auto f = makeFormulation(r);
          for (int i = 0; i < steps; ++i) {
            double sample_t = (i + (midpoint ? .5 : 0.)) * dt;
            double force = variable ? 3 * std::cos(k * sample_t) : 3.;
            s = f->predict(s, V3(0, 0, w), V3(force, 0, 9.81), dt, c).state;
          }
          V3 v, p;
          if (variable) {
            double a = w + k, b = w - k;
            v = V3(1.5 * (std::sin(a*T)/a + std::sin(b*T)/b),
                   1.5 * ((1-std::cos(a*T))/a + (1-std::cos(b*T))/b), 0);
            p = V3(1.5 * ((1-std::cos(a*T))/(a*a) + (1-std::cos(b*T))/(b*b)),
                   1.5 * (T/a-std::sin(a*T)/(a*a)+T/b-std::sin(b*T)/(b*b)), 0);
          } else {
            v = V3(3*std::sin(w*T)/w, 3*(1-std::cos(w*T))/w, 0);
            p = V3(3*(1-std::cos(w*T))/(w*w),
                   3*(T/w-std::sin(w*T)/(w*w)), 0);
          }
          M3 R;
          R << std::cos(w*T),-std::sin(w*T),0,std::sin(w*T),std::cos(w*T),0,0,0,1;
          out << (variable ? "varying_force" : "held_force") << ','
              << (midpoint ? "midpoint" : "left") << ','
              << (r == Recipe::Step1 ? "Step1" : "Step2") << ',' << dt << ','
              << (s.v-v).norm() << ',' << (s.p-p).norm() << ','
              << (s.R-R).norm() << '\n';
        }
}
void so3Csv(const std::filesystem::path &path) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "angle,exp_orthogonality,first_order_orthogonality,exp_log_rotation_error,first_order_reset_error\n";
  V3 axis = V3(1,2,-3).normalized();
  for (double angle : {1e-8,.001,.01,.1,.5,1.,2.,pi-1e-5}) {
    V3 phi = angle * axis;
    M3 R = expSO3(phi), linear = M3::Identity()+hat(phi);
    Vector q = Vector::Zero(15);
    q.head(3) = phi;
    M3 exact_reset = resetJacobian(Recipe::Step2,q).topLeftCorner(3,3);
    out << angle << ',' << (R.transpose()*R-M3::Identity()).norm() << ','
        << (linear.transpose()*linear-M3::Identity()).norm() << ','
        << (expSO3(logSO3(R))-R).norm() << ','
        << (M3::Identity()-.5*hat(phi)-exact_reset).norm() << '\n';
  }
}
void assumptionsCsv(const std::filesystem::path &path) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "case,recipe,backend,prior_yaw,prior_yaw_variance,heading_observation,truth_yaw,posterior_yaw,absolute_truth_error,initial_cost,final_cost,status\n";
  for (const std::string scenario : {"correct_likelihood", "wrong_heading", "overconfident_wrong_prior"})
    for (auto r : recipes)
      for (auto b : backends) {
        Config c;
        c.heading_std = .05;
        State s;
        s.R = expSO3(V3(0,0,1.2));
        Matrix P = Matrix::Identity(15,15)*1e-4;
        P(2,2) = scenario == "overconfident_wrong_prior" ? 1e-5 : 2.;
        double z = scenario == "wrong_heading" ? .8 : 0.;
        auto e = makeEstimator(r.value,b.value,c);
        e->initialize(s,P);
        e->observeHeading(z);
        auto d = e->diagnostics();
        double yaw = std::atan2(e->state().R(1,0),e->state().R(0,0));
        out << scenario << ',' << r.name << ',' << b.name << ",1.2," << P(2,2)
            << ',' << z << ",0," << yaw << ',' << std::abs(wrap(yaw)) << ','
            << d.initial_cost << ',' << d.final_cost << ',' << d.status << '\n';
      }
}
State slipTruth(double t,double lateral) {
  const double w=.3, a=w*t;
  State s;
  s.t=t;
  s.R << std::cos(a),-std::sin(a),0,std::sin(a),std::cos(a),0,0,0,1;
  s.v=s.R*V3(2,lateral,0);
  s.p=V3(2*std::sin(a)/w+lateral*(std::cos(a)-1)/w,
         2*(1-std::cos(a))/w+lateral*std::sin(a)/w,0);
  return s;
}
void slipCsv(const std::filesystem::path &path,unsigned seeds,double duration) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "case,seed,backend,velocity_model,position_rmse,attitude_rmse,mean_velocity_nis,final_accel_bias_norm,lateral_95_coverage\n";
  const double dt=.025;
  Config c;
  c.max_substep=dt;
  c.density_squared.setZero();
  c.density_squared.head(3).setConstant(1e-5);
  c.density_squared.segment(3,3).setConstant(1e-3);
  c.wheel_std=V3::Constant(.05);
  c.heading_std=.03;
  for (double lateral : {0.,.6})
    for (unsigned seed=0;seed<seeds;++seed)
      for (auto b:backends)
        for (bool nhc:{false,true}) {
          Noise noise(20261020+seed);
          auto e=makeEstimator(Recipe::Step2,b.value,c);
          State prior=slipTruth(0,lateral);
          Matrix P=Matrix::Identity(15,15)*.01;
          e->initialize(prior,P);
          double sum_p=0,sum_R=0,sum_nis=0;
          unsigned n=0,covered=0;
          for(int i=1;i<=static_cast<int>(std::round(duration/dt));++i) {
            double t=i*dt;
            V3 gyro=V3(0,0,.3)+std::sqrt(1e-5/dt)*noise.vector();
            V3 accel=V3(-.3*lateral,.6,9.81)+std::sqrt(1e-3/dt)*noise.vector();
            e->advance(t,gyro,accel);
            if(i%8)continue;
            V3 z=V3(2,lateral,0)+.05*noise.vector();
            if(nhc){z.y()=0;z.z()=0;}
            e->observeVelocity(z);
            sum_nis+=e->diagnostics().nis;
            e->observeHeading(.3*t+.03*noise.normal());
            State truth=slipTruth(t,lateral);
            Vector err=error(e->state(),truth);
            Matrix C=e->physicalCovariance().topLeftCorner(15,15);
            V3 body_y=truth.R.col(1);
            double lateral_error=body_y.dot(err.segment(3,3));
            double lateral_variance=body_y.dot(C.block(3,3,3,3)*body_y);
            covered+=std::abs(lateral_error)<=1.959963984540054*std::sqrt(std::max(0.,lateral_variance));
            sum_p+=err.segment(6,3).squaredNorm();sum_R+=err.head(3).squaredNorm();++n;
          }
          out << (lateral==0?"no_slip":"side_slip") << ',' << seed << ',' << b.name
              << ',' << (nhc?"forward_plus_NHC":"measured_3D_velocity") << ','
              << std::sqrt(sum_p/n) << ',' << std::sqrt(sum_R/n) << ',' << sum_nis/n
              << ',' << e->state().ba.norm() << ',' << static_cast<double>(covered)/n << '\n';
        }
}

void scalarCsv(const std::filesystem::path &path) {
  std::ofstream out(path);
  out << std::setprecision(17)
      << "step,truth,measurement,estimate,variance,gain,normalized_error_"
         "squared\n";
  Noise noise(20261020);
  double x = 0, P = 1;
  for (int i = 0; i < 50; ++i) {
    double truth = .1 * i, z = truth + .2 * noise.normal();
    x += .1;
    P += .01;
    double K = P / (P + .04);
    x += K * (z - x);
    P = (1 - K) * (1 - K) * P + K * K * .04;
    out << i << ',' << truth << ',' << z << ',' << x << ',' << P << ',' << K
        << ',' << (x - truth) * (x - truth) / P << '\n';
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::filesystem::path output = argc > 1 ? argv[1] : "results/teaching";
    unsigned seeds = argc > 2 ? static_cast<unsigned>(std::stoul(argv[2])) : 4;
    double duration = argc > 3 ? std::stod(argv[3]) : 12.;
    if (seeds == 0 || !std::isfinite(duration) || duration < .2)
      throw std::runtime_error(
          "positive seed count and duration >= 0.2 seconds required");
    std::filesystem::create_directories(output);
    std::ofstream metrics(output / "metrics.csv"), trace(output / "trace.csv");
    metrics << std::setprecision(17);
    trace << std::setprecision(17);
    metrics << "scenario,seed,recipe,backend,position_rmse,attitude_rmse,"
               "normalized_nees15,z_95_coverage,mean_velocity_nis,updates\n";
    trace << "scenario,seed,recipe,backend,t,truth_x,truth_y,truth_z,x,y,z,"
             "position_error,attitude_error,bg_error,ba_error,nees15,z_covered,"
             "heading_available\n";
    Config c;
    c.max_substep = .025;
    c.density_squared.setZero();
    c.density_squared.head(3).setConstant(1e-5);
    c.density_squared.segment(3, 3).setConstant(1e-3);
    c.wheel_std = V3::Constant(.08);
    c.heading_std = .03;
    const std::vector<std::string> scenarios{
        "matched_heading", "heading_dropout", "pure_dr", "bias_mismatch"};
    Matrix P = Matrix::Zero(15, 15);
    Vector sig(15);
    sig << V3::Constant(.05), V3::Constant(.1), V3::Constant(.1),
        V3::Constant(.02), V3::Constant(.05);
    P = sig.array().square().matrix().asDiagonal();
    for (const auto &scenario : scenarios)
      for (unsigned seed = 0; seed < seeds; ++seed) {
        auto samples = events(20261020 + seed, duration, scenario, c);
        Noise prior_noise(101 + seed);
        State prior = truth(0);
        prior.R =
            prior.R * expSO3(sig.head(3).cwiseProduct(prior_noise.vector()));
        prior.v += sig.segment(3, 3).cwiseProduct(prior_noise.vector());
        prior.p += sig.segment(6, 3).cwiseProduct(prior_noise.vector());
        prior.bg += sig.segment(9, 3).cwiseProduct(prior_noise.vector());
        prior.ba += sig.segment(12, 3).cwiseProduct(prior_noise.vector());
        for (auto r : recipes)
          for (auto b : backends) {
            auto est = makeEstimator(r.value, b.value, c);
            est->initialize(prior, P);
            double sum_p = 0, sum_R = 0, sum_nees = 0, sum_nis = 0;
            unsigned count = 0, covered = 0, measurements = 0;
            for (const auto &sample : samples) {
              est->advance(sample.t, sample.gyro, sample.accel);
              bool heading =
                  scenario != "pure_dr" &&
                  (scenario != "heading_dropout" || sample.t < duration * .35 ||
                   sample.t > duration * .75);
              if (sample.update) {
                est->observeVelocity(sample.velocity);
                sum_nis += est->diagnostics().nis;
                ++measurements;
                if (heading)
                  est->observeHeading(sample.heading);
              }
              if (!sample.update)
                continue;
              State s = truth(sample.t);
              Vector e = error(est->state(), s);
              Matrix C = est->physicalCovariance().topLeftCorner(15, 15);
              Eigen::LDLT<Matrix> ldlt(C);
              double nees =
                  ldlt.info() == Eigen::Success ? e.dot(ldlt.solve(e)) : 0;
              bool cover = std::abs(e[8]) <=
                           1.959963984540054 * std::sqrt(std::max(0., C(8, 8)));
              sum_p += e.segment(6, 3).squaredNorm();
              sum_R += e.head(3).squaredNorm();
              sum_nees += nees;
              covered += cover;
              ++count;
              if (seed == 0)
                trace << scenario << ',' << seed << ',' << r.name << ','
                      << b.name << ',' << sample.t << ',' << s.p.x() << ','
                      << s.p.y() << ',' << s.p.z() << ',' << est->state().p.x()
                      << ',' << est->state().p.y() << ',' << est->state().p.z()
                      << ',' << e.segment(6, 3).norm() << ','
                      << e.head(3).norm() << ',' << e.segment(9, 3).norm()
                      << ',' << e.segment(12, 3).norm() << ',' << nees << ','
                      << cover << ',' << heading << '\n';
            }
            metrics << scenario << ',' << seed << ',' << r.name << ',' << b.name
                    << ',' << std::sqrt(sum_p / count) << ','
                    << std::sqrt(sum_R / count) << ','
                    << sum_nees / (15 * count) << ','
                    << static_cast<double>(covered) / count << ','
                    << sum_nis / measurements << ','
                    << est->diagnostics().updates << '\n';
          }
      }
    anchorCsv(output / "anchor.csv", seeds, duration);
    remainderCsv(output / "remainder.csv");
    constraintsCsv(output / "constraints.csv");
    recoveryCsv(output / "recovery.csv");
    scalarCsv(output / "scalar.csv");
    integrationCsv(output / "integration.csv");
    so3Csv(output / "so3.csv");
    assumptionsCsv(output / "assumptions.csv");
    slipCsv(output / "slip.csv", seeds, duration);
    std::ofstream manifest(output / "manifest.txt");
    manifest << "generator=kf_teaching-v2\nseeds=" << seeds
             << "\nduration=" << std::setprecision(17) << duration
             << "\ndt=0.025\nvelocity_period=0.2\nnoise_rng=mt19937 "
                "Box-Muller\nseed_base=20261020\nprior_seed_base=101\ntruth="
                "closed-form varying-speed circle, independent of estimator "
                "propagation\nheading_dropout_fraction=[0.35,0.75]\nbias_"
                "mismatch=unmodelled gyro_z step +0.12 rad/s at "
                "duration/2\nIMU_sampling=interval midpoint, held per "
                "interval\nmain_comparison_virtual_anchor=disabled\nanchor_"
                "study=TG_B_no_anchor,TG_B_soft_anchor,C15B_structural\nanchor_"
                "study_prior_virtual_variance=0.01\nanchor_study_anchor_"
                "variance=0.001\nanchor_study_anchor_period=0.2\n"
                "integration_sweep=constant_and_varying_force,left_and_midpoint_samples\n"
                "NHC_study=lateral_body_velocity_0_or_0.6_mps,Step2_all_backends\n"
                "NHC_pseudo_variance=0.0025,lateral_and_vertical_zero_constraints\n"
                "wrong_heading=constant_0.8_rad_likelihood_error\n"
                "overconfident_prior=yaw_error1.2_rad_variance1e-5\n";
    std::cout << "Wrote teaching metrics, first-seed traces, remainder, "
                 "constraints, recovery, scalar and manifest to "
              << output << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
