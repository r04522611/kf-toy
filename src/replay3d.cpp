// Initialization and replay protocol adapted from sad research run_dataset.cc.
// Original: gaoxiang12/slam_in_autonomous_driving; see
// docs/source-provenance.md.
#include "kf3d/filter.hpp"
#include "utm.h"
#include <Eigen/Geometry>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace kf3d;
namespace fs = std::filesystem;
constexpr double pi = 3.14159265358979323846;
struct Imu {
  double t;
  V3 gyro, acc;
};
struct StaticInit {
  bool success = false, is_static = false;
  double start = 0;
  std::deque<Imu> samples;
  V3 bg = V3::Zero(), ba = V3::Zero(), gravity = V3::Zero(), cg = V3::Zero(),
     ca = V3::Zero();
  void odom(double l, double r) {
    if (!success)
      is_static = l < 5 && r < 5;
  }
  void imu(const Imu &x) {
    if (success)
      return;
    if (!is_static) {
      samples.clear();
      return;
    }
    if (samples.empty())
      start = x.t;
    samples.push_back(x);
    if (x.t - start > 10 && samples.size() >= 10) {
      V3 mg = V3::Zero(), ma = V3::Zero();
      for (const auto &y : samples) {
        mg += y.gyro;
        ma += y.acc;
      }
      mg /= samples.size();
      ma /= samples.size();
      if (ma.norm() < 1e-9)
        throw std::runtime_error("zero initialization acceleration");
      gravity = -ma.normalized() * 9.81;
      V3 shifted = V3::Zero();
      for (const auto &y : samples)
        shifted += y.acc + gravity;
      shifted /= samples.size();
      cg.setZero();
      ca.setZero();
      for (const auto &y : samples) {
        cg += (y.gyro - mg).cwiseAbs2();
        ca += (y.acc + gravity - shifted).cwiseAbs2();
      }
      cg /= samples.size() - 1;
      ca /= samples.size() - 1;
      if (cg.norm() <= .5 && ca.norm() <= .05) {
        bg = mg;
        ba = shifted;
        success = true;
      }
    }
    while (samples.size() > 2000)
      samples.pop_front();
  }
};
std::vector<std::string> split(const std::string &line) {
  std::vector<std::string> s;
  std::stringstream f(line);
  std::string x;
  while (std::getline(f, x, ','))
    s.push_back(x);
  if (!line.empty() && line.back() == ',')
    s.emplace_back();
  return s;
}
double number(const std::string &s) {
  size_t end = 0;
  double v = std::stod(s, &end);
  if (end != s.size() || !std::isfinite(v))
    throw std::runtime_error("invalid finite number: " + s);
  return v;
}
double yaw(const State &s, double angle) {
  V3 f = s.R * V3(std::cos(angle), std::sin(angle), 0);
  return std::atan2(f.y(), f.x());
}
int main(int argc, char **argv) {
  try {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; i += 2) {
      if (i + 1 >= argc)
        throw std::runtime_error("arguments require values");
      if (!args.emplace(argv[i], argv[i + 1]).second)
        throw std::runtime_error("duplicate argument");
    }
    for (const auto &a : args)
      if (a.first != "--input" && a.first != "--output" &&
          a.first != "--recipe" && a.first != "--backend" &&
          a.first != "--mode" && a.first != "--protocol")
        throw std::runtime_error("unknown argument " + a.first);
    for (auto k : {"--input", "--output", "--recipe", "--backend", "--mode"})
      if (!args.count(k))
        throw std::runtime_error(std::string("missing ") + k);
    const std::string protocol =
        args.count("--protocol") ? args.at("--protocol") : "archive";
    if (protocol != "archive" && protocol != "uniform")
      throw std::runtime_error("protocol must be archive|uniform");
    const auto recipe = parseRecipe(args.at("--recipe"));
    const auto backend = parseBackend(args.at("--backend"));
    const auto mode = args.at("--mode");
    if (mode != "pure" && mode != "heading")
      throw std::runtime_error("mode must be pure|heading");
    fs::path out = args.at("--output");
    if (fs::exists(out))
      throw std::runtime_error("output already exists");
    std::ifstream input(args.at("--input"));
    if (!input)
      throw std::runtime_error("missing input");
    fs::create_directories(out);
    std::ofstream states(out / "states.txt"), gnss(out / "gnss.csv"),
        cov(out / "covariance.csv"), innov(out / "innovations.csv"),
        virt(out / "virtual.csv");
    for (auto f : {&states, &gnss, &cov, &innov, &virt})
      *f << std::setprecision(17);
    gnss
        << "timestamp,fused,heading_valid,initialization,rx,ry,rz,yaw_residual_"
           "rad,nis,heading_fused,pre_rx,pre_ry,pre_rz,pre_yaw_residual_rad\n";
    cov << "timestamp";
    for (int i = 0; i < 15; ++i)
      for (int j = 0; j < 15; ++j)
        cov << ",p" << i << '_' << j;
    cov << '\n';
    innov << "timestamp,type,nis,iterations,initial_cost,final_cost,rejected_"
             "trials,status\n";
    virt << "timestamp,bnx,bny,bnz\n";
    Config config;
    config.position_std = V3::Constant(.1);
    config.wheel_std = V3::Constant(.5);
    config.heading_std = pi / 180;
    config.antenna_yaw = 12.06 * pi / 180;
    config.lever = V3(-.17, -.20, 0);
    config.max_substep = .01;
    config.solver.max_iterations = 32;
    config.solver.step_tolerance = 1e-7;
    const bool legacy = protocol == "archive" &&
                        (recipe == Recipe::Step1 || recipe == Recipe::Step2 ||
                         recipe == Recipe::Step3 || recipe == Recipe::Step4);
    if (legacy) {
      config.solver_whitening = SolverWhitening::LegacyEigen;
      config.clock_stepping = ClockStepping::LegacyAbsolute;
    }
    StaticInit init;
    std::unique_ptr<Estimator> filter;
    Imu held{};
    bool ready = false, started = false, evaluation = false;
    std::vector<double> intervals;
    double last_imu = -1, last_time = -1, last_state = -1, last_cov = -1,
           first_fix = 0, evaluation_start = 0;
    V3 origin = V3::Zero();
    unsigned long seq = 0, heading_count = 0, gnss_rows = 0;
    double propagation_seconds = 0, max_input_age = 0, max_raw_imu_gap = 0;
    std::string recording_id;
    auto advance = [&](double requested) {
      const double before = filter->state().t;
      filter->advance(requested, held.gyro, held.acc);
      const double after = filter->state().t;
      if (std::abs(after - requested) > 1e-8)
        throw std::runtime_error("advance clock mismatch");
      propagation_seconds += after - before;
      max_input_age = std::max(max_input_age, requested - held.t);
    };
    auto snapshot = [&]() {
      const State &s = filter->state();
      if (s.t - last_cov >= 1) {
        last_cov = s.t;
        cov << s.t;
        Matrix p = filter->physicalCovariance();
        for (int i = 0; i < 15; ++i)
          for (int j = 0; j < 15; ++j)
            cov << ',' << p(i, j);
        cov << '\n';
        virt << s.t << ',' << s.bnu.x() << ',' << s.bnu.y() << ',' << s.bnu.z()
             << '\n';
      }
      if (s.t - last_state < .1)
        return;
      last_state = s.t;
      Eigen::Quaterniond q(s.R);
      states << s.t << ' ' << s.p.transpose() << ' ' << q.w() << ' ' << q.x()
             << ' ' << q.y() << ' ' << q.z() << ' ' << s.v.transpose() << ' '
             << s.bg.transpose() << ' ' << s.ba.transpose() << '\n';
    };
    auto log_update = [&](const char *type) {
      const auto &d = filter->diagnostics();
      innov << filter->state().t << ',' << type << ',' << d.nis << ','
            << d.iterations << ',' << d.initial_cost << ',' << d.final_cost
            << ',' << d.rejected_trials << ',' << d.status << '\n';
    };
    std::string line;
    std::getline(input, line);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    const std::string expected =
        "recording_id,sequence,timestamp_s,type,gx_rad_s,gy_rad_s,gz_rad_s,ax_"
        "m_s2,ay_m_s2,az_m_s2,left_pulse,right_pulse,latitude_deg,longitude_"
        "deg,altitude_m,heading_deg,heading_valid";
    if (line != expected)
      throw std::runtime_error("unexpected events header");
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      auto row = split(line);
      if (row.size() != 17)
        throw std::runtime_error("event needs 17 columns");
      if (row[0].empty())
        throw std::runtime_error("empty recording_id");
      if (recording_id.empty())
        recording_id = row[0];
      if (row[0] != recording_id)
        throw std::runtime_error(
            "multiple recording ids require separate initialization/replays");
      if (number(row[1]) != static_cast<double>(seq++))
        throw std::runtime_error("sequence mismatch");
      double t = number(row[2]);
      if (t < last_time)
        throw std::runtime_error("out-of-order event");
      last_time = t;
      const std::string &kind = row[3];
      int lo = 0, hi = 0;
      if (kind == "IMU") {
        lo = 4;
        hi = 10;
      } else if (kind == "ODOM") {
        lo = 10;
        hi = 12;
      } else if (kind == "GNSS") {
        lo = 12;
        hi = 17;
      } else
        throw std::runtime_error("unknown type");
      for (int k = 4; k < 17; ++k) {
        if (k >= lo && k < hi)
          number(row[k]);
        else if (!row[k].empty())
          throw std::runtime_error("unexpected payload field");
      }
      if (kind == "IMU") {
        Imu x{t, V3(number(row[4]), number(row[5]), number(row[6])),
              V3(number(row[7]), number(row[8]), number(row[9]))};
        if (last_imu > 0)
          max_raw_imu_gap = std::max(max_raw_imu_gap, t - last_imu);
        if (last_imu > 0 && !ready && t > last_imu)
          intervals.push_back(t - last_imu);
        last_imu = t;
        if (!init.success) {
          init.imu(x);
          held = x;
          continue;
        }
        if (!ready) {
          if (intervals.empty())
            throw std::runtime_error("no initialization intervals");
          std::sort(intervals.begin(), intervals.end());
          double dt = intervals[intervals.size() / 2];
          config.density_squared << init.cg * dt, init.ca * dt,
              V3::Constant(1e-10), V3::Constant(1e-6);
          filter = makeEstimator(recipe, backend, config);
          ready = true;
          held = x;
          std::ofstream c(out / "config.json");
          c << std::setprecision(17) << "{\"sample_interval_s\":" << dt
            << ",\"density_squared\":[";
          for (int k = 0; k < 12; ++k)
            c << (k ? "," : "") << config.density_squared[k];
          c << "],\"protocol\":\"" << protocol << "\",\"clock_stepping\":\""
            << (legacy ? "legacy_absolute" : "duration")
            << "\",\"solver_whitening\":\""
            << (legacy ? "legacy_eigen" : "carried_root")
            << "\",\"position_std_m\":[0.1,0.1,0.1],\"wheel_std_m_s\":[0.5,0.5,"
               "0.5],\"heading_std_rad\":"
            << config.heading_std
            << ",\"antenna_yaw_rad\":" << config.antenna_yaw
            << ",\"lever_m\":[-0.17,-0.20,0],\"max_substep_s\":0.01,\"fixed_"
               "gravity\":true,\"solver_max_iterations\":32,\"solver_step_"
               "tolerance\":1e-7,\"solver_max_trials\":"
            << config.solver.max_trials
            << ",\"solver_initial_lambda\":" << config.solver.initial_lambda
            << ",\"solver_gradient_tolerance\":"
            << config.solver.gradient_tolerance
            << ",\"solver_cost_tolerance\":" << config.solver.cost_tolerance
            << ",\"input_convention\":\"causal previous-sample ZOH\"}\n";
          continue;
        }
        if (started) {
          advance(t);
          snapshot();
        }
        held = x;
      } else if (kind == "ODOM") {
        double l = number(row[10]), r = number(row[11]);
        init.odom(l, r);
        if (!started)
          continue;
        advance(t);
        double speed = .155 * (l + r) * .5 / 1024 * 2 * pi / .1;
        filter->observeVelocity(V3(speed, 0, 0));
        log_update("wheel");
        snapshot();
      } else {
        if (row[16] != "0" && row[16] != "1")
          throw std::runtime_error("heading validity must be 0/1");
        bool valid = row[16] == "1";
        if (!ready)
          continue;
        double lat = number(row[12]), lon = number(row[13]), east = 0,
               north = 0;
        long zone = 0;
        char hemi = 0;
        if (std::abs(lat) > 90 || std::abs(lon) > 180 ||
            Convert_Geodetic_To_UTM(lat * pi / 180, lon * pi / 180, &zone,
                                    &hemi, &east, &north) != 0)
          throw std::runtime_error("invalid GNSS coordinates");
        V3 antenna(east, north, number(row[14]));
        double heading = wrap((90 - number(row[15])) * pi / 180);
        if (!started) {
          if (!valid)
            continue;
          origin = antenna;
          State s;
          s.t = t;
          s.bg = init.bg;
          s.ba = init.ba;
          s.g = V3(0, 0, -init.gravity.norm());
          s.R = Eigen::Quaterniond::FromTwoVectors(-init.gravity, -s.g)
                    .toRotationMatrix();
          s.R = Eigen::AngleAxisd(wrap(heading - yaw(s, config.antenna_yaw)),
                                  V3::UnitZ())
                    .toRotationMatrix() *
                s.R;
          s.p = -s.R * config.lever;
          Vector stddev(15);
          stddev << 2 * pi / 180, 2 * pi / 180, config.heading_std, .1, .1, .1,
              config.position_std, .001, .001, .001, .1, .1, .1;
          Matrix p = stddev.array().square().matrix().asDiagonal();
          M3 j = s.R * hat(config.lever);
          p.block<3, 3>(6, 0) = j * p.block<3, 3>(0, 0);
          p.block<3, 3>(0, 6) = p.block<3, 3>(6, 0).transpose();
          p.block<3, 3>(6, 6) += j * p.block<3, 3>(0, 0) * j.transpose();
          filter->initialize(s, p);
          started = true;
          first_fix = t;
          gnss << t << ",1,1,1,0,0,0,0,0,0,0,0,0,0\n";
          snapshot();
          continue;
        }
        antenna -= origin;
        advance(t);
        if (!evaluation) {
          if (!valid)
            continue;
          filter->observeGnss(antenna, true, heading);
          log_update("initialization");
          const State &s = filter->state();
          V3 e = antenna - s.p - s.R * config.lever;
          double ey = wrap(heading - yaw(s, config.antenna_yaw));
          if (e.norm() > 1 || std::abs(ey) > 5 * pi / 180)
            throw std::runtime_error(
                "second-fix residual outside protocol check");
          evaluation = true;
          evaluation_start = t;
          std::ofstream f(out / "initialization.json");
          f << std::setprecision(17) << "{\"first_fix_time\":" << first_fix
            << ",\"evaluation_start\":" << t
            << ",\"position_residual_norm_m\":" << e.norm()
            << ",\"heading_residual_rad\":" << ey
            << ",\"position_updates_in_initialization\":2,\"heading_updates_in_"
               "initialization\":2,\"success\":true}\n";
          gnss << t << ",1,1,1," << e.x() << ',' << e.y() << ',' << e.z() << ','
               << ey << ",0,1,0,0,0,0\n";
          last_state = -1;
          last_cov = -1;
          snapshot();
          continue;
        }
        const State before = filter->state();
        V3 pre = antenna - before.p - before.R * config.lever;
        double pre_yaw = wrap(heading - yaw(before, config.antenna_yaw));
        bool fuse = mode == "heading" && valid;
        double nis = 0;
        if (fuse) {
          filter->observeHeading(heading);
          log_update("heading");
          nis = filter->diagnostics().nis;
          ++heading_count;
        }
        const State &s = filter->state();
        V3 post = antenna - s.p - s.R * config.lever;
        gnss << t << ",0," << valid << ",0," << post.x() << ',' << post.y()
             << ',' << post.z() << ',';
        if (valid)
          gnss << wrap(heading - yaw(s, config.antenna_yaw));
        gnss << ',' << nis << ',' << fuse << ',' << pre.x() << ',' << pre.y()
             << ',' << pre.z() << ',';
        if (valid)
          gnss << pre_yaw;
        gnss << '\n';
        ++gnss_rows;
        snapshot();
      }
    }
    if (!evaluation)
      throw std::runtime_error("two-fix initialization incomplete");
    for (auto f : {&states, &gnss, &cov, &innov, &virt}) {
      f->flush();
      if (!*f)
        throw std::runtime_error("output write failure");
    }
    std::ofstream summary(out / "runtime.json");
    summary << std::setprecision(17) << "{\"events\":" << seq
            << ",\"gnss_fused\":0,\"heading_fused\":" << heading_count
            << ",\"heldout\":" << gnss_rows
            << ",\"evaluation_start\":" << evaluation_start
            << ",\"elapsed_seconds\":" << filter->state().t - first_fix
            << ",\"propagation_seconds\":" << propagation_seconds
            << ",\"clock_error_s\":"
            << propagation_seconds - (filter->state().t - first_fix)
            << ",\"max_input_age_s\":" << max_input_age
            << ",\"max_raw_imu_gap_s\":" << max_raw_imu_gap
            << ",\"propagation_steps\":"
            << filter->diagnostics().propagation_steps << "}\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "replay3d: " << e.what() << '\n';
    return 1;
  }
}
