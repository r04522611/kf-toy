#include "kf/filter.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <vector>
static double number(const std::string &s) {
  size_t n = 0;
  double x = std::stod(s, &n);
  if (n != s.size() || !std::isfinite(x))
    throw std::runtime_error("invalid number");
  return x;
}
int main(int argc, char **argv) {
  try {
    std::locale::global(std::locale::classic());
    if (argc != 4)
      throw std::runtime_error(
          "usage: replay input.csv output.csv estimate_bias|fixed_bias");
    std::string mode = argv[3];
    if (mode != "estimate_bias" && mode != "fixed_bias")
      throw std::runtime_error("unknown mode");
    std::ifstream in(argv[1]);
    if (!in)
      throw std::runtime_error("cannot open input");
    std::string line;
    std::getline(in, line);
    if (line != "t_s,gyro_z_rad_s,speed_m_s,heading_rad,truth_x_m,truth_y_m,"
                "truth_yaw_rad")
      throw std::runtime_error("invalid CSV header");
    // Validate input before creating output. Current fixture has dense truth
    // and optional heading.
    struct Row {
      double t, g, v;
      bool heading;
      double h;
    };
    std::vector<Row> rows;
    while (std::getline(in, line)) {
      std::stringstream ss(line);
      std::vector<std::string> c;
      std::string x;
      while (std::getline(ss, x, ','))
        c.push_back(x);
      if (c.size() != 7)
        throw std::runtime_error("expected 7 fields");
      Row a{number(c[0]), number(c[1]), number(c[2]), !c[3].empty(), 0};
      if (a.heading)
        a.h = number(c[3]);
      for (int j = 4; j < 7; ++j)
        (void)number(c[j]);
      if ((rows.empty() && a.t != 0) || (!rows.empty() && a.t <= rows.back().t))
        throw std::runtime_error("timestamps must start at zero and increase");
      rows.push_back(a);
    }
    if (rows.empty())
      throw std::runtime_error("empty replay");
    std::ofstream out(argv[2]);
    if (!out)
      throw std::runtime_error("cannot open output");
    out << std::setprecision(17) << "t_s,x_m,y_m,yaw_rad,bias_rad_s\n";
    kf::HeadingBiasKF filter(mode == "estimate_bias");
    for (size_t i = 0; i < rows.size(); ++i) {
      const auto &a = rows[i];
      if (i)
        filter.predict(rows[i - 1].g, rows[i - 1].v, a.t - rows[i - 1].t);
      if (a.heading)
        filter.observeHeading(a.h);
      const auto &s = filter.state();
      out << a.t << ',' << s.x << ',' << s.y << ',' << s.yaw << ',' << s.bias
          << '\n';
    }
    out.flush();
    if (!out)
      throw std::runtime_error("output write failed");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
