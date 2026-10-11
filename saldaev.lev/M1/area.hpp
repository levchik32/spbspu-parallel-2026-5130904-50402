#ifndef AREA_HPP
#define AREA_HPP
#include <cstddef>
#include <utility>
#include <vector>
#include "shapes.hpp"

namespace saldaev
{
  std::pair< double, double > area(const std::vector< Circle > &circles, size_t threads, size_t tries, size_t seed);
  std::pair< size_t, size_t > calculate(Frame frame, const std::vector< Circle > &circles, size_t tries, size_t seed);
  Frame getBorders(const std::vector< Circle > &circles);
};

#endif
