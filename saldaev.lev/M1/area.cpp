#include "area.hpp"
#include <functional>
#include <future>
#include <limits>
#include <random>
#include <utility>
#include <vector>

std::pair< double, double > saldaev::area(const std::vector< Circle > &circles, size_t threads, size_t tries,
                                          size_t seed)
{
  std::vector< std::future< std::pair< size_t, size_t > > > futures;

  const size_t tries_per_thread = tries / threads;
  const size_t remainder = tries % threads;

  Frame frame = getBorders(circles);
  for (size_t i = 0; i < threads; ++i) {
    size_t part = tries_per_thread + (i < remainder ? 1 : 0);
    futures.push_back(std::async(std::launch::async, calculate, frame, std::cref(circles), part, seed + i));
  }

  size_t hits = 0;
  size_t complete_hits = 0;
  for (size_t i = 0; i < threads; i++) {
    const std::pair< size_t, size_t > res = futures[i].get();
    hits += res.first;
    complete_hits += res.second;
  }

  const double frame_area = (frame.max.x - frame.min.x) * (frame.max.y - frame.min.y);
  return {frame_area * (static_cast< double >(hits) / tries),
          frame_area * (static_cast< double >(complete_hits) / tries)};
}

std::pair< size_t, size_t > saldaev::calculate(Frame frame, const std::vector< Circle > &circles, size_t tries,
                                               size_t seed)
{
  std::default_random_engine gen(seed);
  std::uniform_real_distribution< double > width(frame.min.x, frame.max.x);
  std::uniform_real_distribution< double > hight(frame.min.y, frame.max.y);

  size_t hits = 0;
  size_t complete_hits = 0;

  for (size_t i = 0; i < tries; ++i) {
    const Point p{width(gen), hight(gen)};
    bool in_all = true;
    bool in_any = false;

    for (size_t j = 0; j < circles.size(); ++j) {
      if (circles[j].contain(p)) {
        in_any = true;
      } else {
        in_all = false;
      }
    }

    if (in_any) {
      hits++;
      if (in_all) {
        complete_hits++;
      }
    }
  }

  return {hits, complete_hits};
}

saldaev::Frame saldaev::getBorders(const std::vector< Circle > &circles)
{
  const double lim = std::numeric_limits< double >::infinity();

  Point min{lim, lim};
  Point max{-lim, -lim};

  for (size_t i = 0; i < circles.size(); ++i) {
    const Circle cir = circles[i];
    const Point pos = cir.position;
    const double r = cir.radius;
    min = {min.x > pos.x - r ? pos.x - r : min.x, min.y > pos.y - r ? pos.y - r : min.y};
    max = {max.x < pos.x + r ? pos.x + r : max.x, max.y < pos.y + r ? pos.y + r : max.y};
  }

  return {min, max};
}
